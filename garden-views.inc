/* The complete Mutable Dreams 2.1 instrument on the VIBE FM surface (0.9).
 *
 * 47 pages in seven groups. The six colour keys K1-K6 are the house groups SOUND · ENV · LFO ·
 * HARMONY · FX · SEQ: a key opens its group, the same key again steps through its pages. MENU opens
 * the directory of every page (also DREAM: Itinera, Journey, Banks, System, MIDI). LEFT / RIGHT walk
 * through all pages in order. Encoders 1-4 are the four values of the page, F1-F6 are its soft keys.
 * Every page follows one control table taken from the FM-1 2.1 firmware: same commands, same ranges.
 * Included by adapter-check.c after garden-moments.inc. */

enum Kind { K_ATLAS, K_LIST, K_VOICE, K_BARS, K_MIXER, K_ENV, K_FILTER, K_TIDE, K_LINKS, K_DRIFT, K_HARMONY,
    K_CHORDS, K_BASS, K_PADS, K_SPACE, K_GRAIN, K_RHYTHM, K_SEQ, K_KIT, K_DREAM, K_JOURNEY, K_BANKS, K_INFO,
    K_SEQGRID, K_SEQTOOLS, K_SEQRULES, K_RADIO };
typedef struct { const char *name, *hint; uint8_t group, kind; } view_info;
static const view_info views[V_COUNT] = {
    {"LIVE", "The Biome's four macros. F1-F6: favourite Biomes", G_SOUND, K_ATLAS},
    {"BROWSER", "All Biomes by family. F1 loads, SHIFT + F stores a favourite", G_SOUND, K_LIST},
    {"VOICE", "Tone preset and engine of the selected track", G_SOUND, K_VOICE},
    {"ENGINE", "Colour, harmonics and shape of the track's engine", G_SOUND, K_BARS},
    {"MIX", "Cloud send, base note, pan and clock division", G_SOUND, K_MIXER},
    {"MIXER", "Level and the three effect sends of each track", G_SOUND, K_MIXER},
    {"ENVELOPE", "Attack up to 8 s, decay up to 16 s", G_ENV, K_ENV},
    {"FILTER", "Cutoff, resonance, type and breathing", G_ENV, K_FILTER},
    {"MODULATION", "Marbles, Tides and Warps of the track", G_LFO, K_TIDE},
    {"LFO A", "Where Tides moves, its wave, depth and rate", G_LFO, K_TIDE},
    {"LFO B", "A second LFO for the same track", G_LFO, K_TIDE},
    {"LINKS", "Living links: one track moves another", G_LFO, K_LINKS},
    {"LINK MOTION", "Signal, depth and release of the link", G_LFO, K_LINKS},
    {"DRIFT", "Glide between two captured scenes", G_LFO, K_DRIFT},
    {"DRIFT SCENES", "F1 / F2 capture scene A / B while drift is off", G_LFO, K_DRIFT},
    {"HARMONY", "Root, scale, transpose and tempo", G_HARMONY, K_HARMONY},
    {"PROGRESSION", "Chord pattern, bars, follow and voicing", G_HARMONY, K_CHORDS},
    {"BASSLINE", "Bass mode, motif, glide and note", G_HARMONY, K_BASS},
    {"PHRASE", "The sixteen bass steps, accent and chance", G_HARMONY, K_BASS},
    {"PERFORM", "The 16 performance pads. FX again: pad settings", G_FX, K_PADS},
    {"PADS", "Pad tail, gesture, start, tone pads. F1-F6 moments", G_FX, K_PADS},
    {"SPACE", "Echo time and feedback, room tail, effect return", G_FX, K_SPACE},
    {"GRAIN", "The granular bus: mix, size, position, feedback", G_FX, K_GRAIN},
    {"CLOUD", "Air in the grains, mix, track send, freeze", G_FX, K_GRAIN},
    {"TAPE", "Grain pitch and direction, ducking", G_FX, K_GRAIN},
    {"RHYTHM", "Euclidean hits, length, rotation, chance", G_SEQ, K_RHYTHM},
    {"GROOVE", "Swing, repeats, accent, clock division", G_SEQ, K_RHYTHM},
    {"SEQUENCE", "Step sequence: on, step, note, length", G_SEQ, K_SEQ},
    {"STEP", "Velocity, gate and chance of the step", G_SEQ, K_SEQ},
    {"FLOW", "Direction, shift, feel and generator", G_SEQ, K_SEQ},
    {"SEQ GRID", "All six tracks step by step. F: track, encoders edit", G_SEQ, K_SEQGRID},
    {"SEQ TOOLS", "Copy, paste, transpose, reverse, accent, articulate. F1 applies", G_SEQ, K_SEQTOOLS},
    {"SEQ RULES", "Per-step rules and the fill. F1 queues the fill", G_SEQ, K_SEQRULES},
    {"KIT", "Lo-fi drum kit on track 4: mode, bank, pad, hiss", G_SEQ, K_KIT},
    {"KIT PAD", "Tune, level, cutoff and decay of the pad", G_SEQ, K_KIT},
    {"KIT SEND", "Cloud, echo and room sends of the kit", G_SEQ, K_KIT},
    {"KIT SEQ", "Kit steps: step, gate, length, generator", G_SEQ, K_KIT},
    {"RADIO", "Mutable Dreams FM: the station's programme. F1 on air, F2 skip, F3 off", G_DREAM, K_RADIO},
    {"ITINERA", "Fourteen long dreams. F1 starts, F2 stops", G_DREAM, K_DREAM},
    {"JOURNEY", "Eight sections of Biomes in a row", G_DREAM, K_JOURNEY},
    {"JOURNEY TIME", "Bars, repeats and transposition per section", G_DREAM, K_JOURNEY},
    {"JOURNEY KEEP", "Which tracks a section keeps", G_DREAM, K_JOURNEY},
    {"BANKS", "Sixteen project slots. F1 saves, F2 loads", G_DREAM, K_BANKS},
    {"SYSTEM", "Tempo, ECO / FULL, ducking recovery", G_DREAM, K_INFO},
    {"MIDI", "Clock, input snap, track channel, macro CCs", G_DREAM, K_INFO},
    {"MIDI LEARN", "Slot, target, track. Encoder 4 right arms", G_DREAM, K_INFO},
    {"MIDI MAP", "Port, channel, CC and on / off of the slot", G_DREAM, K_INFO},
};
static const char *const group_names[G_COUNT] = {"SOUND","ENV","LFO","HARMONY","FX","SEQ","DREAM"};
static const uint32_t group_ink[G_COUNT] = {0x83d2a6,0xe2aa78,0x90cbbf,0xb4b8eb,0x85c7de,0xf0cb86,0xd6a7e0};
static uint8_t group_last[G_COUNT] = {V_LIVE,V_ENVELOPE,V_MODULATION,V_HARMONY,V_PERFORM,V_RHYTHM,V_RADIO};

/* ---- page state ---------------------------------------------------------------------------------------- */
static int browse_family = -1, browse_id;            /* -1 = all families */
static int link_slot, learn_slot, journey_edit, bank_slot;
static int it_preset, it_length, it_pace, it_hall;
/* SEQ TOOLS, as on the FM-1: scope step / phrase, an action, its interval or amount */
enum { SEQ_COPY, SEQ_PASTE, SEQ_TRANSPOSE, SEQ_MIRROR, SEQ_ACCENT, SEQ_ARTICULATE };
static const char *const seq_actions[] = {"COPY","PASTE","TRANSPOSE","REVERSE","ACCENT","ARTICULATE"};
static const char *const seq_rules[] = {"ALWAYS","FIRST CYCLE","EVERY 2ND","FILL ONLY"};
static int seq_scope, seq_action, seq_amount = 50, seq_interval;
static uint32_t seq_notice_seen;
static int show_menu, menu_cursor;
static float knob_acc[4];
static char notice[40];
static float notice_until;
static uint16_t bank_used;                            /* bit per project slot that holds a file */

static void say(const char *text) {
    unsigned i = 0;
    for (; text[i] && i < sizeof notice - 1; i++) notice[i] = text[i];
    notice[i] = 0;
    notice_until = md_status()->frames / 44100.f + 2.f;
}

/* ---- the control table ----------------------------------------------------------------------------------- */
enum Fmt { F_PCT, F_INT, F_NOTE, F_ROOT, F_SCALE, F_SIGNED, F_BPM, F_MS, F_ONOFF, F_FILTER, F_BASSMODE,
    F_LTARGET, F_LWAVE, F_GEN, F_DIR, F_CHORD, F_KIT, F_PAD, F_TONE, F_MODEL, F_QUALITY, F_LINKT, F_LINKSIG,
    F_LINKTGT, F_PAN, F_CLOCK, F_LEARNT, F_TRACK, F_JMODE, F_JBIOME, F_REVERSE, F_SLOT, F_FAMILY, F_BIOME,
    F_ITPRESET, F_MINUTES, F_PACE, F_HALL, F_DUCKSRC, F_DIV, F_BASSNOTE, F_SEQNOTE, F_DRIFTMODE, F_PORT,
    F_CHANNEL, F_CC, F_ARM, F_BIPOLAR, F_PADSET, F_KEEP, F_COMPACT, F_FOLLOW, F_GATE, F_PCT100, F_SCOPE, F_ACTION, F_RULE, F_SECONDS, F_ONAIR, F_RLEN, F_RDREAM };
/* Local controls handled here, not by an engine command. */
enum { L_NONE = -1000, L_FAMILY, L_BROWSE, L_LINK_SLOT, L_LEARN_SLOT, L_JSECTION, L_JTRACK, L_KEEP, L_BANK,
    L_IT_PRESET, L_IT_LENGTH, L_IT_PACE, L_IT_HALL, L_PADSET, L_CPU, L_ARM, L_CAPTURE, L_TOUCH,
    L_SCOPE, L_ACTION, L_INTERVAL, L_AMOUNT, L_TAKE, L_RADIO, L_RADIO_LEN, L_RADIO_DREAMS, L_WANDER, L_RADIO_AUTO };
static int radio_on_now(void);
static int radio_length_choice, radio_dreams, radio_autostart, radio_chime;
static void radio_set(int on);
static void radio_skip(void);
static int rec_seconds_choice = 2;   /* 0..3 = 30 / 60 / 90 / 120 s, 90 by default */
typedef struct { const char *label; int cmd, track, value, step, fmt, lo, hi; } Knob;

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static int knob_of(int v, unsigned k, Knob *o) {
    const md_patch *p = md_state();
    const int tr = (int)track;
    const md_track *t = &p->track[tr];
    const md_extension *e = &p->ext[tr];
    const md_sequence *q = &p->sequence[tr];
    const md_step *st = &q->steps[q->edit];
    const md_kit *kit = &p->kit;
    const md_pad *kp = &kit->pads[kit->pad];
    const md_link *l = &p->links[link_slot];
    const md_journey_section *js = &p->journey.section[journey_edit];
    const md_midi_mapping *m = md_mapping((unsigned)learn_slot);
    const int env = md_has_envelope(t->model) || t->model == MD_ELEMENTS;
#define K(L,C,T,V,S,F,LO,HI) do { *o = (Knob){L,C,T,V,S,F,LO,HI}; return 1; } while (0)
    switch (v) {
    case V_LIVE: {
        static const char *const names[4] = {"DENSITY","MOTION","COLOUR","SPACE"};
        K(names[k], MD_MACRO, (int)k, p->macro[k], 20, F_PCT, 0, 1000);
    }
    case V_BROWSER:
        if (k == 0) K("FAMILY", L_FAMILY, 0, browse_family, 1, F_FAMILY, -1, (int)md_biome_family_count() - 1);
        if (k == 1) K("BIOME", L_BROWSE, 0, browse_id, 1, F_BIOME, 0, MD_BIOMES - 1);
        return 0;
    case V_VOICE:
        if (k == 0) K("TONE PRESET", MD_TONE_PRESET, tr, p->tone_preset[tr], 1, F_TONE, -1, MD_TONE_PRESETS - 1);
        if (k == 1) K("ENGINE", MD_MODEL, tr, t->model, 1, F_MODEL, 0, MD_MODELS - 1);
        if (k == 2) K("LEVEL", MD_LEVEL, tr, t->level, 20, F_PCT, 0, 1000);
        K("MUTE", MD_MUTE, tr, t->mute, 1, F_ONOFF, 0, 1);
    case V_ENGINE:
        if (k == 0) K("TONE", MD_TONE, tr, t->tone, 20, F_PCT, 0, 1000);
        if (k == 1) K("HARMONICS", MD_HARM, tr, t->harm, 20, F_PCT, 0, 1000);
        if (k == 2) K(env ? "SHAPE" : "DECAY", env ? MD_SHAPE : MD_DECAY, tr, env ? t->shape : t->decay, 20, F_PCT, 0, 1000);
        K("LEVEL", MD_LEVEL, tr, t->level, 20, F_PCT, 0, 1000);
    case V_MIX:
        if (k == 0) K("CLOUD SEND", MD_SEND, tr, t->send, 20, F_PCT, 0, 1000);
        if (k == 1) K("NOTE", MD_NOTE, tr, t->note, 1, F_NOTE, 24, 96);
        if (k == 2) K("PAN", MD_PAN, tr, t->pan, 20, F_PAN, 0, 1000);
        K("DIVISION", MD_SPEED, tr, t->speed, 1, F_DIV, 1, 8);
    case V_MIXER:
        if (k == 0) K("LEVEL", MD_LEVEL, tr, t->level, 20, F_PCT, 0, 1000);
        if (k == 1) K("CLOUD", MD_SEND, tr, t->send, 20, F_PCT, 0, 1000);
        if (k == 2) K("ECHO", MD_DELAY_SEND, tr, e->delay_send, 20, F_PCT, 0, 1000);
        K("ROOM", MD_REVERB_SEND, tr, e->reverb_send, 20, F_PCT, 0, 1000);
    case V_ENVELOPE:
        if (k == 0) K("ATTACK", MD_ATTACK, tr, t->attack, 20, F_PCT, 0, 1000);
        if (k == 1) K("DECAY", MD_DECAY, tr, t->decay, 20, F_PCT, 0, 1000);
        if (k == 2) K("LEVEL", MD_LEVEL, tr, t->level, 20, F_PCT, 0, 1000);
        K("CLOUD SEND", MD_SEND, tr, t->send, 20, F_PCT, 0, 1000);
    case V_FILTER:
        if (k == 0) K("CUTOFF", MD_CUTOFF, tr, t->cutoff, 20, F_PCT, 0, 1000);
        if (k == 1) K("RESONANCE", MD_RESONANCE, tr, t->resonance, 20, F_PCT, 0, 1000);
        if (k == 2) K("TYPE", MD_FILTER, tr, t->filter, 1, F_FILTER, 0, 3);
        K("BREATHE", MD_BREATHE, tr, t->breathe, 20, F_PCT, 0, 1000);
    case V_MODULATION:
        if (k == 0) K("MARBLES", MD_MARBLES, tr, e->marbles, 20, F_PCT, 0, 1000);
        if (k == 1) K("TIDES", MD_TIDES, tr, e->tides, 20, F_PCT, 0, 1000);
        if (k == 2) K("TIDE RATE", MD_TIDES_RATE, tr, e->tides_rate, 20, F_PCT, 0, 1000);
        K("WARPS", MD_WARPS, tr, e->warps, 20, F_PCT, 0, 1000);
    case V_LFO1:
        if (k == 0) K("TARGET", MD_LFO_TARGET, tr, p->motion.target[tr], 1, F_LTARGET, 0, 6);
        if (k == 1) K("WAVE", MD_LFO_SHAPE, tr, p->motion.wave[tr], 1, F_LWAVE, 0, 3);
        if (k == 2) K("DEPTH", MD_TIDES, tr, e->tides, 20, F_PCT, 0, 1000);
        K("RATE", MD_TIDES_RATE, tr, e->tides_rate, 20, F_PCT, 0, 1000);
    case V_LFO2:
        if (k == 0) K("TARGET", MD_LFO2_TARGET, tr, p->motion.target2[tr], 1, F_LTARGET, 0, 6);
        if (k == 1) K("WAVE", MD_LFO2_SHAPE, tr, p->motion.wave2[tr], 1, F_LWAVE, 0, 3);
        if (k == 2) K("DEPTH", MD_LFO2_DEPTH, tr, p->motion.depth2[tr], 20, F_PCT, 0, 1000);
        K("RATE", MD_LFO2_RATE, tr, p->motion.rate2[tr], 20, F_PCT, 0, 1000);
    case V_LINKS:
        if (k == 0) K("LINK", L_LINK_SLOT, 0, link_slot, 1, F_SLOT, 0, MD_LINKS - 1);
        if (k == 1) K("FROM", MD_LINK_SOURCE, link_slot, l->source, 1, F_TRACK, 0, 5);
        if (k == 2) K("TO", MD_LINK_DEST, link_slot, l->destination, 1, F_TRACK, 0, 5);
        K("MOVES", MD_LINK_TARGET, link_slot, l->target, 1, F_LINKTGT, 0, 5);
    case V_LINK_MOTION:
        if (k == 0) K("LINK", L_LINK_SLOT, 0, link_slot, 1, F_SLOT, 0, MD_LINKS - 1);
        if (k == 1) K("SIGNAL", MD_LINK_SIGNAL, link_slot, l->signal, 1, F_LINKSIG, 0, 3);
        if (k == 2) K("DEPTH", MD_LINK_DEPTH, link_slot, l->depth, 50, F_BIPOLAR, -1000, 1000);
        K("RELEASE", MD_LINK_RELEASE, link_slot, l->release, 20, F_PCT, 0, 1000);
    case V_DRIFT:
        if (k == 0) K("DRIFT", MD_DRIFT_MODE, 0, p->drift.mode, 1, F_DRIFTMODE, 0, 2);
        if (k == 1) K("POSITION", MD_DRIFT_POSITION, 0, p->drift.position, 20, F_PCT, 0, 1000);
        if (k == 2) K("RATE", MD_DRIFT_RATE, 0, p->drift.rate, 20, F_PCT, 0, 1000);
        K("DEPTH", MD_DRIFT_DEPTH, 0, p->drift.depth, 20, F_PCT, 0, 1000);
    case V_DRIFT_SCENES:
        if (k == 2) K("DEPTH", MD_DRIFT_DEPTH, 0, p->drift.depth, 20, F_PCT, 0, 1000);
        if (k == 3) K("DRIFT", MD_DRIFT_MODE, 0, p->drift.mode, 1, F_DRIFTMODE, 0, 2);
        return 0;
    case V_HARMONY:
        if (k == 0) K("ROOT", MD_ROOT, 0, p->root, 1, F_ROOT, 0, 11);
        if (k == 1) K("SCALE", MD_SCALE, 0, p->scale, 1, F_SCALE, 0, 12);
        if (k == 2) K("TRANSPOSE", MD_TRANSPOSE, 0, p->transpose, 1, F_SIGNED, -24, 24);
        K("TEMPO", MD_TEMPO, 0, p->tempo, 1, F_BPM, 30, 220);
    case V_PROGRESSION:
        if (k == 0) K("CHORDS", MD_CHORD_PATTERN, tr, p->chord_pattern, 1, F_CHORD, 0, 8);
        if (k == 1) K("BARS", MD_CHORD_BARS, tr, p->chord_bars, 1, F_INT, 1, 16);
        if (k == 2) K("FOLLOW", MD_CHORD_FOLLOW, tr, (p->chord_tracks >> tr) & 1, 1, F_FOLLOW, 0, 1);
        K("VOICING", MD_CHORD_COMPACT, tr, p->motion.compact, 1, F_COMPACT, 0, 1);
    case V_BASSLINE:
        if (k == 0) K("MODE", MD_BASS_MODE, tr, e->bass_mode, 1, F_BASSMODE, 0, 2);
        if (k == 1) K("MOTIF", MD_BASS_PATTERN, tr, e->pattern, 1, F_INT, 0, 15);
        if (k == 2) K("GLIDE", MD_GLIDE, tr, e->glide, 20, F_PCT, 0, 1000);
        K("NOTE", MD_NOTE, tr, t->note, 1, F_NOTE, 24, 96);
    case V_PHRASE:
        if (k == 0) K("STEP", MD_BASS_STEP, tr, e->edit_step, 1, F_INT, 0, 15);
        if (k == 1) K("SEMITONES", MD_BASS_VALUE, tr, e->notes[e->edit_step], 1, F_BASSNOTE, -1, 24);
        if (k == 2) K("ACCENT", MD_ACCENT, tr, t->accent, 20, F_PCT, 0, 1000);
        K("CHANCE", MD_PROB, tr, t->prob, 20, F_PCT, 0, 1000);
    case V_PERFORM:
        /* 0.18: named as sends, so they do not read like the pads BLOOM / ECHO / TEXTURE on the same screen */
        if (k == 0) K("ROOM SEND", MD_REVERB_SEND, tr, e->reverb_send, 20, F_PCT, 0, 1000);
        if (k == 1) K("ECHO SEND", MD_DELAY_SEND, tr, e->delay_send, 20, F_PCT, 0, 1000);
        if (k == 2) K("CLOUD SEND", MD_SEND, tr, t->send, 20, F_PCT, 0, 1000);
        K("FX RETURN", MD_FX_LEVEL, 0, p->fx_level, 20, F_PCT, 0, 1000);
    case V_PADSET:
        K(shift_held ? pad_knob_shift_labels[k] : pad_knob_labels[k], L_PADSET, (int)k, 0, 1, F_PADSET, 0, 0);
    case V_SPACE:
        if (k == 0) K("ECHO TIME", MD_DELAY_TIME, 0, p->delay_ms, 5, F_MS, 40, 350);
        if (k == 1) K("FEEDBACK", MD_DELAY_FB, 0, p->delay_fb, 20, F_PCT, 0, 800);
        if (k == 2) K("ROOM TAIL", MD_REVERB_TIME, 0, p->reverb_time, 20, F_PCT, 0, 850);
        K("RETURN", MD_FX_LEVEL, 0, p->fx_level, 20, F_PCT, 0, 1000);
    case V_GRAIN:
        if (k == 0) K("MIX", MD_MACRO, 3, p->macro[3], 20, F_PCT, 0, 1000);
        if (k == 1) K("SIZE", MD_GRAIN_SIZE, 0, p->grain_size, 20, F_PCT, 0, 1000);
        if (k == 2) K("POSITION", MD_GRAIN_POS, 0, p->grain_pos, 20, F_PCT, 0, 1000);
        K("FEEDBACK", MD_FEEDBACK, 0, p->feedback, 20, F_PCT, 0, 850);
    case V_CLOUD:
        if (k == 0) K("AIR", MD_GRAIN_NOISE, 0, p->grain_noise, 20, F_PCT, 0, 1000);
        if (k == 1) K("MIX", MD_MACRO, 3, p->macro[3], 20, F_PCT, 0, 1000);
        if (k == 2) K("TRACK SEND", MD_SEND, tr, t->send, 20, F_PCT, 0, 1000);
        K("FREEZE", MD_FREEZE, 0, p->freeze, 1, F_ONOFF, 0, 1);
    case V_TAPE:
        if (k == 0) K("GRAIN PITCH", MD_GRAIN_PITCH, 0, p->grain_pitch, 1, F_SIGNED, -24, 24);
        if (k == 1) K("DIRECTION", MD_REVERSE, 0, p->reverse, 1, F_REVERSE, 0, 1);
        if (k == 2) K("DUCK SOURCE", MD_DUCK_SOURCE, 0, p->duck_source, 1, F_DUCKSRC, -1, 5);
        K("DUCK", MD_DUCK, 0, p->duck_amount, 20, F_PCT, 0, 1000);
    case V_RHYTHM:
        if (k == 0) K("HITS", MD_HITS, tr, t->hits, 1, F_INT, 0, 32);
        if (k == 1) K("LENGTH", MD_LENGTH, tr, t->length, 1, F_INT, 1, 32);
        if (k == 2) K("ROTATE", MD_ROTATE, tr, t->rotate, 1, F_INT, 0, 31);
        K("CHANCE", MD_PROB, tr, t->prob, 20, F_PCT, 0, 1000);
    case V_GROOVE:
        if (k == 0) K("SWING", MD_SWING, 0, p->swing, 20, F_PCT, 0, 600);
        if (k == 1) K("REPEATS", MD_RATCHET, tr, t->ratchet, 1, F_INT, 1, 4);
        if (k == 2) K("ACCENT", MD_ACCENT, tr, t->accent, 20, F_PCT, 0, 1000);
        K("DIVISION", MD_SPEED, tr, t->speed, 1, F_DIV, 1, 8);
    case V_SEQUENCE:
        if (k == 0) K("SEQUENCE", MD_SEQ_ENABLE, tr, q->enabled, 1, F_ONOFF, 0, 1);
        if (k == 1) K("STEP", MD_SEQ_STEP, tr, q->edit, 1, F_INT, 0, 31);
        if (k == 2) K("NOTE", MD_SEQ_NOTE, tr, st->note ? st->note : 23, 1, F_SEQNOTE, 23, 96);
        K("LENGTH", MD_SEQ_LENGTH, tr, q->length, 1, F_INT, 1, 32);
    case V_STEP:
        if (k == 0) K("VELOCITY", MD_SEQ_VELOCITY, tr, st->velocity, 5, F_PCT100, 1, 100);
        if (k == 1) K("GATE", MD_SEQ_GATE, tr, (int)md_step_gate(st), 5, F_PCT100, 0, 100);
        if (k == 2) K("CHANCE", MD_SEQ_CHANCE, tr, (int)md_step_chance(st), 5, F_PCT100, 0, 100);
        K("GENERATE", MD_SEQ_GENERATE, tr, q->generator, 1, F_GEN, 0, 15);
    case V_FLOW:
        if (k == 0) K("DIRECTION", MD_SEQ_DIRECTION, tr, p->motion.direction[tr], 1, F_DIR, 0, 4);
        if (k == 1) K("SHIFT", MD_SEQ_SHIFT, tr, p->motion.shift[tr], 1, F_INT, 0, 31);
        if (k == 2) K("FEEL", MD_SEQ_FEEL, tr, p->motion.feel[tr], 5, F_PCT100, 0, 100);
        K("GENERATE", MD_SEQ_GENERATE, tr, q->generator, 1, F_GEN, 0, 15);
    case V_SEQ_GRID: {
        const int drums = kit->enabled && tr == 3;
        if (k == 0) K("TRACK", L_JTRACK, 0, tr, 1, F_TRACK, 0, 5);
        if (k == 1) K("STEP", drums ? MD_KIT_STEP : MD_SEQ_STEP, tr, drums ? kit->step : q->edit, 1, F_INT, 0, 31);
        if (k == 2) { if (drums) K("PAD", MD_KIT_PAD, 3, kit->pad, 1, F_PAD, 0, 7);
                      K("NOTE", MD_SEQ_NOTE, tr, st->note ? st->note : 23, 1, F_SEQNOTE, 23, 96); }
        K("LENGTH", drums ? MD_KIT_LENGTH : MD_SEQ_LENGTH, tr, drums ? kit->length : q->length, 1, F_INT, 1, 32);
    }
    case V_SEQ_TOOLS:
        if (k == 0) K("SCOPE", L_SCOPE, 0, seq_scope, 1, F_SCOPE, 0, 1);
        if (k == 1) K("STEP", MD_SEQ_STEP, tr, q->edit, 1, F_INT, 0, 31);
        if (k == 2) { if (seq_action >= SEQ_ACCENT) K("AMOUNT", L_AMOUNT, 0, seq_amount, 5, F_PCT100, 0, 100);
                      K("SHIFT st", L_INTERVAL, 0, seq_interval, 1, F_SIGNED, -24, 24); }
        K("ACTION", L_ACTION, 0, seq_action, 1, F_ACTION, 0, seq_scope ? SEQ_ARTICULATE : SEQ_TRANSPOSE);
    case V_SEQ_RULES:
        if (k == 0) K("TRACK", L_JTRACK, 0, tr, 1, F_TRACK, 0, 5);
        if (k == 1) K("STEP", MD_SEQ_STEP, tr, q->edit, 1, F_INT, 0, 31);
        if (k == 2) K("RULE", MD_SEQ_RULE, tr, (int)md_step_rule(st), 1, F_RULE, 0, 3);
        K("NOTE", MD_SEQ_NOTE, tr, st->note ? st->note : 23, 1, F_SEQNOTE, 23, 96);
    case V_KIT:
        if (k == 0) K("KIT", MD_KIT_MODE, 3, kit->enabled, 1, F_ONOFF, 0, 1);
        if (k == 1) K("BANK", MD_KIT_BANK, 3, kit->bank, 1, F_KIT, 0, 4);
        if (k == 2) K("PAD", MD_KIT_PAD, 3, kit->pad, 1, F_PAD, 0, 7);
        K("HISS", MD_KIT_HISS, 3, kit->hiss, 5, F_PCT100, 0, 100);
    case V_KIT_PAD:
        if (k == 0) K("TUNE", MD_KIT_TUNE, 3, kp->tune, 1, F_SIGNED, -12, 12);
        if (k == 1) K("LEVEL", MD_KIT_LEVEL, 3, kp->level, 20, F_PCT, 0, 1000);
        if (k == 2) K("CUTOFF", MD_KIT_CUTOFF, 3, kp->cutoff, 20, F_PCT, 0, 1000);
        K("DECAY", MD_KIT_DECAY, 3, kp->decay, 20, F_PCT, 0, 1000);
    case V_KIT_SEND:
        if (k == 0) K("PAD", MD_KIT_PAD, 3, kit->pad, 1, F_PAD, 0, 7);
        if (k == 1) K("CLOUD", MD_KIT_SEND, 3, kp->send, 20, F_PCT, 0, 1000);
        if (k == 2) K("ECHO", MD_DELAY_SEND, 3, p->ext[3].delay_send, 20, F_PCT, 0, 1000);
        K("ROOM", MD_REVERB_SEND, 3, p->ext[3].reverb_send, 20, F_PCT, 0, 1000);
    case V_KIT_SEQ:
        if (k == 0) K("STEP", MD_KIT_STEP, 3, kit->step, 1, F_INT, 0, 31);
        if (k == 1) K("GATE", MD_KIT_GATE, 3, (int)((kit->gates[kit->pad] >> kit->step) & 1u), 1, F_ONOFF, 0, 1);
        if (k == 2) K("LENGTH", MD_KIT_LENGTH, 3, kit->length, 1, F_INT, 1, 32);
        K("GENERATE", MD_KIT_GENERATE, 3, kit->generator, 1, F_GEN, 0, 15);
    case V_RADIO:
        if (k == 0) K("BROADCAST", L_RADIO, 0, radio_on_now(), 1, F_ONAIR, 0, 1);
        if (k == 1) K("PIECES", L_RADIO_LEN, 0, radio_length_choice, 1, F_RLEN, 0, 2);
        if (k == 2) K("ITINERA", L_RADIO_DREAMS, 0, radio_dreams, 1, F_RDREAM, 0, 1);
        K("AT START", L_RADIO_AUTO, 0, radio_autostart, 1, F_ONAIR, 0, 1);
    case V_ITINERA:
        if (k == 0) K("DREAM", L_IT_PRESET, 0, it_preset, 1, F_ITPRESET, 0, MD_ITINERA_PRESETS - 1);
        if (k == 1) K("LENGTH", L_IT_LENGTH, 0, it_length, 1, F_MINUTES, 0, 3);
        if (k == 2) K("PACE", L_IT_PACE, 0, it_pace, 1, F_PACE, 0, 2);
        K("HALL", L_IT_HALL, 0, it_hall, 1, F_HALL, 0, 2);
    case V_JOURNEY:
        if (k == 0) K("JOURNEY", MD_JOURNEY_MODE, 0, p->journey.mode, 1, F_JMODE, 0, 2);
        if (k == 1) K("SECTION", L_JSECTION, 0, journey_edit, 1, F_SLOT, 0, 7);
        if (k == 2) K("BIOME", MD_JOURNEY_BIOME, journey_edit, js->biome == 65535 ? -1 : js->biome, 1, F_JBIOME, -1, MD_BIOMES - 1);
        K("SECTIONS", MD_JOURNEY_LENGTH, 0, p->journey.length, 1, F_INT, 1, 8);
    case V_JOURNEY_TIME:
        if (k == 0) K("SECTION", L_JSECTION, 0, journey_edit, 1, F_SLOT, 0, 7);
        if (k == 1) K("BARS", MD_JOURNEY_BARS, journey_edit, js->bars, 1, F_INT, 1, 64);
        if (k == 2) K("REPEATS", MD_JOURNEY_REPEATS, journey_edit, js->repeats, 1, F_INT, 1, 16);
        K("TRANSPOSE", MD_JOURNEY_TRANSPOSE, journey_edit, js->transpose, 1, F_SIGNED, -24, 24);
    case V_JOURNEY_KEEP:
        if (k == 0) K("SECTION", L_JSECTION, 0, journey_edit, 1, F_SLOT, 0, 7);
        if (k == 1) K("TRACK", L_JTRACK, 0, tr, 1, F_TRACK, 0, 5);
        if (k == 2) K("KEEP", L_KEEP, journey_edit, (js->keep >> tr) & 1, 1, F_KEEP, 0, 1);
        K("JOURNEY", MD_JOURNEY_MODE, 0, p->journey.mode, 1, F_JMODE, 0, 2);
    case V_BANKS:
        if (k == 0) K("SLOT", L_BANK, 0, bank_slot, 1, F_SLOT, 0, 15);
        if (k == 3) K("REC TAKE", L_TAKE, 0, rec_seconds_choice, 1, F_SECONDS, 0, 3);
        return 0;
    case V_SYSTEM:
        if (k == 0) K("TEMPO", MD_TEMPO, 0, p->tempo, 1, F_BPM, 30, 220);
        if (k == 1) K("QUALITY", MD_QUALITY, 0, p->quality, 1, F_QUALITY, 0, 1);
        if (k == 2) K("DUCK RECOVERY", MD_DUCK_RELEASE, 0, p->duck_release, 20, F_PCT, 0, 1000);
        K("TOUCH", L_TOUCH, 0, (int)touch_percent, 5, F_PCT100, 0, 100);
    case V_MIDI:
        if (k == 0) K("CLOCK", MD_CLOCK_SOURCE, 0, md_clock_source(), 1, F_CLOCK, 0, 3);
        if (k == 1) K("SCALE SNAP", MD_MIDI_QUANTIZE, 0, md_midi_quantize(), 1, F_ONOFF, 0, 1);
        if (k == 2) K("CHANNEL", MD_MIDI_ROUTE, tr, md_midi_route(track), 1, F_CHANNEL, 0, 16);
        K("MACRO CC", MD_MIDI_MACRO, 0, md_midi_macro_enabled(), 1, F_ONOFF, 0, 1);
    case V_LEARN:
        if (k == 0) K("SLOT", L_LEARN_SLOT, 0, learn_slot, 1, F_SLOT, 0, 15);
        if (k == 1) K("TARGET", MD_LEARN_TARGET, learn_slot, m->target, 1, F_LEARNT, 0, 31);
        if (k == 2) K("TRACK", MD_LEARN_TRACK, learn_slot, m->track, 1, F_TRACK, 0, 5);
        K("LEARN", L_ARM, learn_slot, md_learn_armed() == learn_slot, 1, F_ARM, 0, 1);
    case V_MAP:
        if (k == 0) K("PORT", MD_LEARN_PORT, learn_slot, m->port, 1, F_PORT, 0, 3);
        if (k == 1) K("CHANNEL", MD_LEARN_CHANNEL, learn_slot, m->channel, 1, F_CHANNEL, 0, 16);
        if (k == 2) K("CC", MD_LEARN_CC, learn_slot, m->cc, 1, F_CC, 1, 119);
        K("ACTIVE", MD_LEARN_ENABLE, learn_slot, m->enabled, 1, F_ONOFF, 0, 1);
    }
#undef K
    return 0;
}

static const char *knob_text(const Knob *kb, char out[24]) {
    static const char *const filters[] = {"OFF","LP12","LP24","LPG"};
    static const char *const bass[] = {"OFF","EUCLID","STEP"};
    static const char *const jmodes[] = {"OFF","ONCE","LOOP"};
    static const char *const drift[] = {"OFF","HOLD","FLOW"};
    static const char *const clocks[] = {"INTERNAL","USB","TRS","BLE"};
    static const char *const ports[] = {"USB","TRS","BLE","ANY"};
    static const char *const paces[] = {"SLOW","STEADY","LIVELY"};
    static const char *const halls[] = {"CLOSE","WIDE","VAST"};
    static const char *const minutes[] = {"30 MIN","60 MIN","120 MIN","240 MIN"};
    const int v = kb->value;
    switch (kb->fmt) {
    case F_PCT: return vg_itoa(v / 10, out);
    case F_PCT100: return vg_itoa(v, out);
    case F_NOTE: case F_SEQNOTE: {
        if (kb->fmt == F_SEQNOTE && v < 24) return "REST";
        const char *n = notes[((v % 12) + 12) % 12];
        unsigned i = 0;
        while (n[i]) { out[i] = n[i]; i++; }
        vg_itoa(v / 12 - 1, out + i);
        return out;
    }
    case F_ROOT: return notes[clampi(v, 0, 11)];
    case F_SCALE: return md_scale_name(v);
    case F_SIGNED: case F_BIPOLAR: {
        const int shown = kb->fmt == F_BIPOLAR ? v / 10 : v;
        if (shown > 0) { out[0] = '+'; vg_itoa(shown, out + 1); return out; }
        return vg_itoa(shown, out);
    }
    case F_BPM: return vg_itoa(v, out);
    case F_MS: { vg_itoa(v, out); unsigned i = 0; while (out[i]) i++; out[i] = ' '; out[i + 1] = 'm'; out[i + 2] = 's'; out[i + 3] = 0; return out; }
    case F_ONOFF: return v ? "ON" : "OFF";
    case F_FILTER: return filters[clampi(v, 0, 3)];
    case F_BASSMODE: return bass[clampi(v, 0, 2)];
    case F_LTARGET: return md_lfo_target_name(v);
    case F_LWAVE: return md_lfo_wave_name(v);
    case F_GEN: return md_generator_name(v);
    case F_DIR: return md_direction_name(v);
    case F_CHORD: return v ? md_chord_name(v) : "OFF";
    case F_KIT: return md_kit_name(v);
    case F_PAD: return md_pad_name(v);
    case F_TONE: return md_tone_name(v);
    case F_MODEL: return md_model_name(v);
    case F_QUALITY: return v ? "FULL" : "ECO";
    case F_LINKTGT: return md_link_target_name(v);
    case F_LINKSIG: return md_link_signal_name(v);
    case F_PAN: if (v == 500) return "CENTRE"; out[0] = v < 500 ? 'L' : 'R'; vg_itoa((v < 500 ? 500 - v : v - 500) / 5, out + 1); return out;
    case F_CLOCK: return clocks[clampi(v, 0, 3)];
    case F_PORT: return ports[clampi(v, 0, 3)];
    case F_CHANNEL: if (!v) return kb->cmd == MD_MIDI_ROUTE ? "OFF" : "ANY"; out[0] = 'C'; out[1] = 'H'; vg_itoa(v, out + 2); return out;
    case F_CC: out[0] = 'C'; out[1] = 'C'; vg_itoa(v, out + 2); return out;
    case F_LEARNT: return md_learn_target_name(v);
    case F_TRACK: return track_labels[clampi(v, 0, 5)];
    case F_JMODE: return jmodes[clampi(v, 0, 2)];
    case F_DRIFTMODE: return drift[clampi(v, 0, 2)];
    case F_JBIOME: return v < 0 ? "HOME" : md_biome_name(v);
    case F_REVERSE: return v ? "REVERSE" : "FORWARD";
    case F_SLOT: return vg_itoa(v + 1, out);
    case F_FAMILY: return v < 0 ? "ALL" : md_biome_family_name((unsigned)v);
    case F_BIOME: return md_biome_name(v);
    case F_ITPRESET: return md_itinera_name((unsigned)v);
    case F_MINUTES: return minutes[clampi(v, 0, 3)];
    case F_PACE: return paces[clampi(v, 0, 2)];
    case F_HALL: return halls[clampi(v, 0, 2)];
    case F_DUCKSRC: return v < 0 ? "OFF" : track_labels[clampi(v, 0, 5)];
    case F_DIV: out[0] = '/'; vg_itoa(v, out + 1); return out;
    case F_BASSNOTE: if (v < 0) return "REST"; return vg_itoa(v, out);
    case F_ARM: return v ? "LISTENING" : "ARM";
    case F_PADSET: return pad_knob_value((unsigned)kb->track, out);
    case F_KEEP: return v ? "KEEP" : "CHANGE";
    case F_COMPACT: return v ? "COMPACT" : "OPEN";
    case F_FOLLOW: return v ? "FOLLOWS" : "FREE";
    case F_SCOPE: return v ? "PHRASE" : "STEP";
    case F_ONAIR: return v ? "ON AIR" : "OFF AIR";
    case F_RLEN: { static const char *const s[3] = {"SHORT","RADIO","LONG"}; return s[clampi(v, 0, 2)]; }
    case F_RDREAM: return v ? "EVERY 4 H" : "OFF";
    case F_SECONDS: { static const char *const s[4] = {"30 s","60 s","90 s","120 s"}; return s[clampi(v, 0, 3)]; }
    case F_ACTION: return seq_actions[clampi(v, 0, SEQ_ARTICULATE)];
    case F_RULE: return seq_rules[clampi(v, 0, 3)];
    default: return vg_itoa(v, out);
    }
}

static float knob_fill(const Knob *kb) {
    if (kb->hi <= kb->lo) return 0.f;
    const float f = (float)(kb->value - kb->lo) / (float)(kb->hi - kb->lo);
    return f < 0.f ? 0.f : f > 1.f ? 1.f : f;
}

/* ---- Biome browser --------------------------------------------------------------------------------------- */
static int browse_pos;                                  /* position in the curated wheel */
static int browse_matches(int id) { return browse_family < 0 || md_biome_family_index(id) == browse_family; }
static void browse_step(int delta) {
    int pos = browse_pos;
    for (int guard = 0; guard < MD_BIOMES && delta; guard++) {
        pos = (pos + (delta > 0 ? 1 : MD_BIOMES - 1)) % MD_BIOMES;
        const int id = md_biome_at((unsigned)pos);
        if (id >= 0 && browse_matches(id)) { delta += delta > 0 ? -1 : 1; browse_pos = pos; browse_id = id; }
    }
}
static void browse_sync(void) {
    for (unsigned i = 0; i < MD_BIOMES; i++) if (md_biome_at(i) == browse_id) { browse_pos = (int)i; return; }
}

static void load_biome_id(int id) {
    thin_end();
    latched &= ~(1u << PAD_THIN);                        /* a new scene ends THIN; its latch goes with it (0.18) */
    if (md_command_space() < 2) return;
    md_command(MD_SCENE, 0, id);
    md_command(MD_SCENE_CONFIRM, 0, 1);
    for (unsigned i = 0; i < 6; i++) if (favorites[i] == id) { selected = i; break; }   /* the highlight follows */
}

/* ---- SEQ TOOLS: the FM-1's phrase tools ----------------------------------------------------------------- */
static void seq_apply(void) {
    if (md_state()->kit.enabled && track >= 3) { say("SELECT A SYNTH TRACK"); return; }
    if (seq_action != SEQ_COPY && md_record_mode()) { say("TURN RECORDING OFF"); return; }
    if (seq_action == SEQ_TRANSPOSE && !seq_interval) { say("SET A TRANSPOSE INTERVAL"); return; }
    if (seq_action >= SEQ_ACCENT) {
        md_command(MD_SEQ_EXPRESSION, (int)track, seq_amount + (seq_action == SEQ_ARTICULATE ? MD_EXPRESSION_GATE_OFFSET : 0));
        return;
    }
    static const int commands[] = {MD_SEQ_COPY, MD_SEQ_PASTE, MD_SEQ_TRANSPOSE, MD_SEQ_MIRROR};
    md_command(commands[seq_action], (int)track, seq_action == SEQ_TRANSPOSE ? seq_interval + 24 + 64 * seq_scope : seq_scope);
}

/* The engine's answer to a tool, shown once. */
static void seq_notice_poll(void) {
    static const char *const notices[] = {"", "COPIED TO CLIPBOARD", "PASTED - F2 UNDOES", "TRANSPOSED - F2 UNDOES",
        "REVERSED - F2 UNDOES", "COPY THE SAME SCOPE FIRST", "NOTE LIMIT - NO CHANGE", "NO NOTES IN SELECTION",
        "ENABLE A SYNTH SEQUENCE", "ACCENTED - F2 UNDOES", "GATES SET - F2 UNDOES", "NO CHANGE AT THIS AMOUNT"};
    const uint32_t n = md_sequence_notice();
    if (n == seq_notice_seen) return;
    seq_notice_seen = n;
    const unsigned status = n & 15u;
    if (page == V_SEQ_TOOLS && status > 0 && status < sizeof notices / sizeof *notices) say(notices[status]);
}

/* ---- banks: sixteen project slots as files -------------------------------------------------------------- */
static void bank_name(int slot, char out[12]) {
    out[0] = 'b'; out[1] = 'a'; out[2] = 'n'; out[3] = 'k'; out[4] = '-';
    out[5] = (char)('0' + slot / 10); out[6] = (char)('0' + slot % 10);
    out[7] = '.'; out[8] = 'b'; out[9] = 'i'; out[10] = 'n'; out[11] = 0;
}
static void banks_scan(void) {
    bank_used = 0;
    char name[12];
    for (int s = 0; s < 16; s++) { bank_name(s, name); if (vibe_store_size(name) > 0) bank_used |= (uint16_t)(1u << s); }
}
static void bank_save(void) {
    char name[12];
    bank_name(bank_slot, name);
    const md_patch *pp = md_project_state();
    if (vibe_store_write(name, pp, (int)sizeof *pp) == 0) { bank_used |= (uint16_t)(1u << bank_slot); say("PROJECT SAVED"); }
    else say("COULD NOT SAVE");
}
static void bank_load(void) {
    char name[12];
    bank_name(bank_slot, name);
    if (md_playing()) { say("STOP FIRST, THEN LOAD"); return; }
    const int n = vibe_store_read(name, &patch_file, (int)sizeof patch_file);
    if (n <= 0) { say("EMPTY SLOT"); return; }
    say(md_import(&patch_file, (unsigned)n) ? "PROJECT LOADED" : "NOT A PROJECT");
}

/* ---- navigation ------------------------------------------------------------------------------------------ */
static void set_view(int v) {
    if (v < 0 || v >= V_COUNT) return;
    page = v;
    group_last[views[v].group] = (uint8_t)v;
    show_controls = 0;
    show_menu = 0;
    for (unsigned i = 0; i < 4; i++) knob_acc[i] = 0.f;
    if (v == V_BROWSER) browse_sync();
    if (v == V_BANKS) banks_scan();
}

/* A colour key: its group; again: the next page of that group. FX starts at PERFORM. */
static void group_key(int g) {
    if (views[page].group != g) {
        set_view(g == G_FX ? V_PERFORM : group_last[g]);
        return;
    }
    int v = page;
    do { v = (v + 1) % V_COUNT; } while (views[v].group != g);
    set_view(v);
}

static int view_soft(unsigned i, const char **label) {
    static const char *const fav_keys[6] = {"F1","F2","F3","F4","F5","F6"};
    (void)fav_keys;
    switch (page) {
    case V_LIVE: *label = ""; return 1;                        /* favourite Biome names, drawn separately */
    case V_BROWSER: { static const char *const b[6] = {"LOAD","PREV","NEXT","FAMILY","",""}; *label = b[i]; return i < 4; }
    case V_PADSET: *label = ""; return 2;
    case V_ITINERA: { static const char *const b[6] = {"START","STOP","","","",""}; *label = b[i]; return i < 2; }
    case V_BANKS: { static const char *const b[6] = {"SAVE","LOAD","","","",""}; *label = b[i]; return i < 2; }
    case V_DRIFT_SCENES: { static const char *const b[6] = {"SCENE A","SCENE B","","","",""}; *label = b[i]; return i < 2; }
    case V_LEARN: case V_MAP: { static const char *const b[6] = {"PREV","NEXT","LEARN","CLEAR","",""}; *label = b[i]; return i < 4; }
    case V_SEQ_TOOLS: { static const char *const b[6] = {"APPLY","UNDO","","","",""}; *label = b[i]; return i < 2; }
    case V_MIDI: { static const char *const b[6] = {"NOTES OUT","","","","",""}; *label = b[i]; return i < 1; }
    case V_RADIO: { static const char *const b[6] = {"ON AIR","SKIP","OFF AIR","ID CHIME","",""}; *label = b[i]; return i < 4; }
    case V_SEQ_RULES: { static const char *const b[6] = {"FILL","UNDO","","","",""}; *label = b[i]; return i < 2; }
    default: *label = track_labels[i]; return (int)i < md_active_tracks() ? 3 : 0;
    }
}

static void soft_key(unsigned id) {
    switch (page) {
    case V_LIVE: select_biome(id); return;
    case V_PADSET: moment_key(id); return;
    case V_BROWSER:
        if (shift_held) { if (id < 6) { favorites[id] = (uint16_t)browse_id; say("FAVOURITE STORED"); } return; }   /* (0.18: before navigating) */
        if (id == 0) load_biome_id(browse_id);
        else if (id == 1) browse_step(-1);
        else if (id == 2) browse_step(1);
        else if (id == 3) { browse_family = browse_family + 1 >= (int)md_biome_family_count() ? -1 : browse_family + 1;
                            if (!browse_matches(browse_id)) browse_step(1); }
        return;
    case V_ITINERA:
        if (id == 0) {
            if (md_playing()) { say("STOP FIRST, THEN START A DREAM"); return; }
            md_command(MD_ITINERA_START, it_preset, it_length + it_pace * 4 + it_hall * 12);
            say("DREAM BEGINS");
        } else if (id == 1) stop_all();
        return;
    case V_BANKS: if (id == 0) bank_save(); else if (id == 1) bank_load(); return;
    case V_RADIO:
        if (id == 0) radio_set(1); else if (id == 1) radio_skip(); else if (id == 2) radio_set(0);
        else if (id == 3) { radio_chime = !radio_chime; say(radio_chime ? "STATION CHIME ON" : "STATION CHIME OFF"); }
        return;
    case V_MIDI:
        if (id == 0) { settings.notes_out = !settings.notes_out; say(settings.notes_out ? "NOTES OUT: CH 1-6, KIT 10" : "NOTES OUT OFF"); }
        return;
    case V_SEQ_TOOLS: if (id == 0) seq_apply(); else if (id == 1) md_command(MD_RETURN, 0, 1); return;
    case V_SEQ_RULES:
        if (id == 0) { if (md_playing()) md_command(MD_FILL, 0, -1); else say("PLAY TO QUEUE THE FILL"); }
        else if (id == 1) md_command(MD_RETURN, 0, 1);
        return;
    case V_DRIFT_SCENES:
        if (id < 2) {
            if (md_state()->drift.mode) say("TURN DRIFT OFF TO CAPTURE");
            else { md_command(MD_DRIFT_CAPTURE, 0, (int)id); say(id ? "SCENE B CAPTURED" : "SCENE A CAPTURED"); }
        }
        return;
    case V_LEARN: case V_MAP:
        if (id == 0) learn_slot = clampi(learn_slot - 1, 0, 15);
        else if (id == 1) learn_slot = clampi(learn_slot + 1, 0, 15);
        else if (id == 2) { md_command(MD_LEARN_ARM, learn_slot, 1); say("MOVE A CONTROLLER"); }
        else if (id == 3) md_command(MD_LEARN_CLEAR, learn_slot, 1);
        return;
    default:
        if (id < (unsigned)md_active_tracks()) track = id;
    }
}

/* Encoders 1-4 on the open page. Fractional detents add up until a whole step. */
static void view_encoder(unsigned k, float detents) {
    Knob kb;
    if (!knob_of(page, k, &kb)) return;
    if (kb.cmd == L_PADSET) { pad_setting(k, detents); return; }
    knob_acc[k] += detents;
    const int steps = (int)knob_acc[k];
    if (!steps) return;
    knob_acc[k] -= (float)steps;
    const int target = kb.value + steps * kb.step;
    switch (kb.cmd) {
    case L_FAMILY:
        browse_family = clampi(target, -1, (int)md_biome_family_count() - 1);
        if (!browse_matches(browse_id)) browse_step(1);
        return;
    case L_BROWSE: browse_step(steps); return;
    case L_LINK_SLOT: link_slot = clampi(target, 0, MD_LINKS - 1); return;
    case L_LEARN_SLOT: learn_slot = clampi(target, 0, 15); return;
    case L_JSECTION: journey_edit = clampi(target, 0, 7); return;
    case L_JTRACK: track = (unsigned)clampi(target, 0, md_active_tracks() - 1); return;
    case L_KEEP: md_command(MD_JOURNEY_KEEP_TRACK, journey_edit, (int)track * 2 + (steps > 0)); return;
    case L_BANK: bank_slot = clampi(target, 0, 15); return;
    case L_IT_PRESET: it_preset = clampi(target, 0, MD_ITINERA_PRESETS - 1); return;
    case L_IT_LENGTH: it_length = clampi(target, 0, 3); return;
    case L_IT_PACE: it_pace = clampi(target, 0, 2); return;
    case L_IT_HALL: it_hall = clampi(target, 0, 2); return;
    case L_ARM: md_command(MD_LEARN_ARM, learn_slot, steps > 0); return;
    case L_SCOPE:
        seq_scope = clampi(target, 0, 1);
        if (!seq_scope && seq_action > SEQ_TRANSPOSE) seq_action = SEQ_TRANSPOSE;
        return;
    case L_ACTION: seq_action = clampi(target, 0, seq_scope ? SEQ_ARTICULATE : SEQ_TRANSPOSE); return;
    case L_INTERVAL: seq_interval = clampi(target, -24, 24); return;
    case L_AMOUNT: seq_amount = clampi(target, 0, 100); return;
    case L_TAKE: rec_seconds_choice = clampi(target, 0, 3); return;
    case L_RADIO: radio_set(clampi(target, 0, 1)); return;
    case L_RADIO_LEN: radio_length_choice = clampi(target, 0, 2); return;
    case L_RADIO_DREAMS: radio_dreams = clampi(target, 0, 1); return;
    case L_WANDER: settings.wander = clampi(target, 0, 1); return;
    case L_RADIO_AUTO: radio_autostart = clampi(target, 0, 1); say(radio_autostart ? "ON AIR AT EVERY START" : "STARTS QUIET"); return;
    case L_TOUCH: touch_percent = (unsigned)clampi(target, 0, 100); touch_set = 1; md_set_velocity_touch(touch_percent); return;
    }
    if (kb.cmd == MD_MIDI_ROUTE) { md_set_midi_route(track, (unsigned)clampi(target, 0, 16)); return; }
    if (kb.cmd == MD_MIDI_MACRO) { md_set_midi_macro_enabled(clampi(target, 0, 1)); return; }
    if (kb.cmd == MD_LEARN_CC) { md_command(MD_LEARN_CC, learn_slot, md_next_learn_cc(kb.value, steps)); return; }
    if (md_command_space()) md_command(kb.cmd, kb.track, clampi(target, kb.lo, kb.hi));
}
