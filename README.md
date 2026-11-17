# Mutable Dreams · Performance Garden — for VIBE FM

**Preview 0.32.0 - reviewed 17 November 2026. By Kid Bunka.** The complete Mutable Dreams 2.1 instrument (six tracks of generative ambient music, 271 Biomes,
Itinera dreams, the Mutable Dreams FM radio) as a native firmware for **VIBE FM**, built with the VIBE FM SDK – plus
what only the VIBE can do:

- **The garden listens** – IN 1 / IN 2 go into the garden's own grain cloud, echo and room. **SING** follows your voice:
  the garden plays along (FOLLOW), harmonises (HARMONY) or answers your phrase (ANSWER), in the Biome's scale.
  **MOVES** lets your loudness open a macro; **KEY** makes a sung note the key.
- **16 performance pads** in four rows, a **gesture looper** for your hands, moments, latches.
- **CV / gate** out for a modular, CV in for a macro; **DAW song position** – the sequencer follows the song and its loops.
- **A stage** (1280 × 720) with the night garden, PLACES (where the sound goes) and all 50 pages; **the Dream Console**
  editor page; **FIRST STEPS**, a guided tour for new players; six stems on OUT 3–8; 8 × 8 grids; VIBE FM Link.

## Download and play

[Download the firmware ZIP and illustrated guide](https://github.com/kidbee85-beep/mutable-dreams-performance-garden/releases/tag/v0.32.0-preview).
Load the ZIP into the official [VIBE FM plug-in](https://thefirmwarehub.com/native/), or load the WASM in the [VIBE FM Playground](https://play.thefirmwarehub.com/).
This package is for **VIBE FM**, not an FM-1 or SMK firmware. VIBE hardware verification remains pending.

### New in 0.32

Seven animated page-family backgrounds, a soft bloom when changing pages, garden-coloured navigation and pad lighting, and four plant-like macro meters on the Stage. The sound engine and parameter/state format are unchanged from the reviewed 0.31.

### Desktop checks and host notes

The exact 0.32 WASM passed 36 functional test scripts and the strict SDK checker. An additional AU/VST3 integration suite passed 26/26 checks in anyDAW Studio 0.3.61 **with the private host compatibility patch** in `docs/anydaw-vibe-080.patch`. The complete app was tested for project-file save/load, three automation lanes, a cold app restart and animated playback. The focused 120.454-second real-time run reported zero new late blocks. This is not a hardware or multi-hour soak result.

VIBE 0.8.0 exposes VST3 MIDI/Program-Change proxies that must not be replayed as saved musical controls; doing so can reload a default Biome. The AU also needs its firmware state applied after audio preparation. [Host compatibility notes](docs/anydaw-host-notes.md) explain the scope and remaining raw-chunk limitation. The standard anyDAW app has not been updated by this firmware release.

## Build

Needs wasi-sdk (tested: wasi-sdk 34, whose `clang --version` reports 23.1.0-wasi-sdk) and Python 3.

```sh
cd port
WASI_SDK_BIN=/path/to/wasi-sdk/bin python3 build_mutable_dreams.py
# -> build/mutable-dreams-garden.wasm, .json, .png (from port/icon/), .editor/ (with licenses/) and the drop-in .zip
cd ..  # return to the repository root
```

### Generated tables (only when you change their sources)

```sh
python3 engine-2.1/tools/generate_presets.py   # the 271 Biomes      -> engine-2.1/app/dsp/preset_bank.inc
python3 engine-2.1/tools/generate_satin.py     # Satin wavetables     -> engine-2.1/app/dsp/satin_tables.inc
python3 engine-2.1/tools/generate_lofi.py      # lo-fi kit samples    -> engine-2.1/app/dsp/lofi_samples.inc
python3 port/tools/make_worlds.py              # Kid Bunka's 48 worlds (port/tools/worlds/worlds.txt) -> port/garden-worlds-data.inc
```

Run the generator commands from the repository root. Each writes its file byte for byte as shipped; the build only compiles the results.

The build compiles the Mutable Dreams engine (`engine-2.1/`, C++) and the VIBE FM port (`port/`, C) into one
WebAssembly module with only the extensions VIBE FM allows (`-mno-reference-types -mno-call-indirect-overlong …`).

## Test

```sh
# From the repository root:
cd port
NODE=node   # Tested with Node 24.22.3
export WASM_VALIDATE=/path/to/wabt/bin/wasm-validate
"$NODE" official/tools/vibe-check.mjs build/mutable-dreams-garden.wasm || exit $?
for t in test_*.mjs; do
  [ "$t" = test_host.mjs ] && continue
  "$NODE" "$t" build/mutable-dreams-garden.wasm || exit $?
done
mkdir -p build/test-results
for t in tests-codex/*.mjs; do
  case "$t" in */test_host.mjs|*/review-029-host.mjs) continue ;; esac
  "$NODE" "$t" build/mutable-dreams-garden.wasm "build/test-results/$(basename "$t").json" --require-pass || exit $?
done
"$NODE" audit_controls.mjs build/mutable-dreams-garden.wasm build/test-results/audit_controls.json || exit $?
cd ..
```

`render_demo.mjs` renders an audio demo; `video/make_demo.mjs` + `video/encode.swift` (macOS) render the demo video
straight from the firmware.

## Licence and credits

- This firmware: **GPL-3.0-only** (`LICENSE`). © 2026 Kid Bunka.
- Engine: Mutable Dreams 2.1 by Kid Bunka; selected DSP components originate from **Mutable Instruments**, authored by
  **Émilie Gillet**, and keep their original **MIT** notices (`port/third_party/MIT-Mutable-Instruments.txt` and the source headers).
  `engine-2.1/LICENSE-MIT.txt` covers the engine's separate MIT-licensed parts; `engine-2.1/LICENSING-GPL.txt` explains its licensing.
- **VIBE FM SDK** © 2026 Sven Trogus, **Apache-2.0** (`port/official/sdk/LICENSE`, `NOTICE`); fonts under their own
  licences (`port/official/sdk/LICENSE-FONTS`). "VIBE FM" is a trademark of its owner; this is a firmware *built with the
  VIBE FM SDK*.
- Editor-page fonts Nunito and Barlow: SIL OFL 1.1 (licence files beside them in `port/editor/fonts`).
- The SING pitch follower (`port/listen.c`) was written together with Codex.

See `THIRD-PARTY.md` for the full list.

The lo-fi kit, Satin tables and Biome banks are generated by the engine tools. The source tree also carries
MIT-licensed Mutable Instruments Elements sample data and Plaits sound banks/wavetables; these are declared in
`fmhub.json` and covered by the Mutable Instruments notices above.

## Watch, follow and support

[Project website](https://kidbee85-beep.github.io/mutable-dreams/#performance-garden) · [YouTube / Kid Bunka](https://www.youtube.com/@kidbunka) · [Instagram @mutabledreams](https://www.instagram.com/mutabledreams/) · [Kid Bunka @kid_bunka](https://www.instagram.com/kid_bunka/)

[Support development via PayPal](https://paypal.com/donate/?business=kidbee85@gmail.com). Voluntary donations go entirely into updates, new firmware and synthesizer software projects. Thank you for helping the garden grow.
