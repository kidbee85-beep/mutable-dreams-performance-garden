/* Mutable Dreams · Performance Garden for VIBE FM – by Kid Bunka. SPDX-License-Identifier: GPL-3.0-only */
#ifndef MD_LISTEN_H
#define MD_LISTEN_H

/* C99, allocation-free, single audio-thread owner. No library dependencies.
 * Call init once, then feed/poll/set_gate on the same audio thread. Rates:
 * 44100, 48000, 88200, 96000, 176400, 192000 Hz; other rates use 48000.
 * Input is nominally -1..1; finite overloads are clipped, NaN/Inf are silence.
 * A NULL input is silence. Nonpositive lengths do nothing. Work is scheduled
 * by samples, so splitting a stream into different blocks changes no state.
 */
enum { MD_LISTEN_WINDOW = 160, MD_LISTEN_LAGS = 152,
       MD_LISTEN_RING = 512, MD_LISTEN_HOP = 80 };

typedef struct md_listen {
    /* Public for static allocation only; treat all members as private. */
    float halfband[2][64];
    float decimator[128];
    float history[MD_LISTEN_RING * 2];
    float difference[MD_LISTEN_LAGS];
    float normalized[MD_LISTEN_LAGS];
    float sample_rate, analysis_rate;
    float envelope, activity, attack, decay, activity_alpha, gate;
    float pitch_hz, confidence, cents;
    float difference_sum;
    int stages, hb_pos[2], hb_phase[2], dec_pos, dec_phase;
    int ring_pos, filled, hop, frame_start, work_lag, max_lag, min_lag;
    int candidate_lag, candidate_note, candidate_frames, bad_frames;
    int active_samples, quiet_samples, note_age, hold_samples, quiet_hold;
    int note, onsets, releases;
} md_listen;

typedef struct {
    float level;        /* 0..1, RMS-ish, ~5 ms attack / ~120 ms release */
    float pitch_hz;     /* latest periodic estimate, 0 when unvoiced */
    float confidence;   /* 0..1: 1 minus YIN's normalized difference */
    int note;           /* stable MIDI 0..127, -1 when unvoiced */
    float cents;        /* measured deviation from stable note, clamped ±50 */
    int onset;          /* new notes / re-attacks since last poll */
    int release;        /* note ends since last poll; a change ends old note */
} md_listen_out;

void listen_init(md_listen *l, float sample_rate);
void listen_feed(md_listen *l, const float *x, int n);
void listen_poll(md_listen *l, md_listen_out *o);
void listen_set_gate(md_listen *l, float gate);

#endif
