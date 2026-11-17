/* All 50 pages (0.24: 50) from the stage and the Dream Console (0.16). Included by garden-stage.inc.
 *
 * The stage replaces the device in the Web Player, so the whole firmware has to be reachable from it: the chip
 * ALL PAGES (top right) turns the stage into the device's display, mirrored 1.5 x, with its controls beside it.
 * Every element there is one base control and says which: the colour keys K1-K6 (+ MENU > DREAM), MENU, HOME,
 * RETURN, the arrows, ENTER, encoders 1-4, A, B and DATA (drag or wheel), PLAY, STOP. A click on F1-F6 at the
 * bottom of the mirrored display presses that soft key; a click on a page in ALL PAGES opens it.
 * The stage switches to this view by itself whenever the device leaves LIVE or opens MENU / HOME, so a page
 * opened on the hardware is also visible here. GARDEN goes back to LIVE and the landscape. */
enum { MIRROR_X = 16, MIRROR_Y = 56, MIRROR_W = 720, MIRROR_H = 480 };   /* 480 x 320 at 1.5 layout units = 2 x pixels */
enum { SIDE_X = 752, SIDE_W = 192 };
enum { CHIP_PAGES_X = 632, CHIP_PAGES_Y = 10, CHIP_PAGES_W = 192, CHIP_PAGES_H = 34, CHIP_SEG = 64 };   /* GARDEN | PLACES | PAGES */
static int stage_view;                         /* (0.23) 0 the garden, 1 the four places of the sound */
static int stage_pages;                        /* the user chose ALL PAGES on the stage */
static int8_t stage_pointer_knob[10];          /* a pointer dragging an encoder (VE_*), else -1 */
static int stage_knob_y[10];

static int pages_mode(void) { return stage_pages || show_menu || show_controls || page != V_LIVE; }

/* SHIFT held around one control, as on the device: SHIFT + F (favourite, moment), SHIFT + RETURN (undo),
 * SHIFT + encoders (PADS layer). A sticky SHIFT lasts for one control. */
static void shift_wrap(int on) {
    vibe_event s = {.type = VIBE_EV_BUTTON, .id = VB_SHIFT, .down = (uint8_t)(on != 0)};
    vibe_on_event(&s);
}
static void stage_button(int id) {
    const int shift = sticky_shift && id != VB_SHIFT;
    sticky_shift = 0;
    if (shift) shift_wrap(1);
    vibe_event e = {.type = VIBE_EV_BUTTON, .id = (uint8_t)id, .down = 1};
    vibe_on_event(&e);
    e.down = 0;
    vibe_on_event(&e);
    if (shift) shift_wrap(0);
}

static void stage_encoder(int id, float steps) {
    const int shift = sticky_shift;                    /* (stays for the whole drag; cleared on release) */
    if (shift) shift_wrap(1);
    vibe_event e = {.type = VIBE_EV_ENCODER, .id = (uint8_t)id, .value = steps};
    vibe_on_event(&e);
    if (shift) shift_wrap(0);
}

/* a colour group: K1-K6, DREAM has no key of its own (MENU > DREAM) */
static void open_group(int g) {
    if (g < 6) { stage_button(VB_K1 + g); return; }
    sticky_shift = 0;
    show_menu = 0; show_controls = 0;
    group_key(g);
}

/* A click on the device's display (480 x 320 coordinates), from the stage mirror or the Dream Console. */
static void display_click(int x, int y) {
    if (x < 0 || y < 0 || x >= 480 || y >= 320) { sticky_shift = 0; return; }
    if (show_menu) {
        if (y >= MENU_Y && y < MENU_Y + MENU_ROWS * MENU_ROW_H && x >= 12) {      /* (0.27) two banks of 36 */
            const int col = (x - 12) / 156, row = (y - MENU_Y) / MENU_ROW_H;
            const int v = menu_bank_of(menu_cursor) * MENU_BANK + col * MENU_ROWS + row;
            if (col < 3 && row < MENU_ROWS && v < V_COUNT) { menu_cursor = v; set_view(v); }
        } else if (y >= 296) {                                                  /* the footer turns the bank */
            menu_cursor = menu_bank_of(menu_cursor) ? 0 : MENU_BANK;
        }
        sticky_shift = 0;
        return;
    }
    /* the soft keys are drawn at 15 + 75 i, 71 wide: a click in a gap presses nothing (0.18) */
    if (y >= 282 && !show_controls && x >= 15 && (x - 15) % 75 < 71 && (x - 15) / 75 < 6) stage_button(VB_F1 + (x - 15) / 75);
}

/* ---- drawing ------------------------------------------------------------------------------------------------- */
enum { TOUR_BTN_X = CHIP_PAGES_X - 44, TOUR_BTN_W = 36 };   /* (0.26) the ? beside the views: FIRST STEPS */
static void pages_chip(void) {                  /* (0.23) three views: GARDEN, PLACES, PAGES */
    stui_key(TOUR_BTN_X, CHIP_PAGES_Y, TOUR_BTN_W, CHIP_PAGES_H, 10, 0xf0c38c, tour_step >= 0 ? UI_ACTIVE : UI_NORMAL, "", VG_BOLD);
    vg_text("?", TOUR_BTN_X + TOUR_BTN_W / 2, CHIP_PAGES_Y + 5, VG_BIG, tour_step >= 0 ? 0x2a200a : 0xf0c38c, VG_CENTER);
    static const char *const names[3] = {"GARDEN", "PLACES", "PAGES"};
    const int on = pages_mode() ? 2 : stage_view;
    /* (0.27) a segmented control of glass: a dark groove, the chosen view a lit key inside it */
    vg_round(CHIP_PAGES_X, CHIP_PAGES_Y, CHIP_PAGES_W, CHIP_PAGES_H, 10, 0x0c1619);
    vg_rect(CHIP_PAGES_X + 10, CHIP_PAGES_Y + CHIP_PAGES_H - 1, CHIP_PAGES_W - 20, 1, 0x1e3234);
    for (int i = 0; i < 3; i++) {
        if (i == on) stui_key(CHIP_PAGES_X + i * CHIP_SEG + 3, CHIP_PAGES_Y + 3, CHIP_SEG - 6, CHIP_PAGES_H - 7, 8, 0x83d2a6, UI_ACTIVE, names[i], VG_BOLD);
        else vg_text(names[i], CHIP_PAGES_X + i * CHIP_SEG + CHIP_SEG / 2, CHIP_PAGES_Y + 9, VG_BOLD, 0x9abcb2, VG_CENTER);
    }
}

/* PLACES (0.23, the Brick's Space view): the places the sound goes to, large. A click opens the place's page
 * on the device (and shows it under PAGES): SPACE, GRAIN, TAPE, TAPE, and since 0.24 the moon pond (LISTEN, the
 * VIBE's inputs) and the singing flower (SING). Three columns of two. */
enum { PL_X = 24, PL_Y = 72, PL_W = 212, PL_H = 230, PL_GAP = 8, PLACES_N = 6, PL_COLS = 3 };
static const int place_page[PLACES_N] = {V_SPACE, V_GRAIN, V_TAPE, V_TAPE, V_LISTEN, V_SING};
static void draw_places(void) {
    const md_patch *p = md_state();
    const float t = (float)(md_status()->frames % 2646000u) / 44100.f;
    static const char *const titles[PLACES_N] = {"THE ECHO CAVE", "THE SPORE MEADOW", "THE STREAM", "REEDS IN THE WIND", "THE MOON POND", "THE SINGING FLOWER"};
    static const char *const sub[PLACES_N] = {"ECHO + ROOM  -  SPACE", "GRAINS  -  GRAIN", "TAPE  -  PITCH, DIRECTION", "DUCKING  -  TAPE", "IN 1 / IN 2  -  LISTEN", "YOUR VOICE  -  SING"};
    static const uint32_t cols[PLACES_N] = {0xFF8cc8e6u, 0xFFd6a078u, 0xFF86c2b4u, 0xFFa5bed9u, 0xFFc9d3f0u, 0xFFf0c38cu};
    static float bow;                                /* the reeds' bow: up on the duck track's hits, down with the release */
    const int src = p->duck_source;
    const float hit = src >= 0 && src < MD_TRACKS ? md_status()->track_peak[src] : 0.f;
    const float target = (hit > .3f ? 1.f : hit / .3f) * p->duck_amount / 1000.f;
    bow = target > bow ? target : bow * (.97f - .02f * (1.f - p->duck_release / 1000.f));
    uint8_t position[4], window[4];
    md_grain_positions(position, window);
    for (int k = 0; k < PLACES_N; k++) {
        const int x = PL_X + (k % PL_COLS) * (PL_W + PL_GAP), y = PL_Y + (k / PL_COLS) * (PL_H + PL_GAP);
        nd_pixels(stage_fb, STAGE_PW, STAGE_PH);
        nd_glass(st_f(x), st_f(y), st_f(PL_W), st_f(PL_H), 14.f, ARGB(200, 8, 14, 24));
        const float sx = st_f(x + 8), sy = st_f(y + 50), sw = st_f(PL_W - 16), sh = st_f(PL_H - 92);
        switch (k) {
        case 0: nd_echo_cave(sx, sy, sw, sh, (float)p->delay_ms, p->delay_fb / 1000.f, p->reverb_time / 1000.f, p->fx_level / 1000.f, p->freeze, t, cols[0]); break;
        case 1: nd_spore_meadow(sx, sy, sw, sh, position, window, p->freeze, p->reverse, t, cols[1]); break;
        case 2: nd_stream(sx, sy, sw, sh, p->grain_pitch, p->reverse, p->freeze, t, cols[2]); break;
        case 3: nd_reeds(sx, sy, sw, sh, bow, t, cols[3]); break;
        case 4: nd_moon_pond(sx, sy, sw, sh, lis.on, lis_level, lis_scope, lis_scope_at, lis.cloud, lis.echo, lis.room, p->freeze, t, cols[4]); break;
        default: nd_singing_flower(sx, sy, sw, sh, lis.on && lis.sing, lis_note, lis_answer_note, lis_answer_glow, lis_level, t, cols[5]);
        }
        /* (0.27) the place's colour on the top edge, a small light beside its name, its values on a quiet strip */
        vg_round(x + 14, y, PL_W - 28, 2, 1, vg_mix(0x0b1418, cols[k] & 0xffffffu, .75f));
        vg_circle((float)(x + 16), (float)(y + 17), 3.5f, cols[k] & 0xffffffu);
        vg_text(titles[k], x + 26, y + 10, VG_BOLD, cols[k] & 0xffffffu, VG_LEFT);
        vg_text(sub[k], x + 26, y + 30, VG_SMALL, 0x9abcb2, VG_LEFT);
        vg_round(x + 8, y + PL_H - 36, PL_W - 16, 24, 7, 0x0c171b);
        char line[64], v[16];
        unsigned n = 0;
#define PUT(s) do { const char *q_ = (s); while (*q_ && n < sizeof line - 1) line[n++] = *q_++; } while (0)
        switch (k) {
        case 0: PUT("ECHO "); PUT(vg_itoa(p->delay_ms, v)); PUT(" MS  FB "); PUT(vg_itoa(p->delay_fb / 10, v)); PUT("  ROOM "); PUT(vg_itoa(p->reverb_time / 10, v)); break;
        case 1: PUT("SIZE "); PUT(vg_itoa(p->grain_size / 10, v)); PUT("  POS "); PUT(vg_itoa(p->grain_pos / 10, v)); PUT("  FB "); PUT(vg_itoa(p->feedback / 10, v)); break;
        case 2: PUT("PITCH "); if (p->grain_pitch > 0) PUT("+"); PUT(vg_itoa(p->grain_pitch, v)); PUT("  "); PUT(p->reverse ? "BACKWARDS" : "FORWARD"); if (p->freeze) PUT("  FROZEN"); break;
        case 3: PUT("FROM "); PUT(src >= 0 && src < MD_TRACKS ? track_labels[src] : "NONE"); PUT("  AMOUNT "); PUT(vg_itoa(p->duck_amount / 10, v)); break;
        case 4: if (!lis.on) PUT("INPUT OFF - CLICK, F1"); else { PUT("CLOUD "); PUT(vg_itoa((int)(lis.cloud * 100.f), v)); PUT("  ECHO "); PUT(vg_itoa((int)(lis.echo * 100.f), v)); PUT("  ROOM "); PUT(vg_itoa((int)(lis.room * 100.f), v)); } break;
        default: PUT(sing_modes[clampi(lis.sing, 0, SING_MODES - 1)]); PUT("  ON "); PUT(track_labels[clampi(lis.sing_track, 0, 5)]);
        }
#undef PUT
        line[n] = 0;
        vg_text(line, x + 16, y + PL_H - 31, VG_SMALL, 0xd7ebe3, VG_LEFT);
    }
}
static int places_event(int x, int y) {
    for (int k = 0; k < PLACES_N; k++) {
        const int px = PL_X + (k % PL_COLS) * (PL_W + PL_GAP), py = PL_Y + (k / PL_COLS) * (PL_H + PL_GAP);
        if (x >= px && x < px + PL_W && y >= py && y < py + PL_H) { stage_pages = 1; show_menu = 0; show_controls = 0; set_view(place_page[k]); return 1; }
    }
    return 0;
}

static void side_button(int x, int y, int w, int h, const char *label, uint32_t ink, int on) {   /* (0.27) a key of glass */
    stui_key(x, y, w, h, 8, ink, on ? UI_ACTIVE : UI_NORMAL, label, VG_BOLD);
}

/* side panel geometry, shared by drawing and clicks */
enum { GROUP_Y = 78, GROUP_W = 92, GROUP_H = 30, GROUP_GAP = 8, ROW2_Y = 228, NAV_W = 42, ENTER_Y = 266,   /* SHIFT | ENTER */
       KNOB_Y = 340, KNOB2_Y = 420, PLAY_Y = 470, PLAY_H = 28 };

static void knob_draw(float cx, float cy, const char *label, float fill, uint32_t ink) {
    ring_line(st_f(cx), st_f(cy), st_f(16.f), st_f(3.f), 0x1d3532, .375f, 1.125f);    /* (garden-draw: real pixels) */
    if (fill >= 0.f) ring_line(st_f(cx), st_f(cy), st_f(16.f), st_f(3.f), ink, .375f, .375f + .75f * fill);
    vg_text(label, (int)cx, (int)cy + 20, VG_SMALL, 0x9abcb2, VG_CENTER);
}

static void draw_pages(void) {
    /* the display, exactly 2 x in the stage's 1280 x 720 pixels (1.5 x in layout units). (0.29) Not as bare 2 x 2
     * blocks: Scale2x (EPX) – where a diagonal edge runs through a pixel, its quarter takes the neighbour's colour, so
     * curves and letters step half as coarsely; flat areas and straight edges stay exactly as sharp. */
    if (device_fb) {
        const int px = st_s(MIRROR_X), py = st_s(MIRROR_Y);
        for (int y = 0; y < 320; y++) {
            const uint16_t *up = device_fb + (y > 0 ? y - 1 : y) * 480, *mid = device_fb + y * 480, *dn = device_fb + (y < 319 ? y + 1 : y) * 480;
            uint16_t *r0 = stage_fb + (py + 2 * y) * STAGE_PW + px, *r1 = r0 + STAGE_PW;
            for (int x = 0; x < 480; x++) {
                const uint16_t E = mid[x], B = up[x], H = dn[x], D = mid[x > 0 ? x - 1 : x], F = mid[x < 479 ? x + 1 : x];
                uint16_t e0 = E, e1 = E, e2 = E, e3 = E;
                if (B != H && D != F) {
                    if (D == B) e0 = D;
                    if (B == F) e1 = F;
                    if (D == H) e2 = D;
                    if (H == F) e3 = F;
                }
                r0[2 * x] = e0; r0[2 * x + 1] = e1; r1[2 * x] = e2; r1[2 * x + 1] = e3;
            }
        }
    }
    const uint32_t ink = group_ink[views[page].group];
    vg_text("F1-F6: CLICK THE DISPLAY", SIDE_X, MIRROR_Y, VG_SMALL, 0x7fa59b, VG_LEFT);
    /* colour keys K1-K6 and DREAM, then MENU */
    for (int g = 0; g <= G_COUNT; g++) {
        const int x = SIDE_X + (g % 2) * (GROUP_W + GROUP_GAP), y = GROUP_Y + (g / 2) * (GROUP_H + 6);
        if (g == G_COUNT) { side_button(x, y, GROUP_W, GROUP_H, "MENU", 0xe0f5ec, show_menu); continue; }
        side_button(x, y, GROUP_W, GROUP_H, group_names[g], group_ink[g], !show_menu && views[page].group == g);
    }
    static const char *const nav[4] = {"HOME", "RET", "<", ">"};
    for (int i = 0; i < 4; i++)
        side_button(SIDE_X + i * (NAV_W + 8), ROW2_Y, NAV_W, GROUP_H, nav[i], 0xd7ebe3, i == 0 && show_controls);
    side_button(SIDE_X, ENTER_Y, GROUP_W, GROUP_H, "SHIFT", 0xf0cb86, sticky_shift);
    side_button(SIDE_X + GROUP_W + GROUP_GAP, ENTER_Y, GROUP_W, GROUP_H, "ENTER", 0xd7ebe3, 0);
    vg_text("ENCODERS: DRAG UP OR DOWN", SIDE_X, KNOB_Y - 34, VG_SMALL, 0x7fa59b, VG_LEFT);
    for (unsigned k = 0; k < 4; k++) {
        Knob kb;
        char label[8] = {'1' + (char)k, 0};
        float fill = -1.f;
        if (!show_menu && knob_of(page, k, &kb)) {
            unsigned i = 0;
            while (kb.label[i] && i < 7) { label[i] = kb.label[i]; i++; }
            label[i] = 0;
            if (kb.cmd != L_PADSET) fill = knob_fill(&kb);
        }
        knob_draw(SIDE_X + 24.f + k * 48.f, (float)KNOB_Y, label, fill, ink);
    }
    knob_draw(SIDE_X + 32.f, (float)KNOB2_Y, "A DEPTH", settings.texture_depth, colors[ROW_TEXTURE]);
    knob_draw(SIDE_X + 96.f, (float)KNOB2_Y, "B DEPTH", settings.space_depth, colors[ROW_SPACE]);
    knob_draw(SIDE_X + 160.f, (float)KNOB2_Y, "DATA", -1.f, 0xd7ebe3);
    const int playing = md_playing();
    side_button(SIDE_X, PLAY_Y, GROUP_W, PLAY_H, "PLAY", 0x83d2a6, playing);
    side_button(SIDE_X + GROUP_W + GROUP_GAP, PLAY_Y, GROUP_W, PLAY_H, "STOP", 0xd7ebe3, 0);
    vg_text(sticky_shift ? "SHIFT: for the next control" : show_menu ? "Click a page to open it" :
            show_controls ? "HOME again closes the help" : "Right-click = SHIFT + click", SIDE_X, PLAY_Y + PLAY_H + 10, VG_SMALL,
            sticky_shift ? 0xf0cb86 : 0x9abcb2, VG_LEFT);
}

/* ---- input --------------------------------------------------------------------------------------------------- */
static int in_box(int x, int y, int bx, int by, int bw, int bh) { return x >= bx && x < bx + bw && y >= by && y < by + bh; }

static int knob_at(int x, int y) {
    for (int k = 0; k < 4; k++) if (in_box(x, y, SIDE_X + k * 48, KNOB_Y - 22, 48, 56)) return VE_1 + k;
    for (int k = 0; k < 3; k++) if (in_box(x, y, SIDE_X + k * 64, KNOB2_Y - 22, 64, 56)) return k == 0 ? VE_A : k == 1 ? VE_B : VE_DATA;
    return -1;
}

/* 1 = the event was the page view's */
static int pages_event(const vibe_event *event, unsigned pointer) {
    const int x = VIBE_STAGE_X(event), y = VIBE_STAGE_Y(event);
    if (event->id == VIBE_STAGE_DOWN && in_box(x, y, TOUR_BTN_X, CHIP_PAGES_Y, TOUR_BTN_W, CHIP_PAGES_H)) {   /* (0.26) ? */
        if (tour_step >= 0) tour_end(); else { stage_pages = 0; show_menu = 0; show_controls = 0; stage_view = 0; tour_start_from_page(); }
        return 1;
    }
    if (event->id == VIBE_STAGE_DOWN && in_box(x, y, CHIP_PAGES_X, CHIP_PAGES_Y, CHIP_PAGES_W, CHIP_PAGES_H)) {
        const int seg = (x - CHIP_PAGES_X) / CHIP_SEG;
        if (seg >= 2) { if (!pages_mode()) { stage_pages = 1; show_controls = 0; show_menu = 1; menu_cursor = page; } }
        else { stage_pages = 0; show_menu = 0; show_controls = 0; if (page != V_LIVE) set_view(V_LIVE); stage_view = seg; }
        return 1;
    }
    if (!pages_mode() && stage_view == 1 && event->id == VIBE_STAGE_DOWN && x < PL_X + PL_COLS * PL_W + (PL_COLS - 1) * PL_GAP) return places_event(x, y) || 1;
    if (stage_pointer_knob[pointer] >= 0 && event->id == VIBE_STAGE_UP) { stage_pointer_knob[pointer] = -1; sticky_shift = 0; return 1; }
    if (!pages_mode()) { stage_pointer_knob[pointer] = -1; return 0; }
    if (event->id == VIBE_STAGE_WHEEL) { stage_encoder(VE_DATA, (float)event->value); return 1; }
    if (stage_pointer_knob[pointer] >= 0) {
        if (event->id == VIBE_STAGE_MOVE) {
            const int steps = (stage_knob_y[pointer] - y) / 8;
            if (steps) { stage_knob_y[pointer] -= steps * 8; stage_encoder(stage_pointer_knob[pointer], (float)steps); }
        }
        return 1;
    }
    if (event->id != VIBE_STAGE_DOWN) return 1;
    if (event->midi[1] & (VIBE_STAGE_MOD_SHIFT | VIBE_STAGE_MOD_RIGHT)) sticky_shift = 1;   /* shift- / right-click = SHIFT + */
    if (in_box(x, y, MIRROR_X, MIRROR_Y, MIRROR_W, MIRROR_H)) { display_click((x - MIRROR_X) * 2 / 3, (y - MIRROR_Y) * 2 / 3); return 1; }
    for (int g = 0; g <= G_COUNT; g++) {
        if (!in_box(x, y, SIDE_X + (g % 2) * (GROUP_W + GROUP_GAP), GROUP_Y + (g / 2) * (GROUP_H + 6), GROUP_W, GROUP_H)) continue;
        if (g == G_COUNT) stage_button(VB_MENU); else open_group(g);
        return 1;
    }
    static const int nav[4] = {VB_HOME, VB_RETURN, VB_LEFT, VB_RIGHT};
    for (int i = 0; i < 4; i++) if (in_box(x, y, SIDE_X + i * (NAV_W + 8), ROW2_Y, NAV_W, GROUP_H)) {
        if (nav[i] == VB_RETURN && !show_controls && !show_menu && page == V_LIVE) stage_pages = 0;
        stage_button(nav[i]);
        return 1;
    }
    if (in_box(x, y, SIDE_X, ENTER_Y, GROUP_W, GROUP_H)) { sticky_shift = !sticky_shift; return 1; }
    if (in_box(x, y, SIDE_X + GROUP_W + GROUP_GAP, ENTER_Y, GROUP_W, GROUP_H)) { stage_button(VB_ENTER); return 1; }
    const int knob = knob_at(x, y);
    if (knob >= 0) { stage_pointer_knob[pointer] = (int8_t)knob; stage_knob_y[pointer] = y; return 1; }
    if (in_box(x, y, SIDE_X, PLAY_Y, GROUP_W, PLAY_H)) { stage_button(VB_PLAY); return 1; }
    if (in_box(x, y, SIDE_X + GROUP_W + GROUP_GAP, PLAY_Y, GROUP_W, PLAY_H)) { stage_button(VB_STOP); return 1; }
    return 1;
}
