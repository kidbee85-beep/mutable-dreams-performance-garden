/* Forest, air, water and stardust for the TEXTURE pad row. 44.1 kHz only. */
#include "garden-textures.h"
#include "vibe_dsp.h"
#include "dsp/dreams.h"

static float noise(uint32_t *s) {
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return (float)(int32_t)x * (1.0f / 2147483648.0f);
}
static float chance(uint32_t *s) { return noise(s) * 0.5f + 0.5f; }

void garden_textures_clear(garden_textures *t) {
    *t = (garden_textures){0};
    t->seed[0] = 0x6d2b79f5u;
    t->seed[1] = 0x1b873593u;
    t->rustle = t->rustle_target = 0.4f;
}

static void spark(garden_textures *t, float hz, float glide, float decay, float amp) {
    for (unsigned i = 0; i < TEXTURE_SPARKS; ++i) {
        texture_spark *s = &t->sparks[i];
        if (s->amp > 0.0001f) continue;
        s->phase = 0.0f;
        s->step = hz / MD_RATE;
        s->glide = glide;
        s->decay = decay;
        s->amp = amp;
        s->pan = 0.15f + 0.7f * chance(&t->seed[0]);
        return;
    }
}

void garden_textures_chime(garden_textures *t, int root, int scale) {
    const int base = 72 + root;
    const int notes[3] = {base, md_quantize(base + 7, root, scale), base + 12};
    for (unsigned i = 0; i < 3; i++) { t->chime_hz[i] = vd_mtof((float)notes[i]); t->chime_wait[i] = 1u + i * 6174u; }
}

void garden_textures_render(garden_textures *t, float *lr, unsigned n,
                            const float level[TEXTURE_COUNT], float pulse, float tempo,
                            int root, int scale) {
    const float forest = level[TEXTURE_FOREST], air = level[TEXTURE_AIR];
    const float water = level[TEXTURE_WATER], dust = level[TEXTURE_STARDUST];
    const int beds = forest > 0.0001f || air > 0.0001f || water > 0.0001f;
    const float pulse_step = (tempo < 30.f ? 30.f : tempo) * (2.0f / 60.0f) / MD_RATE;
    for (unsigned k = 0; k < n; ++k) {
        float out[2] = {0.0f, 0.0f};
        if (beds) {
            if (++t->rustle_clock >= 2205) {
                const float r = chance(&t->seed[0]);
                t->rustle_clock = 0;
                t->rustle_target = 0.2f + 0.8f * r * r;
            }
            t->rustle += (t->rustle_target - t->rustle) * 0.0015f;
            t->breath += 0.12f / MD_RATE;
            if (t->breath >= 1.0f) t->breath -= 1.0f;
            const float breath = 0.55f + 0.45f * vd_sin(t->breath);
            for (unsigned c = 0; c < 2; ++c) {
                const float x = noise(&t->seed[c]);
                // Forest: a 500 Hz to 2.5 kHz band whose leaves rustle in gusts.
                t->band[c][0] += (x - t->band[c][0]) * 0.30f;
                t->band[c][1] += (x - t->band[c][1]) * 0.069f;
                out[c] += (t->band[c][0] - t->band[c][1]) * forest * t->rustle * 0.085f;
                // Air: breathing high-passed hiss with the harshest top removed.
                t->air_hp[c] += (x - t->air_hp[c]) * 0.25f;
                t->air_lp[c] += ((x - t->air_hp[c]) - t->air_lp[c]) * 0.63f;
                out[c] += t->air_lp[c] * air * breath * 0.05f;
                // Water: a low murmur under the droplets.
                t->water_lp[c] += (x - t->water_lp[c]) * 0.04f;
                out[c] += t->water_lp[c] * water * 0.11f;
            }
        }
        if (water > 0.0001f && chance(&t->seed[1]) < water * (9.0f / MD_RATE))
            spark(t, 700.0f + 1700.0f * chance(&t->seed[1]), 1.00005f, 0.9991f, 0.018f + 0.026f * water);
        if (dust > 0.0001f && chance(&t->seed[1]) < dust * (3.5f / MD_RATE)) {
            const int note = md_quantize(84 + (int)(chance(&t->seed[1]) * 14.0f), root, scale);
            spark(t, vd_mtof((float)note), 1.0f, 0.99993f, 0.014f + 0.018f * dust);
        }
        for (unsigned c = 0; c < 3; c++)
            if (t->chime_wait[c] && !--t->chime_wait[c]) spark(t, t->chime_hz[c], 1.0f, 0.99988f, 0.05f);
        for (unsigned i = 0; i < TEXTURE_SPARKS; ++i) {
            texture_spark *s = &t->sparks[i];
            if (s->amp <= 0.0001f) continue;
            const float y = vd_sin(s->phase) * s->amp;
            out[0] += y * (1.0f - s->pan);
            out[1] += y * s->pan;
            s->phase += s->step;
            if (s->phase >= 1.0f) s->phase -= 1.0f;
            s->step *= s->glide;
            if (s->step > 0.45f) s->step = 0.45f;
            s->amp *= s->decay;
        }
        // Pulse: an eighth-note breath of the whole mix, deeper with the pad.
        t->pulse_phase += pulse_step;
        if (t->pulse_phase >= 1.0f) t->pulse_phase -= 1.0f;
        // The dip opens over the first 3 % of the eighth so the wrap never clicks.
        const float fall = 1.0f - t->pulse_phase;
        const float rise = t->pulse_phase < 0.03f ? t->pulse_phase * (1.0f / 0.03f) : 1.0f;
        const float gain = 1.0f - 0.62f * pulse * rise * fall * fall * fall;
        lr[2 * k] = (lr[2 * k] + out[0]) * gain;
        lr[2 * k + 1] = (lr[2 * k + 1] + out[1]) * gain;
    }
}
