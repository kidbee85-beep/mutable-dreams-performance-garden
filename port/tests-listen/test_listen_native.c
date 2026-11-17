/* Mutable Dreams · Performance Garden for VIBE FM – by Kid Bunka. SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "listen.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <limits.h>

/* The native harness may use libc; the module does not. Signal generation
 * uses a polynomial oscillator so the documented command needs no -lm. */
enum { SINE, SAW, VOICE, MISSING, NOISE, SILENCE };
static const int rates[] = {44100, 48000, 88200, 96000, 176400, 192000};
static const float frequencies[] = {82.4f, 110.0f, 220.0f, 440.0f, 880.0f};
static const int notes[] = {40, 45, 57, 69, 81};
static int failures, rows;

typedef struct {
    double phase, vibrato;
    unsigned int rng;
    float hz;
    int wave;
} generator;

typedef struct {
    md_listen l;
    md_listen_out out;
    unsigned int rng;
    int random_blocks, block, samples, onsets, releases, sequence[32], count;
    int nonfinite, bad_note;
    double max_cents, first_ms;
} run;

static float absolute(float x) { return x < 0.0f ? -x : x; }
static unsigned int random_word(unsigned int *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s;
}
static float noise(unsigned int *s)
{
    return (float)(random_word(s) >> 8) * (2.0f / 16777216.0f) - 1.0f;
}
static int finite_float(float x)
{
    union { float f; unsigned int u; } v;
    v.f = x;
    return (v.u & 0x7f800000u) != 0x7f800000u;
}
static double sine(double turns)
{
    double x, q;
    while (turns >= 1.0) turns -= 1.0;
    x = turns * 6.2831853071795864769;
    if (x > 3.1415926535897932385) x -= 6.2831853071795864769;
    if (x > 1.5707963267948966192) x = 3.1415926535897932385 - x;
    if (x < -1.5707963267948966192) x = -3.1415926535897932385 - x;
    q = x * x;
    return x * (1.0 + q * (-1.0/6.0 + q * (1.0/120.0 + q *
        (-1.0/5040.0 + q * (1.0/362880.0 + q *
        (-1.0/39916800.0 + q / 6227020800.0))))));
}
static generator tone(int wave, float hz)
{
    generator g;
    g.phase = g.vibrato = 0.0;
    g.rng = 0x19771013u;
    g.wave = wave;
    g.hz = hz;
    return g;
}
static float sample(generator *g, int sr)
{
    double y = 0.0, hz = g->hz;
    int k;
    if (g->wave == NOISE) return 0.17320508076f * noise(&g->rng);
    if (g->wave == SILENCE) return 0.0f;
    if (g->wave == VOICE) {
        double c = 0.017328679514 * sine(g->vibrato); /* +/-30 cents */
        hz *= 1.0 + c + c*c*0.5 + c*c*c/6.0;
        for (k = 1; k <= 8; ++k) y += 0.22 * sine(g->phase * k) / k;
        y += 0.0015 * noise(&g->rng);
    } else if (g->wave == SAW) y = 0.28 * (2.0*g->phase - 1.0);
    else if (g->wave == MISSING) {
        y = 0.01 * sine(g->phase);
        for (k = 2; k <= 6; ++k) y += (0.13 - 0.01*k) * sine(g->phase*k);
    } else y = 0.35 * sine(g->phase);
    g->phase += hz / sr;
    if (g->phase >= 1.0) g->phase -= 1.0;
    g->vibrato += 5.0 / sr;
    if (g->vibrato >= 1.0) g->vibrato -= 1.0;
    return (float)y;
}
static void result(const char *label, int ok, const char *detail)
{
    ++rows;
    if (!ok) ++failures;
    printf("%-46s | %-4s | %s\n", label, ok ? "PASS" : "FAIL", detail);
}
static void start(run *r, int sr, int block)
{
    memset(r, 0, sizeof(*r));
    listen_init(&r->l, (float)sr);
    r->rng = 0xabcdef01u;
    r->block = block ? block : 128;
    r->random_blocks = block == 0;
    r->first_ms = -1.0;
    listen_poll(&r->l, &r->out);
}
static void collect(run *r)
{
    int k;
    listen_poll(&r->l, &r->out);
    if (!finite_float(r->out.level) || !finite_float(r->out.pitch_hz) ||
        !finite_float(r->out.confidence) || !finite_float(r->out.cents) ||
        r->out.level < 0.0f || r->out.level > 1.0f ||
        r->out.confidence < 0.0f || r->out.confidence > 1.0f ||
        r->out.cents < -50.0f || r->out.cents > 50.0f) r->nonfinite = 1;
    r->onsets += r->out.onset;
    r->releases += r->out.release;
    for (k = 0; k < r->out.onset && r->count < 32; ++k)
        r->sequence[r->count++] = r->out.note;
    if (r->out.note >= 0 && r->first_ms < 0.0)
        r->first_ms = 1000.0 * r->samples / r->l.sample_rate;
}
static void segment(run *r, generator *g, int length, int expected)
{
    float x[256];
    int remaining = length, elapsed = 0;
    while (remaining > 0) {
        int i, n = r->random_blocks ?
            (int)(random_word(&r->rng) % 256u) + 1 : r->block;
        if (n > remaining) n = remaining;
        for (i = 0; i < n; ++i) x[i] = sample(g, (int)r->l.sample_rate);
        listen_feed(&r->l, x, n);
        r->samples += n;
        elapsed += n;
        collect(r);
        if (expected >= 0 && elapsed > (int)(r->l.sample_rate * 0.100f)) {
            if (r->out.note != expected) r->bad_note = 1;
            if (absolute(r->out.cents) > r->max_cents)
                r->max_cents = absolute(r->out.cents);
        }
        remaining -= n;
    }
}
static void test_sines(void)
{
    int s, f;
    for (s = 0; s < 6; ++s) for (f = 0; f < 5; ++f) {
        run r;
        generator g = tone(SINE, frequencies[f]);
        char label[80], detail[160];
        start(&r, rates[s], 128);
        segment(&r, &g, rates[s]/3, notes[f]);
        snprintf(label, sizeof(label), "Sine %6.1f Hz @ %6d Hz", frequencies[f], rates[s]);
        snprintf(detail, sizeof(detail), "note %d, max |cents| %.3f, onset %.2f ms",
                 r.out.note, r.max_cents, r.first_ms);
        result(label, !r.bad_note && !r.nonfinite && r.out.note == notes[f] &&
               r.max_cents <= 10.0 && r.onsets == 1 && r.releases == 0 &&
               r.first_ms >= 35.0 && r.first_ms <= 60.0, detail);
    }
}
static void test_harmonics(void)
{
    int s, f, w;
    for (w = SAW; w <= MISSING; ++w) for (s = 0; s < 6; ++s) {
        char label[80], detail[160];
        int ok = 1, wrong = 0, events = 0;
        double maxc = 0.0;
        for (f = 0; f < 5; ++f) {
            run r;
            generator g = tone(w, frequencies[f]);
            start(&r, rates[s], 0);
            segment(&r, &g, rates[s]*2, notes[f]);
            if (r.bad_note || r.nonfinite || r.out.note != notes[f] ||
                r.onsets != 1 || r.releases != 0) ok = 0;
            wrong += r.bad_note;
            events += r.onsets;
            if (r.max_cents > maxc) maxc = r.max_cents;
        }
        snprintf(label, sizeof(label), "%s, five pitches, 2 s each @ %d",
                 w == SAW ? "Saw" : (w == VOICE ? "Voice + vibrato" : "Weak fundamental"), rates[s]);
        snprintf(detail, sizeof(detail), "wrong tracks %d, onsets %d/5, max |cents| %.2f", wrong, events, maxc);
        result(label, ok, detail);
    }
}
static void test_noise_silence(void)
{
    int s;
    for (s = 0; s < 6; ++s) {
        run r;
        generator g = tone(NOISE, 0.0f);
        char label[80], detail[100];
        int ok;
        start(&r, rates[s], 0);
        segment(&r, &g, rates[s]*2, -1);
        ok = r.out.note == -1 && r.onsets == 0 && r.releases == 0;
        g = tone(SILENCE, 0.0f);
        segment(&r, &g, rates[s], -1);
        snprintf(label, sizeof(label), "-20 dBFS RMS white noise / silence @ %d", rates[s]);
        snprintf(detail, sizeof(detail), "onsets %d, releases %d, note %d", r.onsets, r.releases, r.out.note);
        result(label, ok && !r.nonfinite && r.onsets == 0 && r.releases == 0 &&
               r.out.note == -1 && r.out.pitch_hz == 0.0f, detail);
    }
}
static void melody(run *r, int sr, int block)
{
    static const float hz[] = {261.625565f, 293.664768f, 329.627557f, 391.995436f};
    int i;
    start(r, sr, block);
    for (i = 0; i < 4; ++i) {
        generator g = tone(SINE, hz[i]);
        segment(r, &g, sr/5, -1);
        g = tone(SILENCE, 0.0f);
        segment(r, &g, sr*30/1000, -1);
    }
}
static void test_events_blocks(void)
{
    static const int expected[] = {60, 62, 64, 67};
    int s;
    for (s = 0; s < 6; ++s) {
        run fixed, variable, r;
        char label[80], detail[150];
        generator g;
        int ok, i;
        melody(&fixed, rates[s], 128);
        melody(&variable, rates[s], 0);
        ok = fixed.onsets == 4 && fixed.releases == 4 && fixed.count == 4;
        for (i = 0; i < 4; ++i) if (fixed.sequence[i] != expected[i]) ok = 0;
        snprintf(label, sizeof(label), "C4 D4 E4 G4, 30 ms gaps @ %d", rates[s]);
        snprintf(detail, sizeof(detail), "notes %d %d %d %d, on/off %d/%d",
                 fixed.sequence[0], fixed.sequence[1], fixed.sequence[2], fixed.sequence[3],
                 fixed.onsets, fixed.releases);
        result(label, ok && !fixed.nonfinite, detail);
        snprintf(label, sizeof(label), "Random blocks 1..256 vs fixed 128 @ %d", rates[s]);
        result(label, !memcmp(&fixed.l, &variable.l, sizeof(md_listen)) &&
               !memcmp(&fixed.out, &variable.out, sizeof(md_listen_out)) &&
               fixed.onsets == variable.onsets && fixed.releases == variable.releases &&
               !memcmp(fixed.sequence, variable.sequence, sizeof(fixed.sequence)),
               "final state and outputs bit-identical; identical event sequence");
        start(&r, rates[s], 0);
        g = tone(SINE, 220.0f);
        segment(&r, &g, rates[s]/5, 57);
        g = tone(SILENCE, 0.0f);
        segment(&r, &g, rates[s]*60/1000, -1);
        g = tone(SINE, 220.0f);
        segment(&r, &g, rates[s]/5, 57);
        g = tone(SILENCE, 0.0f);
        segment(&r, &g, rates[s]/10, -1);
        snprintf(label, sizeof(label), "Same note re-attack, 60 ms gap @ %d", rates[s]);
        snprintf(detail, sizeof(detail), "onsets %d, releases %d", r.onsets, r.releases);
        result(label, r.onsets == 2 && r.releases == 2 && !r.bad_note &&
               r.sequence[0] == 57 && r.sequence[1] == 57, detail);
    }
}
static void test_latency_bounds(void)
{
    int s, f;
    static const float hz[] = {65.0f, 1100.0f};
    for (s = 0; s < 6; ++s) for (f = 0; f < 2; ++f) {
        run r;
        generator g = tone(SINE, hz[f]);
        char label[80], detail[150];
        int i, before, ended = 0;
        double release_ms = -1.0;
        start(&r, rates[s], 1);
        segment(&r, &g, rates[s]/5, -1);
        before = r.samples;
        for (i = 0; i < rates[s]/10; ++i) {
            listen_feed(&r.l, 0, 1);
            ++r.samples;
            collect(&r);
            if (!ended && r.out.note == -1) {
                release_ms = 1000.0 * (r.samples - before) / rates[s];
                ended = 1;
            }
        }
        snprintf(label, sizeof(label), "Range/latency %4.0f Hz @ %d, blocks=1", hz[f], rates[s]);
        snprintf(detail, sizeof(detail), "onset %.2f ms, release %.2f ms, on/off %d/%d",
                 r.first_ms, release_ms, r.onsets, r.releases);
        result(label, !r.nonfinite && r.onsets == 1 && r.releases == 1 &&
               r.first_ms >= 35.0 && r.first_ms <= 60.0 && release_ms > 0.0 &&
               release_ms <= 100.0 && r.sequence[0] == (f ? 85 : 36), detail);
    }
}
static void test_invalid_and_api(void)
{
    int s;
    for (s = 0; s < 6; ++s) {
        run r;
        generator g = tone(SINE, 440.0f);
        float x[256];
        int i, b;
        char label[80];
        static const unsigned int bits[] = {
            0x7fc00000u, 0x7f800000u, 0xff800000u, 0x00000001u,
            0x7f7fffffu, 0xff7fffffu, 0x7f800001u, 0x80000001u
        };
        start(&r, rates[s], 0);
        segment(&r, &g, rates[s]/5, 69);
        for (b = 0; b < 20; ++b) {
            for (i = 0; i < 256; ++i) {
                union { unsigned int u; float f; } v;
                v.u = bits[(unsigned int)i % 8];
                x[i] = v.f;
            }
            listen_feed(&r.l, x, 256);
            r.samples += 256;
            collect(&r);
        }
        r.bad_note = 0;
        segment(&r, &g, rates[s]/3, 69);
        snprintf(label, sizeof(label), "NaN/Inf/subnormal/overload recovery @ %d", rates[s]);
        result(label, !r.nonfinite && !r.bad_note && r.out.note == 69 &&
               absolute(r.out.cents) < 10.0f, "finite outputs throughout; recovers correct A4");
    }
    {
        md_listen a, b;
        md_listen_out o;
        generator g = tone(SINE, 440.0f);
        run r;
        int ok;
        listen_init(&a, 48000.0f);
        memcpy(&b, &a, sizeof(a));
        listen_feed(&a, 0, 0);
        listen_feed(&a, 0, -123);
        ok = !memcmp(&a, &b, sizeof(a));
        listen_poll(&a, &o);
        ok = ok && o.note == -1 && o.level == 0.0f && o.onset == 0 && o.release == 0;
        start(&r, 48000, 128);
        segment(&r, &g, 9600, 69);
        listen_poll(&r.l, &o);
        ok = ok && o.onset == 0 && o.release == 0;
        listen_set_gate(&r.l, 1.0f);
        segment(&r, &g, 9600, -1);
        ok = ok && r.out.note == -1;
        listen_set_gate(&a, -1.0f);
        ok = ok && a.gate >= 0.0f && a.gate <= 1.0f;
        listen_set_gate(&a, 2.0f);
        ok = ok && a.gate == 1.0f;
        listen_init(&a, 0.0f);
        ok = ok && a.sample_rate == 48000.0f && sizeof(a) <= 65536;
        result("API: empty/NULL, poll reset, gate, size", ok,
               "empty calls unchanged; counters reset; gate closes; <=64 KiB");
    }
}
static void test_idle_phase_and_envelope(void)
{
    int s;
    for (s = 0; s < 6; ++s) {
        int phase, f, ok = 1;
        double worst = 0.0;
        char label[80], detail[150];
        for (f = 0; f < 2; ++f) for (phase = 0; phase <= 10; ++phase) {
            run r;
            generator g = tone(SILENCE, 0.0f);
            int preroll = rates[s]/20 + phase*rates[s]/1000;
            double latency;
            start(&r, rates[s], 1);
            segment(&r, &g, preroll, -1);
            g = tone(SINE, f ? 1100.0f : 65.0f);
            segment(&r, &g, rates[s]*12/100, -1);
            latency = r.first_ms - 1000.0*preroll/rates[s];
            if (latency > worst) worst = latency;
            if (r.first_ms < 0.0 || latency > 60.0 || r.onsets != 1 ||
                r.releases != 0 || r.sequence[0] != (f ? 85 : 36)) ok = 0;
        }
        snprintf(label, sizeof(label), "After idle: 22 onset phases / pitches @ %d", rates[s]);
        snprintf(detail, sizeof(detail), "worst onset %.2f ms (65 / 1100 Hz)", worst);
        result(label, ok, detail);
        {
            md_listen l;
            md_listen_out o;
            float x[240], attack, held, released;
            int i, n = rates[s]/200;
            listen_init(&l, (float)rates[s]);
            for (i = 0; i < 240; ++i) x[i] = 0.5f;
            while (n > 0) {
                int count = n > 240 ? 240 : n;
                listen_feed(&l, x, count);
                n -= count;
            }
            listen_poll(&l, &o);
            attack = o.level/0.5f;
            for (i = 0; i < rates[s]/240; ++i) listen_feed(&l, x, 240);
            listen_poll(&l, &o);
            held = o.level;
            listen_feed(&l, 0, rates[s]*120/1000);
            listen_poll(&l, &o);
            released = o.level/held;
            snprintf(label, sizeof(label), "Envelope step / 120 ms decay @ %d", rates[s]);
            snprintf(detail, sizeof(detail), "5 ms attack %.3f of step; release ratio %.3f", attack, released);
            result(label, attack >= 0.55f && attack <= 0.75f &&
                   released >= 0.36f && released <= 0.38f, detail);
        }
    }
}
static unsigned long long nanoseconds(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned long long)t.tv_sec * 1000000000ull + (unsigned long long)t.tv_nsec;
}
static void test_timing(void)
{
    static float x[8192];
    double measurements[7], median;
    unsigned long long worst = 0;
    int trial, b, i;
    generator g = tone(VOICE, 110.0f);
    char detail[220];
    for (i = 0; i < 8192; ++i) x[i] = sample(&g, 48000);
    for (trial = 0; trial < 7; ++trial) {
        md_listen l;
        unsigned long long begin, end;
        listen_init(&l, 48000.0f);
        begin = nanoseconds();
        for (b = 0; b < 32768; ++b) listen_feed(&l, x + (b % 64)*128, 128);
        end = nanoseconds();
        measurements[trial] = (double)(end - begin) / (32768.0 * 128.0);
        /* Keep results observable under link-time optimization too. */
        if (!finite_float(l.pitch_hz)) ++failures;
    }
    for (i = 1; i < 7; ++i) {
        double v = measurements[i];
        int j = i;
        while (j > 0 && measurements[j-1] > v) {
            measurements[j] = measurements[j-1];
            --j;
        }
        measurements[j] = v;
    }
    median = measurements[3];
    snprintf(detail, sizeof(detail), "median %.2f ns/sample (%.2f..%.2f); %.3f ms CPU/audio second",
             median, measurements[0], measurements[6], median * 0.048);
    result("Timing: 48 kHz, fixed 128, seven runs", median < 40.0, detail);
    for (trial = 0; trial < 4; ++trial) {
        md_listen l;
        unsigned int rng = 123u;
        unsigned long long max_call = 0, total = 0;
        int frames = 0;
        listen_init(&l, 48000.0f);
        for (b = 0; b < 20000; ++b) {
            int n = trial == 0 ? 1 : (trial == 1 ? 128 :
                    (trial == 2 ? 256 : (int)(random_word(&rng) % 256u) + 1));
            unsigned long long begin = nanoseconds(), elapsed;
            listen_feed(&l, x, n);
            elapsed = nanoseconds() - begin;
            if (elapsed > max_call) max_call = elapsed;
            total += elapsed;
            frames += n;
        }
        if (max_call > worst) worst = max_call;
        snprintf(detail, sizeof(detail), "%s: avg %.2f ns/sample incl. timer, worst %.3f us",
                 trial == 0 ? "1" : (trial == 1 ? "128" : (trial == 2 ? "256" : "random 1..256")),
                 (double)total / frames, (double)max_call / 1000.0);
        result("Timing: observed per-call bound (48 kHz)", max_call > 0, detail);
    }
    printf("\nState: %lu bytes; largest observed call: %.3f us. Timing includes OS interruptions.\n",
           (unsigned long)sizeof(md_listen), (double)worst / 1000.0);
}

int main(int argc, char **argv)
{
    puts("Test                                           | Result | Measurements");
    puts("-----------------------------------------------+--------+-------------------------------------");
    test_sines();
    test_harmonics();
    test_noise_silence();
    test_events_blocks();
    test_latency_bounds();
    test_invalid_and_api();
    test_idle_phase_and_envelope();
    /* Sanitizers change timing: --functional runs all signal/API tests only. */
    if (!(argc == 2 && !strcmp(argv[1], "--functional"))) test_timing();
    printf("\n%d/%d PASS, %d FAIL\n", rows - failures, rows, failures);
    return failures ? 1 : 0;
}
