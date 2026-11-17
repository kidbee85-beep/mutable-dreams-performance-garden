/* FIRST STEPS (0.26): the tour's card (garden-tour.inc has the steps). Included by adapter-check.c after
 * garden-intro.inc, where the night garden's drawing is known. */

/* the card, in real pixels: on the display (480 x 320) over the lower part of the page; on the stage (1280 x 720) under
 * the garden. (Defined before the stage's scale layer: these vg_* calls take pixels.) */
static void tour_card(uint16_t *fb, int fw, int fh, float x, float y, float w, float h, int big) {
    if (tour_step < 0 && tour_flash <= 0.f) return;
    const float t = md_status()->frames / 44100.f;
    nd_pixels(fb, fw, fh);
    if (tour_step < 0) return;
    if (big) {                                        /* the stage: one solid fill (the SDK's fast path), a thin edge */
        vg_round((int)x, (int)y, (int)w, (int)h, 12, 0x0a1820);
        vg_rect((int)x + 12, (int)y, (int)w - 24, 2, 0x5a4a2a);
    } else nd_glass(x, y, w, h, 10.f, ARGB(240, 10, 22, 30));
    const tour_text *tx = &tour_texts[tour_step];
    const int ix = (int)x + 14, iy = (int)y + (big ? 10 : 7);
    /* the steps as seeds: done, now (pulsing), to come */
    for (int k = 0; k < TOUR_STEPS; k++) {
        const float cx = x + w - 16.f - (float)(TOUR_STEPS - 1 - k) * (big ? 16.f : 11.f), cy = y + (big ? 18.f : 13.f);
        const float pulse = k == tour_step ? 1.f + .25f * vd_sin(t * 1.2f) : 1.f;
        vg_circle(cx, cy, (k == tour_step ? 4.5f : 3.f) * pulse, k < tour_step ? 0x83d2a6 : k == tour_step ? 0xf0c38c : 0x34514b);
    }
    vg_text(tx->title, ix, iy, big ? VG_BIG : VG_BOLD, 0xf0c38c, VG_LEFT);
    vg_text(tx->line1, ix, iy + (big ? 46 : 18), big ? VG_BIG : VG_SMALL, 0xe0f5ec, VG_LEFT);
    vg_text(tx->line2, ix, iy + (big ? 84 : 31), big ? VG_BIG : VG_SMALL, 0xa1c6ba, VG_LEFT);
    vg_text(big ? "HOME ends the tour  -  ? starts it again" : "HOME: end tour", (int)(x + w) - 12, (int)(y + h) - (big ? 22 : 15),
            VG_SMALL, 0x6f8f88, VG_RIGHT);
    if (tour_flash > 0.f && tour_step > 0)            /* the step before was done: a little bloom */
        for (int p = 0; p < 6; p++) { const float a = (float)p * 1.047f + t; nd_disc(x + 22.f + nd_cosf(a) * 9.f * tour_flash, y + h - 18.f + nd_sinf(a) * 9.f * tour_flash, 3.f * tour_flash, ARGB((uint32_t)(220.f * tour_flash), 160, 240, 190)); }
}
