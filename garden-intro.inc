/* The first seconds (0.12): a seed spiral grows in the golden angle while the title appears, on the
 * display and on the stage. It fades after 2.8 s of sound, or at once on the first touch. */
static int intro_on = 1;
enum { INTRO_FRAMES = 123480 };               /* 2.8 s at 44.1 kHz */

static float intro_time(void) { return md_status()->frames / 44100.f; }
static int intro_active(void) { return intro_on && md_status()->frames < INTRO_FRAMES; }
static void intro_dismiss(void) { intro_on = 0; }

/* scale 1 = the 480 x 320 display, 2 = the 960 x 540 stage */
static void draw_intro(int w, int h, int scale) {
    const float t = intro_time();
    const float fade = t > 2.2f ? 1.f - (t - 2.2f) / .6f : 1.f;
    if (fade <= 0.f) return;
    vg_rect_a(0, 0, w, h, 0x0a1719, .96f * fade);
    const float cx = w * .5f, cy = h * .44f;
    const int seeds = (int)(t / 2.f * 240.f);
    for (int k = 1; k < (seeds > 240 ? 240 : seeds); k++) {
        const float a = k * .381966f;                    /* the golden angle in turns */
        const float r = 5.5f * scale * vd_sqrt((float)k);
        const float x = cx + vd_sin(a + .25f) * r, y = cy + vd_sin(a) * r * .82f;
        const float grow = (float)(seeds - k) / 40.f;
        const float size = (1.f + (k % 5) * .35f) * scale * (grow > 1.f ? 1.f : grow);
        const uint32_t ink = vg_mix(0x83d2a6, 0xf0cb86, (float)k / 240.f);
        vg_circle(x, y, size, vg_mix(0x0a1719, ink, fade * (.4f + .6f * (grow > 1.f ? 1.f : grow))));
    }
    const float title = t < .6f ? 0.f : t > 1.4f ? 1.f : (t - .6f) / .8f;
    vg_text("MUTABLE DREAMS", (int)cx, (int)(h * .78f), scale > 1 ? VG_BIG | VG_X2 : VG_BIG,
            vg_mix(0x0a1719, 0xf2fbf7, title * fade), VG_CENTER);
    vg_text("Performance Garden  -  Kid Bunka", (int)cx, (int)(h * .78f) + (scale > 1 ? 52 : 28), scale > 1 ? VG_BOLD | VG_X2 : VG_SMALL,
            vg_mix(0x0a1719, 0x83d2a6, title * fade), VG_CENTER);
}
