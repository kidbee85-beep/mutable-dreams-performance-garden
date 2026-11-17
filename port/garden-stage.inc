/* SDK 23 stage for the Performance Garden: 960 x 540, shown by the host on request.
 * Included at the end of adapter-check.c and shares its state. The display and the
 * controls still do everything; every stage element names its control on the device.
 *
 *   left   the dream landscape, reacting to TEXTURE and SPACE (live or gesture)
 *   below  four lanes TONE / TEXTURE / SPACE / FORM, the last eight seconds; the
 *          gesture's share is drawn lighter, so a loop shows as a returning shape
 *   right  the 16 pads (click = pad, higher in the pad = harder), Biomes F1-F6, PLAY / STOP
 */
#define STAGE_W 960                               /* the layout, in the 960 x 540 units it was designed in */
#define STAGE_H 540
#define STAGE_PW 1280                             /* 0.18: drawn at 1280 x 720, the SDK's largest stage – a third */
#define STAGE_PH 720                              /* sharper; the mirrored display is exactly 2 x its pixels */
static uint16_t stage_fb[STAGE_PW * STAGE_PH];

/* The scale layer: the stage code below keeps its 960 x 540 coordinates, these draw them at 4 / 3. Text takes the
 * next larger face (14 px -> 15 px SemiBold), the big title the 60 px face; the widths come back in layout units. */
static int st_s(float v) { return (int)(v * 4.f / 3.f + (v < 0.f ? -.5f : .5f)); }
static float st_f(float v) { return v * 4.f / 3.f; }
static int st_font(int f) {
    if (f == (VG_BIG | VG_X2)) return VG_BIG | VG_X2;
    if (f == VG_SMALL || f == VG_BOLD) return VG_MED;
    if (f == VG_MED) return VG_BIG;
    return f;
}
static void st_rect(int x, int y, int w, int h, uint32_t c) { vg_rect(st_s(x), st_s(y), st_s(x + w) - st_s(x), st_s(y + h) - st_s(y), c); }
static void st_round(int x, int y, int w, int h, int r, uint32_t c) { vg_round(st_s(x), st_s(y), st_s(x + w) - st_s(x), st_s(y + h) - st_s(y), st_s(r), c); }
static void st_circle(float x, float y, float r, uint32_t c) { vg_circle(st_f(x), st_f(y), st_f(r), c); }
static void st_line(float x0, float y0, float x1, float y1, float w, uint32_t c) { vg_line(st_f(x0), st_f(y0), st_f(x1), st_f(y1), st_f(w), c); }
static void st_tri(float x0, float y0, float x1, float y1, float x2, float y2, uint32_t c) { vg_tri(st_f(x0), st_f(y0), st_f(x1), st_f(y1), st_f(x2), st_f(y2), c); }
static int st_text(const char *s, int x, int y, int f, uint32_t c, int a) { return vg_text(s, st_s(x), st_s(y), st_font(f), c, a) * 3 / 4; }
static int st_text_width(const char *s, int f) { return vg_text_width(s, st_font(f)) * 3 / 4; }
#define vg_rect st_rect
#define vg_round st_round
#define vg_circle st_circle
#define vg_line st_line
#define vg_tri st_tri
#define vg_text st_text
#define vg_text_width st_text_width
/* (0.27) the garden's controls in layout units (garden-ui.inc, compiled again for the stage) */
static void st_frame(int x, int y, int w, int h, int t, uint32_t c) { vg_frame(st_s(x), st_s(y), st_s(w), st_s(h), st_s(t) > 0 ? st_s(t) : 1, c); }
static int st_font_height(int f) { return vg_font_height(st_font(f)) * 3 / 4; }
#define vg_frame st_frame
#define vg_font_height st_font_height
#define UI_STAGE
#include "garden-ui.inc"
#undef UI_STAGE

enum { HISTORY = 240, HISTORY_TICK = 1470 };   /* 240 x 1/30 s = 8 s */
static uint8_t history_live[4][HISTORY], history_gesture[4][HISTORY];
static unsigned history_head, history_clock;
static int8_t stage_pointer_pad[10];
static int8_t stage_pointer_macro[10];            /* a pointer dragging macro slider 0..3, else -1 */
enum { MACRO_X = 24 + 652 - 4 * 26 - 10, MACRO_W = 18, MACRO_GAP = 8, MACRO_TOP = 162, MACRO_H = 150 };

#include "garden-pages.inc"

static uint8_t byte_level(float v) { return (uint8_t)(v <= 0.f ? 0 : v >= 1.f ? 255 : v * 255.f + .5f); }

static void stage_init(void) {
    /* The stage is the instrument's face where a host can (the Web Player opens with it); the device
     * stays one key away and everything still works on the display and the controls. */
    vibe_stage(stage_fb, STAGE_PW, STAGE_PH, 0x0a1719, VIBE_STAGE_REPLACES);
    for (unsigned i = 0; i < 4; i++)
        for (unsigned j = 0; j < HISTORY; j++) history_live[i][j] = history_gesture[i][j] = 0;
    history_head = history_clock = 0;
    for (unsigned i = 0; i < 10; i++) stage_pointer_pad[i] = stage_pointer_macro[i] = -1;
    for (unsigned i = 0; i < 10; i++) stage_pointer_knob[i] = -1;
}

/* Audio side: one history column every HISTORY_TICK engine samples. */
static void stage_record(unsigned frames) {
    history_clock += frames;
    if (history_clock < HISTORY_TICK) return;
    history_clock -= HISTORY_TICK;
    if (history_clock >= HISTORY_TICK) history_clock = 0;
    float live[4] = {0}, ghost[4] = {0};
    for (unsigned i = 0; i < PAD_COUNT; i++) {
        const unsigned row = i / 4;
        if (pads.pads[i].level > live[row]) live[row] = pads.pads[i].level;
        if (i >= 4 && i < 12 && gesture.output[i - 4] > ghost[row]) ghost[row] = gesture.output[i - 4];
    }
    if (thin_mask && live[ROW_FORM] < .6f) live[ROW_FORM] = .6f;
    history_head = (history_head + 1) % HISTORY;
    for (unsigned r = 0; r < 4; r++) {
        history_live[r][history_head] = byte_level(live[r]);
        history_gesture[r][history_head] = byte_level(ghost[r]);
    }
}

/* ---- layout ------------------------------------------------------------------------------------------- */
enum { LAND_X = 24, LAND_Y = 72, LAND_W = 652, LAND_H = 300 };
enum { LANE_X = 24, LANE_Y = 388, LANE_H = 32, LANE_GAP = 6, LANE_PLOT = 150 };
enum { GRID_X = 700, GRID_Y = 72, PAD = 54, GAP = 6 };
enum { CHIP_Y = 330, CHIP_W = 74, CHIP_H = 40 };
enum { TRANSPORT_Y = 438, TRANSPORT_W = 74, TRANSPORT_H = 40 };

static void ring(float cx, float cy, float rx, float ry, float width, uint32_t color) {
    float px = cx + rx, py = cy;
    for (int i = 1; i <= 24; i++) {
        const float t = i / 24.f;
        const float x = cx + vd_sin(t + .25f) * rx, y = cy + vd_sin(t) * ry;
        vg_line(px, py, x, y, width, color);
        px = x; py = y;
    }
}

static void stage_levels(float texture[4], float space[4]) {
    for (unsigned i = 0; i < 4; i++) {
        const float a = pads.pads[4 + i].level, b = pads.pads[8 + i].level;
        texture[i] = a > gesture.output[i] ? a : gesture.output[i];
        space[i] = b > gesture.output[4 + i] ? b : gesture.output[4 + i];
    }
}

static void stage_landscape(void) {
    static const uint32_t ink[] = {0x80bd9a,0xdda47a,0xadc9bc,0xcca8ad,0xa8c28b,0x8eafd6};
    const float time = (md_status()->frames % 1764000u) / 44100.f;
    const md_patch *p = md_state();
    float tex[4], space[4];
    stage_levels(tex, space);
    const float forest = tex[0], air = tex[1], water = tex[2], dust = tex[3];
    const float bloom = space[0], echo = space[1], grain = space[2], dusk = space[3];
    const uint32_t hue = ink[selected];
    const uint32_t base = vg_mix(vg_mix(0x0b201d, hue, .12f), 0x04060c, .55f * dusk);
    (void)forest; (void)air; (void)water; (void)dust; (void)bloom; (void)echo; (void)grain; (void)p; (void)time;
    /* 0.19: the night garden of the Brick and the MPC (garden-night.inc), drawn in the stage's own pixels.
     * The lanes as heard (pads, gesture, automation, moments, wander) times the live depths. */
    {
        static uint16_t sky[869 * 400];
        static nd_scene scene = {.sky = sky, .w = 869, .h = 400, .biome = -1, .left = .07f, .step = .132f, .grow = 1.25f, .every = .6f};   /* (right of the plants: the macro sliders) */
        float lanes[8];
        for (unsigned i = 0; i < 8; i++) lanes[i] = sounding[i] * (i < 4 ? settings.texture_depth : settings.space_depth);
        const int px = st_s(LAND_X), py = st_s(LAND_Y);
        nd_draw(&scene, stage_fb, STAGE_PW, STAGE_PH, px, py, 869, 400, lanes, (int)track, 1);
        if (lis.on) { nd_pixels(stage_fb, STAGE_PW, STAGE_PH); nd_input_rain((float)px, (float)py, 869.f, 400.f, lis_level, lis_note, (float)(md_status()->frames % 2646000u) / 44100.f); }
        nd_sigil_px(stage_fb, STAGE_PW, STAGE_PH, (float)(px + 34), (float)(py + 40), 20.f, md_state()->biome_id,
                    (float)(md_status()->frames % 2646000u) / 44100.f);
    }
    char biome_title[24];
    {   /* the Biome's whole name, as far as it fits beside the macros */
        const char *name = md_biome_name(md_state()->biome_id);
        unsigned i = 0;
        while (name[i] && i < 20) { biome_title[i] = name[i]; i++; }
        biome_title[i] = 0;
    }
    {   /* 0.20: the name in the sharp 60 px face (capitals, digits and a few signs only); else the doubled face */
        char upper[24];
        int ok = 1;
        unsigned i = 0;
        for (; biome_title[i]; i++) {
            char c = biome_title[i];
            if (c >= 'a' && c <= 'z') c = (char)(c - 32);
            if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '.' || c == ',')) ok = 0;
            upper[i] = c;
        }
        upper[i] = 0;
        if (ok && vg_text_width(upper, VG_HUGE) < LAND_W - 80 - 120)
            vg_text(upper, LAND_X + 56, LAND_Y + 6, VG_HUGE, 0xfffcf4, VG_LEFT);
        else
            vg_text(biome_title, LAND_X + 56, LAND_Y + 12, VG_BIG | VG_X2, 0xfffcf4, VG_LEFT);
    }
    /* An Itinera dream in progress: its name, chapter and the long arc as a thin line. */
    md_itinera_view dream;
    md_itinera_read(&dream);
    if (dream.active) {
        const int bx = LAND_X + LAND_W - 238, by = LAND_Y + 14;
        vg_round(bx, by, 224, 58, 8, vg_mix(base, 0x000000, .35f));
        vg_text(md_itinera_name(dream.preset), bx + 12, by + 6, VG_BOLD, 0xf2fbf7, VG_LEFT);
        char chapter[16] = "CHAPTER ";
        chapter[8] = (char)('1' + (dream.chapter & 3u)); chapter[9] = 0;
        vg_text(chapter, bx + 12, by + 24, VG_SMALL, vg_mix(hue, 0xf2fbf7, .4f), VG_LEFT);
        vg_rect(bx + 12, by + 44, 200, 3, vg_mix(base, 0xf2fbf7, .15f));
        vg_rect(bx + 12, by + 44, (int)(200.f * (dream.progress > 1000 ? 1.f : dream.progress / 1000.f)), 3, vg_mix(hue, 0xf2fbf7, .5f));
    }
    /* The four macros as sliders on the landscape: drag them, or turn encoders 1-4 on LIVE. */
    {
        static const char *const names[4] = {"D", "M", "C", "S"};
        const md_patch *mp = md_state();
        for (int i = 0; i < 4; i++) {
            const int x = MACRO_X + i * (MACRO_W + MACRO_GAP);
            const int h = mp->macro[i] * MACRO_H / 1000;
            nd_pixels(stage_fb, STAGE_PW, STAGE_PH);        /* (0.20) a glass tube, the weather's sign under it */
            nd_glass(st_f(x), st_f(MACRO_TOP), st_f(MACRO_W), st_f(MACRO_H), 10.f, ARGB(150, 6, 10, 20));
            nd_weather_sign(i, st_f(x + MACRO_W / 2.f), st_f(MACRO_TOP + MACRO_H + 36.f), st_f(9.f), 0xFFd7ebe3u, mp->macro[i] / 1000.f,
                            (float)(md_status()->frames % 2646000u) / 44100.f);
            {   /* (0.32) the Brick's plant sliders: a stem grows to the value, leaves along it, a bud that opens at the top */
                const uint32_t green = vg_mix(base, vg_mix(hue, 0xf2fbf7, .3f), .8f);
                const float cx = x + MACRO_W / 2.f, y0 = (float)(MACRO_TOP + MACRO_H - 3), top = y0 - (float)(h > 6 ? h - 6 : 0);
                const float t = (float)(md_status()->frames % 2646000u) / 44100.f, sway = 1.2f * vd_sin(t * .12f + (float)i * .3f);
                vg_line(cx, y0, cx + sway, top, 2.4f, green);
                for (float ly = y0 - 14.f; ly > top + 10.f; ly -= 18.f) {
                    const int side = ((int)((y0 - ly) / 18.f)) & 1;
                    const float lx = cx + sway * (y0 - ly) / (y0 - top + 1.f);
                    vg_line(lx, ly, lx + (side ? 6.f : -6.f), ly - 5.f, 2.f, green);
                    vg_circle(lx + (side ? 6.5f : -6.5f), ly - 5.5f, 2.2f, green);
                }
                const float open = mp->macro[i] / 1000.f, bx = cx + sway;
                nd_pixels(stage_fb, STAGE_PW, STAGE_PH);
                nd_disc(st_f(bx), st_f(top), st_f(9.f + 6.f * open), ARGB((uint32_t)(30.f + 50.f * open), (hue >> 16) & 255u, (hue >> 8) & 255u, hue & 255u));
                for (int p = 0; p < 5; p++) {
                    const float a = (float)p * .2f + t * .02f, r = 2.f + 3.5f * open;
                    vg_circle(bx + vd_sin(a + .25f) * r, top + vd_sin(a) * r, 2.f + 1.2f * open, vg_mix(green, 0xf2fbf7, .25f + .4f * open));
                }
                vg_circle(bx, top, 2.f, 0xf2e6b0);
            }
            vg_text(names[i], x + MACRO_W / 2, MACRO_TOP + MACRO_H + 4, VG_BOLD, vg_mix(base, 0xf2fbf7, .7f), VG_CENTER);
            char num[6];                                      /* (0.27, Codex design review) the value, readable, also at 0 */
            vg_itoa(mp->macro[i] / 10, num);
            {   const int nw = vg_text_width(num, VG_SMALL) + 8;   /* (0.31) a dark seat: readable when the moon passes behind */
                vg_round(x + MACRO_W / 2 - nw / 2, MACRO_TOP + 1, nw, vg_font_height(VG_SMALL) + 3, 5, 0x0a1218); }
            vg_text(num, x + MACRO_W / 2, MACRO_TOP + 3, VG_SMALL, 0xe4f1ea, VG_CENTER);
        }
        {   /* the letters' legend */
            const int lx = MACRO_X + 2 * (MACRO_W + MACRO_GAP) - MACRO_GAP / 2;
            vg_text("D density  M motion", lx, MACRO_TOP + MACRO_H + 52, VG_SMALL, vg_mix(base, 0xf2fbf7, .65f), VG_CENTER);
            vg_text("C colour  S space", lx, MACRO_TOP + MACRO_H + 64, VG_SMALL, vg_mix(base, 0xf2fbf7, .65f), VG_CENTER);
        }
        vg_text("MACROS", MACRO_X + 2 * (MACRO_W + MACRO_GAP) - MACRO_GAP / 2, MACRO_TOP - 16, VG_SMALL,
                vg_mix(base, 0xf2fbf7, .55f), VG_CENTER);
    }
    if (radio.on) {
        char air[56] = "MUTABLE DREAMS FM  -  ";
        unsigned k = 22;
        char line[40];
        radio_line(line, sizeof line);
        for (unsigned i = 0; line[i] && k < sizeof air - 1; i++) air[k++] = line[i];
        air[k] = 0;
        vg_text(air, LAND_X + 20, LAND_Y + 60, VG_BOLD, 0xe8705e, VG_LEFT);
    } else
        vg_text("Favourites: F1-F6 on LIVE, or NEXT (pad 15)", LAND_X + 20, LAND_Y + 60, VG_SMALL, vg_mix(base, 0xd7ebe3, .6f), VG_LEFT);
}

static void stage_lanes(void) {
    static const char *hints[] = {"pads 1-4", "pads 5-8", "pads 9-12", "pads 13-16"};
    const float column = (float)(LAND_W - LANE_PLOT) / HISTORY;
    for (unsigned r = 0; r < 4; r++) {
        const int y = LANE_Y + (int)r * (LANE_H + LANE_GAP);
        nd_pixels(stage_fb, STAGE_PW, STAGE_PH);           /* (0.20: glass over the night) */
        nd_glass(st_f(LANE_X), st_f((float)y), st_f(LAND_W), st_f(LANE_H), 8.f, ARGB(200, 9, 16, 27));
        vg_text(lanes[r], LANE_X + 10, y + 3, VG_BOLD, colors[r], VG_LEFT);
        const int looping = gesture.mode == GESTURE_LOOPING && (r == ROW_TEXTURE || r == ROW_SPACE);
        vg_text(looping ? "GESTURE LOOP" : hints[r], LANE_X + 10, y + 17, VG_SMALL, looping ? 0xf2fbf7 : 0xadc7bc, VG_LEFT);
        {   /* (0.27, Codex design review) a lane with nothing played says what its pads do, rather than staying an empty slab */
            int any = 0;
            for (unsigned j = 0; j < HISTORY && !any; j++) any = history_live[r][j] || history_gesture[r][j];
            static const char *const idle[4] = {"tap pads 1-4: notes in the Biome's key", "hold pads 5-8: forest, air, water, stars",
                                                "hold pads 9-12: bloom, echo, grain, dusk", "13 thin  -  14 pulse  -  15 next Biome  -  16 gesture"};
            if (!any && !looping) vg_text(idle[r], LANE_X + LANE_PLOT + 8, y + 9, VG_SMALL, 0x87a398, VG_LEFT);
        }
        for (unsigned j = 0; j < HISTORY; j++) {
            const unsigned at = (history_head + 1 + j) % HISTORY;
            const float x = LANE_X + LANE_PLOT + j * column;
            const int g = history_gesture[r][at] * (LANE_H - 6) / 255;
            const int l = history_live[r][at] * (LANE_H - 6) / 255;
            /* (0.27) a soft area under a bright line: the lane's colour low, its value as a lit edge */
            if (g) { vg_rect((int)x, y + LANE_H - 3 - g, (int)(column + .99f), g, vg_mix(0x0d1a20, 0xf2fbf7, .16f));
                     vg_rect((int)x, y + LANE_H - 3 - g, (int)(column + .99f), 1, vg_mix(0x0d1a20, 0xf2fbf7, .6f)); }
            if (l) { vg_rect((int)x, y + LANE_H - 3 - l, (int)(column + .99f), l, vg_mix(0x0d1a20, colors[r], .38f));
                     vg_rect((int)x, y + LANE_H - 3 - l, (int)(column + .99f), 2, colors[r]); }
        }
    }
}

static int pad_at(int x, int y, float *velocity) {
    if (x < GRID_X || y < GRID_Y) return -1;
    const int col = (x - GRID_X) / (PAD + GAP), row = (y - GRID_Y) / (PAD + GAP);
    if (col > 3 || row > 3 || (x - GRID_X) % (PAD + GAP) >= PAD || (y - GRID_Y) % (PAD + GAP) >= PAD) return -1;
    if (velocity) *velocity = 1.f - .7f * (float)((y - GRID_Y) % (PAD + GAP)) / PAD;
    return row * 4 + col;
}

static void stage_controls(void) {
    const float nod = garden_nod();
    for (unsigned i = 0; i < PAD_COUNT; i++) {
        const int x = GRID_X + (int)(i % 4) * (PAD + GAP), y = GRID_Y + (int)(i / 4) * (PAD + GAP);
        const float level = pad_light(i);
        const uint32_t ink = pad_color(i);
        /* (0.27, Codex design review) one flat surface and a fine edge; sounding: the category's colour fills it as far
           as it sounds and its edge lights; latched: a lasting bar below and an L */
        const int held = level > .05f, latch = (latched >> i) & 1u;
        if (held) {                                          /* (0.32) a sounding pad glows into the night around it */
            nd_pixels(stage_fb, STAGE_PW, STAGE_PH);
            nd_disc(st_f(x + PAD / 2.f), st_f(y + PAD / 2.f), st_f(PAD * .78f), ARGB((uint32_t)(70.f * level), (ink >> 16) & 255u, (ink >> 8) & 255u, ink & 255u));
        }
        const float breath = .05f * garden_wind(i) + .07f * nod;   /* (0.32) idle pads breathe with the device's LEDs */
        vg_round(x, y, PAD, PAD, 9, held || latch ? ink : vg_mix(0x52766b, ink, .25f));
        vg_round(x + (held ? 2 : 1), y + (held ? 2 : 1), PAD - (held ? 4 : 2), PAD - (held ? 4 : 2), held ? 7 : 8,
                 vg_mix(vg_mix(0x183431, ink, .10f + breath), ink, .78f * level));
        vg_rect(x + 9, y + (held ? 2 : 1), PAD - 18, 1, vg_mix(0x2c4d47, 0xffffff, .3f * level));
        if (latch) vg_round(x + 8, y + PAD - 6, PAD - 16, 3, 1, level > .55f ? 0x0c1c17 : ink);
        nd_pixels(stage_fb, STAGE_PW, STAGE_PH);            /* (0.20) the pad's sign, as on the MPC */
        nd_pad_sign((int)i, st_f(x + PAD - 14.f), st_f(y + 13.f), st_f(8.f), level > .55f ? 0xFF0a1719u : (ink | 0xFF000000u), level,
                    (float)(md_status()->frames % 2646000u) / 44100.f);
        const char *name = i == 7 ? "STARS" : pad_names[i];   /* STARDUST does not fit 54 px */
        if (i == PAD_GESTURE) name = gesture_label();
        if (latched & (1u << i)) vg_text("L", x + PAD - 7, y + PAD - 19, VG_SMALL, level > .55f ? 0x0c1c17 : 0xe4f1ea, VG_CENTER);
        char number[4];
        vg_text(vg_itoa((int)i + 1, number), x + 6, y + 4, VG_SMALL, level > .55f ? 0x173029 : 0x7fa59b, VG_LEFT);
        vg_text(name, x + PAD / 2, y + 24, VG_SMALL, level > .55f ? 0x0a1719 : 0xd7ebe3, VG_CENTER);
        const float bar = i == PAD_GESTURE && gesture.mode != GESTURE_IDLE ? dream_gesture_progress(&gesture) : level;
        vg_rect(x + 6, y + PAD - 8, (int)((PAD - 12) * bar), 3, ink);
    }
    vg_text("Higher = harder - right-click latches 1-14", GRID_X, GRID_Y + 4 * (PAD + GAP) - 2, VG_SMALL, 0x7fa59b, VG_LEFT);
    for (unsigned i = 0; i < 6; i++) {
        const int x = GRID_X + (int)(i % 3) * (CHIP_W + GAP), y = CHIP_Y + (int)(i / 3) * (CHIP_H + GAP);
        const int on = i == selected;
        char fav[10];
        short_name(md_biome_name(favorites[i]), fav, 8);
        stui_key(x, y, CHIP_W, CHIP_H, 8, 0x83d2a6, on ? UI_ACTIVE : UI_NORMAL, "", VG_BOLD);   /* (0.27) */
        char key[4] = {'F', (char)('1' + i), 0};
        vg_text(key, x + 7, y + 4, VG_SMALL, on ? 0x14302a : 0x7fa59b, VG_LEFT);
        vg_text(fav, x + CHIP_W / 2, y + 19, VG_BOLD, on ? 0x0a1418 : 0xd7ebe3, VG_CENTER);
    }
    const int playing = md_playing();
    /* Before the first PLAY the button breathes, so a first visitor knows where the dream starts. */
    const float breathe = playing ? 0.f : .5f + .5f * vd_sin(md_status()->frames / 44100.f * .6f);
    /* (0.27) the transport as keys of glass; before the first PLAY a soft halo breathes around PLAY */
    if (!playing) vg_round(GRID_X - 3, TRANSPORT_Y - 3, TRANSPORT_W + 6, TRANSPORT_H + 6, 11, vg_mix(0x0b1418, 0x83d2a6, .12f + .3f * breathe));
    stui_key(GRID_X, TRANSPORT_Y, TRANSPORT_W, TRANSPORT_H, 9, 0x83d2a6, playing ? UI_ACTIVE : UI_NORMAL, "PLAY", VG_BOLD);
    stui_key(GRID_X + TRANSPORT_W + GAP, TRANSPORT_Y, TRANSPORT_W, TRANSPORT_H, 9, 0x9abcb2, UI_NORMAL, "STOP", VG_BOLD);
    /* Mutable Dreams FM: the station's programme (RADIO page, F1) */
    const int ax = GRID_X + 2 * (TRANSPORT_W + GAP);
    stui_key(ax, TRANSPORT_Y, TRANSPORT_W, TRANSPORT_H, 9, 0xe8705e, radio.on ? UI_ACTIVE : UI_NORMAL, "", VG_BOLD);
    vg_circle((float)(ax + 12), (float)(TRANSPORT_Y + 20), 4.f, radio.on ? 0xffe0d8 : 0x557a72);
    vg_text("ON AIR", ax + TRANSPORT_W / 2 + 6, TRANSPORT_Y + 13, VG_BOLD, radio.on ? 0x2a0d0a : 0xd7ebe3, VG_CENTER);
    vg_text("ON AIR: Mutable Dreams FM, the radio", GRID_X, TRANSPORT_Y + TRANSPORT_H + 6, VG_SMALL, 0x7fa59b, VG_LEFT);
    const char *state = gesture.mode == GESTURE_OVERDUB ? "Overdub: this pass adds to the gesture" :
                        gesture_armed ? "The gesture starts on the next bar" :
                        gesture.mode == GESTURE_RECORDING ? (settings.bars == 8 ? "Capturing 8 bars of TEXTURE and SPACE" : "Capturing 4 bars of TEXTURE and SPACE") :
                        gesture.mode == GESTURE_LOOPING ? "The gesture loops - tap 16 to stop" :
                        "Tap 16, play TEXTURE and SPACE, it loops";
    vg_text(state, GRID_X, TRANSPORT_Y + TRANSPORT_H + 26, VG_SMALL, gesture.mode ? 0xf2fbf7 : 0x9abcb2, VG_LEFT);
    /* (0.26) FIRST STEPS: the pad or key the step asks for glows */
    const int target = tour_stage_target();
    if (target) {
        const int tx = target == 1 ? GRID_X : GRID_X + ((target - 2) % 4) * (PAD + GAP);
        const int ty = target == 1 ? TRANSPORT_Y : GRID_Y + ((target - 2) / 4) * (PAD + GAP);
        const int tw = target == 1 ? TRANSPORT_W : PAD, th = target == 1 ? TRANSPORT_H : PAD;
        const float pulse = .5f + .5f * vd_sin(md_status()->frames / 44100.f * 1.4f);
        const uint32_t c = vg_mix(0x8a6a3a, 0xffe6a0, pulse);
        vg_rect(tx - 4, ty - 4, tw + 8, 3, c); vg_rect(tx - 4, ty + th + 1, tw + 8, 3, c);
        vg_rect(tx - 4, ty - 4, 3, th + 8, c); vg_rect(tx + tw + 1, ty - 4, 3, th + 8, c);
    }
}

void vibe_on_stage(uint16_t *fb, int w, int h) {
    (void)fb; (void)w; (void)h;
    static unsigned stage_frame_no;                      /* (0.25) the first stage frame builds no sky: it only shows the dark */
    nd_frame_begin(stage_frame_no++ ? 1 : 0);
    {   /* 0.20: the whole stage over the Biome's night (garden-night.inc), kept */
        static uint16_t sky[(STAGE_PW / 2) * (STAGE_PH / 2)];
        static nd_scene night = {.sky = sky, .w = STAGE_PW / 2, .h = STAGE_PH / 2, .biome = -1};
        nd_backdrop_half(&night, stage_fb, STAGE_PW, STAGE_PH, .5f);   /* (0.23: at half size, a quarter of the work) */
    }
    if (!rate_ratio) {
        vg_text("Use a host rate from 44.1 to 192 kHz", 24, 40, VG_BIG | VG_X2, 0xe7a77d, VG_LEFT);
        return;
    }
    vg_text("MUTABLE DREAMS", LAND_X, 14, VG_HUGE, 0xe0f5ec, VG_LEFT);
    vg_text("PERFORMANCE GARDEN", 384, 30, VG_BOLD, 0x7fa59b, VG_LEFT);
    char tempo[12];
    vg_itoa(md_state()->tempo, tempo);
    char rec_text[32];
    if (rec_label(rec_text)) vg_text(rec_text, STAGE_W - 24, 16, VG_BOLD, rec_state == REC_TAKING ? 0xe8705e : 0xe8a25e, VG_RIGHT);
    else vg_text(md_playing() ? "PLAYING" : "PRESS PLAY", STAGE_W - 24, 16, VG_BOLD, md_playing() ? 0x83d2a6 : 0x9abcb2, VG_RIGHT);
    vg_text(tempo, STAGE_W - 24, 34, VG_BOLD, 0xe0f5ec, VG_RIGHT);
    vg_text("BPM", STAGE_W - 24 - vg_text_width(tempo, VG_BOLD) - 6, 34, VG_SMALL, 0x7fa59b, VG_RIGHT);
    char depth[24];
    if (!pages_mode()) {                                  /* (0.24: under PAGES the mirror sits there; A / B DEPTH are knobs) */
        percent_text("A TEXTURE", settings.texture_depth, depth);
        vg_text(depth, 384, 50, VG_BOLD, colors[ROW_TEXTURE], VG_LEFT);   /* (0.23: under the title line, the views' control took its place) */
        percent_text("B SPACE", settings.space_depth, depth);
        vg_text(depth, 500, 50, VG_BOLD, colors[ROW_SPACE], VG_LEFT);
    }
    pages_chip();
    if (radio.on) {
        vg_circle(388.f, 19.f, 5.f, 0xe8705e);
        vg_text(radio.in_dream ? "ON AIR - DREAM" : "ON AIR", 400, 12, VG_BOLD, 0xe8705e, VG_LEFT);
    } else if (moment_active >= 0) {
        char m[12] = "MOMENT ";
        m[7] = (char)('1' + moment_active); m[8] = 0;
        vg_text(m, 384, 12, VG_BOLD, 0xf0cb86, VG_LEFT);
    } else if (settings.wander) vg_text(idle_frames > 2u * MD_RATE ? "WANDERING" : "WANDER ON", 384, 12, VG_BOLD, 0x90cbbf, VG_LEFT);
    if (!pages_mode() && stage_view == 1) {
        draw_places();
        stage_controls();
        if (intro_active()) draw_intro(stage_fb, STAGE_PW, STAGE_PH, 2);
        return;
    }
    if (pages_mode()) {
        draw_pages();
        if (intro_active()) draw_intro(stage_fb, STAGE_PW, STAGE_PH, 2);
        return;
    }
    stage_landscape();
    stage_lanes();
    stage_controls();
    if (tour_step >= 0) tour_card(stage_fb, STAGE_PW, STAGE_PH, st_f(24), st_f(386), st_f(652), st_f(152), 1);   /* (0.26) FIRST STEPS */
    if (intro_active()) draw_intro(stage_fb, STAGE_PW, STAGE_PH, 2);
}

/* Stage input: everything it does is also a control on the device. */
static void stage_event(const vibe_event *in) {
    vibe_event scaled = *in;                      /* the host's pixels (1280 x 720) -> layout units (960 x 540) */
    if (in->id == VIBE_STAGE_DOWN || in->id == VIBE_STAGE_MOVE || in->id == VIBE_STAGE_UP)
        scaled.value = (float)((VIBE_STAGE_Y(in) * 3 / 4) * 4096 + VIBE_STAGE_X(in) * 3 / 4);
    const vibe_event *event = &scaled;
    const unsigned pointer = event->midi[0] < 10 ? event->midi[0] : 0;
    if (event->id == VIBE_STAGE_HIDDEN) {
        for (unsigned i = 0; i < 10; i++) if (stage_pointer_pad[i] >= 0) {
            vibe_event up = {.type=VIBE_EV_PAD, .id=(uint8_t)stage_pointer_pad[i], .down=0};
            pad_from_stage = 1; vibe_on_event(&up); pad_from_stage = 0;
            stage_pointer_pad[i] = -1;
        }
        for (unsigned i = 0; i < 10; i++) stage_pointer_knob[i] = -1;
        return;
    }
    /* 0.18: what this pointer holds in the garden is let go first, even if the stage has switched to ALL PAGES since */
    if (event->id == VIBE_STAGE_UP && stage_pointer_pad[pointer] >= 0) {
        vibe_event up = {.type=VIBE_EV_PAD, .id=(uint8_t)stage_pointer_pad[pointer], .down=0};
        stage_pointer_pad[pointer] = -1;
        pad_from_stage = 1; vibe_on_event(&up); pad_from_stage = 0;
        return;
    }
    if (event->id == VIBE_STAGE_UP && stage_pointer_macro[pointer] >= 0) { stage_pointer_macro[pointer] = -1; return; }
    if (pages_event(event, pointer)) return;
    if ((event->id == VIBE_STAGE_DOWN || event->id == VIBE_STAGE_MOVE) && stage_pointer_macro[pointer] >= 0) {
        const int y = VIBE_STAGE_Y(event);
        const int v = clampi((MACRO_TOP + MACRO_H - y) * 1000 / MACRO_H, 0, 1000);
        if (md_command_space()) md_command(MD_MACRO, stage_pointer_macro[pointer], v);
        return;
    }
    if (event->id != VIBE_STAGE_DOWN) return;
    const int x = VIBE_STAGE_X(event), y = VIBE_STAGE_Y(event);
    for (int i = 0; i < 4; i++) {
        const int mx = MACRO_X + i * (MACRO_W + MACRO_GAP);
        if (x >= mx - 4 && x < mx + MACRO_W + 4 && y >= MACRO_TOP - 6 && y < MACRO_TOP + MACRO_H + 6) {
            stage_pointer_macro[pointer] = (int8_t)i;
            if (md_command_space()) md_command(MD_MACRO, i, clampi((MACRO_TOP + MACRO_H - y) * 1000 / MACRO_H, 0, 1000));
            return;
        }
    }
    float velocity = 1.f;
    const int pad = pad_at(x, y, &velocity);
    if (pad >= 0) {
        if (stage_pointer_pad[pointer] >= 0) return;
        stage_pointer_pad[pointer] = (int8_t)pad;
        vibe_event down = {.type=VIBE_EV_PAD, .id=(uint8_t)pad, .down=1, .value=velocity};
        stage_shift = (event->midi[1] & (VIBE_STAGE_MOD_RIGHT | VIBE_STAGE_MOD_SHIFT)) != 0;   /* right-click = SHIFT + pad */
        pad_from_stage = 1; vibe_on_event(&down); pad_from_stage = 0;
        stage_shift = 0;
        return;
    }
    if (x >= GRID_X && y >= CHIP_Y && y < CHIP_Y + 2 * CHIP_H + GAP) {
        const int col = (x - GRID_X) / (CHIP_W + GAP), row = (y - CHIP_Y) / (CHIP_H + GAP);
        if (col < 3 && (x - GRID_X) % (CHIP_W + GAP) < CHIP_W && (y - CHIP_Y) % (CHIP_H + GAP) < CHIP_H) {
            select_biome((unsigned)(row * 3 + col));
        }
        return;
    }
    if (y >= TRANSPORT_Y && y < TRANSPORT_Y + TRANSPORT_H && x >= GRID_X) {
        if (x < GRID_X + TRANSPORT_W) md_command(MD_PLAY, 0, 1);
        else if (x >= GRID_X + TRANSPORT_W + GAP && x < GRID_X + 2 * TRANSPORT_W + GAP) stop_all();
        else if (x >= GRID_X + 2 * (TRANSPORT_W + GAP) && x < GRID_X + 3 * TRANSPORT_W + 2 * GAP) radio_set(!radio.on);
    }
}
