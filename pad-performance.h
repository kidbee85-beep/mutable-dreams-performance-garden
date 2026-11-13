#pragma once
#include "vibe.h"

enum { PAD_COUNT = 16 };
typedef struct {
    float velocity, pressure, level;
    unsigned held, has_pressure;
} dream_pad;
typedef struct {
    dream_pad pads[PAD_COUNT];
    unsigned pressure_channel;
} dream_pads;

void dream_pads_clear(dream_pads *state);
void dream_pads_event(dream_pads *state, const vibe_event *event);
void dream_pads_update(dream_pads *state, float attack, float release);

/* A captured movement of the eight texture and space pads. It records one
 * frame every GESTURE_TICK engine samples for a fixed number of bars and then
 * loops as a relative layer on top of whatever the hands play live. */
enum { GESTURE_LANES = 8, GESTURE_FRAMES = 6400, GESTURE_TICK = 441 };   /* 6400: 8 bars down to 30 BPM (0.18) */
enum { GESTURE_IDLE, GESTURE_RECORDING, GESTURE_LOOPING, GESTURE_OVERDUB };
typedef struct {
    unsigned char frames[GESTURE_FRAMES][GESTURE_LANES];
    unsigned mode, length, wanted, position, clock;
    float gain, output[GESTURE_LANES];
    float speed, place;       /* playback: 0.5, 1, 2 or -1 (reverse); place = fractional loop position */
} dream_gesture;

void dream_gesture_clear(dream_gesture *g);
/* Tap: idle -> record `frames` ticks, recording -> loop what exists, looping -> stop. */
void dream_gesture_tap(dream_gesture *g, unsigned frames);
/* Advance by n engine samples. `live` are the eight lane levels 0..1. */
void dream_gesture_advance(dream_gesture *g, const float live[GESTURE_LANES], unsigned n);
float dream_gesture_progress(const dream_gesture *g);
/* SHIFT + GESTURE while looping: one more pass adds what is played now (the stronger value stays). */
void dream_gesture_overdub(dream_gesture *g);
