/* Garden Listens (0.24): what the VIBE has and the FM-1 has not – the audio inputs, CV and gate.
 *
 * LISTEN  IN 1 / IN 2 go into the garden's own effects: the grain tape (CLOUD), the echo and the room, and straight
 *         through. A loud input makes room in the garden (DUCK). HOLD freezes the cloud with the input in it.
 * SING    listen.c follows the pitch at IN 1. The garden answers on a track, in the Biome's scale:
 *         FOLLOW plays what you sing, HARMONY a third or a fifth above it, ANSWER waits until you stop and sings
 *         your phrase back, changed a little (the call and response of a garden).
 * CV      CV OUT / GATE OUT follow a track's notes (1 V/oct), CV IN moves one of the four macros.
 *
 * Nothing here runs unless the player switches it on; with the input off the firmware sounds as in 0.23.
 * Included by adapter-check.c before garden-state.inc (the settings are saved in garden.bin 10). */
#include "listen.h"

enum { SING_OFF, SING_FOLLOW, SING_HARMONY, SING_ANSWER, SING_MODES };
enum { CV_IN_OFF = 0 };               /* 1..4 = the macros DENSITY .. SPACE */
typedef struct {
    int32_t on, mode, stereo, song_free;               /* song_free (0.25): 1 = ignore the DAW's song position */
    float through, cloud, echo, room, duck;
    int32_t sing, sing_track, sing_octave, sing_sense;
    int32_t cv_track, cv_in, cv_gate_ms, cv_octave;
    int32_t moves, moves_base;                          /* (0.26) the input's loudness moves a macro (0 off, 1..4), its resting value */
} garden_listen;
typedef struct { int32_t a[4]; float f[5]; int32_t b[8]; } garden_listen10;   /* garden.bin 10 (0.24 / 0.25) */
static garden_listen lis = {0, 0, 0, 0, 1.f, .55f, .3f, .45f, 0.f, SING_OFF, 0, 0, 50, -1, CV_IN_OFF, 60, 0, 0, -1};
static const char *const listen_modes[3] = {"LINE", "MIC", "HI-Z"};
static const char *const sing_modes[SING_MODES] = {"OFF", "FOLLOW", "HARMONY", "ANSWER"};
static const char *const cv_in_names[5] = {"OFF", "DENSITY", "MOTION", "COLOUR", "SPACE"};
static void say(const char *text);

static void listen_ear_reset(void);       /* (0.30) below */
static void listen_apply(void) {
    static int was_on = -1;                 /* (0.30, Codex review 029) the ear forgets what it heard when the input goes off / on */
    if (was_on >= 0 && was_on != lis.on) listen_ear_reset();
    was_on = lis.on;
    if (lis.on) {
        vibe_request(VIBE_REQ_AUDIO_INPUT);
        vibe_input_mode(0, lis.mode);
        vibe_input_mode(1, lis.mode);
        vibe_input_link(lis.stereo);
    }
    md_input_set(lis.on ? lis.cloud : 0.f, lis.on ? lis.echo : 0.f, lis.on ? lis.room : 0.f, lis.on ? lis.duck : 0.f);
    md_input_through(lis.on ? lis.through : 0.f);   /* (0.30) */
}

/* ---- what the audio flow leaves for the pictures (written in vibe_on_audio, read when drawing) ------------- */
enum { SCOPE = 96 };
static float lis_scope[SCOPE];        /* the last input, one value per ~1.5 ms, for the moon pond */
static unsigned lis_scope_at;
static float lis_level, lis_peak_hold;
static md_listen lis_ear;
static float lis_rate = 48000.f;            /* (0.30) the host's rate, for a fresh ear */
static md_listen_out lis_heard;       /* the last poll */
static int lis_note = -1;             /* the note being sung, -1 none */
static int lis_answer_note = -1;      /* the note the garden plays right now (for the picture) */
static float lis_answer_glow;
static float input_l[VIBE_MAX_BLOCK], input_r[VIBE_MAX_BLOCK], input_e[2 * (VIBE_MAX_BLOCK + 8)];

/* ---- SING ---------------------------------------------------------------------------------------------------- */
enum { PHRASE = 16 };
static int8_t phrase[PHRASE];
static float phrase_len[PHRASE];              /* (0.25) seconds each sung note lasted (was audio blocks: Codex review 04) */
static int phrase_n, phrase_play, phrase_wait;   /* wait: blocks of silence until the answer starts */
static float phrase_clock;
static unsigned sing_rng = 0x2545f491u;
static int sing_played = -1;

static int sing_scale_note(int note) {
    const md_patch *p = md_state();
    note += 12 * lis.sing_octave;
    while (note < 24) note += 12;
    while (note > 96) note -= 12;
    return md_quantize(note, p->root, p->scale);
}
/* the next note of the Biome's scale above `note`, `steps` times */
static int sing_scale_up(int note, int steps) {
    const md_patch *p = md_state();
    int n = note;
    for (int s = 0; s < steps; s++) {
        int guard = 0;
        do { n++; } while (md_quantize(n, p->root, p->scale) != n && ++guard < 12);
    }
    return n > 96 ? n - 12 : n;
}
static void sing_fire(int note) {
    const int tr = lis.sing_track;
    if (md_command_space() < 2) return;
    md_command(MD_TRIGGER, tr, note);
    sing_played = note;
    lis_answer_note = note;
    lis_answer_glow = 1.f;
}

/* Once per audio block, after listen_feed. `seconds` = this block's length. */
static unsigned lis_onsets;                /* (0.30) every new sung note, counted – KEY waits for a fresh one */
static void sing_tick(float seconds) {
    listen_poll(&lis_ear, &lis_heard);
    if (lis_heard.onset > 0 && lis_heard.note >= 0) lis_onsets++;
    const int heard = lis_heard.note;
    if (lis_answer_glow > 0.f) lis_answer_glow -= seconds * 1.4f;
    if (!lis.on || lis.sing == SING_OFF) { lis_note = heard; phrase_n = phrase_play = 0; return; }
    const int started = lis_heard.onset > 0 && heard >= 0;
    if (lis.sing == SING_FOLLOW || lis.sing == SING_HARMONY) {
        if (started) {
            const int n = sing_scale_note(heard);
            sing_fire(lis.sing == SING_HARMONY ? sing_scale_up(n, (sing_rng >> 9) & 1 ? 4 : 2) : n);
            sing_rng = sing_rng * 1664525u + 1013904223u;
        }
    } else if (lis.sing == SING_ANSWER) {
        if (started) {
            if (phrase_play) phrase_play = 0, phrase_n = 0;            /* singing again interrupts the answer */
            if (phrase_n < PHRASE) { phrase[phrase_n] = (int8_t)sing_scale_note(heard); phrase_len[phrase_n] = 0.f; phrase_n++; }
            phrase_wait = 0;
        }
        if (phrase_n && !phrase_play) {
            if (heard >= 0 || lis_heard.level > .03f) { phrase_wait = 0; if (phrase_len[phrase_n - 1] < 60.f) phrase_len[phrase_n - 1] += seconds; }
            else {
                /* a beat of silence after the call: the garden answers */
                const float tempo = (float)md_state()->tempo;
                phrase_clock += seconds;
                if (phrase_clock > 60.f / (tempo < 30.f ? 30.f : tempo)) { phrase_play = 1; phrase_wait = 0; phrase_clock = 0.f; }
            }
            if (heard >= 0) phrase_clock = 0.f;
        }
        if (phrase_play) {
            /* the answer: the same rhythm in eighths of the tempo, the phrase a little changed – the last note moves
               to a neighbour of the scale, every other answer runs backwards */
            const float tempo = (float)md_state()->tempo, eighth = 30.f / (tempo < 30.f ? 30.f : tempo);
            phrase_clock -= seconds;
            if (phrase_clock <= 0.f) {
                const int k = phrase_play - 1;
                if (k >= phrase_n) { phrase_play = 0; phrase_n = 0; phrase_clock = 0.f; return; }
                const int reverse = (sing_rng >> 7) & 1;
                const int source = reverse ? phrase_n - 1 - k : k;
                int note = phrase[source];
                if (k == phrase_n - 1) note = sing_scale_up(note, (sing_rng >> 11) & 1 ? 1 : 2);
                sing_fire(note);
                phrase_play++;
                phrase_clock = eighth * (phrase_len[source] > eighth * 1.5f ? 2.f : 1.f);   /* a long sung note: a quarter */
                if (phrase_play > phrase_n) sing_rng = sing_rng * 1664525u + 1013904223u;
            }
        }
    }
    lis_note = heard;
}

/* ---- MOVES and KEY (0.26) -------------------------------------------------------------------------------------
 * MOVES: the input's loudness lifts one macro above where it rests (a singer breathes the garden open); off: back.
 * KEY: the next note sung becomes the Biome's root. Both outside the audio flow (draw / event rate). */
static int moves_sent = -1, key_armed;
static unsigned key_arm_mark;              /* (0.30) the onset count when KEY was armed */
static int moves_macro_value(int m) { return m >= 1 && m <= 4 ? md_state()->macro[m - 1] : 0; }
static void moves_set(int m) {
    if (lis.moves >= 1 && lis.moves <= 4 && lis.moves_base >= 0 && md_command_space() > 2) md_command(MD_MACRO, lis.moves - 1, lis.moves_base);
    lis.moves = m < 0 || m > 4 ? 0 : m;
    lis.moves_base = lis.moves ? moves_macro_value(lis.moves) : -1;
    moves_sent = -1;
}
static void moves_follow(void) {
    if (lis.moves < 1 || lis.moves > 4 || lis.moves_base < 0) return;
    {   /* (0.30, Codex review 029) someone turned the macro while MOVES runs: that is the new resting value */
        const int cur = moves_macro_value(lis.moves);
        if (moves_sent >= 0 && (cur - moves_sent > 8 || moves_sent - cur > 8)) {
            int base = cur - (moves_sent - lis.moves_base);
            lis.moves_base = base < 0 ? 0 : base > 1000 ? 1000 : base;
            moves_sent = cur;
        }
    }
    float e = lis.on ? lis_level * 2.5f : 0.f;
    e = e > 1.f ? 1.f : e;
    const int value = lis.moves_base + (int)((float)(1000 - lis.moves_base) * .85f * e);
    if (moves_sent < 0 || value - moves_sent > 8 || moves_sent - value > 8) {
        if (md_command_space() > 4 && md_command(MD_MACRO, lis.moves - 1, value)) moves_sent = value;
    }
}
static void key_follow(void) {
    if (!key_armed || !lis.on) return;
    if (lis_note >= 0 && lis_onsets != key_arm_mark && md_command_space() > 2) {   /* (0.30) a note sung after arming */
        md_command(MD_ROOT, 0, lis_note % 12);
        key_armed = 0;
        static const char *const nn[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        static char line[24] = "THE GARDEN IS NOW IN ";
        unsigned k = 21; const char *q = nn[lis_note % 12]; while (*q && k < 23) line[k++] = *q++; line[k] = 0;
        say(line);
    }
}

/* ---- the DAW's song position (0.25) ---------------------------------------------------------------------------
 * While the host plays and tells where it is (SDK 8), the sequencer stands where the song stands: at PLAY, after a
 * loop or a jump it is placed at the host's sixteenth. Small differences are left alone (no stumbling). MIDI page F2. */
static uint32_t song_locates;
static void song_follow(void) {
    if (lis.song_free) return;
    const vibe_transport_info *tp = vibe_transport();
    if (!tp || !tp->has || !tp->playing || !md_playing()) return;
    if (!(tp->beat == tp->beat) || tp->beat < 0.) return;
    const double host = tp->beat * 4.0, here = md_transport_position();
    if (here < 0.) return;
    const double d = host > here ? host - here : here - host;
    if (d > .25) { md_transport_locate(host); song_locates++; }
}
static void song_play_starts(void) {                     /* with the host's PLAY: start where the song is */
    if (lis.song_free) return;
    const vibe_transport_info *tp = vibe_transport();
    if (tp && tp->has && tp->beat == tp->beat && tp->beat > 0.) md_transport_locate(tp->beat * 4.0);
}

/* ---- CV / GATE ---------------------------------------------------------------------------------------------- */
static uint32_t cv_events_seen;
static float cv_gate_left;
static int cv_note = -1, cv_macro_sent = -1;
static float cv_in_volts;
static void cv_tick(float seconds) {
    cv_in_volts = vibe_cv_in();
    if (lis.cv_track < 0) return;
    const md_stats *s = md_status();
    const int t = lis.cv_track;
    if (s->events[t] != cv_events_seen) {
        cv_events_seen = s->events[t];
        const int note = s->last_note[t];
        if (note >= 0) {
            cv_note = note;
            float v = (float)(note - 36 + 12 * lis.cv_octave) / 12.f;   /* C2 = 0 V */
            vibe_cv_out(v < 0.f ? 0.f : v > 5.f ? 5.f : v);
        }
        vibe_gate_out(1);
        cv_gate_left = (float)lis.cv_gate_ms * .001f;
    } else if (cv_gate_left > 0.f) {
        cv_gate_left -= seconds;
        if (cv_gate_left <= 0.f) vibe_gate_out(0);
    }
}
/* CV IN moves a macro: 0..+5 V = 0..100 %, outside the audio flow (a command only when it really moved) */
static void cv_in_follow(void) {
    if (lis.cv_in <= CV_IN_OFF || lis.cv_in > 4) { cv_macro_sent = -1; return; }
    float v = cv_in_volts / 5.f;
    v = v < 0.f ? 0.f : v > 1.f ? 1.f : v;
    const int value = (int)(v * 1000.f + .5f);
    if (cv_macro_sent < 0 || value - cv_macro_sent > 8 || cv_macro_sent - value > 8) {
        if (md_command_space() > 4 && md_command(MD_MACRO, lis.cv_in - 1, value)) cv_macro_sent = value;
    }
}

/* ---- the audio flow --------------------------------------------------------------------------------------------
 * Before the engine renders `fresh` frames: the host block of IN 1/IN 2 at the engine's rate into md_input_push,
 * and into the ear. After: the dry input through to OUT L/R. */
/* (0.25) IN 1 / IN 2 to the engine's 44.1 kHz: a continuous band-limited resampler (Codex review 05). A windowed
 * sinc (8 zero crossings, Blackman, cut-off 0.45 of the engine rate) read from a ring of host samples that runs on
 * across blocks; no allocation, a fixed amount of work per output sample. At 44.1 kHz the input goes straight in. */
enum { RS_ZC = 8, RS_RES = 256, RS_TAB = RS_ZC * RS_RES + 2, RS_RING = 1024, RS_OUT = 2 * VIBE_MAX_BLOCK + 16 };
static float rs_tab[RS_TAB];
static float rs_ring[2][RS_RING];
static double rs_written, rs_pos;                     /* host samples written; the next output's place (host samples) */
static float rs_step = 1.f, rs_fc2 = 1.f, rs_half;    /* host samples per output; 2 x cut-off (cycles / host sample); half width */
static int rs_bypass = 1;
static float rs_out[2 * RS_OUT];
/* Filter coefficients are computed only at init. The SDK's fast visual sine is too approximate here. */
static float rs_sine(float phase) {
    phase -= (float)(int)phase;
    if (phase < 0.f) phase += 1.f;
    float x = phase * 6.2831853071795864769f;
    if (x > 3.1415926535897932385f) x -= 6.2831853071795864769f;
    if (x > 1.5707963267948966192f) x = 3.1415926535897932385f - x;
    if (x < -1.5707963267948966192f) x = -3.1415926535897932385f - x;
    const float z = x * x;
    return x * (1.f + z * (-1.f/6.f + z * (1.f/120.f + z * (-1.f/5040.f + z *
           (1.f/362880.f + z * (-1.f/39916800.f + z * (1.f/6227020800.f)))))));
}
static void rs_init(float host_rate) {
    rs_step = host_rate / 44100.f;
    rs_bypass = rs_step > .9999f && rs_step < 1.0001f;
    rs_fc2 = .9f / (rs_step > 1.f ? rs_step : 1.f);
    rs_half = (float)RS_ZC / rs_fc2;
    for (int i = 0; i < RS_TAB; i++) {
        const float u = (float)i / (float)RS_RES, x = u / (float)RS_ZC;
        const float sinc = i == 0 ? 1.f : rs_sine(u * .5f) / (3.14159265f * u);
        const float win = x >= 1.f ? 0.f : .42f + .5f * rs_sine(x * .5f + .25f) + .08f * rs_sine(x + .25f);
        rs_tab[i] = sinc * win;
    }
    rs_written = 0.; rs_pos = (double)rs_half;
    for (int c = 0; c < 2; c++) for (int i = 0; i < RS_RING; i++) rs_ring[c][i] = 0.f;
}
static unsigned rs_run(const float *l, const float *r, int frames, int stereo) {
    for (int i = 0; i < frames; i++) {
        const unsigned at = (unsigned)(((unsigned long long)rs_written + (unsigned)i) & (RS_RING - 1));
        rs_ring[0][at] = l[i]; rs_ring[1][at] = r[i];
    }
    rs_written += frames;
    unsigned n = 0;
    while (n < RS_OUT && rs_pos + rs_half < rs_written - 1.) {
        const double first = rs_pos - rs_half;
        long long k0 = (long long)first; if ((double)k0 < first) k0++;
        const long long k1 = (long long)(rs_pos + rs_half);
        float al = 0.f, ar = 0.f;
        for (long long k = k0; k <= k1; k++) {
            float d = (float)((double)k - rs_pos); if (d < 0.f) d = -d;
            const float u = d * rs_fc2 * (float)RS_RES;
            const int iu = (int)u;
            if (iu >= RS_TAB - 1) continue;
            const float w = rs_tab[iu] + (rs_tab[iu + 1] - rs_tab[iu]) * (u - (float)iu);
            const unsigned at = (unsigned)((unsigned long long)k & (RS_RING - 1));
            al += rs_ring[0][at] * w;
            if (stereo) ar += rs_ring[1][at] * w;
        }
        al *= rs_fc2; ar = stereo ? ar * rs_fc2 : al;
        rs_out[2 * n] = al; rs_out[2 * n + 1] = ar;
        n++;
        rs_pos += rs_step;
    }
    /* (keep the doubles small; shift by a multiple of the ring so its index stays aligned – Codex probe resampler-counter) */
    if (rs_written > 1e9) { rs_written -= 536870912.; rs_pos -= 536870912.; }
    return n;
}

/* ---- the audio flow --------------------------------------------------------------------------------------------
 * Before the engine renders: the host block of IN 1/IN 2 at the engine's rate into md_input_push, and into the ear.
 * After: the dry input through to OUT L/R, gliding, with a gentle ceiling while the input is on. */
static float feed_tail;                               /* (0.30) seconds the input still flows after INPUT OFF: THROUGH glides out */
static void listen_audio_before(const float *in1, int frames, unsigned fresh, float host_rate) {
    (void)fresh;
    if (lis.on) feed_tail = .25f; else if (feed_tail > 0.f) feed_tail -= (float)frames / host_rate;
    const int hear = lis.on || feed_tail > 0.f;
    if (!hear) {
        lis_level *= .9f;
        sing_tick((float)frames / host_rate);
        if (lis.cv_track >= 0 || lis.cv_in) cv_tick((float)frames / host_rate);
        return;
    }
    const float *in2 = lis.stereo ? vibe_input(1) : 0;
    float peak = 0.f;
    for (int i = 0; i < frames; i++) {
        float l = in1 ? in1[i] : 0.f, r = in2 ? in2[i] : l;
        if (!(l == l) || l > 4.f || l < -4.f) l = 0.f;
        if (!(r == r) || r > 4.f || r < -4.f) r = 0.f;
        input_l[i] = l; input_r[i] = r;
        const float al = l < 0.f ? -l : l, ar = r < 0.f ? -r : r, a = al > ar ? al : ar;   /* (0.30) IN 2 counts too in stereo */
        if (a > peak) peak = a;
    }
    if (lis.on) listen_feed(&lis_ear, input_l, frames);
    {   /* (0.30) into the engine – the effects and THROUGH, which now lives in the engine's own mix (REC hears it) */
        if (rs_bypass) {
            for (int i = 0; i < frames; i++) { input_e[2 * i] = input_l[i]; input_e[2 * i + 1] = input_r[i]; }
            md_input_push(input_e, (unsigned)frames);
        } else {
            const unsigned n = rs_run(input_l, input_r, frames, lis.stereo);
            if (n) md_input_push(rs_out, n);
        }
    }
    lis_level += (peak - lis_level) * (peak > lis_level ? .5f : .06f);
    if (peak > lis_peak_hold) lis_peak_hold = peak; else lis_peak_hold *= .995f;
    /* the scope: a value every ~1.5 ms */
    const int stride = (int)(host_rate / 660.f) > 1 ? (int)(host_rate / 660.f) : 1;
    static int scope_phase;
    static float scope_peak;
    for (int i = 0; i < frames; i++) {
        const float al = input_l[i] < 0.f ? -input_l[i] : input_l[i], ar = input_r[i] < 0.f ? -input_r[i] : input_r[i];
        const float m = 2.f * (al > ar ? al : ar);                         /* (0.30) no cancelling between IN 1 and IN 2 */
        if (m > scope_peak) scope_peak = m;
        if (++scope_phase >= stride) { scope_phase = 0; lis_scope[lis_scope_at++ % SCOPE] = scope_peak; scope_peak = 0.f; }   /* the loudness, not the raw wave (no aliasing) */
    }
    sing_tick((float)frames / host_rate);
    cv_tick((float)frames / host_rate);
}
/* a soft ceiling: untouched up to 0.8, then a smooth knee to at most 1.0 (no hard clip). Only while the input path
 * is in use, so INPUT OFF stays sample-identical to 0.23 (Codex review 02 / 04). */
static inline float listen_ceiling(float x) {
    const float a = x < 0.f ? -x : x;
    if (a <= .8f) return x;
    float t = (a - .8f) * 5.f;                         /* 0.. */
    if (t > 3.f) t = 3.f;
    const float y = .8f + .2f * (t * (27.f + t * t) / (27.f + 9.f * t * t));   /* tanh-like, reaches 1.0 at t = 3 */
    return x < 0.f ? -y : y;
}
/* (0.30) the ceiling, while the input path is in use: on the engine's mix before REC, and once more at the host's rate
 * (its output interpolation may overshoot a hair). INPUT OFF and no tail: untouched, sample-identical to 0.23. */
static int listen_ceiling_on(void) { return lis.on || feed_tail > 0.f; }
static void listen_ceiling_stereo(float *lr, unsigned n) {
    if (!listen_ceiling_on()) return;
    for (unsigned i = 0; i < 2 * n; i++) lr[i] = listen_ceiling(lr[i]);
}
static void listen_audio_after(float *left, float *right, int frames) {
    if (!listen_ceiling_on()) return;
    for (int i = 0; i < frames; i++) { left[i] = listen_ceiling(left[i]); right[i] = listen_ceiling(right[i]); }
}

static void listen_init_all(float rate) {
    lis_rate = rate;
    listen_init(&lis_ear, rate);
    listen_set_gate(&lis_ear, .004f + (100 - lis.sing_sense) * .0006f);
    rs_init(rate);
}
static void listen_ear_reset(void) {            /* (0.30) a fresh ear: no note held over from before */
    listen_init(&lis_ear, lis_rate);
    listen_set_gate(&lis_ear, .004f + (100 - lis.sing_sense) * .0006f);
    lis_note = -1; lis_heard.note = -1; lis_heard.onset = lis_heard.release = 0;
    phrase_n = phrase_play = 0;
}
static void listen_sense_set(int sense) {
    lis.sing_sense = sense < 0 ? 0 : sense > 100 ? 100 : sense;
    listen_set_gate(&lis_ear, .004f + (100 - lis.sing_sense) * .0006f);
}
static void listen_restore(const garden_listen *s) {
    lis.on = s->on != 0;
    lis.mode = s->mode >= 0 && s->mode <= 2 ? s->mode : 0;
    lis.stereo = s->stereo != 0;
    lis.song_free = s->song_free != 0;
    lis.through = s->through >= 0.f && s->through <= 1.f ? s->through : 1.f;
    lis.cloud = s->cloud >= 0.f && s->cloud <= 1.f ? s->cloud : .55f;
    lis.echo = s->echo >= 0.f && s->echo <= 1.f ? s->echo : .3f;
    lis.room = s->room >= 0.f && s->room <= 1.f ? s->room : .45f;
    lis.duck = s->duck >= 0.f && s->duck <= 1.f ? s->duck : 0.f;
    lis.sing = s->sing >= 0 && s->sing < SING_MODES ? s->sing : SING_OFF;
    lis.sing_track = s->sing_track >= 0 && s->sing_track < MD_TRACKS ? s->sing_track : 0;
    lis.sing_octave = s->sing_octave >= -2 && s->sing_octave <= 2 ? s->sing_octave : 0;
    listen_sense_set(s->sing_sense >= 0 && s->sing_sense <= 100 ? s->sing_sense : 50);
    lis.cv_track = s->cv_track >= -1 && s->cv_track < MD_TRACKS ? s->cv_track : -1;
    lis.cv_in = s->cv_in >= 0 && s->cv_in <= 4 ? s->cv_in : 0;
    lis.cv_gate_ms = s->cv_gate_ms >= 5 && s->cv_gate_ms <= 1000 ? s->cv_gate_ms : 60;
    lis.cv_octave = s->cv_octave >= -2 && s->cv_octave <= 2 ? s->cv_octave : 0;
    lis.moves = s->moves >= 0 && s->moves <= 4 ? s->moves : 0;
    lis.moves_base = lis.moves && s->moves_base >= 0 && s->moves_base <= 1000 ? s->moves_base : -1;
    if (!lis.moves) lis.moves_base = -1;
    listen_apply();
}
