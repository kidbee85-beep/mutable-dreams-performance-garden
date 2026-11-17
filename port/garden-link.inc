/* VIBE FM Link (SDK 24 / 25, 0.14): a controller with a screen – the FM-1 under VIBE FM Link – shows the page,
 * its four values and a 240 x 240 picture of the dream. Mutable Dreams on the FM-1 it came from, now as the
 * VIBE's remote. Called at the end of vibe_on_draw; the host sends only what changed. */
static uint16_t link_px[240 * 240];

static void link_copy11(char *out, const char *in) {
    unsigned i = 0;
    while (in[i] && i < 11) { out[i] = in[i]; i++; }
    out[i] = 0;
}

static void link_publish(void) {
    vibe_link_page(views[page].name);
    for (unsigned k = 0; k < 4; k++) {
        Knob kb;
        if (!knob_of(page, k, &kb)) continue;
        char text[24], name[12], value[12];
        link_copy11(name, kb.label);
        link_copy11(value, knob_text(&kb, text));
        vibe_link_encoder(VE_1 + (int)k, name, value);
    }
    /* A / B are the live depths on every page */
    char a[12], b[12];
    vg_itoa((int)(settings.texture_depth * 100.f + .5f), a);
    vg_itoa((int)(settings.space_depth * 100.f + .5f), b);
    vibe_link_encoder(VE_A, "TEXT DEPTH", a);        /* (0.18: "SPACE" was also encoder 4 on LIVE) */
    vibe_link_encoder(VE_B, "SPACE DEPTH", b);
}

/* The 240 x 240 picture: the page in its colour, four rings for the four values, the dream underneath. */
static void link_draw(uint16_t *display_fb) {
    const view_info *vi = &views[page];
    const uint32_t ink = group_ink[vi->group];
    vg_begin_size(link_px, 240, 240);
    {   /* 0.21: the Biome's night behind (garden-night.inc) */
        static uint16_t sky[240 * 240];
        static nd_scene night = {.sky = sky, .w = 240, .h = 240, .biome = -1};
        nd_backdrop(&night, link_px, 240, 240, 0, 0, 240, 240, .55f);
    }
    vg_text(vi->name, 120, 8, VG_BIG, 0xf2fbf7, VG_CENTER);
    vg_text(radio.on ? "ON AIR - MUTABLE DREAMS FM" : md_playing() ? "PLAYING" : "PRESS PLAY", 120, 36, VG_SMALL,
            radio.on ? 0xe8705e : ink, VG_CENTER);
    for (unsigned k = 0; k < 4; k++) {
        Knob kb;
        if (!knob_of(page, k, &kb)) continue;
        const float cx = 62.f + (k % 2) * 116.f, cy = 80.f + (k / 2) * 68.f;
        ring_line(cx, cy, 22.f, 3.f, 0x1d3532, .375f, 1.125f);
        if (kb.cmd != L_PADSET) ring_line(cx, cy, 22.f, 3.f, ink, .375f, .375f + .75f * knob_fill(&kb));
        char text[24], value[12];
        link_copy11(value, knob_text(&kb, text));
        if (vg_text_width(value, VG_BOLD) > 40) value[6] = 0;     /* long names: the first letters inside the ring */
        vg_text(value, (int)cx, (int)cy - 8, VG_BOLD, 0xf2fbf7, VG_CENTER);
        vg_text(kb.label, (int)cx, (int)cy + 26, VG_SMALL, 0x9abcb2, VG_CENTER);
    }
    /* the dream: the six tracks as their plants, small (0.21) */
    {
        static uint16_t sky[240 * 48];
        static nd_scene garden = {.sky = sky, .w = 240, .h = 48, .biome = -1, .left = .08f, .step = .168f, .grow = 1.5f};
        float lanes[8];
        for (unsigned i = 0; i < 8; i++) lanes[i] = sounding[i] * (i < 4 ? settings.texture_depth : settings.space_depth);
        nd_draw(&garden, link_px, 240, 240, 0, 192, 240, 48, lanes, (int)track, 0);
    }
    vibe_link_screen(link_px);
    vg_begin(display_fb);
}
