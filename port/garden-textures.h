#pragma once
/* Four playable natural textures and a tempo pulse, added to the engine's
 * 44.1 kHz stereo before the host-rate adapter. They never touch the patch. */
#include <stdint.h>

enum { TEXTURE_FOREST, TEXTURE_AIR, TEXTURE_WATER, TEXTURE_STARDUST, TEXTURE_COUNT };
enum { TEXTURE_SPARKS = 8 };

typedef struct { float phase, step, glide, amp, decay, pan; } texture_spark;
typedef struct {
    uint32_t seed[2];
    float band[2][2], air_lp[2], air_hp[2], water_lp[2];
    float rustle, rustle_target, breath, pulse_phase, pulse_gain;
    unsigned rustle_clock;
    texture_spark sparks[TEXTURE_SPARKS];
    float chime_hz[3];                         /* the station ident: three bell notes, 0.14 s apart */
    unsigned chime_wait[3];
} garden_textures;

void garden_textures_clear(garden_textures *t);
/* Mutable Dreams FM: a soft three-note ident (root, fifth, octave of the Biome's key) */
void garden_textures_chime(garden_textures *t, int root, int scale);
/* levels: TEXTURE_COUNT values 0..1. pulse 0..1 with tempo in BPM.
 * root/scale pick stardust pitches through the engine's own quantiser. */
void garden_textures_render(garden_textures *t, float *stereo, unsigned frames,
                            const float levels[TEXTURE_COUNT], float pulse, float tempo,
                            int root, int scale);
