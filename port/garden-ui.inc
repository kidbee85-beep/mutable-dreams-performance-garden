/* The garden's controls (0.27): one look for keys, tabs, value cells and meters – on the display and on the stage.
 *
 * Night-garden glass: a soft shadow below, a body in two tones (the upper half a little lighter, like light on glass),
 * a one-pixel highlight on the top edge, the group's colour as accent. States: 0 off (only a faint outline), 1 normal,
 * 2 active (filled with the accent, dark text), 3 lit (normal with the accent's LED). Only vg_round / vg_rect / vg_circle /
 * vg_frame: cheap, no per-pixel blending.
 *
 * Included twice: by adapter-check.c for the display (ui_*), and by garden-stage.inc after its scale layer with
 * UI_STAGE defined (stui_*), where the same calls take the stage's layout units. */
#ifdef UI_STAGE
#define UI(n) stui_##n
#else
#define UI(n) ui_##n
#endif

#ifndef UI_STATES
#define UI_STATES
enum { UI_OFF, UI_NORMAL, UI_ACTIVE, UI_LIT };
#endif

/* (0.32) the garden's small signs, in whatever units the caller draws: a sprout (two leaves on a stem) and a curl of vine */
static void UI(sprout)(float x, float y, float s, uint32_t c) {
    vg_line(x, y + s, x, y - s * .2f, s * .22f + .6f, c);
    vg_circle(x - s * .45f, y - s * .15f, s * .38f, c);
    vg_circle(x + s * .5f, y - s * .45f, s * .42f, c);
}
static void UI(curl)(float x, float y, float r, float dir, uint32_t c, float width) {
    float px = x, py = y;
    for (int i = 1; i <= 14; i++) {
        const float u = (float)i / 14.f, a = dir * u * 1.35f, rr = r * (1.f - .72f * u);
        const float nx = x + dir * (r - rr * vd_sin(a + .25f)) , ny = y - rr * vd_sin(a) - r * .15f * u;
        vg_line(px, py, nx, ny, width, c); px = nx, py = ny;
    }
    vg_circle(px, py, width * 1.3f, c);
}

/* a key: F1-F6, PLAY, the stage's chips. (0.27, Codex design review: one flat surface, one fine edge, one 1 px light;
 * active = the accent with dark ink and a small square marker; an unassigned key is not drawn at all) */
static void UI(key)(int x, int y, int w, int h, int r, uint32_t accent, int state, const char *label, int font) {
    if (state == UI_OFF && !(label && *label)) return;                      /* unassigned: empty canvas */
    const int ty = y + (h - vg_font_height(font)) / 2;
    if (state == UI_OFF) {                                                  /* assigned but unavailable */
        vg_round(x, y, w, h, r, 0x101c20);
        vg_text(label, x + w / 2 - 4, ty, font, 0x71877e, VG_CENTER);
        vg_frame(x + w - 12, y + 4, 6, 6, 1, 0x71877e);
        vg_line((float)(x + w - 12), (float)(y + 10), (float)(x + w - 6), (float)(y + 4), 1.f, 0x71877e);
        return;
    }
    const int on = state == UI_ACTIVE;
    /* (0.32, Mutable Dreams) glass with a fine pale edge like the Brick's cards; active: the group's colour in two tones
       with a sprout where the square marker was; lit: a firefly in the corner */
    vg_round(x, y, w, h, r, on ? vg_mix(accent, 0xffffff, .35f) : 0x3f6a60);                       /* the edge */
    vg_round(x + 1, y + 1, w - 2, h - 2, r > 1 ? r - 1 : r, on ? vg_mix(accent, 0x0c1c17, .18f) : 0x13292a);
    vg_round(x + 1, y + 1, w - 2, (h - 2) / 2, r > 1 ? r - 1 : r, on ? accent : 0x183431);        /* lighter upper half */
    vg_rect(x + r + 2, y + 1, w - 2 * r - 4, 1, on ? vg_mix(accent, 0xffffff, .55f) : 0x2f544d);   /* one line of light */
    const int roomy = vg_text_width(label ? label : "", font) <= w - 24;
    if (on && roomy) UI(sprout)((float)(x + w - 9), (float)(y + 8), 3.2f, 0x0c1c17);
    else if (state == UI_LIT) { vg_circle((float)(x + w - 8), (float)(y + 7), 3.4f, vg_mix(accent, 0x101c20, .55f)); vg_circle((float)(x + w - 8), (float)(y + 7), 1.8f, vg_mix(accent, 0xffffff, .3f)); }
    if (label && *label) vg_text(label, x + w / 2, ty, font, on ? 0x0c1c17 : 0xc6dcd3, VG_CENTER);
}

/* a tab: the colour groups. Active: filled with the group's colour, dark ink and a small marker; others quiet */
static void UI(tab)(int x, int y, int w, int h, uint32_t ink, int on, const char *label) {
    const int ty = y + (h - vg_font_height(VG_BOLD)) / 2;
    if (on) {
        vg_round(x, y, w, h, 7, ink);
        vg_rect(x + 7, y + 1, w - 14, 1, vg_mix(ink, 0xffffff, .45f));
        const int roomy = vg_text_width(label, VG_BOLD) <= w - 24;            /* (0.31) no marker under a long name */
        if (roomy) UI(sprout)((float)(x + 9), (float)(y + h / 2), 2.6f, 0x0c1c17);   /* (0.32) a sprout, not a square */
        vg_text(label, x + w / 2 + (roomy ? 3 : 0), ty, VG_BOLD, 0x0c1c17, VG_CENTER);
    } else {
        vg_round(x, y, w, h, 7, 0x183431);
        vg_text(label, x + w / 2, ty, VG_BOLD, 0x87a398, VG_CENTER);
    }
}

/* a meter: a slim track, the value as a glowing fill with a bead at its end; bipolar fills from the middle */
static void UI(meter)(int x, int y, int w, float fill, int bipolar, uint32_t ink) {
    fill = fill < 0.f ? 0.f : fill > 1.f ? 1.f : fill;
    /* (0.32, the Brick's sliders) a thin ground line; the value grows as grass from it, a bud at its tip */
    vg_rect(x, y + 2, w, 1, 0x24403b);
    const int mid = x + w / 2, end = x + (int)((float)w * fill);
    const int from = bipolar ? (end < mid ? end : mid) : x, to = bipolar ? (end < mid ? mid : end) : end;
    if (to > from) vg_rect(from, y + 2, to - from, 1, vg_mix(ink, 0x101c20, .25f));
    for (int bx = from + 3; bx < to - 2; bx += 7) {                  /* (sparse blades, so it reads as grass, not a ruler) */
        const int hgt = 1 + (int)(((unsigned)bx * 2654435761u >> 30) & 3u);
        vg_rect(bx, y + 2 - hgt, 1, hgt, vg_mix(ink, 0x101c20, .35f));
    }
    if (bipolar) vg_rect(mid, y - 2, 1, 6, 0x4a6863);
    vg_circle((float)end, (float)y + 1.5f, 2.6f, vg_mix(ink, 0xffffff, .55f));
}

/* a value cell: the encoder's name, its value large, the meter – on a quiet glass card */
static void UI(cell)(int x, int y, int w, int h, const char *label, const char *value, float fill, int bipolar, int meter, uint32_t ink) {
    vg_round(x, y, w, h, 5, 0x101c20);
    const int fs = vg_font_height(VG_SMALL), fb = vg_font_height(VG_BOLD);
    /* (0.28, Codex design review: numbers 16 px and more) a short number in the large face, names in the bold one */
    int digits = 1;
    for (const char *q = value; *q; q++) if (!((*q >= '0' && *q <= '9') || *q == '-' || *q == '+' || *q == ' ' || *q == 'm' || *q == 's')) digits = 0;
    const int vw = vg_text_width(value, VG_BIG), lw = vg_text_width(label, VG_SMALL);
    if (digits && vw + lw + 17 <= w) {                 /* the number large on the left, its name and meter beside it */
        vg_text(value, x + 6, y + (h - vg_font_height(VG_BIG)) / 2, VG_BIG, 0xe4f1ea, VG_LEFT);
        const int rx = x + 6 + vw + 5;                  /* (0.31) a little tighter: longer names fit beside their number */
        vg_text(label, rx, y + 5, VG_SMALL, 0xadc7bc, VG_LEFT);
        if (meter) UI(meter)(rx, y + h - 10, x + w - 7 - rx, fill, bipolar, ink);
        return;
    }
    const int top = y + (h - 6 - fs - fb) / 2;                                   /* label and value centred above the meter */
    vg_text(label, x + 7, top, VG_SMALL, 0xadc7bc, VG_LEFT);
    vg_text(value, x + 7, top + fs, VG_BOLD, 0xe4f1ea, VG_LEFT);
    if (meter) UI(meter)(x + 7, y + h - 5, w - 14, fill, bipolar, ink);
}

#undef UI
