/* Momentary RGB-pad controls for the native VIBE instrument. */
#include "pad-performance.h"

enum { FIRST_NOTE = 36 };

static float unit(float value) {
    if (!(value > 0.0f)) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

void dream_pads_clear(dream_pads *state) {
    for (unsigned i = 0; i < PAD_COUNT; ++i) {
        state->pads[i].held = 0;
        state->pads[i].has_pressure = 0;
    }
}

void dream_pads_event(dream_pads *state, const vibe_event *event) {
    if (event->type == VIBE_EV_PAD && (unsigned)event->id < PAD_COUNT) {
        dream_pad *pad = &state->pads[event->id];
        pad->held = event->down != 0;
        pad->velocity = pad->held ? unit(event->value) : 0.0f;
        pad->has_pressure = 0;
        return;
    }
    if (event->type != VIBE_EV_MIDI || (event->midi[0] & 0x0f) != state->pressure_channel) return;
    unsigned status = event->midi[0] & 0xf0;
    if (status == 0xb0 && (event->midi[1] == 120 || event->midi[1] == 123)) {
        dream_pads_clear(state);
    } else if (status == 0xa0) {
        unsigned index = (unsigned)event->midi[1] - FIRST_NOTE;
        if (index < PAD_COUNT && state->pads[index].held) {
            state->pads[index].pressure = (event->midi[2] & 127) / 127.0f;
            state->pads[index].has_pressure = 1;
        }
    } else if (status == 0xd0) {
        for (unsigned i = 0; i < PAD_COUNT; ++i) {
            if (state->pads[i].held) {
                state->pads[i].pressure = (event->midi[1] & 127) / 127.0f;
                state->pads[i].has_pressure = 1;
            }
        }
    }
}

void dream_pads_update(dream_pads *state, float attack, float release) {
    for (unsigned i = 0; i < PAD_COUNT; ++i) {
        dream_pad *pad = &state->pads[i];
        float desired = pad->has_pressure ? pad->velocity*0.42f + pad->pressure*0.58f : pad->velocity;
        float target = pad->held ? desired : 0.0f;
        pad->level += (target - pad->level) * (target > pad->level ? unit(attack) : unit(release));
    }
}


void dream_gesture_clear(dream_gesture *g) {
    g->mode = GESTURE_IDLE;
    g->length = g->wanted = g->position = g->clock = 0;
    g->gain = 0.0f;
    g->speed = 1.0f;
    g->place = 0.0f;
    for (unsigned i = 0; i < GESTURE_LANES; ++i) g->output[i] = 0.0f;
}

static void gesture_finish(dream_gesture *g) {
    unsigned any = 0;
    for (unsigned f = 0; f < g->length && !any; ++f)
        for (unsigned i = 0; i < GESTURE_LANES; ++i) any |= g->frames[f][i] > 2;
    g->mode = any ? GESTURE_LOOPING : GESTURE_IDLE;
    g->position = 0;
    g->place = 0.0f;
}

void dream_gesture_tap(dream_gesture *g, unsigned frames) {
    if (g->mode == GESTURE_IDLE) {
        g->mode = GESTURE_RECORDING;
        g->length = g->position = g->clock = 0;
        g->wanted = frames < 8 ? 8 : frames > GESTURE_FRAMES ? GESTURE_FRAMES : frames;
    } else if (g->mode == GESTURE_RECORDING) {
        gesture_finish(g);
    } else {
        g->mode = GESTURE_IDLE;
    }
}

void dream_gesture_advance(dream_gesture *g, const float live[GESTURE_LANES], unsigned n) {
    const float target = g->mode == GESTURE_LOOPING || g->mode == GESTURE_OVERDUB ? 1.0f : 0.0f;
    const float speed = n / (n + (target > g->gain ? 2200.0f : 6600.0f));
    g->gain += (target - g->gain) * speed;
    if (g->gain < 0.0001f && target == 0.0f) g->gain = 0.0f;
    g->clock += n;
    while (g->clock >= GESTURE_TICK) {
        g->clock -= GESTURE_TICK;
        if (g->mode == GESTURE_RECORDING) {
            for (unsigned i = 0; i < GESTURE_LANES; ++i)
                g->frames[g->length][i] = (unsigned char)(unit(live[i]) * 255.0f + 0.5f);
            if (++g->length >= g->wanted) gesture_finish(g);
        } else if ((g->mode == GESTURE_LOOPING || g->mode == GESTURE_OVERDUB) && g->length) {
            /* Speed and direction are a playback choice: the take itself stays as recorded. An overdub pass
             * writes where the loop plays, at the same speed, for one turn of the loop (0.18). */
            if (g->mode == GESTURE_OVERDUB) {
                for (unsigned i = 0; i < GESTURE_LANES; ++i) {
                    const unsigned char v = (unsigned char)(unit(live[i]) * 255.0f + 0.5f);
                    if (v > g->frames[g->position][i]) g->frames[g->position][i] = v;
                }
                if (g->wanted <= 1) g->mode = GESTURE_LOOPING; else --g->wanted;
            }
            const float speed = g->speed == 0.5f || g->speed == 2.0f || g->speed == -1.0f ? g->speed : 1.0f;
            g->place += speed;
            while (g->place >= (float)g->length) g->place -= (float)g->length;
            while (g->place < 0.0f) g->place += (float)g->length;
            g->position = (unsigned)g->place;
            if (g->position >= g->length) g->position = g->length - 1;
        }
    }
    if (!g->length) {
        for (unsigned i = 0; i < GESTURE_LANES; ++i) g->output[i] = 0.0f;
        return;
    }
    const unsigned at = g->mode == GESTURE_RECORDING ? g->length - 1 : g->position;
    for (unsigned i = 0; i < GESTURE_LANES; ++i)
        g->output[i] = g->mode == GESTURE_RECORDING ? 0.0f : g->frames[at][i] * (g->gain / 255.0f);
}

void dream_gesture_overdub(dream_gesture *g) {
    if (g->mode == GESTURE_LOOPING && g->length) {
        const float speed = g->speed == 0.5f || g->speed == 2.0f || g->speed == -1.0f ? g->speed : 1.0f;
        g->mode = GESTURE_OVERDUB;
        g->wanted = (unsigned)((float)g->length / (speed < 0.0f ? -speed : speed) + 0.5f);   /* ticks of one turn */
    }
    else if (g->mode == GESTURE_OVERDUB) g->mode = GESTURE_LOOPING;
}

float dream_gesture_progress(const dream_gesture *g) {
    if (g->mode == GESTURE_RECORDING) return g->wanted ? (float)g->length / g->wanted : 0.0f;
    if (g->mode == GESTURE_LOOPING || g->mode == GESTURE_OVERDUB) return g->length ? (float)g->position / g->length : 0.0f;
    return 0.0f;
}

#ifdef TEST
#include <assert.h>
#include <stdio.h>
int main(void) {
    dream_pads state = {.pressure_channel=2};
    vibe_event down = {.type=VIBE_EV_PAD,.id=8,.down=1,.value=.5f};
    dream_pads_event(&state,&down);
    dream_pads_update(&state,1.f,1.f);
    assert(state.pads[8].level==.5f);
    vibe_event pressure = {.type=VIBE_EV_MIDI,.midi={0xa2,44,127}};
    dream_pads_event(&state,&pressure);
    dream_pads_update(&state,1.f,1.f);
    assert(state.pads[8].level>.77f && state.pads[8].level<.80f && state.pads[9].level==0.f);
    pressure.midi[0]=0xa1;
    pressure.midi[2]=0;
    dream_pads_event(&state,&pressure);
    assert(state.pads[8].pressure==1.f);
    down.down=0;
    dream_pads_event(&state,&down);
    dream_pads_update(&state,1.f,.1f);
    assert(state.pads[8].level>0.f && state.pads[8].level<.77f);
    vibe_event channel = {.type=VIBE_EV_MIDI,.midi={0xd2,127,0}};
    down.down=1;
    dream_pads_event(&state,&down);
    dream_pads_event(&state,&channel);
    assert(state.pads[8].has_pressure && !state.pads[9].has_pressure);
    vibe_event clear = {.type=VIBE_EV_MIDI,.midi={0xb2,123,0}};
    dream_pads_event(&state,&clear);
    dream_pads_update(&state,1.f,1.f);
    assert(state.pads[8].level==0.f);
    static dream_gesture g;
    dream_gesture_clear(&g);
    float live[GESTURE_LANES]={0};
    dream_gesture_tap(&g,10);
    assert(g.mode==GESTURE_RECORDING);
    for(unsigned k=0;k<10;k++){live[2]=k<5?1.f:0.f;dream_gesture_advance(&g,live,GESTURE_TICK);}
    assert(g.mode==GESTURE_LOOPING&&g.length==10);
    live[2]=0.f;
    for(unsigned k=0;k<200;k++) dream_gesture_advance(&g,live,64);
    float seen=0.f;
    for(unsigned k=0;k<20;k++){dream_gesture_advance(&g,live,GESTURE_TICK);if(g.output[2]>seen)seen=g.output[2];assert(g.output[0]==0.f);}
    assert(seen>.9f);
    dream_gesture_tap(&g,10);
    assert(g.mode==GESTURE_IDLE);
    for(unsigned k=0;k<2000;k++) dream_gesture_advance(&g,live,64);
    assert(g.output[2]==0.f&&g.gain==0.f);
    dream_gesture_tap(&g,10);
    for(unsigned k=0;k<10;k++) dream_gesture_advance(&g,live,GESTURE_TICK);
    assert(g.mode==GESTURE_IDLE);
    /* overdub: a looping take with lane 2 gains lane 5 in one pass, then loops both */
    dream_gesture_clear(&g);
    dream_gesture_tap(&g,10);
    for(unsigned k=0;k<10;k++){live[2]=1.f;live[5]=0.f;dream_gesture_advance(&g,live,GESTURE_TICK);}
    assert(g.mode==GESTURE_LOOPING);
    dream_gesture_overdub(&g);
    assert(g.mode==GESTURE_OVERDUB);
    live[2]=0.f;
    for(unsigned k=0;k<10;k++){live[5]=k<4?1.f:0.f;dream_gesture_advance(&g,live,GESTURE_TICK);}
    assert(g.mode==GESTURE_LOOPING);
    unsigned both=0;
    for(unsigned f=0;f<10;f++) both+=g.frames[f][2]==255&&g.frames[f][5]==255;
    assert(both>=3);
    /* speed: half takes twice the ticks per pass, reverse walks backwards */
    dream_gesture_clear(&g);
    dream_gesture_tap(&g,8);
    for(unsigned k=0;k<8;k++){for(unsigned l=0;l<GESTURE_LANES;l++)live[l]=0.f;live[0]=k/7.f;dream_gesture_advance(&g,live,GESTURE_TICK);}
    assert(g.mode==GESTURE_LOOPING&&g.position==0);
    g.speed=0.5f;
    for(unsigned k=0;k<4;k++) dream_gesture_advance(&g,live,GESTURE_TICK);
    assert(g.position==2);
    g.speed=-1.f;
    dream_gesture_advance(&g,live,GESTURE_TICK);
    assert(g.position==1);
    dream_gesture_advance(&g,live,GESTURE_TICK);
    dream_gesture_advance(&g,live,GESTURE_TICK);
    assert(g.position==7);
    g.speed=2.f;
    dream_gesture_advance(&g,live,GESTURE_TICK);
    assert(g.position==1);
    puts("Gesture speed: half, reverse with wrap, double.");
    puts("Overdub adds a second lane in one pass and keeps the first.");
    puts("Gesture records eight lanes, loops with a ramp, stops cleanly and ignores an empty take.");
    puts("VIBE PAD velocity, per-pad MIDI pressure, channel pressure, release and all-notes-off verified.");
}
#endif
