/* Performance settings, DAW parameters (SDK 8) and saved state (SDK 21) for the
 * Performance Garden. Included by adapter-check.c after the pad and Biome helpers.
 *
 * Parameter ids are permanent: DAW automation hangs on them. Never rename an id. */

typedef struct {
    float texture_depth, space_depth, tail;   /* encoder A, encoder B, seconds of pad release */
    int bars, grid, target;                   /* gesture bars 4/8, 1 = start on the next bar, 0 = selected track else T1..T6 */
    int cc_out;                               /* 1 = the performance lanes go out as MIDI CC 20-28, channel 1 */
    int wander;                               /* 1 = the garden moves by itself while the hands rest */
    int tone_mode;                            /* 0 note, 1 chord, 2 arpeggio */
    int gesture_speed;                        /* 0 half, 1 normal, 2 double, 3 reverse */
    int notes_out;                            /* 1 = the dream's notes as MIDI: tracks on channels 1-6, the kit on 10 */
} garden_settings;
static garden_settings settings;
/* Radio settings (behaviour in garden-radio.inc), saved here */
static int radio_length_choice, radio_dreams, radio_autostart, radio_chime;
/* MOMENTS data (behaviour in garden-moments.inc) */
enum { MOMENTS = 6, MOMENT_LANES = 9 };
typedef struct { float lane[MOMENT_LANES], texture_depth, space_depth; uint32_t used; } garden_moment;
static garden_moment moments[MOMENTS];
static int moment_active = -1;                  /* -1 = none */
static void moment_recall(int m);
static int gesture_armed;
static unsigned gesture_step;
static float perf[9];                         /* automation lanes: 8 texture/space + pulse, 0..1 */
static float depth_hint_until;                /* show encoder A/B on the display until this time (s) */
static int depth_hint_which;

enum {
    P_BIOME, P_TEMPO, P_ROOT, P_SCALE, P_DENSITY, P_MOTION, P_COLOUR, P_SPACE_MACRO, P_FX_RETURN,
    P_TEXTURE_DEPTH, P_SPACE_DEPTH, P_TAIL, P_BARS, P_GRID, P_TARGET,
    P_PERF, P_PULSE = P_PERF + 8, P_CC_OUT, P_WANDER, P_MOMENT, P_TONE_MODE, P_GESTURE_SPEED, P_NOTES_OUT, P_COUNT
};
static const char *const biome_choices[] = {"Favourite 1","Favourite 2","Favourite 3","Favourite 4","Favourite 5","Favourite 6"};
static const char *const scale_choices[] = {"Chromatic","Minor","Major","Dorian","Minor pent","Major pent",
    "Whole tone","Phrygian","Ukr dorian","Harm minor","Dbl harm min","Harm major","Dbl harm maj"};
static const char *const bar_choices[] = {"4 bars","8 bars"};
static const char *const grid_choices[] = {"Free","Next bar"};
static const char *const cc_choices[] = {"Off","CC 20-28"};
static const char *const wander_choices[] = {"Off","On"};
static const char *const tone_mode_choices[] = {"Note","Chord","Arpeggio"};
static const char *const speed_choices[] = {"Half","Normal","Double","Reverse"};
static const char *const notes_choices[] = {"Off","Ch 1-6, kit 10"};
static const char *const moment_choices[] = {"None","M1","M2","M3","M4","M5","M6"};
static const char *const target_choices[] = {"Selected","T1","T2","T3","T4","T5","T6"};
static const vibe_param garden_params[P_COUNT] = {
    {"biome","Biome",0,5,0,"",6,biome_choices},
    {"tempo","Tempo",30,220,90,"BPM",191,0},
    {"root","Root",0,11,0,"",12,notes},
    {"scale","Scale",0,12,1,"",13,scale_choices},
    {"macro_density","Density",0,100,50,"%",0,0},
    {"macro_motion","Motion",0,100,50,"%",0,0},
    {"macro_colour","Colour",0,100,50,"%",0,0},
    {"macro_space","Space",0,100,50,"%",0,0},
    {"fx_return","FX return",0,100,50,"%",0,0},
    {"texture_depth","Texture depth",0,100,100,"%",0,0},
    {"space_depth","Space depth",0,100,100,"%",0,0},
    {"pad_tail","Pad tail",0.05f,6,0.3f,"s",0,0},
    {"gesture_bars","Gesture length",0,1,0,"",2,bar_choices},
    {"gesture_grid","Gesture start",0,1,0,"",2,grid_choices},
    {"tone_target","Tone pads play",0,6,0,"",7,target_choices},
    {"perf_forest","Forest",0,1,0,"",0,0},
    {"perf_air","Air",0,1,0,"",0,0},
    {"perf_water","Water",0,1,0,"",0,0},
    {"perf_stardust","Stardust",0,1,0,"",0,0},
    {"perf_bloom","Bloom",0,1,0,"",0,0},
    {"perf_echo","Echo",0,1,0,"",0,0},
    {"perf_grain","Grain",0,1,0,"",0,0},
    {"perf_dusk","Dusk",0,1,0,"",0,0},
    {"perf_pulse","Pulse",0,1,0,"",0,0},
    {"cc_out","Performance as MIDI CC",0,1,0,"",2,cc_choices},
    {"wander","Wander",0,1,0,"",2,wander_choices},
    {"moment","Moment",0,6,0,"",7,moment_choices},
    {"tone_mode","Tone pads",0,2,0,"",3,tone_mode_choices},
    {"gesture_speed","Gesture playback",0,3,1,"",4,speed_choices},
    {"notes_out","Dream as MIDI",0,1,0,"",2,notes_choices},
};
static const char *const garden_groups[P_COUNT] = {
    "DREAM · BIOME","DREAM · BIOME","DREAM · BIOME","DREAM · BIOME",
    "DREAM · MACROS","DREAM · MACROS","DREAM · MACROS","DREAM · MACROS","DREAM · MACROS",
    "PADS · PLAYING","PADS · PLAYING","PADS · PLAYING","PADS · GESTURE","PADS · GESTURE","PADS · PLAYING",
    "PERFORM · TEXTURE","PERFORM · TEXTURE","PERFORM · TEXTURE","PERFORM · TEXTURE",
    "PERFORM · SPACE","PERFORM · SPACE","PERFORM · SPACE","PERFORM · SPACE","PERFORM · FORM",
    "PADS · PLAYING", "PADS · MOMENTS", "PADS · MOMENTS", "PADS · PLAYING", "PADS · GESTURE", "PADS · PLAYING",
};
/* What was last told to the host, and a short hold after a host change while the engine catches up. */
static float reported[P_COUNT];
static uint8_t param_hold[P_COUNT];

static void settings_defaults(void) {
    settings = (garden_settings){1.f, 1.f, .3f, 4, 0, 0, 0, 0, TONE_NOTE, 1, 0};
    gesture_armed = 0;
    for (unsigned i = 0; i < 9; i++) perf[i] = 0.f;
}

static unsigned tone_track(void) {
    const unsigned t = settings.target ? (unsigned)settings.target - 1u : track;
    return t < (unsigned)md_active_tracks() ? t : track;
}

static unsigned gesture_ticks(void) {
    const float tempo = (float)md_state()->tempo;
    return (unsigned)(24000.f * settings.bars / (tempo < 30.f ? 30.f : tempo));
}

/* GESTURE pad and pedal switch: with grid on and the dream playing, the take waits for the next bar. */
static void gesture_press(void) {
    /* 0.18: a tap during a layer pass ends the pass and keeps looping (it used to stop the whole loop) */
    if (gesture.mode == GESTURE_OVERDUB) { dream_gesture_overdub(&gesture); say("GESTURE LOOPS"); return; }
    if (gesture.mode == GESTURE_IDLE && settings.grid && md_playing()) {
        gesture_armed = !gesture_armed;
        gesture_step = md_status()->step;
        say(gesture_armed ? "GESTURE STARTS ON THE NEXT BAR" : "GESTURE CANCELLED");
        return;
    }
    gesture_armed = 0;
    const int was = gesture.mode;
    dream_gesture_tap(&gesture, gesture_ticks());
    /* say what just happened: the pad's states are not obvious from its name */
    say(gesture.mode == GESTURE_RECORDING ? "CAPTURING PADS 5-12" : gesture.mode == GESTURE_LOOPING ? "GESTURE LOOPS" :
        was == GESTURE_RECORDING ? "NOTHING CAPTURED - PLAY PADS 5-12" : "GESTURE STOPPED");
}

/* Audio side, once per block. */
static void gesture_grid_tick(void) {
    if (!gesture_armed) return;
    if (!md_playing()) { gesture_armed = 0; return; }
    const unsigned step = md_status()->step;
    if (step != gesture_step && (step & 15u) == 0u) {
        gesture_armed = 0;
        dream_gesture_tap(&gesture, gesture_ticks());
    }
    gesture_step = step;
}

static float engine_param(unsigned i) {
    const md_patch *p = md_project_state();
    switch (i) {
    case P_BIOME: return (float)selected;
    case P_TEMPO: return (float)p->tempo;
    case P_ROOT: return (float)p->root;
    case P_SCALE: return (float)p->scale;
    case P_DENSITY: case P_MOTION: case P_COLOUR: case P_SPACE_MACRO: return p->macro[i - P_DENSITY] / 10.f;
    case P_FX_RETURN: return p->fx_level / 10.f;
    case P_TEXTURE_DEPTH: return settings.texture_depth * 100.f;
    case P_SPACE_DEPTH: return settings.space_depth * 100.f;
    case P_TAIL: return settings.tail;
    case P_BARS: return settings.bars == 8 ? 1.f : 0.f;
    case P_GRID: return (float)settings.grid;
    case P_TARGET: return (float)settings.target;
    case P_CC_OUT: return (float)settings.cc_out;
    case P_WANDER: return (float)settings.wander;
    case P_MOMENT: return (float)(moment_active + 1);
    case P_TONE_MODE: return (float)settings.tone_mode;
    case P_GESTURE_SPEED: return (float)settings.gesture_speed;
    case P_NOTES_OUT: return (float)settings.notes_out;
    default: return perf[i - P_PERF];
    }
}

static float clampf(float v, float lo, float hi) { return !(v == v) ? lo : v < lo ? lo : v > hi ? hi : v; }   /* (0.25: NaN -> lo, Codex review 03) */
/* (0.25) a saved float: finite and in range, else the default (a damaged garden.bin never reaches the audio) */
static float restore_f(float v, float lo, float hi, float def) { return v == v && v >= lo && v <= hi ? v : def; }

static void apply_param(unsigned i, float v) {
    if (i >= P_COUNT) return;
    const vibe_param *d = &garden_params[i];
    v = clampf(v, d->min, d->max);
    reported[i] = v;
    param_hold[i] = 16;
    const int n = (int)(v + .5f);
    switch (i) {
    case P_BIOME: if ((unsigned)n != selected) select_biome((unsigned)n); break;
    case P_TEMPO: md_command(MD_TEMPO, 0, n); break;
    case P_ROOT: md_command(MD_ROOT, 0, n); break;
    case P_SCALE: md_command(MD_SCALE, 0, n); break;
    case P_DENSITY: case P_MOTION: case P_COLOUR: case P_SPACE_MACRO:
        md_command(MD_MACRO, (int)(i - P_DENSITY), (int)(v * 10.f + .5f)); break;
    case P_FX_RETURN: md_command(MD_FX_LEVEL, 0, (int)(v * 10.f + .5f)); break;
    case P_TEXTURE_DEPTH: settings.texture_depth = v / 100.f; break;
    case P_SPACE_DEPTH: settings.space_depth = v / 100.f; break;
    case P_TAIL: settings.tail = v; break;
    case P_BARS: settings.bars = n ? 8 : 4; break;
    case P_GRID: settings.grid = n; break;
    case P_TARGET: settings.target = n; break;
    case P_CC_OUT: settings.cc_out = n != 0; break;
    case P_WANDER: settings.wander = n != 0; break;
    case P_TONE_MODE: settings.tone_mode = n; break;
    case P_GESTURE_SPEED: settings.gesture_speed = n; break;
    case P_NOTES_OUT: settings.notes_out = n != 0; break;
    case P_MOMENT: if (n - 1 != moment_active) moment_recall(n ? n - 1 : moment_active); break;
    default: perf[i - P_PERF] = v; break;
    }
}

/* The firmware's own changes (knobs, Biome keys, a loaded patch) go to the host. Once per block. */
static void params_report(void) {
    for (unsigned i = 0; i < P_COUNT; i++) {
        if (param_hold[i]) { param_hold[i]--; continue; }
        const float v = engine_param(i);
        const float d = v - reported[i];
        if (d > .001f || d < -.001f) {
            reported[i] = v;
            vibe_param_set((int)i, v);
        }
    }
}

static void params_init(void) {
    vibe_params(garden_params, P_COUNT);
    vibe_param_groups(garden_groups);
    for (unsigned i = 0; i < P_COUNT; i++) { reported[i] = garden_params[i].def; param_hold[i] = 0; }
}

/* ---- saved state ---------------------------------------------------------------------------------------- */
enum { GARDEN_MAGIC = 0x4d444756u /* "MDGV" */, GARDEN_VERSION = 11 };   /* 11: MOVES (0.26); 10: Garden Listens – inputs, SING, CV (0.24); 9, 8, 7 still load */   /* 10: Garden Listens – inputs, SING, CV (0.24); 9, 8, 7 still load */   /* 9: wandering worlds (0.21); 8 and 7 still load */   /* 8: MIDI + SYSTEM settings, 6400-frame gestures; 7 still loads */   /* 7: radio settings */   /* 6: notes out */   /* 5: favourite Biomes, 2.1 pages */
enum { GARDEN_VERSION_4 = 4 };   /* 2: cc_out, latched pads; 3: wander, moments; 4: tone mode, gesture speed */
typedef struct {
    uint32_t magic, version, selected, track, page;
    garden_settings settings;
    uint32_t gesture_mode, gesture_length, latched;
    garden_moment moments[MOMENTS];
    uint16_t favorites[6];
    int32_t radio_length, radio_dreams, radio_autostart, radio_chime;
} garden_header7;
/* 8: what the engine does not keep in its patch – the MIDI page, MIDI LEARN / MAP and TOUCH (0.18) */
typedef struct {
    garden_header7 base;
    int32_t clock, snap, macro_cc, touch;
    uint8_t routes[MD_TRACKS], pad[2];
    md_midi_mapping mappings[MD_MIDI_BINDINGS];
} garden_header8;
/* 9: the world entered and how often the worlds wander on (minutes, 0 off) (0.21) */
static int world_wander, world_here;
typedef struct { garden_header8 h8; int32_t world_wander, world_here; } garden_header9;
/* 10: LISTEN, SING and CV (0.24) */
typedef struct { garden_header9 h9; garden_listen10 listen; } garden_header10;
/* 11: MOVES (0.26) */
static int tour_done_saved;                            /* (0.26, garden-tour.inc) */
typedef struct { garden_header9 h9; garden_listen listen; int32_t tour_done; } garden_header;
static uint8_t garden_file[sizeof(garden_header) + GESTURE_FRAMES * GESTURE_LANES];
static md_patch patch_file;

/* (0.31) before a snapshot: what the host holds for each parameter now, in case it changed it without an audio block since
   (a DAW saving while its audio is suspended) */
static void params_pull(void) {
    for (unsigned i = 0; i < P_COUNT; i++) {
        const float v = vibe_param_get((int)i);
        const float d = v - reported[i];
        if (v == v && (d > .001f || d < -.001f)) apply_param(i, v);
    }
}

static void state_save(void) {
    params_pull();
    md_flush_commands();  /* SAVE may arrive before the next audio callback (a suspended DAW). */
    const md_patch *project = md_project_state();
    /* (0.31, Codex review 11 B5) a refused write is no longer silent: the player is told to save again */
    const int patch_ok = vibe_store_write("patch.bin", project, (int)sizeof *project) >= 0;
    garden_header8 full = {{GARDEN_MAGIC, GARDEN_VERSION, selected, track, (uint32_t)page, settings,
                       gesture.mode == GESTURE_LOOPING || gesture.mode == GESTURE_OVERDUB ? GESTURE_LOOPING : GESTURE_IDLE,
                       gesture.length, latched, {{{0}}}}, 0};
    garden_header7 *hb = &full.base;
    for (unsigned m = 0; m < MOMENTS; m++) hb->moments[m] = moments[m];
    for (unsigned i = 0; i < 6; i++) hb->favorites[i] = favorites[i];
    hb->radio_length = radio_length_choice; hb->radio_dreams = radio_dreams;
    hb->radio_autostart = radio_autostart; hb->radio_chime = radio_chime;
    if (gesture.mode == GESTURE_RECORDING) hb->gesture_length = 0;
    full.clock = md_clock_source(); full.snap = md_midi_quantize(); full.macro_cc = md_midi_macro_enabled();
    full.touch = touch_set ? (int32_t)touch_percent : -1;     /* -1: never set, the engine's default stays */
    for (unsigned t = 0; t < MD_TRACKS; t++) full.routes[t] = (uint8_t)md_midi_route(t);
    for (unsigned s = 0; s < MD_MIDI_BINDINGS; s++) { const md_midi_mapping *m = md_mapping(s); if (m) full.mappings[s] = *m; }
    const garden_header h = {{full, world_wander, world_here}, lis, tour_done_saved};
    const unsigned bytes = h.h9.h8.base.gesture_length * GESTURE_LANES;
    for (unsigned i = 0; i < sizeof h; i++) garden_file[i] = ((const uint8_t *)&h)[i];
    for (unsigned i = 0; i < bytes; i++) garden_file[sizeof h + i] = ((const uint8_t *)gesture.frames)[i];
    const int garden_ok = vibe_store_write("garden.bin", garden_file, (int)(sizeof h + bytes)) >= 0;
    if (!patch_ok || !garden_ok) say(patch_ok || garden_ok ? "SAVE INCOMPLETE - SAVE AGAIN" : "SAVE FAILED - SAVE AGAIN");
}

/* At init, transport stopped: the saved patch through the engine's own version migration. */
static void state_restore(void) {
    const int patch_bytes = vibe_store_read("patch.bin", &patch_file, (int)sizeof patch_file);
    if (patch_bytes > 0) md_import(&patch_file, (unsigned)patch_bytes);
    const int n = vibe_store_read("garden.bin", garden_file, (int)sizeof garden_file);
    if (n < (int)sizeof(garden_header7)) return;
    garden_header7 h;
    for (unsigned i = 0; i < sizeof h; i++) ((uint8_t *)&h)[i] = garden_file[i];
    if (h.magic != GARDEN_MAGIC || h.version < 7 || h.version > GARDEN_VERSION) return;
    const unsigned head = h.version == 7 ? sizeof(garden_header7) : h.version == 8 ? sizeof(garden_header8) : h.version == 9 ? sizeof(garden_header9) : h.version == 10 ? sizeof(garden_header10) : sizeof(garden_header);   /* where the gesture starts */
    if (n < (int)head) return;
    if (h.version >= 11) {
        garden_header z;
        for (unsigned i = 0; i < sizeof z; i++) ((uint8_t *)&z)[i] = garden_file[i];
        listen_restore(&z.listen);
        tour_done_saved = z.tour_done != 0;
    } else if (h.version == 10) {                      /* 0.24 / 0.25: the same fields without MOVES */
        garden_header10 z;
        garden_listen l = lis;
        for (unsigned i = 0; i < sizeof z; i++) ((uint8_t *)&z)[i] = garden_file[i];
        for (unsigned i = 0; i < sizeof z.listen; i++) ((uint8_t *)&l)[i] = ((const uint8_t *)&z.listen)[i];
        l.moves = 0; l.moves_base = -1;
        listen_restore(&l);
        tour_done_saved = 1;                           /* (a player from 0.24 / 0.25 knows the garden) */
    }
    if (h.version < 11) tour_done_saved = 1;            /* (0.26) a saved garden from before: no first-steps tour */
    if (h.version >= 9) {
        garden_header9 y;
        for (unsigned i = 0; i < sizeof y; i++) ((uint8_t *)&y)[i] = garden_file[i];
        world_wander = y.world_wander == 5 || y.world_wander == 10 || y.world_wander == 20 ? y.world_wander : 0;
        world_here = y.world_here >= -1 && y.world_here < 48 ? y.world_here : -1;
    }
    if (h.version >= 8) {
        garden_header8 x;
        for (unsigned i = 0; i < sizeof x; i++) ((uint8_t *)&x)[i] = garden_file[i];
        md_command(MD_CLOCK_SOURCE, 0, x.clock < 0 ? 0 : x.clock > 3 ? 3 : x.clock);
        md_command(MD_MIDI_QUANTIZE, 0, x.snap != 0);
        md_set_midi_macro_enabled(x.macro_cc != 0);
        if (x.touch >= 0 && x.touch <= 100) { touch_percent = (unsigned)x.touch; touch_set = 1; md_set_velocity_touch(touch_percent); }
        for (unsigned t = 0; t < MD_TRACKS; t++) md_set_midi_route(t, x.routes[t] <= 16 ? x.routes[t] : 0);
        for (unsigned s = 0; s < MD_MIDI_BINDINGS; s++) if (md_mapping_valid(&x.mappings[s])) md_set_mapping(s, &x.mappings[s]);
    }
    selected = h.selected < 6 ? h.selected : 0;
    track = h.track < MD_TRACKS ? h.track : 0;
    page = h.page < V_COUNT ? (int)h.page : V_LIVE;
    if (h.version < 10 && page >= V_LISTEN) page = page + 2 < V_CV ? page + 2 : V_LIVE;   /* (0.24: LISTEN and SING came in after TAPE) */
    for (unsigned i = 0; i < 6; i++) favorites[i] = h.favorites[i] < MD_BIOMES ? h.favorites[i] : favorites[i];
    settings.texture_depth = restore_f(h.settings.texture_depth, 0.f, 1.f, 1.f);
    settings.space_depth = restore_f(h.settings.space_depth, 0.f, 1.f, 1.f);
    settings.tail = restore_f(h.settings.tail, .05f, 6.f, .3f);
    settings.bars = h.settings.bars == 8 ? 8 : 4;
    settings.grid = h.settings.grid != 0;
    settings.target = h.settings.target >= 0 && h.settings.target <= 6 ? h.settings.target : 0;
    settings.cc_out = h.settings.cc_out != 0;
    settings.wander = h.settings.wander != 0;
    settings.notes_out = h.settings.notes_out != 0;
    radio_length_choice = h.radio_length >= 0 && h.radio_length <= 2 ? h.radio_length : 1;
    radio_dreams = h.radio_dreams != 0;
    radio_autostart = h.radio_autostart != 0;
    radio_chime = h.radio_chime != 0;
    settings.tone_mode = h.settings.tone_mode >= 0 && h.settings.tone_mode < TONE_MODES ? h.settings.tone_mode : TONE_NOTE;
    settings.gesture_speed = h.settings.gesture_speed >= 0 && h.settings.gesture_speed <= 3 ? h.settings.gesture_speed : 1;
    for (unsigned m = 0; m < MOMENTS; m++) {
        garden_moment k = h.moments[m];
        for (unsigned i = 0; i < MOMENT_LANES; i++) k.lane[i] = restore_f(k.lane[i], 0.f, 1.f, 0.f);
        k.texture_depth = restore_f(k.texture_depth, 0.f, 1.f, 1.f);
        k.space_depth = restore_f(k.space_depth, 0.f, 1.f, 1.f);
        k.used = k.used != 0;
        moments[m] = k;
    }
    /* Latched texture and space pads come back open; tone, THIN and PULSE wait for the hands. */
    for (unsigned i = 4; i < 12; i++) if ((h.latched >> i) & 1u) {
        latched |= 1u << i;
        pads.pads[i].held = 1;
        pads.pads[i].velocity = .8f;
    }
    if (h.gesture_length && h.gesture_length <= GESTURE_FRAMES &&
        n >= (int)(head + h.gesture_length * GESTURE_LANES)) {
        for (unsigned i = 0; i < h.gesture_length * GESTURE_LANES; i++)
            ((uint8_t *)gesture.frames)[i] = garden_file[head + i];
        gesture.length = h.gesture_length;
        gesture.position = 0;
        gesture.mode = h.gesture_mode == GESTURE_LOOPING ? GESTURE_LOOPING : GESTURE_IDLE;
    }
}

/* ---- surface helpers ---------------------------------------------------------------------------------- */
static float now_seconds(void) { return md_status()->frames / 44100.f; }

static void show_depth(int which) {
    depth_hint_which = which;
    depth_hint_until = now_seconds() + 1.5f;
}

static const char *const pad_knob_labels[4] = {"TAIL", "GESTURE", "START", "TONE PADS"};
static const char *const pad_knob_shift_labels[4] = {"TONE MODE", "PLAYBACK", "WANDER", "MIDI CC OUT"};
static int tone_mode(void) { return settings.tone_mode; }
static float gesture_speed_value(void) {
    static const float speeds[4] = {.5f, 1.f, 2.f, -1.f};
    return speeds[settings.gesture_speed & 3];
}
/* The GESTURE pad's caption, on the display and the stage. */
static const char *gesture_label(void) {
    static const char *const loops[4] = {"LOOP /2", "LOOP", "LOOP x2", "LOOP REV"};
    if (gesture_armed) return "WAIT";
    if (gesture.mode == GESTURE_RECORDING) return "CAPTURE";   /* (0.18: not "REC" – that is the WAV take) */
    if (gesture.mode == GESTURE_OVERDUB) return "LAYER";
    if (gesture.mode == GESTURE_LOOPING) return loops[settings.gesture_speed & 3];
    return "GESTURE";
}

/* FX page, second layer: encoders 1-4 set how the pads behave. */
static void pad_setting(unsigned index, float detents) {
    if (index == 0 && shift_held && detents != 0.f) {
        int m = settings.tone_mode + (detents > 0.f ? 1 : -1);
        settings.tone_mode = m < 0 ? 0 : m >= TONE_MODES ? TONE_MODES - 1 : m;
    } else if (index == 1 && shift_held && detents != 0.f) {
        int m = settings.gesture_speed + (detents > 0.f ? 1 : -1);
        settings.gesture_speed = m < 0 ? 0 : m > 3 ? 3 : m;
    } else if (index == 0 && !shift_held) {
        float t = settings.tail * vd_exp2(detents * .1f);
        settings.tail = clampf(t, .05f, 6.f);
    } else if (index == 1 && detents != 0.f && !shift_held) {
        settings.bars = detents > 0.f ? 8 : 4;
    } else if (index == 2 && detents != 0.f && shift_held) {
        settings.wander = detents > 0.f;
    } else if (index == 2 && detents != 0.f) {
        settings.grid = detents > 0.f;
    } else if (index == 3 && detents != 0.f && shift_held) {
        settings.cc_out = detents > 0.f;
    } else if (index == 3 && detents != 0.f) {
        int t = settings.target + (detents > 0.f ? 1 : -1);
        settings.target = t < 0 ? 0 : t > 6 ? 6 : t;
    }
}

static const char *pad_knob_value(unsigned index, char text[12]) {
    static const char *const modes[TONE_MODES] = {"NOTE", "CHORD", "ARP"};
    static const char *const speeds[4] = {"HALF", "NORMAL", "DOUBLE", "REVERSE"};
    if (index == 0 && shift_held) return modes[settings.tone_mode];
    if (index == 1 && shift_held) return speeds[settings.gesture_speed & 3];
    if (index == 0) {
        vg_ftoa(settings.tail, settings.tail < 1.f ? 2 : 1, text);
        unsigned n = 0; while (text[n]) n++;
        text[n] = ' '; text[n + 1] = 's'; text[n + 2] = 0;
        return text;
    }
    if (index == 1) return settings.bars == 8 ? "8 BARS" : "4 BARS";
    if (index == 2 && shift_held) return settings.wander ? "ON" : "OFF";
    if (index == 2) return settings.grid ? "NEXT BAR" : "FREE";
    if (shift_held) return settings.cc_out ? "CC 20-28" : "OFF";
    return settings.target ? track_labels[settings.target - 1] : "SELECTED";
}

static void percent_text(const char *name, float v, char *out) {
    unsigned n = 0;
    while (name[n]) { out[n] = name[n]; n++; }
    out[n++] = ' ';
    char digits[12];
    vg_itoa((int)(v * 100.f + .5f), digits);
    for (unsigned i = 0; digits[i]; i++) out[n++] = digits[i];
    out[n++] = '%';
    out[n] = 0;
}

/* ---- the plugin's Editor (SDK 9/15): cards, controls, the gesture as a curve ------------------------------- */
enum { EDITOR_CLEAR_GESTURE = 1, EDITOR_NEXT_BIOME, EDITOR_RECORD_GESTURE, EDITOR_MUTATE, EDITOR_UNDO };
static const vibe_card garden_cards[] = {
    {"BIOME", 4, "Biome, tempo and key - F1-F6 on LIVE"},
    {"MACROS", 8, "The four macros of SOUND and the FX return"},
    {"PLAYING", 6, "Encoder A / B, FX twice for tail and tone pads, SHIFT latches a pad"},
    {"MOMENTS", 6, "F1-F6 on PADS recall, SHIFT + F stores; wander moves the garden"},
    {"GESTURE", 6, "Pad 16 records TEXTURE and SPACE and loops it"},
    {"TEXTURE", 5, "Automate pads 5-8: forest, air, water, stardust"},
    {"SPACE", 5, "Automate pads 9-12: bloom, echo, grain, dusk"},
    {"FORM", 2, "Pad 14"},
};

/* The recorded gesture, all eight lanes summed, as 64 points. Flat while nothing is recorded. */
static int gesture_curve(const char *tab, const char *card, float *points, int max) {
    (void)tab; (void)card;
    const int count = max < 64 ? max : 64;
    for (int i = 0; i < count; i++) {
        float v = 0.f;
        if (gesture.length) {
            const unsigned at = (unsigned)i * gesture.length / (unsigned)count;
            for (unsigned l = 0; l < GESTURE_LANES; l++) v += gesture.frames[at][l];
            v /= 255.f * 3.f;
        }
        points[i] = v > 1.f ? 1.f : v;
    }
    return count;
}

static void editor_init(void) {
    vibe_editor_cards(garden_cards, (int)(sizeof garden_cards / sizeof garden_cards[0]));
    vibe_editor_accent(0x83d2a6);
    vibe_editor_tab_color("DREAM", 0x83d2a6);
    vibe_editor_tab_color("PADS", 0xc0a0ed);
    vibe_editor_tab_color("PERFORM", 0x86a8eb);
    vibe_editor_control(P_BIOME, VIBE_CTL_MENU);
    vibe_editor_control(P_ROOT, VIBE_CTL_MENU);
    vibe_editor_control(P_SCALE, VIBE_CTL_MENU);
    vibe_editor_control(P_TEXTURE_DEPTH, VIBE_CTL_SLIDER);
    vibe_editor_control(P_SPACE_DEPTH, VIBE_CTL_SLIDER);
    vibe_editor_control(P_BARS, VIBE_CTL_TOGGLE);
    vibe_editor_control(P_GRID, VIBE_CTL_TOGGLE);
    vibe_editor_control(P_TARGET, VIBE_CTL_MENU);
    /* Two XY pads per card: play the textures and spaces with the mouse, record them as automation. */
    vibe_editor_control(P_PERF + 0, VIBE_CTL_XY_X); vibe_editor_control(P_PERF + 1, VIBE_CTL_XY_Y);
    vibe_editor_control(P_PERF + 2, VIBE_CTL_XY_X); vibe_editor_control(P_PERF + 3, VIBE_CTL_XY_Y);
    vibe_editor_control(P_PERF + 4, VIBE_CTL_XY_X); vibe_editor_control(P_PERF + 5, VIBE_CTL_XY_Y);
    vibe_editor_control(P_PERF + 6, VIBE_CTL_XY_X); vibe_editor_control(P_PERF + 7, VIBE_CTL_XY_Y);
    vibe_editor_control(P_PULSE, VIBE_CTL_SLIDER);
    vibe_editor_control(P_CC_OUT, VIBE_CTL_TOGGLE);
    vibe_editor_control(P_WANDER, VIBE_CTL_TOGGLE);
    vibe_editor_control(P_MOMENT, VIBE_CTL_MENU);
    vibe_editor_control(P_TONE_MODE, VIBE_CTL_MENU);
    vibe_editor_control(P_GESTURE_SPEED, VIBE_CTL_MENU);
    vibe_editor_control(P_NOTES_OUT, VIBE_CTL_TOGGLE);
    vibe_editor_curve("GESTURE", gesture_curve);
    vibe_editor_button("GESTURE", "CAPTURE / STOP", EDITOR_RECORD_GESTURE);
    vibe_editor_button("GESTURE", "CLEAR", EDITOR_CLEAR_GESTURE);
    vibe_editor_button("BIOME", "NEXT BIOME", EDITOR_NEXT_BIOME);
    vibe_editor_button("BIOME", "VARIATION", EDITOR_MUTATE);
    vibe_editor_button("BIOME", "UNDO", EDITOR_UNDO);
    vibe_param_help(P_BIOME, "Six Biomes from the Mutable Dreams library. Same as F1-F6 on the LIVE page.");
    vibe_param_help(P_TEXTURE_DEPTH, "Encoder A. Scales everything the TEXTURE row plays, live, gesture and automation.");
    vibe_param_help(P_SPACE_DEPTH, "Encoder B or an expression pedal. Scales the SPACE row.");
    vibe_param_help(P_TAIL, "How long a texture or space pad keeps sounding after you let go.");
    vibe_param_help(P_BARS, "Length of a gesture take: four or eight bars of the current tempo.");
    vibe_param_help(P_GRID, "Next bar: a tapped gesture waits for the next bar line, so the loop sits in time.");
    vibe_param_help(P_TONE_MODE, "Note, a triad in the scale, or an arpeggio on the eighths. Device: SHIFT + encoder 1 on PADS.");
    vibe_param_help(P_GESTURE_SPEED, "Plays the gesture at half or double speed, or backwards. Device: SHIFT + encoder 2 on PADS.");
    vibe_param_help(P_NOTES_OUT, "Every note the dream plays goes out as MIDI: tracks 1-6 on channels 1-6, the kit on 10. Record it in the DAW.");
    vibe_param_help(P_WANDER, "Hands resting 2 s: one or two lanes drift on their own, up to 55 %. Device: SHIFT + encoder 3 on PADS.");
    vibe_param_help(P_MOMENT, "Glides to a stored moment. Store on the device: PADS layer, SHIFT + F1-F6. Tap the key again to glide out.");
    vibe_param_help(P_CC_OUT, "Texture and space lanes and PULSE as CC 20-28, channel 1. Device: SHIFT + encoder 4 on PADS.");
    vibe_param_help(P_TARGET, "Which track the four TONE pads play. Selected follows F1-F6 on the track pages.");
    for (unsigned i = 0; i < 9; i++)
        vibe_param_help(P_PERF + i, "Automation lane. Adds to the pad and the gesture, the stronger one wins.");
}

static void editor_event(const vibe_event *event) {
    if (event->id == EDITOR_CLEAR_GESTURE) { dream_gesture_clear(&gesture); gesture_armed = 0; }
    else if (event->id == EDITOR_NEXT_BIOME) select_biome((selected + 1) % 6);
    else if (event->id == EDITOR_RECORD_GESTURE) gesture_press();
    else if (event->id == EDITOR_MUTATE) mutate_dream();
    else if (event->id == EDITOR_UNDO && md_command_space()) md_command(MD_RETURN, 0, 1);
}
