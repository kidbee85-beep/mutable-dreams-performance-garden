# SING analysis module for Mutable Dreams / VIBE FM

Built for Kid Bunka. All work and generated artifacts stayed in this folder. No firmware integration, publishing, pushes, or messages were performed.

## Delivered

- `listen.h`, `listen.c`: the requested C99 API and statically allocated state.
- `test_listen.c`: deterministic native signal, event, latency, safety, and timing tests with a PASS/FAIL table.
- `test-results.txt`: **108/108 PASS, 0 FAIL** with the exact requested native build.
- `sanitizer-results.txt`: **103/103 functional checks PASS**, no AddressSanitizer or UndefinedBehaviorSanitizer diagnostics.
- `compile-checks.txt`: compiler, freestanding, symbol, and wasm checks; `listen.wasm.o` is the wasm object.

## Implementation

One or two 31-tap halfband FIR stages reduce the higher input rates to 44.1/48 kHz. A symmetric 63-tap Blackman FIR then decimates by five to **8.82/9.6 kHz**. The final filter has approximately 45 dB attenuation at the decimated Nyquist frequency and stronger rejection farther into the stop band. It retains the fundamental and useful lower harmonics through the requested pitch range.

Pitch detection uses YIN's cumulative mean normalized squared difference, a 0.12 periodicity threshold, the first qualifying local minimum, and parabolic interpolation of the raw difference. Each comparison uses 160 samples; lag coverage includes 65–1100 Hz. Analysis starts every 80 decimated samples. A duplicated circular history supplies a contiguous, unchanged analysis region without copying a snapshot.

Each decimated sample performs **at most two lag comparisons**. Local-minimum selection is incremental. A job finishes within 75 decimated samples, before its history can be overwritten and before the next job starts. Scheduling depends entirely on input samples, never callback boundaries.

A candidate enters the ±0.45-semitone band around its MIDI centre and must appear in two successive analysis windows. The intervening 0.10-semitone band supplies hysteresis. Activity must have lasted at least 35 ms; a committed note is held at least 35 ms. A note change releases the old note and starts the new one. Silence detection uses a separate 2.5 ms power estimate and 4 ms quiet confirmation, independent of the slower displayed level. Three invalid analysis frames also end an established note. Counters accumulate until polling and saturate safely.

## Accuracy and events

Every signal family below was tested at all six requested input rates. Each saw, voice, and weak-fundamental pitch ran for two seconds with randomized 1–256-frame blocks.

| Test | Result |
| --- | --- |
| Sines, 82.4 / 110 / 220 / 440 / 880 Hz | All 30 correct notes; maximum absolute cents output **0.805 cents** |
| Saw, five pitches | No octave errors, dropouts, or extra onsets; maximum absolute deviation 3.16 cents |
| Voice, harmonics 1–8 at 1/k, 5 Hz ±30-cent vibrato, noise | No octave errors or retriggers; maximum absolute deviation 30.50 cents, including intended vibrato |
| Weak fundamental, strong harmonics 2–6 | Fundamental retained at every pitch/rate; maximum absolute deviation 4.03 cents |
| White noise at −20 dBFS RMS, then silence | No notes or onsets |
| C4 D4 E4 G4, 200 ms notes / 30 ms gaps | Exactly four correct onsets and four releases at every rate |
| Same-note re-attack after 60 ms silence | Exactly two onsets and two releases at every rate |
| Random blocks versus fixed 128 | Identical event sequence; final entire state and outputs bit-identical |
| NaN, infinities, signalling NaN, subnormals, finite overload | All outputs finite throughout; correct A4 recovered |

The required sine tests include startup and event-count assertions; stable tracking is inspected after 100 ms. The range endpoints 65 and 1100 Hz are also tested separately with one-frame callbacks.

## Latency and level

- Required sines, including poll quantization in fixed 128-frame callbacks: **48.67–52.24 ms** to the stable note/onset.
- After idle, 22 combinations of onset phase and endpoint pitch per rate: worst measured onset **58.19 ms** with sample-by-sample polling.
- Endpoint tone to silence: release **15.96–16.56 ms** after silence starts. The melody's 30 ms gaps each produce a release.
- A level step reaches **62.7–62.8% after 5 ms**. After 120 ms of silence, displayed level is **36.8%** of its initial value. Power smoothing uses 10 ms attack / 60 ms release before the square root to obtain these amplitude responses. This is an RMS-ish performance envelope, not a calibrated RMS meter.

Polling only at callback boundaries adds up to one callback of observation delay. Attacks near the gate or poorly periodic signals can take longer than the clean synthetic signals.

## Native cost and bounded work

Measured with Apple clang 21.0.0, native ARM64 macOS, ordinary `-O2`, without fast-math. Seven 4,194,304-sample runs at 48 kHz with 128-frame callbacks gave a median **23.98 ns/input sample** (22.77–24.89), below the 40 ns target. This is **1.151 ms CPU per audio second**, about 0.115% of one core. Synthesis, polling, and per-call timers are outside this average measurement.

Separate wall-clock measurements over 20,000 calls per block mode:

| Callback frames | Average ns/sample, including timer | Largest observed call |
| --- | ---: | ---: |
| 1 | 44.70 | 13 µs |
| 128 | 22.43 | 108 µs |
| 256 | 22.24 | 87 µs |
| Random 1–256 | 22.40 | 84 µs |

These observed maxima include OS scheduling and clock overhead; they are not hardware worst-case execution guarantees. Single-frame timing is particularly dominated by the timer. The largest measured 256-frame block took **87 µs**; the largest call across all measured modes took **108 µs**.

The algorithmic bound is independent of OS timing: at 48 kHz, any 256-frame call has at most 52 decimated samples, 104 lag comparisons, **16,640 squared differences**, and one frame decision. No full-frame copy, search burst, or unbounded catch-up occurs. Higher rates produce fewer analysis samples per call and add at most two fixed FIR stages. Calls longer than 256 are supported and have linear cost in their length. Initialization clears the struct once; polling has constant cost.

## Build and safety verification

The requested command succeeds:

```sh
cc -O2 test_listen.c listen.c -o t && ./t
```

Strict C99 builds pass `-Wall -Wextra -Wpedantic -Werror`. The module also compiles with `-ffreestanding -fno-builtin`; both ordinary and freestanding native objects have **no undefined symbols**. The required wasm check succeeds:

```sh
clang --target=wasm32 -nostdlib -O2 -c listen.c -o listen.wasm.o
```

The wasm object imports linear memory only, with **no function imports**. There are no libc/libm calls, heap allocations, recursion, or runtime static initializers. Coefficient tables are immutable; all mutable state lives in the **6,488-byte struct**, well below 64 KiB. Initialization uses volatile stores to prevent implicit libc `memset`. Logarithm and square-root approximations are internal arithmetic.

The sanitizer build runs the same functional suite with `--functional`; benchmarks are intentionally measured only in the native `-O2` build because instrumentation changes execution cost.

## Integration and limits

Use one static `md_listen`, call `listen_init`, then feed mono input and poll on the same audio thread. The output counters reset on each poll; they count changes since that poll, rather than exposing an event queue. A NULL input advances silence; nonpositive lengths do nothing. Unsupported or invalid sample rates fall back to 48 kHz. The default gate is 0.02.

Input is nominally −1..1: finite overloads are clipped, and NaN/Inf are replaced by zero. Tiny inputs, analysis samples, and envelope tails are explicitly flushed; the activity threshold has a 0.00001 RMS floor even with gate zero. Build without fast-math so invalid-input checks retain IEEE float semantics.

This is a monophonic follower. Chords, strongly inharmonic sounds, breathy/aperiodic attacks, or signals whose audible harmonics imply a different fundamental remain ambiguous. A same-pitch re-attack is counted when the quiet/gate path separates the attacks; a level accent that never closes that path does not create another onset. Confidence measures periodicity, not a probability of the MIDI note being correct. Cents are clamped to ±50 while the stable note catches up to a transition. The evidence uses synthetic fixtures; real microphone/guitar recordings and execution timing on the actual VIBE FM wasm runtime remain integration checks.
