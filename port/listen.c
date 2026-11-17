/* Mutable Dreams · Performance Garden for VIBE FM – by Kid Bunka. SPDX-License-Identifier: GPL-3.0-only */
#include "listen.h"

typedef char md_listen_size_check[(sizeof(md_listen) <= 65536) ? 1 : -1];
typedef char md_listen_float_check[(sizeof(float) == sizeof(unsigned int) &&
                                   sizeof(float) == 4) ? 1 : -1];

/* Immutable coefficients, no global mutable state. At 88.2/96/176.4/192 kHz
 * first use one/two 31-tap halfband filters. The final 63-tap Blackman FIR
 * decimates 44.1/48 kHz by five to 8.82/9.6 kHz. Its cutoff is 0.065 Fs;
 * the output Nyquist frequency is well inside its stop band. Symmetry and
 * halfband zeros keep the filtering cost small. */
static const float halfband_coeff[8] = {
    -0.001700396904f, 0.002937331571f, -0.006730091366f, 0.0140938879f,
    -0.02678503582f, 0.04909896059f, -0.09693833278f, 0.3156195633f
};
static const float decimator_coeff[32] = {
    0.0f, -3.040785573e-06f, -2.72227835e-05f, -8.834363489e-05f,
    -0.0001842115794f, -0.0002861131003f, -0.0003366273063f, -0.0002573861754f,
    3.105582023e-05f, 0.0005782355953f, 0.00136717602f, 0.002282329061f,
    0.00309530453f, 0.003481275122f, 0.003074035757f, 0.001558362428f,
    -0.001213693847f, -0.005107499726f, -0.009623774467f, -0.0138818305f,
    -0.01669079117f, -0.01671365057f, -0.01270624408f, -0.00379086021f,
    0.01029149926f, 0.02901014691f, 0.05100453883f, 0.07420063439f,
    0.09607702761f, 0.1140402476f, 0.1258416771f, 0.1299554878f
};

static float clamp(float x, float low, float high)
{
    return x < low ? low : (x > high ? high : x);
}

static float clean_input(float x)
{
    union { float f; unsigned int u; } v;
    v.f = x;
    if ((v.u & 0x7f800000u) == 0x7f800000u) return 0.0f;
    if (x > -1.0e-15f && x < 1.0e-15f) return 0.0f;
    return clamp(x, -1.0f, 1.0f);
}

/* Positive normal float only. Avoids a native compiler's optional sqrtf
 * library call, including when the firmware is built without fast-math. */
static float root(float x)
{
    union { float f; unsigned int u; } v;
    float y;
    if (x <= 0.0f) return 0.0f;
    v.f = x;
    v.u = (v.u >> 1) + 0x1fc00000u;
    y = v.f;
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    return 0.5f * (y + x / y);
}

/* log2 via 2*atanh((m-1)/(m+1)); error below 0.003 cents over this range. */
static float midi_pitch(float hz)
{
    union { float f; unsigned int u; } v;
    float t, t2, p;
    int exponent;
    v.f = hz / 440.0f;
    exponent = (int)(v.u >> 23) - 127;
    v.u = (v.u & 0x007fffffu) | 0x3f800000u;
    t = (v.f - 1.0f) / (v.f + 1.0f);
    t2 = t * t;
    p = t * (2.88539008178f + t2 * (0.96179669393f +
        t2 * (0.57707801636f + t2 * (0.41219858311f +
        t2 * 0.32059889798f))));
    return 69.0f + 12.0f * ((float)exponent + p);
}

static void end_note(md_listen *l)
{
    if (l->note >= 0 && l->releases < 2147483647) ++l->releases;
    l->note = -1;
    l->candidate_note = -1;
    l->candidate_frames = 0;
    l->pitch_hz = l->confidence = l->cents = 0.0f;
}

void listen_init(md_listen *l, float sample_rate)
{
    unsigned int i;
    /* Volatile prevents a compiler from turning this into libc memset. */
    volatile unsigned char *bytes = (volatile unsigned char *)l;
    for (i = 0; i < sizeof(*l); ++i) bytes[i] = 0;
    if (!(sample_rate == 44100.0f || sample_rate == 48000.0f ||
          sample_rate == 88200.0f || sample_rate == 96000.0f ||
          sample_rate == 176400.0f || sample_rate == 192000.0f))
        sample_rate = 48000.0f;
    l->sample_rate = sample_rate;
    while (sample_rate > 50000.0f) {
        sample_rate *= 0.5f;
        ++l->stages;
    }
    l->analysis_rate = sample_rate * 0.2f;
    l->max_lag = (int)(l->analysis_rate / 65.0f) + 2;
    l->min_lag = (int)(l->analysis_rate / 1100.0f);
    /* Smooth power, then expose its root: a 10 ms power attack reaches
     * ~63% amplitude in 5 ms; a 60 ms power decay is 120 ms in amplitude. */
    l->attack = 1.0f / (0.010f * l->sample_rate);
    l->decay = 1.0f / (0.060f * l->sample_rate);
    l->activity_alpha = 1.0f / (0.0025f * l->sample_rate);
    l->hold_samples = (int)(0.035f * l->sample_rate + 0.5f);
    l->quiet_hold = (int)(0.004f * l->sample_rate + 0.5f);
    l->note = l->candidate_note = -1;
    l->gate = 0.02f;
}

void listen_set_gate(md_listen *l, float gate)
{
    if (!(gate >= 0.0f)) gate = 0.02f;
    l->gate = clamp(gate, 0.0f, 1.0f);
}

/* Independent partial sums permit SIMD even under ordinary C99 -O2 float
 * semantics. The fixed loop has exactly 160 squared differences. */
static float difference(const float *a, int lag)
{
    const float *b = a + lag;
    float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;
    int i;
    for (i = 0; i < MD_LISTEN_WINDOW; i += 4) {
        float d0 = a[i] - b[i], d1 = a[i+1] - b[i+1];
        float d2 = a[i+2] - b[i+2], d3 = a[i+3] - b[i+3];
        s0 += d0 * d0; s1 += d1 * d1;
        s2 += d2 * d2; s3 += d3 * d3;
    }
    return (s0 + s1) + (s2 + s3);
}

static void finish_frame(md_listen *l)
{
    int k = l->candidate_lag;
    if (k && l->active_samples >= l->hold_samples) {
        float left = l->difference[k-1], mid = l->difference[k];
        float right = l->difference[k+1];
        float curvature = left + right - 2.0f * mid;
        float offset = curvature > 1.0e-20f ?
            clamp(0.5f * (left - right) / curvature, -0.5f, 0.5f) : 0.0f;
        float hz = l->analysis_rate / ((float)k + offset);
        float midi;
        int note;
        if (hz < 64.5f || hz > 1110.0f) k = 0;
        else {
            l->pitch_hz = hz;
            l->confidence = clamp(1.0f - l->normalized[k], 0.0f, 1.0f);
            l->bad_frames = 0;
            midi = midi_pitch(hz);
            note = (int)(midi + 0.5f);
            /* A new candidate must enter its +/-0.45 semitone band.
             * The old note is held in the intervening Schmitt dead band.
             * Two overlapping windows confirm a change; each represents
             * ~32-34 ms of audio, and the activity must last >=35 ms. */
            if (note != l->note && midi >= (float)note - 0.45f &&
                midi <= (float)note + 0.45f && note >= 0 && note <= 127) {
                if (note != l->candidate_note) {
                    l->candidate_note = note;
                    l->candidate_frames = 1;
                } else if (l->candidate_frames < 2) ++l->candidate_frames;
                if (l->candidate_frames >= 2 &&
                    (l->note < 0 || l->note_age >= l->hold_samples)) {
                    if (l->note >= 0 && l->releases < 2147483647) ++l->releases;
                    l->note = note;
                    l->note_age = 0;
                    if (l->onsets < 2147483647) ++l->onsets;
                }
            } else {
                l->candidate_note = -1;
                l->candidate_frames = 0;
            }
            l->cents = l->note < 0 ? 0.0f :
                clamp(100.0f * (midi - (float)l->note), -50.0f, 50.0f);
            return;
        }
    }
    l->candidate_note = -1;
    l->candidate_frames = 0;
    l->pitch_hz = l->confidence = l->cents = 0.0f;
    if (l->bad_frames < 3) ++l->bad_frames;
    if (l->bad_frames >= 3 && l->note_age >= l->hold_samples) end_note(l);
}

static void analysis_sample(md_listen *l, float x)
{
    int p = l->ring_pos, slice;
    /* The threshold also prevents subnormal squared differences. */
    if (x > -1.0e-8f && x < 1.0e-8f) x = 0.0f;
    l->history[p] = l->history[p + MD_LISTEN_RING] = x;
    l->ring_pos = (p + 1) & (MD_LISTEN_RING - 1);
    if (l->filled < MD_LISTEN_WINDOW + l->max_lag + 1) {
        ++l->filled;
        return;
    }
    if (l->hop == 0) {
        /* This region cannot be overwritten for >200 analysis samples;
         * all work finishes in at most 75. No snapshot copy or spike. */
        l->frame_start = (l->ring_pos - l->filled) & (MD_LISTEN_RING - 1);
        l->work_lag = 1;
        l->difference_sum = 0.0f;
        l->candidate_lag = 0;
        l->normalized[0] = 1.0f;
        l->hop = MD_LISTEN_HOP;
    }
    --l->hop;
    /* Exactly two lag slices per decimated sample, independent of blocks.
     * The final scan is incremental too: no full-lag scan at frame end. */
    for (slice = 0; slice < 2 && l->work_lag; ++slice) {
        int lag = l->work_lag;
        float d = difference(l->history + l->frame_start, lag);
        l->difference[lag] = d;
        l->difference_sum += d;
        l->normalized[lag] = l->difference_sum > 1.0e-12f ?
            d * (float)lag / l->difference_sum : 1.0f;
        if (lag > l->min_lag && !l->candidate_lag &&
            l->normalized[lag-1] < 0.12f &&
            l->normalized[lag-1] <= l->normalized[lag-2] &&
            l->normalized[lag-1] < l->normalized[lag])
            l->candidate_lag = lag - 1;
        if (lag == l->max_lag + 1) {
            l->work_lag = 0;
            finish_frame(l);
        } else ++l->work_lag;
    }
}

void listen_feed(md_listen *l, const float *x, int n)
{
    int i;
    float threshold = l->gate * l->gate;
    if (threshold < 1.0e-10f) threshold = 1.0e-10f;
    for (i = 0; i < n; ++i) {
        float v = x ? clean_input(x[i]) : 0.0f;
        float power = v * v, y;
        int stage, j, start, p;
        l->envelope += (power > l->envelope ? l->attack : l->decay) *
            (power - l->envelope);
        l->activity += l->activity_alpha * (power - l->activity);
        if (l->envelope < 1.0e-20f) l->envelope = 0.0f;
        if (l->activity < 1.0e-20f) l->activity = 0.0f;
        if (l->note_age < l->hold_samples) ++l->note_age;
        if (l->activity < threshold) {
            l->active_samples = 0;
            if (l->quiet_samples < l->quiet_hold) ++l->quiet_samples;
            if (l->quiet_samples >= l->quiet_hold &&
                (l->note < 0 || l->note_age >= l->hold_samples)) end_note(l);
        } else {
            l->quiet_samples = 0;
            if (l->active_samples < l->hold_samples) ++l->active_samples;
        }
        for (stage = 0; stage < l->stages; ++stage) {
            float *b = l->halfband[stage];
            p = l->hb_pos[stage];
            b[p] = b[p + 32] = v;
            l->hb_pos[stage] = (p + 1) & 31;
            l->hb_phase[stage] ^= 1;
            if (l->hb_phase[stage]) break;
            start = (p + 2) & 31;
            y = 0.500808226947f * b[start + 15];
            for (j = 0; j < 8; ++j)
                y += halfband_coeff[j] * (b[start + 2*j] + b[start + 30-2*j]);
            v = y;
        }
        if (stage != l->stages) continue;
        p = l->dec_pos;
        l->decimator[p] = l->decimator[p + 64] = v;
        l->dec_pos = (p + 1) & 63;
        if (++l->dec_phase != 5) continue;
        l->dec_phase = 0;
        start = (p + 2) & 63;
        y = decimator_coeff[31] * l->decimator[start + 31];
        for (j = 1; j < 31; ++j)
            y += decimator_coeff[j] *
                (l->decimator[start + j] + l->decimator[start + 62-j]);
        analysis_sample(l, y);
    }
}

void listen_poll(md_listen *l, md_listen_out *o)
{
    o->level = clamp(root(l->envelope), 0.0f, 1.0f);
    o->pitch_hz = l->pitch_hz;
    o->confidence = l->confidence;
    o->note = l->note;
    o->cents = l->cents;
    o->onset = l->onsets;
    o->release = l->releases;
    l->onsets = l->releases = 0;
}
