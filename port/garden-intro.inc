/* The first seconds (0.12): a seed spiral grows in the golden angle while the title appears, on the
 * display and on the stage. It fades after 2.8 s of sound, or at once on the first touch. */
static int intro_on = 1;
enum { INTRO_FRAMES = 123480 };               /* 2.8 s at 44.1 kHz */

static float intro_time(void) { return md_status()->frames / 44100.f; }
static int intro_active(void) { return intro_on && md_status()->frames < INTRO_FRAMES; }
static void intro_dismiss(void) { intro_on = 0; }

/* 0.21: a night garden wakes (garden-night.inc), as on the Brick: an aurora in the six track colours, six plants of
 * the first Biome's sounds grow from the horizon and bloom, spores rise; MUTABLE DREAMS appears, "by Kid Bunka" under
 * it. At the end the plants sink back and the living garden takes over. Drawn over the page's own night.
 * fb: the picture (the display or the stage), w x h its pixels; scale 1 the display, 2 the stage. */
static void draw_intro(uint16_t *fb, int w, int h, int scale) {
    const float t = intro_time();
    const float fade = t > 2.2f ? 1.f - (t - 2.2f) / .6f : 1.f;
    if (fade <= 0.f || !fb) return;
    const md_patch *p = md_state();
    nd_fb = fb, nd_fw = w, nd_fh = h;
    nd_cx0 = 0, nd_cy0 = 0, nd_cx1 = w, nd_cy1 = h;
    nd_s = (float)h / 720.f;
    GD_H = 720.f; GD_HORIZON = 560.f; GD_W = (float)w / nd_s;
    vg_rect_a(0, 0, w, h, 0x03060c, .91f * fade);                                 /* the page dims behind (the SDK's fast veil) */
    /* the aurora, track by track: six soft ribbons, one short vertical span per pixel column (0.25: the 768 discs
       cost ~5 ms a frame on the stage; the spans touch each pixel once – same curve, same colours) */
    {
        const float a0 = 22.f * fade * (t < .5f ? t / .5f : 1.f), half = 13.f * nd_s;
        const int step = nd_s > 1.5f ? 2 : 1;                                       /* (the stage: every 2nd column is enough) */
        for (int k = 0; k < 6; k++) {
            const uint32_t c = BK_ALPHA(nd_track_col(k), (uint32_t)a0);
            for (int px = 0; px < w; px += step) {
                const float u = (float)px / (float)w;
                const float yc = (150.f + 40.f * nd_sinf(u * 4.f + t * .6f + (float)k * .7f) + (float)k * 14.f) * nd_s;
                int y0 = (int)(yc - half - 1.f), y1 = (int)(yc + half + 1.f);
                if (y0 < 0) y0 = 0;
                if (y1 >= h) y1 = h - 1;
                for (int py = y0; py <= y1; py++) {
                    float cv = half + .5f - nd_fabsf((float)py + .5f - yc);
                    cv = cv > 1.f ? 1.f : cv;
                    cv *= 1.f - nd_fabsf((float)py + .5f - yc) / (half + 1.f);    /* soft towards the edges */
                    nd_blend(px, py, c, cv);
                    if (step == 2 && px + 1 < w) nd_blend(px + 1, py, c, cv);
                }
            }
        }
    }
    nd_rect(0, GD_HORIZON, GD_W, 2.f / nd_s, ARGB((uint32_t)(80.f * fade), 150, 240, 200));
    for (int i = 0; i < 6; i++) {                                                  /* six plants grow, one after the other */
        float g = (t - .2f - .12f * (float)i) / 1.1f;
        g = g < 0.f ? 0.f : g > 1.f ? 1.f : g;
        g = g * g * (3.f - 2.f * g) * (fade < 1.f ? fade : 1.f);
        if (g <= .02f) continue;
        gd_plant pl = {0};
        pl.i = i;
        pl.col = nd_track_col(i);
        pl.x = GD_W * (.14f + .145f * (float)i);
        pl.foot = GD_HORIZON;
        pl.h = 260.f * g;
        pl.sway = 10.f * nd_sinf(t * 1.3f + (float)i);
        pl.bloom = g > .8f ? (g - .8f) * 5.f : 0.f;
        gd_draw_species(&pl, i < md_active_tracks() ? gd_species(i) : SP_TULIP, t);
        if (pl.bloom > .5f)                                                        /* spores rise from the blooms */
            for (int k = 0; k < 4; k++) {
                const float u = nd_fmodf(t * .7f + (float)k * .25f + (float)i * .13f, 1.f);
                nd_disc(pl.tipx + 18.f * nd_sinf(u * 6.f + (float)k), pl.tipy - u * 120.f, 3.f, BK_ALPHA(pl.col, (uint32_t)(200.f * (1.f - u) * fade)));
            }
    }
    const float title = t < .7f ? 0.f : t > 1.4f ? 1.f : (t - .7f) / .7f;
    const int ty = (int)(h * (scale > 1 ? .70f : .14f));   /* (the display: above the garden) */
    vg_text("MUTABLE DREAMS", w / 2, ty, scale > 1 ? VG_HUGE : VG_BIG, vg_mix(0x05080f, 0xfffcf4, title * fade), VG_CENTER);
    vg_text("Performance Garden  -  by Kid Bunka", w / 2, ty + (scale > 1 ? 74 : 30), scale > 1 ? VG_BIG : VG_BOLD,
            vg_mix(0x05080f, 0x96f0c8, title * fade), VG_CENTER);
    (void)p;
}
