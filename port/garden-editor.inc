/* The plugin's own Editor page (SDK 16, 0.15): the firmware side of editor/index.html.
 * The page sends small commands (first byte = what) and gets a status packet a few times a second.
 * Everything here is also on the display and the controls; the page is a second face, never the only one.
 *
 * page -> firmware:  1 PLAY · 2 STOP · 3 ON AIR toggle · 4 SKIP · 5 NEXT Biome · 6 VARIATION · 7 UNDO · 8 REC
 *                    9 GESTURE · 10 pad down (id, velocity 0-127) · 11 pad up (id) · 12 moment (k, store)
 *                    13 favourite (k) · 14 OVERDUB · 15 latch toggle (id)
 *                    0.16, the device's pages: 16 button (vibe_button id) · 17 colour group (0-6, 6 = DREAM)
 *                    18 encoder (VE id, signed steps) · 19 display click (x / 2, y / 2 in display pixels)
 *                    0.17, the Biome library: 20 send the library · 21 load Biome (id lo, hi) · 22 store as favourite (slot, id lo, hi)
 *                    0.24: 26 input on / off · 27 SING mode (0 off, 1 follow, 2 harmony, 3 answer)
 * firmware -> page:  101 library entries · 102 library names (families, scales) – only after 20, see editor_library
 *                    0.19, the worlds: 23 send the worlds · 24 enter world (index) · 25 wander (0, 5, 10, 20 minutes)
 *                    -> 104 the worlds' families, then 103 the worlds (total, first, count; per world: family, Biome (2),
 *                    tempo, name, story)
 * firmware -> page:  100 status (see editor_status) */
static unsigned editor_frame;
static int library_next = -2;
static int worlds_next = -2;                           /* (0.19) the same for the worlds */                          /* -2 idle, -1 the names next, else the next wheel position */

static void editor_pad(int id, int down, float velocity) {
    if (id < 0 || id >= PAD_COUNT) return;
    vibe_event p = {.type = VIBE_EV_PAD, .id = (uint8_t)id, .down = (uint8_t)(down != 0), .value = down ? velocity : 0.f};
    pad_from_stage = 1;
    vibe_on_event(&p);
    pad_from_stage = 0;
}

static void editor_data(const vibe_event *event) {
    const uint8_t *b = vibe_editor_data();
    const int n = (int)event->value;
    if (!b || n < 1) return;
    switch (b[0]) {
    case 1: md_command(MD_PLAY, 0, 1); break;
    case 2: stop_all(); break;
    case 3: radio_set(!radio.on); break;
    case 4: radio_skip(); break;
    case 5: select_biome((selected + 1) % 6); break;
    case 6: mutate_dream(); break;
    case 7: if (md_command_space()) md_command(MD_RETURN, 0, 1); break;
    case 8: rec_button(); break;
    case 9: gesture_press(); break;
    case 10: if (n >= 3) editor_pad(b[1], 1, b[2] / 127.f); break;
    case 11: if (n >= 2) editor_pad(b[1], 0, 0.f); break;
    case 12: if (n >= 3 && b[1] < MOMENTS) { if (b[2]) moment_store(b[1]); else moment_recall(b[1]); } break;
    case 13: if (n >= 2 && b[1] < 6) select_biome(b[1]); break;
    case 14: dream_gesture_overdub(&gesture); break;
    case 15: if (n >= 2 && b[1] < PAD_COUNT) { stage_shift = 1; editor_pad(b[1], 1, .8f); editor_pad(b[1], 0, 0.f); stage_shift = 0; } break;
    case 16: if (n >= 2 && b[1] == VB_SHIFT) sticky_shift = !sticky_shift;      /* SHIFT for the next control (0.18) */
             else if (n >= 2 && b[1] <= VB_DATA_PUSH) stage_button(b[1]);
             break;
    case 17: if (n >= 2 && b[1] < G_COUNT) open_group(b[1]); break;
    case 18: if (n >= 3 && b[1] <= VE_DATA) stage_encoder(b[1], (float)(int8_t)b[2]); break;
    case 19: if (n >= 3) display_click(b[1] * 2, b[2] * 2); break;
    case 20: library_next = -1; break;
    case 23: worlds_next = -1; break;
    case 24: if (n >= 2 && b[1] < WORLDS) enter_world(b[1]); break;
    case 25: if (n >= 2) { world_wander = b[1] == 5 || b[1] == 10 || b[1] == 20 ? b[1] : 0; world_wander_frames = 0; } break;
    case 26: if (n >= 2) { lis.on = b[1] != 0; listen_apply(); say(lis.on ? "INPUT ON - IN 1 / IN 2 LISTEN" : "INPUT OFF"); } break;   /* (0.24) */
    case 28: if (tour_step >= 0) tour_end(); else tour_start_from_page(); break;   /* (0.26) ? : FIRST STEPS on / off */
    case 27: if (n >= 2 && b[1] < SING_MODES) { lis.sing = b[1]; if (b[1] && !lis.on) { lis.on = 1; listen_apply(); } } break;
    case 21: if (n >= 3 && b[1] + 256 * b[2] < MD_BIOMES) { load_biome_id(b[1] + 256 * b[2]); say("NEW BIOME FROM THE LIBRARY"); } break;
    case 22: if (n >= 4 && b[1] < 6 && b[2] + 256 * b[3] < MD_BIOMES) {
                 favorites[b[1]] = (uint16_t)(b[2] + 256 * b[3]); selected = b[1]; say("FAVOURITE STORED"); }
             break;
    }
}

static unsigned put_text(uint8_t *out, unsigned k, const char *s, unsigned max) {
    unsigned n = 0;
    while (s[n] && n < max) n++;
    out[k++] = (uint8_t)n;
    for (unsigned i = 0; i < n; i++) out[k++] = (uint8_t)s[i];
    return k;
}

/* status: 100, playing, on air, in dream, character, seconds left (2), gesture mode, armed, rec state, rec seconds,
 * moment active + 1, selected favourite, latched (2), moments used (bits), 16 pad lights, then texts:
 * Biome name, family, radio line, page name, six favourite names; then the page's group and MENU (1) / HOME (2) open */
/* The library, when the page asks (command 20): first the names (102: families, scales), then the 271 Biomes in the
 * curated wheel order without Blank, about 90 a packet (101: total 271, index of the first, count, then per Biome: id (2), tempo, root,
 * scale, family, name). One packet a frame, before the status; the status waits meanwhile. 1 = sent or waiting. */
static int editor_library(void) {
    if (library_next < -1) return 0;
    static uint8_t out[4096];
    unsigned k = 0;
    if (library_next == -1) {
        out[k++] = 102;
        const unsigned nf = md_biome_family_count();
        out[k++] = (uint8_t)nf;
        for (unsigned i = 0; i < nf; i++) k = put_text(out, k, md_biome_family_name(i), 24);
        out[k++] = 13;
        for (int i = 0; i < 13; i++) k = put_text(out, k, md_scale_name(i), 24);
        if (vibe_editor_post(out, (int)k) == 0) library_next = 0;
        return 1;
    }
    static unsigned sent;                               /* entries sent so far (Blank is not one of the 271) */
    if (library_next == 0) sent = 0;
    unsigned pos = (unsigned)library_next;
    out[k++] = 101;
    out[k++] = (uint8_t)((MD_BIOMES - 1) & 255); out[k++] = (uint8_t)((MD_BIOMES - 1) >> 8);
    out[k++] = (uint8_t)(sent & 255); out[k++] = (uint8_t)(sent >> 8);
    const unsigned count_at = k++;
    unsigned count = 0;
    for (; pos < MD_BIOMES && k < sizeof out - 48; pos++) {
        const int id = md_biome_at(pos);
        if (id == MD_BLANK_BIOME) continue;
        md_biome_card card = {0};
        md_biome_card_info(id, &card);
        out[k++] = (uint8_t)(id & 255); out[k++] = (uint8_t)(id >> 8);
        out[k++] = card.tempo; out[k++] = card.root; out[k++] = card.scale;
        out[k++] = (uint8_t)md_biome_family_index(id);
        k = put_text(out, k, md_biome_name(id), 40);
        count++;
    }
    out[count_at] = (uint8_t)count;
    if (vibe_editor_post(out, (int)k) == 0) { sent += count; library_next = pos < MD_BIOMES ? (int)pos : -2; }
    return 1;
}

static int editor_worlds(void) {
    if (worlds_next < -1) return 0;
    static uint8_t out[4096];
    unsigned k = 0;
    if (worlds_next == -1) {
        out[k++] = 104;
        out[k++] = WORLD_FAMILIES;
        for (unsigned i = 0; i < WORLD_FAMILIES; i++) k = put_text(out, k, world_families[i], 40);
        if (vibe_editor_post(out, (int)k) == 0) worlds_next = 0;
        return 1;
    }
    const unsigned start = (unsigned)worlds_next;
    out[k++] = 103; out[k++] = WORLDS; out[k++] = (uint8_t)start;
    const unsigned count_at = k++;
    unsigned count = 0, i = start;
    for (; i < WORLDS && k < sizeof out - 200; i++, count++) {
        const world_spec *w = &worlds[i];
        out[k++] = (uint8_t)w->family;
        out[k++] = (uint8_t)(w->biome & 255); out[k++] = (uint8_t)(w->biome >> 8);
        out[k++] = (uint8_t)(w->tempo > 0 ? w->tempo : 0);
        k = put_text(out, k, w->name, 40);
        k = put_text(out, k, w->story, 140);
    }
    out[count_at] = (uint8_t)count;
    if (vibe_editor_post(out, (int)k) == 0) worlds_next = i < WORLDS ? (int)i : -2;
    return 1;
}

static void editor_status(void) {
    if (editor_library()) return;
    if (editor_worlds()) return;
    if (++editor_frame % 6u) return;
    static uint8_t out[512];
    unsigned k = 0;
    out[k++] = 100;
    out[k++] = (uint8_t)md_playing();
    out[k++] = (uint8_t)radio.on;
    out[k++] = (uint8_t)radio.in_dream;
    out[k++] = (uint8_t)radio.character;
    const unsigned left = radio.piece_frames < radio.piece_length ? (radio.piece_length - radio.piece_frames) / 44100u : 0u;
    out[k++] = (uint8_t)(left & 255u); out[k++] = (uint8_t)(left >> 8);
    out[k++] = (uint8_t)gesture.mode;
    out[k++] = (uint8_t)gesture_armed;
    out[k++] = (uint8_t)rec_state;
    out[k++] = (uint8_t)rec_seconds();
    out[k++] = (uint8_t)(moment_active + 1);
    out[k++] = (uint8_t)selected;
    out[k++] = (uint8_t)(latched & 255u); out[k++] = (uint8_t)(latched >> 8);
    uint8_t used = 0;
    for (unsigned m = 0; m < MOMENTS; m++) used |= (uint8_t)(moments[m].used << m);
    out[k++] = used;
    for (unsigned i = 0; i < PAD_COUNT; i++) { const float l = pad_light(i); out[k++] = (uint8_t)(l <= 0.f ? 0 : l >= 1.f ? 255 : l * 255.f); }
    const int biome = md_state()->biome_id;
    k = put_text(out, k, radio.in_dream ? md_itinera_name((unsigned)radio.world) : md_biome_name(biome), 40);
    k = put_text(out, k, md_biome_family(biome), 24);
    char line[48] = "";
    if (radio.on) radio_line(line, sizeof line);
    k = put_text(out, k, line, 47);
    k = put_text(out, k, views[page].name, 16);
    for (unsigned i = 0; i < 6; i++) k = put_text(out, k, md_biome_name(favorites[i]), 24);
    out[k++] = (uint8_t)views[page].group;                /* 0.16: the page's colour group, MENU / HOME open */
    out[k++] = (uint8_t)((show_menu ? 1 : 0) | (show_controls ? 2 : 0) | (sticky_shift ? 4 : 0));
    out[k++] = (uint8_t)(biome & 255); out[k++] = (uint8_t)(biome >> 8);   /* 0.17: the Biome that plays */
    out[k++] = (uint8_t)(world_here >= 0 && worlds[world_here].biome == biome ? world_here + 1 : 0);   /* 0.19: the world (+1) */
    out[k++] = (uint8_t)world_wander;                                       /* 0.21: minutes until the next world (0 off) */
    /* 0.24: the garden listens – input on, SING mode, input level, the note heard (+1), the garden's answer (+1) */
    out[k++] = (uint8_t)lis.on; out[k++] = (uint8_t)lis.sing;
    { const float l = lis.on ? lis_level * 1.6f : 0.f; out[k++] = (uint8_t)(l >= 1.f ? 255 : l * 255.f); }
    out[k++] = (uint8_t)(lis.on && lis_note >= 0 ? lis_note + 1 : 0);
    out[k++] = (uint8_t)(lis_answer_glow > 0.f && lis_answer_note >= 0 ? lis_answer_note + 1 : 0);
    out[k++] = (uint8_t)(tour_step + 1);                                  /* 0.26: FIRST STEPS step + 1 (0 = no tour) */
    vibe_editor_post(out, (int)k);       /* -1 = the page has not taken the last one: the next try comes in a moment */
}
