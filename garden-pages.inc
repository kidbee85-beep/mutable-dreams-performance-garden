/* All 47 pages from the stage and the Dream Console (0.16). Included by garden-stage.inc.
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
enum { CHIP_PAGES_X = 700, CHIP_PAGES_Y = 10, CHIP_PAGES_W = 124, CHIP_PAGES_H = 34 };
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
        if (y >= 39 && y < 39 + 16 * 15 && x >= 12) {
            const int col = (x - 12) / 156, row = (y - 39) / 15;
            const int v = col * 16 + row;
            if (col < 3 && row < 16 && v < V_COUNT) { menu_cursor = v; set_view(v); }
        }
        sticky_shift = 0;
        return;
    }
    /* the soft keys are drawn at 15 + 75 i, 71 wide: a click in a gap presses nothing (0.18) */
    if (y >= 282 && !show_controls && x >= 15 && (x - 15) % 75 < 71 && (x - 15) / 75 < 6) stage_button(VB_F1 + (x - 15) / 75);
}

/* ---- drawing ------------------------------------------------------------------------------------------------- */
static void pages_chip(void) {
    const int on = pages_mode();
    vg_round(CHIP_PAGES_X, CHIP_PAGES_Y, CHIP_PAGES_W, CHIP_PAGES_H, 8, on ? 0x3b6c60 : 0x192b2d);
    vg_text(on ? "< GARDEN" : "ALL PAGES", CHIP_PAGES_X + CHIP_PAGES_W / 2, CHIP_PAGES_Y + 9, VG_BOLD, 0xf2fbf7, VG_CENTER);
}

static void side_button(int x, int y, int w, int h, const char *label, uint32_t ink, int on) {
    vg_round(x, y, w, h, 7, on ? vg_mix(0x192b2d, ink, .55f) : 0x192b2d);
    vg_text(label, x + w / 2, y + h / 2 - 8, VG_BOLD, on ? 0x0a1719 : ink, VG_CENTER);
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
    /* the display, exactly 2 x in the stage's 1280 x 720 pixels (1.5 x in layout units): every device pixel a
     * sharp 2 x 2 block */
    if (device_fb) {
        const int px = st_s(MIRROR_X), py = st_s(MIRROR_Y);
        for (int y = 0; y < 640; y++) {
            const uint16_t *src = device_fb + (y >> 1) * 480;
            uint16_t *row = stage_fb + (py + y) * STAGE_PW + px;
            for (int x = 0; x < 960; x++) row[x] = src[x >> 1];
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
    if (event->id == VIBE_STAGE_DOWN && in_box(x, y, CHIP_PAGES_X, CHIP_PAGES_Y, CHIP_PAGES_W, CHIP_PAGES_H)) {
        if (pages_mode()) { stage_pages = 0; show_menu = 0; show_controls = 0; set_view(V_LIVE); }
        else { stage_pages = 1; show_controls = 0; show_menu = 1; menu_cursor = page; }
        return 1;
    }
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
