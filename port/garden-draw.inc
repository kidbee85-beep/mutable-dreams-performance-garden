/* (0.32) the garden's shared pulse: a slow wind over the pads (by row and column) and a nod on every beat –
   the same on the device's LEDs and on the stage, so hardware and picture breathe together */
static float garden_nod(void) {
    static unsigned last_beat = ~0u;
    static float beat_at = -10.f;
    const float t = (float)(md_status()->frames % 2646000u) / 44100.f;
    const unsigned beat = md_status()->step >> 2;
    if (md_playing() && beat != last_beat) { last_beat = beat; beat_at = t; }
    float since = t - beat_at; if (since < 0.f) since = 10.f;
    return md_playing() && since < .5f ? (1.f - since * 2.f) * (1.f - since * 2.f) : 0.f;
}
static float garden_wind(unsigned pad) {
    const float t = (float)(md_status()->frames % 2646000u) / 44100.f;
    return .5f + .5f * vd_sin(t * .14f - (float)(pad % 4) * .09f - (float)(pad / 4) * .05f);
}

/* Display design for every page of the 2.1 instrument (0.9). Included after garden-views.inc.
 * One visual language: deep teal glass, the group's colour as ink, soft rounded panels,
 * values as leaf bars, and a live picture of what the page changes in the middle. */

enum { BOX_X = 14, BOX_Y = 96, BOX_W = 452, BOX_H = 178 };
static const uint32_t glass = 0x102726, glass_hi = 0x183634, dim_text = 0x7fa59b, soft_text = 0xa1c6ba;

static float seconds_now(void) { return (md_status()->frames % 1764000u) / 44100.f; }

static void short_name(const char *name, char *out, unsigned max) {
    unsigned i = 0;
    while (name[i] && name[i] != ' ' && i < max) { out[i] = name[i]; i++; }
    out[i] = 0;
}

/* (0.32) a page change: a veil of light with petals sweeps across the glass, 0.45 s */
static int bloom_page = -1;
static float bloom_since;
static void page_bloom(void) {
    const float now = md_status()->frames / 44100.f;
    if (page != bloom_page) { bloom_page = page; bloom_since = now; }
    const float u = (now - bloom_since) / .45f;
    if (u < 0.f || u >= 1.f) return;
    const uint32_t ink = group_ink[views[page].group];
    const float x = (float)BOX_X + (float)(BOX_W + 80) * u - 40.f, a = 1.f - u;
    for (int k = 0; k < 6; k++) nd_disc(x - (float)k * 10.f, (float)(BOX_Y + BOX_H / 2), 46.f - (float)k * 4.f,
                                        ARGB((uint32_t)(14.f * a), (ink >> 16) & 255u, (ink >> 8) & 255u, ink & 255u));
    for (int p = 0; p < 9; p++) {
        const float py = (float)BOX_Y + 14.f + (float)p * 18.f + 6.f * vd_sin(u * 1.5f + (float)p * .3f);
        const float px = x - 20.f * vd_sin((float)p * .37f) - 30.f * u;
        nd_disc(px, py, 2.2f + 1.5f * (float)(p & 1), ARGB((uint32_t)(210.f * a), 250, 230, 200));
    }
}

static void panel(uint32_t ink) {
    if (device_fb) {                                   /* 0.20: glass over the Biome's night */
        nd_pixels(device_fb, 480, 320);
        nd_glass((float)BOX_X, (float)BOX_Y, (float)BOX_W, (float)BOX_H, 10.f, ARGB(172, 9, 17, 27));   /* (0.32) the night shows through */
        {   /* (0.32) the group's own life, inside the glass, behind the page's picture */
            const int cx0 = nd_cx0, cy0 = nd_cy0, cx1 = nd_cx1, cy1 = nd_cy1; const float s0 = nd_s, w0 = GD_W, h0 = GD_H;
            nd_cx0 = BOX_X + 2; nd_cy0 = BOX_Y + 2; nd_cx1 = BOX_X + BOX_W - 2; nd_cy1 = BOX_Y + BOX_H - 2;
            nd_s = .5f; GD_W = (float)(BOX_W - 4) / nd_s; GD_H = (float)(BOX_H - 4) / nd_s;
            nd_ambience_at(views[page].group, md_status()->frames / 44100.f);
            nd_cx0 = cx0; nd_cy0 = cy0; nd_cx1 = cx1; nd_cy1 = cy1; nd_s = s0; GD_W = w0; GD_H = h0;
        }
        page_bloom();
    } else vg_round(BOX_X, BOX_Y, BOX_W, BOX_H, 10, glass);
    vg_rect(BOX_X + 10, BOX_Y, BOX_W - 20, 1, vg_mix(glass, ink, .35f));
    /* (0.32) vines growing at two corners of the glass, in the group's colour, swaying a little */
    const float sway = vd_sin(md_status()->frames / 44100.f * .07f) * 1.5f;
    ui_curl((float)(BOX_X + BOX_W - 22), (float)(BOX_Y + 1), 12.f + sway, 1.f, vg_mix(0x101c20, ink, .55f), 1.2f);
    ui_sprout((float)(BOX_X + BOX_W - 30), (float)(BOX_Y - 2), 2.4f, vg_mix(0x101c20, ink, .6f));
    ui_curl((float)(BOX_X + 18), (float)(BOX_Y + BOX_H - 2), 9.f - sway, -1.f, vg_mix(0x101c20, ink, .45f), 1.f);
}

static void caption(const char *text) { vg_text(text, 26, 252, VG_SMALL, soft_text, VG_LEFT); }

static void ring_line(float cx, float cy, float r, float width, uint32_t color, float from, float to) {
    float px = cx + vd_sin(from + .25f) * r, py = cy + vd_sin(from) * r;
    const int n = 28;
    for (int i = 1; i <= n; i++) {
        const float t = from + (to - from) * i / n;
        const float x = cx + vd_sin(t + .25f) * r, y = cy + vd_sin(t) * r;
        vg_line(px, py, x, y, width, color);
        px = x; py = y;
    }
}

#include "garden-listen-draw.inc"   /* (0.24) LISTEN, SING, CV GATE and the moon pond */

/* ---- per kind -------------------------------------------------------------------------------------------- */
/* (0.19) the worlds on the BROWSER page: the list, and the chosen world's Biome sign, tempo and story */
static void draw_worlds(uint32_t ink) {
    panel(ink);
    vg_text(world_family < 0 ? "ALL WORLDS" : world_families[world_family], 26, 104, VG_SMALL, dim_text, VG_LEFT);
    int ids[7];
    ids[3] = world_pos;
    for (int side = -1; side <= 1; side += 2) {
        int pos = world_pos, found = 0;
        for (int guard = 0; guard < WORLDS && found < 3; guard++) {
            pos = (pos + (side > 0 ? 1 : WORLDS - 1)) % WORLDS;
            if (world_matches(pos)) { ids[3 + side * (found + 1)] = pos; found++; }
        }
        for (; found < 3; found++) ids[3 + side * (found + 1)] = -1;
    }
    for (int i = 0; i < 7; i++) {
        if (ids[i] < 0) continue;
        const int y = 120 + i * 19, near = i == 3;
        if (near) vg_round(20, y - 3, 270, 21, 6, vg_mix(glass, ink, .28f));
        if (device_fb) { nd_pixels(device_fb, 480, 320); nd_biome_sigil(worlds[ids[i]].biome, 34.f, (float)y + 8.f, near ? 6.f : 4.5f, near ? 0xFFfffcf4u : BK_ALPHA(nd_track_col(worlds[ids[i]].biome), 170), 0.f); }
        vg_text(worlds[ids[i]].name, 46, y, near ? VG_BOLD : VG_SMALL,
                vg_mix(glass, near ? 0xf2fbf7 : soft_text, near ? 1.f : .9f - (i < 3 ? 3 - i : i - 3) * .2f), VG_LEFT);
        if (ids[i] == world_here && md_state()->biome_id == worlds[world_here].biome) vg_circle(282.f, (float)y + 7.f, 3.f, ink);
    }
    const world_spec *w = &worlds[world_pos];
    vg_round(304, 112, 150, 150, 8, glass_hi);
    if (device_fb) nd_sigil_px(device_fb, 480, 320, 326.f, 134.f, 13.f, w->biome, (float)(md_status()->frames % 2646000u) / 44100.f);
    char line[40];
    vg_text(w->tempo > 0 ? vg_itoa(w->tempo, line) : "--", 346, 122, VG_BOLD, 0xf2fbf7, VG_LEFT);
    vg_text("BPM", 346 + vg_text_width(w->tempo > 0 ? line : "--", VG_BOLD) + 4, 124, VG_SMALL, dim_text, VG_LEFT);
    {   /* the Biome's name, as far as it fits */
        const char *b = md_biome_name(w->biome);
        unsigned i = 0;
        while (b[i] && i < 18) { line[i] = b[i]; i++; }
        line[i] = 0;
        vg_text(line, 346, 138, VG_SMALL, soft_text, VG_LEFT);
    }
    /* the story, wrapped into the panel */
    const char *st = w->story;
    int y = 162;
    while (*st && y < 252) {
        char row[40];
        int n = 0, cut = 0;
        while (st[n] && n < 39) {
            row[n] = st[n]; row[n + 1] = 0;
            if (vg_text_width(row, VG_SMALL) > 136) break;
            if (st[n] == ' ') cut = n;
            n++;
        }
        if (st[n] && cut > 0) n = cut;
        row[n] = 0;
        vg_text(row, 312, y, VG_SMALL, 0xd7ebe3, VG_LEFT);
        st += n;
        while (*st == ' ') st++;
        y += 17;
    }
}

static void draw_list(uint32_t ink) {
    if (browse_worlds) { draw_worlds(ink); return; }
    panel(ink);
    char line[40];
    vg_text(browse_family < 0 ? "ALL FAMILIES" : md_biome_family_name((unsigned)browse_family), 26, 104, VG_SMALL, dim_text, VG_LEFT);
    /* the cursor in the middle, neighbours fading out */
    int ids[7], pos = browse_pos;
    ids[3] = browse_id;
    for (int side = -1; side <= 1; side += 2) {
        int p = pos, found = 0;
        for (int guard = 0; guard < MD_BIOMES && found < 3; guard++) {
            p = (p + (side > 0 ? 1 : MD_BIOMES - 1)) % MD_BIOMES;
            const int id = md_biome_at((unsigned)p);
            if (id >= 0 && browse_matches(id)) { ids[3 + side * (found + 1)] = id; found++; }
        }
        for (; found < 3; found++) ids[3 + side * (found + 1)] = -1;
    }
    for (int i = 0; i < 7; i++) {
        if (ids[i] < 0) continue;
        const int y = 120 + i * 19, near = i == 3;
        if (near) vg_round(20, y - 3, 270, 21, 6, vg_mix(glass, ink, .28f));
        if (device_fb) { nd_pixels(device_fb, 480, 320); nd_biome_sigil(ids[i], 34.f, (float)y + 8.f, near ? 6.f : 4.5f, near ? 0xFFfffcf4u : BK_ALPHA(nd_track_col(ids[i]), 170), 0.f); }
        vg_text(md_biome_name(ids[i]), 46, y, near ? VG_BOLD : VG_SMALL,
                vg_mix(glass, near ? 0xf2fbf7 : soft_text, near ? 1.f : .9f - (i < 3 ? 3 - i : i - 3) * .2f), VG_LEFT);
    }
    md_biome_card card;
    if (md_biome_card_info(browse_id, &card)) {
        vg_round(304, 112, 150, 150, 8, glass_hi);
        vg_text(md_biome_family(browse_id), 314, 120, VG_SMALL, ink, VG_LEFT);
        vg_text(vg_itoa(card.tempo, line), 314, 140, VG_BIG, 0xf2fbf7, VG_LEFT);
        vg_text("BPM", 314 + vg_text_width(line, VG_BIG) + 6, 148, VG_SMALL, dim_text, VG_LEFT);
        vg_text(notes[card.root % 12], 314, 172, VG_BOLD, 0xf2fbf7, VG_LEFT);
        vg_text(md_scale_name(card.scale), 340, 172, VG_SMALL, soft_text, VG_LEFT);
        /* (0.22) the Biome's sign, large, turning slowly */
        if (device_fb) {
            nd_pixels(device_fb, 480, 320);
            nd_disc(408.f, 222.f, 30.f, BK_ALPHA(nd_track_col(browse_id), 30));
            nd_biome_sigil(browse_id, 408.f, 222.f, 22.f, nd_track_col(browse_id), seconds_now());
        }
    }
}

static void draw_voice(uint32_t ink) {
    panel(ink);
    const md_patch *p = md_state();
    const md_track *t = &p->track[track];
    vg_text(md_model_name(t->model), 26, 106, VG_BIG, ink, VG_LEFT);
    vg_text(md_tone_name(p->tone_preset[track]), 26, 134, VG_BOLD, 0xf2fbf7, VG_LEFT);
    /* a living waveform from tone and harmonics */
    const float time = seconds_now();
    const float h = t->harm / 1000.f, c = t->tone / 1000.f;
    float px = 30.f, py = 205.f;
    for (int i = 1; i <= 120; i++) {
        const float x = 30.f + i * 3.5f, f = i / 120.f;
        const float y = 205.f - (vd_sin(f * 3.f + time * .1f) * 26.f + vd_sin(f * (6.f + h * 18.f) + time * .07f) * 14.f * c)
                        * (t->mute ? .25f : 1.f);
        vg_line(px, py, x, y, 2.f, vg_mix(glass, ink, .45f + .5f * c));
        px = x; py = y;
    }
    caption(t->mute ? "Track muted" : "Encoder 1: tone preset · 2: engine");
}

static void draw_bars(uint32_t ink) {
    panel(ink);
    for (unsigned k = 0; k < 4; k++) {
        Knob kb;
        if (!knob_of(page, k, &kb)) continue;
        const int x = 40 + (int)k * 106;
        const float f = knob_fill(&kb);
        vg_round(x, 112, 54, 130, 12, glass_hi);
        vg_round(x, 112 + (int)(130.f * (1.f - f)), 54, (int)(130.f * f) + 1, 12, vg_mix(glass_hi, ink, .55f + .4f * f));
        vg_text(kb.label, x + 27, 248, VG_SMALL, soft_text, VG_CENTER);
    }
    vg_text(md_model_name(md_state()->track[track].model), 452, 104, VG_SMALL, dim_text, VG_RIGHT);
}

static void draw_mixer(uint32_t ink) {
    panel(ink);
    const md_patch *p = md_state();
    for (int i = 0; i < md_active_tracks(); i++) {
        const md_track *t = &p->track[i];
        const md_extension *e = &p->ext[i];
        const int x = 26 + i * 64, sel = i == (int)track;
        if (sel) vg_round(x - 6, 104, 58, 164, 8, glass_hi);
        const float peak = md_status()->track_peak[i];
        const int lv = t->level * 120 / 1000;
        vg_rect(x + 6, 230 - 120, 10, 120, 0x1d3532);
        vg_rect(x + 6, 230 - lv, 10, lv, vg_mix(0x1d3532, page_ink[i], t->mute ? .25f : .8f));
        vg_rect(x + 18, 230 - (int)(peak > 1.f ? 120 : peak * 120), 3, (int)(peak > 1.f ? 120 : peak * 120), 0xf2fbf7);
        const int sends[3] = {t->send, e->delay_send, e->reverb_send};
        for (int s = 0; s < 3; s++)
            vg_circle((float)(x + 36), 120.f + s * 22.f, 3.f + sends[s] * 6.f / 1000.f, vg_mix(glass_hi, ink, .3f + sends[s] * .6f / 1000.f));
        vg_circle((float)(x + 11 + (t->pan - 500) * 20 / 500), 244.f, 3.f, soft_text);
        vg_text(track_labels[i], x + 11, 252, VG_BOLD, sel ? 0xf2fbf7 : dim_text, VG_CENTER);
    }
    vg_text("CLOUD", 452, 112, VG_SMALL, dim_text, VG_RIGHT);
    vg_text("ECHO", 452, 134, VG_SMALL, dim_text, VG_RIGHT);
    vg_text("ROOM", 452, 156, VG_SMALL, dim_text, VG_RIGHT);
}

static void draw_envelope(uint32_t ink) {
    panel(ink);
    const md_track *t = &md_state()->track[track];
    const float attack = .04f + .38f * t->attack / 1000.f, decay = .14f + .5f * t->decay / 1000.f;
    float px = 30.f, py = 240.f;
    for (int i = 0; i <= 64; i++) {
        const float f = i / 64.f, x = 30.f + f * 420.f;
        float level = f < attack ? f / attack : 1.f - (f - attack) / decay;
        if (level < 0.f) level = 0.f;
        const float y = 240.f - level * level * (60.f + t->level * .07f);
        vg_line(x, 240.f, x, y, 6.6f, vg_mix(glass, ink, .16f + .12f * level));
        if (i) vg_line(px, py, x, y, 2.2f, ink);
        px = x; py = y;
    }
    vg_text(md_has_envelope(t->model) ? "Melodic voice: holds MIDI notes" : "Percussion: engine-shaped tail", 452, 104, VG_SMALL, dim_text, VG_RIGHT);
}

static void draw_filter(uint32_t ink) {
    panel(ink);
    static const char *const kinds[] = {"FILTER OFF","LOWPASS 12 dB","LOWPASS 24 dB","LOW PASS GATE"};
    const md_track *t = &md_state()->track[track];
    const float cut = t->cutoff / 1000.f, res = t->resonance / 1000.f, slope = t->filter == 2 ? 2.f : 1.f;
    const float breath = vd_sin(seconds_now() * .12f) * .04f * t->breathe / 1000.f;
    float px = 30.f, py = 0.f;
    for (int i = 0; i <= 84; i++) {
        const float f = i / 84.f, x = 30.f + f * 420.f, c = cut + breath;
        float g = 1.f;
        if (t->filter) {
            const float over = f - c;
            g = over < 0.f ? 1.f + res * 1.6f * vd_maxf(0.f, 1.f - vd_absf(over) * 12.f) : 1.f - over * 3.f * slope + res * 1.6f * vd_maxf(0.f, 1.f - over * 12.f);
            if (g < 0.f) g = 0.f;
        }
        const float y = 236.f - g * 70.f;
        vg_line(x, 236.f, x, y, 5.2f, vg_mix(glass, ink, .12f + .1f * g));
        if (i) vg_line(px, py, x, y, 2.f, ink);
        px = x; py = y;
    }
    vg_text(kinds[clampi(t->filter, 0, 3)], 452, 104, VG_SMALL, dim_text, VG_RIGHT);
}

static void draw_tide(uint32_t ink) {
    panel(ink);
    const md_patch *p = md_state();
    const md_extension *e = &p->ext[track];
    const float time = seconds_now();
    for (int layer = 0; layer < 2; layer++) {
        const float depth = layer ? p->motion.depth2[track] / 1000.f : e->tides / 1000.f;
        const float rate = layer ? p->motion.rate2[track] : e->tides_rate;
        float px = 30.f, py = 180.f;
        for (int i = 0; i <= 80; i++) {
            const float f = i / 80.f, x = 30.f + f * 420.f;
            const float y = 180.f - vd_sin(f * (1.2f + rate * .004f) + time * .11f + layer * .3f) * (8.f + depth * 52.f);
            if (i) vg_line(px, py, x, y, layer ? 1.4f : 2.4f, layer ? vg_mix(glass, page_ink[2], .7f) : ink);
            px = x; py = y;
        }
    }
    /* Marbles: remembered variation as scattered seeds */
    for (int i = 0; i < (int)(e->marbles / 40); i++)
        vg_circle(36.f + (i * 67 % 410), 120.f + (i * 37 % 110), 1.6f, vg_mix(glass, 0xf0cb86, .6f));
    char text[48] = "A ";
    unsigned n = 2;
    const char *a = md_lfo_target_name(p->motion.target[track]);
    while (*a && n < 20) text[n++] = *a++;
    text[n++] = ' '; text[n++] = ' '; text[n++] = 'B'; text[n++] = ' ';
    a = md_lfo_target_name(p->motion.target2[track]);
    while (*a && n < 46) text[n++] = *a++;
    text[n] = 0;
    caption(text);
}

static void draw_links(uint32_t ink) {
    panel(ink);
    const md_patch *p = md_state();
    float nx[6], ny[6];
    for (int i = 0; i < 6; i++) {
        nx[i] = 240.f + vd_sin(i / 6.f + .25f) * 150.f;
        ny[i] = 180.f + vd_sin(i / 6.f) * 62.f;
    }
    for (int l = 0; l < MD_LINKS; l++) {
        const md_link *k = &p->links[l];
        if (!k->depth) continue;
        const int sel = l == link_slot;
        const float strength = vd_absf((float)k->depth) / 1000.f;
        vg_line(nx[k->source], ny[k->source], nx[k->destination], ny[k->destination], sel ? 3.f : 1.6f,
                vg_mix(glass, sel ? ink : soft_text, .35f + .6f * strength));
        const float t = vd_sin(seconds_now() * .2f + l * .25f) * .5f + .5f;
        vg_circle(nx[k->source] + (nx[k->destination] - nx[k->source]) * t, ny[k->source] + (ny[k->destination] - ny[k->source]) * t,
                  2.6f, sel ? 0xf2fbf7 : soft_text);
    }
    for (int i = 0; i < 6; i++) {
        const float energy = md_status()->track_peak[i] > .3f ? 1.f : md_status()->track_peak[i] / .3f;
        vg_circle(nx[i], ny[i], 13.f, vg_mix(glass_hi, page_ink[i], .35f + .5f * energy));
        vg_text(track_labels[i], (int)nx[i], (int)ny[i] - 7, VG_BOLD, 0x0a1719, VG_CENTER);
    }
    const md_link *k = &p->links[link_slot];
    char text[48] = "LINK ";
    char num[4];
    vg_itoa(link_slot + 1, num);
    unsigned n = 5;
    for (unsigned i = 0; num[i]; i++) text[n++] = num[i];
    text[n++] = ':'; text[n++] = ' ';
    const char *s = k->depth ? md_link_target_name(k->target) : "OFF - turn DEPTH on LINK MOTION";
    while (*s && n < 46) text[n++] = *s++;
    text[n] = 0;
    caption(text);
}

static void draw_drift(uint32_t ink) {
    panel(ink);
    const md_drift *d = &md_state()->drift;
    const int ready = md_drift_ready();
    const float pos = md_drift_position() / 1000.f;
    vg_line(60.f, 176.f, 420.f, 176.f, 4.f, glass_hi);
    vg_line(60.f, 176.f, 60.f + 360.f * pos, 176.f, 4.f, ink);
    for (int s = 0; s < 2; s++) {
        const int have = (d->captured >> s) & 1;
        const float x = s ? 420.f : 60.f;
        vg_circle(x, 176.f, 18.f, have ? vg_mix(glass_hi, ink, .7f) : glass_hi);
        vg_text(s ? "B" : "A", (int)x, 168, VG_BOLD, have ? 0x0a1719 : dim_text, VG_CENTER);
    }
    vg_circle(60.f + 360.f * pos, 176.f, 7.f, 0xf2fbf7);
    caption(ready ? (d->mode ? "Drifting between scene A and scene B" : "Both scenes captured. Turn DRIFT on")
                  : "Capture scene A and scene B on DRIFT SCENES (F1 / F2)");
}

static void draw_harmony(uint32_t ink) {
    panel(ink);
    const md_patch *p = md_state();
    for (int i = 0; i < 12; i++) {
        const float a = i / 12.f - .25f;
        const float x = 150.f + vd_sin(a + .25f) * 62.f, y = 182.f + vd_sin(a) * 62.f;
        const int pitch = (p->root + i) % 12;
        const int in = md_quantize(48 + p->root + i, p->root, p->scale) == 48 + p->root + i;
        vg_circle(x, y, i == 0 ? 11.f : 8.f, i == 0 ? 0xe2aa78 : in ? ink : 0x2f514d);
        vg_text(notes[pitch], (int)x, (int)y - 7, VG_SMALL, in || !i ? 0x0a1719 : dim_text, VG_CENTER);
    }
    vg_text(md_scale_name(p->scale), 260, 140, VG_BIG, 0xf2fbf7, VG_LEFT);
    vg_text(md_chord_name(p->chord_pattern), 260, 172, VG_BOLD, ink, VG_LEFT);
    char t[12];
    vg_text(vg_itoa(p->tempo, t), 260, 200, VG_BOLD, soft_text, VG_LEFT);
    vg_text("BPM", 262 + vg_text_width(t, VG_BOLD), 202, VG_SMALL, dim_text, VG_LEFT);
}

static void draw_chords(uint32_t ink) {
    panel(ink);
    const md_patch *p = md_state();
    vg_text(p->chord_pattern ? md_chord_name(p->chord_pattern) : "NO PROGRESSION", 26, 108, VG_BIG, ink, VG_LEFT);
    for (int i = 0; i < 8; i++) {
        const int degree = p->chord_pattern ? md_chord_degree(p->chord_pattern, i) : 0;
        const int h = 20 + (degree < 0 ? 0 : degree % 8) * 11;
        vg_round(30 + i * 52, 236 - h, 40, h, 6, vg_mix(glass_hi, ink, p->chord_pattern ? .4f + (i % 2) * .2f : .1f));
    }
    for (int i = 0; i < md_active_tracks(); i++)
        vg_text(track_labels[i], 452 - (5 - i) * 24, 108, VG_SMALL, (p->chord_tracks >> i) & 1 ? ink : 0x3c5752, VG_RIGHT);
}

static void draw_bass(uint32_t ink) {
    panel(ink);
    const md_extension *e = &md_state()->ext[track];
    for (int i = 0; i < 16; i++) {
        const int n = e->notes[i], x = 28 + i * 27, sel = i == e->edit_step;
        if (n < 0) { vg_round(x, 232, 21, 6, 3, glass_hi); continue; }
        const int h = 14 + n * 4;
        vg_round(x, 238 - h, 21, h, 4, vg_mix(glass_hi, ink, sel ? .95f : .45f));
    }
    caption(e->bass_mode == 2 ? "STEP: the sixteen notes play in order" : e->bass_mode == 1 ? "EUCLID: notes follow the hits" : "Bassline off for this track");
}

static void draw_space(uint32_t ink) {
    panel(ink);
    const md_patch *p = md_state();
    /* 0.22: the Brick's echo cave (garden-night.inc) */
    if (device_fb) {
        nd_pixels(device_fb, 480, 320);
        nd_echo_cave(60.f, 104.f, 360.f, 142.f, (float)p->delay_ms, p->delay_fb / 1000.f, p->reverb_time / 1000.f, p->fx_level / 1000.f,
                     p->freeze, seconds_now(), ink | 0xFF000000u);
    }
    caption("The echo cave: rings = echoes, mist = room");
}

static void draw_grain(uint32_t ink) {
    panel(ink);
    const md_patch *p = md_state();
    uint8_t position[4], window[4];
    md_grain_positions(position, window);
    /* 0.22: the Brick's spore meadow (garden-night.inc): a mushroom where each grain reads the tape */
    if (device_fb) {
        nd_pixels(device_fb, 480, 320);
        nd_spore_meadow(24.f, 104.f, 432.f, 142.f, position, window, p->freeze, p->reverse, seconds_now(), ink | 0xFF000000u);
    }
    caption(p->freeze ? "FREEZE: the tape holds still" : p->reverse ? "Grains play backwards" : "Grain windows on the tape");
}

/* (0.27) CLOUD: the grain cloud itself – a soft cloud of light whose grains sparkle with the MIX, air (noise) drifting
 * through it as mist, the track's send as a thread of light rising into it; FREEZE turns it to crystal. */
static void draw_cloud(uint32_t ink) {
    panel(ink);
    const md_patch *p = md_state();
    const float t = seconds_now(), air = p->grain_noise / 1000.f, mix = p->macro[3] / 1000.f, send = p->track[track].send / 1000.f;
    if (device_fb) {
        nd_pixels(device_fb, 480, 320);
        const float cx = 240.f, cy = 168.f;
        const uint32_t body = p->freeze ? ARGB(255, 200, 232, 255) : (ink | 0xFF000000u);
        for (int k = 0; k < 9; k++) {                                   /* the cloud: overlapping soft discs */
            const float a = (float)k * .698f, rr = 30.f + 10.f * nd_sinf((float)k * 2.1f);
            const float bx = cx + nd_cosf(a) * 70.f * (k ? 1.f : 0.f) * (.7f + .3f * nd_sinf(t * .2f + (float)k)), by = cy + nd_sinf(a) * 26.f * (k ? 1.f : 0.f);
            nd_disc(bx, by, rr + 6.f, BK_ALPHA(body, 18));
            nd_disc(bx, by, rr, BK_ALPHA(body, (uint32_t)(26.f + 40.f * mix)));
        }
        for (int i = 0; i < 26; i++) {                                  /* grains: sparks inside, as many as the mix */
            if ((float)i > 6.f + 20.f * mix) break;
            const uint32_t h = (uint32_t)(i + 11) * 2654435761u;
            const float u = (float)(h % 1000u) / 1000.f, v = (float)((h >> 10) % 1000u) / 1000.f;
            const float tw = p->freeze ? 1.f : .5f + .5f * nd_sinf(t * (2.f + u * 3.f) + (float)i);
            nd_disc(cx - 100.f + 200.f * u, cy - 30.f + 60.f * v + (p->freeze ? 0.f : 3.f * nd_sinf(t + (float)i)), 1.2f + 1.6f * tw,
                    p->freeze ? ARGB(230, 235, 250, 255) : ARGB((uint32_t)(120.f + 120.f * tw), 255, 246, 210));
        }
        for (int i = 0; i < 14; i++) {                                  /* air: mist drifting through, as much as AIR */
            if ((float)i > 14.f * air) break;
            const float u = nd_fmodf((float)i / 14.f + (p->freeze ? 0.f : t * .03f), 1.f);
            nd_disc(70.f + 340.f * u, cy + 18.f * nd_sinf((float)i * 1.7f + t * .3f), 9.f + 6.f * nd_sinf((float)i), ARGB((uint32_t)(28.f * air + 10.f), 210, 225, 235));
        }
        if (send > .01f)                                                /* the selected track's send: a thread of light up into the cloud */
            for (int s = 0; s < 10; s++) {
                const float f = (float)s / 10.f, y0 = 246.f - f * 54.f;
                nd_disc(cx + 6.f * nd_sinf(f * 6.f + t), y0, 1.6f + 1.4f * send, BK_ALPHA(nd_track_col((int)track), (uint32_t)(60.f + 160.f * send * (1.f - f * .5f))));
            }
        if (p->freeze) for (int i = 0; i < 6; i++) { const float a = (float)i * 1.047f; nd_line(cx + nd_cosf(a) * 90.f, cy + nd_sinf(a) * 34.f, cx + nd_cosf(a) * 104.f, cy + nd_sinf(a) * 40.f, 1.f, ARGB(180, 235, 250, 255)); }
    }
    caption(p->freeze ? "FREEZE: the cloud is crystal - it keeps what it holds" : air > .02f ? "The grain cloud - air drifts through it as mist" : "The grain cloud - turn AIR for mist, MIX for sparks");
}

static void draw_rhythm(uint32_t ink) {
    panel(ink);
    const md_track *t = &md_state()->track[track];
    int hit = 0;
    const int head = md_sequence_playhead(track, &hit);
    const int len = t->length < 1 ? 1 : t->length;
    for (int s = 0; s < len; s++) {
        const float a = (float)s / len - .25f;
        const float x = 150.f + vd_sin(a + .25f) * 66.f, y = 184.f + vd_sin(a) * 66.f;
        const int on = md_hit((unsigned)s, (unsigned)t->hits, (unsigned)t->length, (unsigned)t->rotate);
        vg_circle(x, y, s == head ? 8.f : 6.f, s == head ? 0xf2fbf7 : on ? ink : glass_hi);
    }
    char text[12];
    vg_text(vg_itoa(t->hits, text), 150, 170, VG_BIG, 0xf2fbf7, VG_CENTER);
    vg_text("HITS", 150, 196, VG_SMALL, dim_text, VG_CENTER);
    vg_text(md_model_name(t->model), 280, 120, VG_BOLD, ink, VG_LEFT);
    char sw[12];
    vg_text("SWING", 280, 150, VG_SMALL, dim_text, VG_LEFT);
    vg_text(vg_itoa(md_state()->swing / 10, sw), 340, 150, VG_SMALL, soft_text, VG_LEFT);
    vg_text("REPEATS", 280, 170, VG_SMALL, dim_text, VG_LEFT);
    vg_text(vg_itoa(t->ratchet, sw), 340, 170, VG_SMALL, soft_text, VG_LEFT);
}

static void draw_sequence(uint32_t ink) {
    panel(ink);
    const md_patch *p = md_state();
    const md_track *t = &p->track[track];
    const md_sequence *q = &p->sequence[track];
    int hit = 0;
    const int head = md_sequence_playhead(track, &hit);
    for (int i = 0; i < 32; i++) {
        const int x = 27 + (i % 16) * 27, y = 120 + (i / 16) * 62;
        const int active = q->enabled ? i < q->length && q->steps[i].note : i < t->length && md_hit((unsigned)i, t->hits, t->length, t->rotate);
        const int h = q->enabled && active ? 14 + (q->steps[i].note % 24) : 20;
        vg_round(x, y + 44 - h, 21, h, 4, head == i ? 0xf0cb86 : i == q->edit ? 0xf2fbf7 : active ? vg_mix(glass_hi, ink, .7f) : glass_hi);
    }
    caption(q->enabled ? "Recorded sequence" : "Euclidean rhythm - SEQUENCE ON plays the steps");
}

static void draw_kit(uint32_t ink) {
    panel(ink);
    const md_kit *k = &md_state()->kit;
    for (int i = 0; i < 8; i++) {
        const int x = 26 + (i % 4) * 110, y = 106 + (i / 4) * 46, sel = i == k->pad;
        vg_round(x, y, 102, 38, 7, sel ? vg_mix(glass_hi, ink, .6f) : glass_hi);
        vg_text(md_pad_name(i), x + 51, y + 11, VG_SMALL, sel ? 0x0a1719 : soft_text, VG_CENTER);
    }
    const int len = k->length ? k->length : 16;
    for (int s = 0; s < 32; s++) {
        const int on = (k->gates[k->pad] >> s) & 1u;
        vg_round(26 + s * 13, 214, 10, 22, 3, s >= len ? 0x142624 : s == k->step ? 0xf2fbf7 : on ? ink : glass_hi);
    }
    caption(k->enabled ? md_kit_name(k->bank) : "Kit off - KIT ON: T4 plays the kit, T5 and T6 rest");   /* (0.31) */
}

static void draw_dream(uint32_t ink) {
    panel(ink);
    md_itinera_view it;
    md_itinera_read(&it);
    const float time = seconds_now();
    /* 0.22: the Brick's dream map (garden-night.inc): twelve stations, a light walking the route */
    if (device_fb) {
        nd_pixels(device_fb, 480, 320);
        nd_dream_map(24.f, 150.f, 432.f, 84.f, it.active ? (int)it.preset : it_preset, it.progress > 1000 ? 1.f : it.progress / 1000.f, it.active, time, ink | 0xFF000000u);
    }
    vg_text(md_itinera_name(it.active ? it.preset : (unsigned)it_preset), 26, 106, VG_BIG, 0xf2fbf7, VG_LEFT);
    if (it.active) {
        char line[32] = "CHAPTER ";
        line[8] = (char)('1' + (it.chapter & 3)); line[9] = 0;
        vg_text(line, 26, 136, VG_BOLD, ink, VG_LEFT);
        vg_text(md_biome_name(it.biome), 120, 136, VG_SMALL, soft_text, VG_LEFT);
        vg_round(26, 238, 428, 6, 3, glass_hi);
        vg_round(26, 238, (int)(428.f * (it.progress > 1000 ? 1.f : it.progress / 1000.f)), 6, 3, ink);
        char m[16];
        vg_itoa((int)(it.remaining / 60u), m);
        unsigned n = 0; while (m[n]) n++;
        m[n] = ' '; m[n + 1] = 'M'; m[n + 2] = 'I'; m[n + 3] = 'N'; m[n + 4] = ' '; m[n + 5] = 'L'; m[n + 6] = 'E'; m[n + 7] = 'F'; m[n + 8] = 'T'; m[n + 9] = 0;
        vg_text(m, 452, 106, VG_SMALL, dim_text, VG_RIGHT);
    } else {
        vg_text("Stopped. F1 begins the dream with these settings.", 26, 136, VG_SMALL, soft_text, VG_LEFT);
    }
}

static void draw_journey(uint32_t ink) {
    panel(ink);
    const md_journey *j = &md_state()->journey;
    const int now = md_journey_position();
    for (int s = 0; s < 8; s++) {
        const md_journey_section *a = &j->section[s];
        const int x = 22 + s * 55, used = s < j->length, sel = s == journey_edit;
        vg_round(x, 112, 49, 120, 7, now == s ? vg_mix(glass_hi, ink, .7f) : sel ? vg_mix(glass_hi, ink, .35f) : used ? glass_hi : 0x132423);
        char name[10];
        short_name(a->biome == 65535 ? "HOME" : md_biome_name(a->biome), name, 6);
        vg_text(name, x + 24, 120, VG_SMALL, used ? 0xf2fbf7 : 0x3c5752, VG_CENTER);
        char n[8];
        vg_text(vg_itoa(a->bars, n), x + 24, 150, VG_BOLD, used ? ink : 0x3c5752, VG_CENTER);
        vg_text("BARS", x + 24, 170, VG_SMALL, dim_text, VG_CENTER);
        for (int t = 0; t < 6; t++)
            vg_circle((float)(x + 9 + t * 6), 214.f, 2.f, (a->keep >> t) & 1 ? ink : 0x2a403d);
    }
    caption(j->mode ? (md_journey_active() ? "Journey running" : "Journey ready - PLAY starts it") : "Journey off - JOURNEY ONCE or LOOP");
}

static void draw_banks(uint32_t ink) {
    panel(ink);
    for (int s = 0; s < 16; s++) {
        const int x = 26 + (s % 8) * 54, y = 112 + (s / 8) * 62, used = (bank_used >> s) & 1u, sel = s == bank_slot;
        vg_round(x, y, 46, 52, 8, sel ? vg_mix(glass_hi, ink, .55f) : used ? glass_hi : 0x132423);
        char n[4];
        vg_text(vg_itoa(s + 1, n), x + 23, y + 8, VG_BOLD, sel ? 0x0a1719 : used ? 0xf2fbf7 : 0x3c5752, VG_CENTER);
        if (used) vg_circle((float)(x + 23), (float)(y + 38), 4.f, sel ? 0x0a1719 : ink);
    }
    caption("Projects stay with the DAW project, the browser or the card");
}

static void draw_info(uint32_t ink) {
    panel(ink);
    char line[48];
    if (page == V_SYSTEM) {
        vg_text(md_quality_active() ? "FULL QUALITY NOW" : "ECO NOW", 26, 110, VG_BIG, ink, VG_LEFT);
        vg_text(stems_live ? "STEMS: SIX TRACKS ON OUT 3-8" : "STEMS: THE HOST HAS 2 OUTPUTS", 26, 144, VG_BOLD, stems_live ? ink : dim_text, VG_LEFT);
        vg_text("DAW: Program Change on channel 16 picks a Biome.", 26, 166, VG_SMALL, soft_text, VG_LEFT);
        vg_text(vg_itoa(md_tempo_now(), line), 26, 196, VG_BIG, 0xf2fbf7, VG_LEFT);
        vg_text("BPM NOW", 80, 204, VG_SMALL, dim_text, VG_LEFT);
    } else if (page == V_MIDI) {
        for (int i = 0; i < md_active_tracks(); i++) {
            const int r = md_midi_route((unsigned)i);
            vg_text(track_labels[i], 30 + i * 72, 120, VG_BOLD, i == (int)track ? 0xf2fbf7 : soft_text, VG_LEFT);
            line[0] = r ? 'C' : '-'; line[1] = r ? 'H' : 0; if (r) vg_itoa(r, line + 2); else line[1] = 0;
            vg_text(line, 30 + i * 72, 142, VG_SMALL, ink, VG_LEFT);
        }
        vg_text("Keys and pads always play; MIDI channels pick the track.", 26, 163, VG_SMALL, soft_text, VG_LEFT);
        vg_round(26, 200, 200, 44, 8, settings.notes_out ? vg_mix(glass_hi, ink, .55f) : glass_hi);
        vg_text("DREAM AS MIDI", 38, 206, VG_BOLD, settings.notes_out ? 0x0a1719 : 0xf2fbf7, VG_LEFT);
        vg_text(settings.notes_out ? "ON - tracks ch 1-6, kit ch 10" : "OFF - F1 turns it on", 38, 224, VG_SMALL, settings.notes_out ? 0x0a1719 : dim_text, VG_LEFT);
        vg_text("Choose a Biome, then PLAY. Radio is optional.", 26, 179, VG_SMALL, soft_text, VG_LEFT);   /* (0.27, Codex review 09) */
        vg_text("Record played tracks in the DAW", 240, 214, VG_SMALL, dim_text, VG_LEFT);
    } else {
        for (int s = 0; s < 16; s++) {
            const md_midi_mapping *m = md_mapping((unsigned)s);
            const int x = 26 + (s % 4) * 110, y = 106 + (s / 4) * 36, sel = s == learn_slot;
            vg_round(x, y, 102, 30, 6, sel ? vg_mix(glass_hi, ink, .5f) : glass_hi);
            line[0] = 0;
            vg_text(m->enabled ? md_learn_target_name(m->target) : "EMPTY", x + 6, y + 3, VG_SMALL, sel ? 0x0a1719 : m->enabled ? 0xf2fbf7 : 0x3c5752, VG_LEFT);
            if (m->enabled) { line[0] = 'C'; line[1] = 'C'; vg_itoa(m->cc, line + 2); vg_text(line, x + 6, y + 16, VG_SMALL, sel ? 0x0a1719 : dim_text, VG_LEFT); }
        }
        if (md_learn_armed() >= 0) vg_text("LISTENING", 452, 98, VG_SMALL, 0xf0cb86, VG_RIGHT);
    }
}


/* ---- SEQ GRID / TOOLS / RULES ----------------------------------------------------------------------------- */
static unsigned seq_row_length(const md_patch *p, unsigned row) {
    if (p->kit.enabled && row >= 4) return 0;
    if (p->kit.enabled && row == 3) return p->kit.length;
    if (p->sequence[row].enabled) return p->sequence[row].length;
    return p->ext[row].bass_mode == 2 ? (unsigned)clampi(p->track[row].length, 1, 16) : (unsigned)p->track[row].length;
}

/* One step's glyph: ALWAYS a square, FIRST CYCLE a single bar, EVERY 2ND a double bar, FILL a plus. */
static void step_glyph(int x, int y, unsigned rule, uint32_t ink, int strong) {
    if (rule == MD_RULE_FIRST) vg_rect(x + 3, y + 3, 4, 10, ink);
    else if (rule == MD_RULE_SECOND) { vg_rect(x + 1, y + 3, 3, 10, ink); vg_rect(x + 6, y + 3, 3, 10, ink); }
    else if (rule == MD_RULE_FILL) { vg_rect(x + 3, y + 2, 4, 12, ink); vg_rect(x, y + 6, 10, 4, ink); }
    else vg_round(x, y + 3, 10, 10, 2, ink);
    if (strong) vg_rect(x + 3, y, 4, 2, 0xf2fbf7);
}

static void draw_seq_grid(uint32_t ink) {
    panel(ink);
    const md_patch *p = md_state();
    for (unsigned row = 0; row < MD_TRACKS; row++) {
        const int y = 102 + (int)row * 24;
        const unsigned length = seq_row_length(p, row);
        const int sel = row == track, drums = p->kit.enabled && row == 3;
        const md_sequence *q = &p->sequence[row];
        const md_track *t = &p->track[row];
        const uint32_t tink = t->mute ? 0x3c5752 : page_ink[row];
        if (sel) vg_round(18, y - 2, 444, 24, 5, glass_hi);
        char tag[6] = {'T', (char)('1' + row), ' ', (char)(!length ? '-' : drums ? 'K' : q->enabled ? 'S' : 'G'), 0};
        vg_text(tag, 24, y + 2, VG_BOLD, sel ? 0xf2fbf7 : tink, VG_LEFT);
        if (!length) continue;
        uint32_t kit_steps = 0;
        if (drums) for (unsigned pad = 0; pad < 8; pad++) kit_steps |= p->kit.gates[pad];
        int hit = 0;
        const int head = md_sequence_playhead(row, &hit);
        for (unsigned i = 0; i < MD_STEPS; i++) {
            const int x = 64 + (int)i * 12;
            if (i >= length) { vg_rect(x + 4, y + 9, 2, 2, 0x22403b); continue; }
            if (!(i % 4)) vg_rect(x - 1, y, 1, 18, vg_mix(glass, tink, .25f));
            const int on = drums ? (int)((kit_steps >> i) & 1u) : q->enabled ? q->steps[i].note != 0 :
                p->ext[row].bass_mode == 2 ? p->ext[row].notes[i % 16] >= 0 :
                md_hit(i, (unsigned)clampi(t->hits + (p->macro[0] - 500) * t->length / 1500, 0, t->length), (unsigned)t->length, (unsigned)t->rotate);
            const unsigned rule = !drums && q->enabled ? md_step_rule(&q->steps[i]) : MD_RULE_ALWAYS;
            if (on) step_glyph(x, y + 1, rule, tink, !drums && q->enabled && q->steps[i].velocity >= 75);
            else vg_rect(x + 3, y + 9, 4, 2, 0x2a403d);
            if (sel && (drums || q->enabled) && (int)i == (drums ? p->kit.step : q->edit)) vg_rect(x, y + 17, 10, 2, ink);
            if ((int)i == head) vg_rect(x, y - 1, 10, 2, hit ? 0xf2fbf7 : 0x557a72);
        }
    }
    caption("S steps  G generated  K kit  -  square always, bar first, double 2nd, plus fill");
}

static void draw_seq_tools(uint32_t ink) {
    panel(ink);
    const md_patch *p = md_state();
    const int drums = p->kit.enabled && track == 3;
    const md_sequence *q = &p->sequence[track];
    vg_text(seq_actions[seq_action], 26, 104, VG_BIG, ink, VG_LEFT);
    vg_text(drums ? "KIT - SELECT A SYNTH TRACK" : md_model_name(p->track[track].model), 452, 108, VG_SMALL, dim_text, VG_RIGHT);
    if (seq_action >= SEQ_ACCENT && q->enabled && !drums) {
        const int mode = seq_action == SEQ_ARTICULATE ? MD_EXPRESSION_GATE : MD_EXPRESSION_ACCENT;
        const unsigned first = q->edit & ~15u;
        vg_text(mode == MD_EXPRESSION_GATE ? "GATE LENGTH" : "VELOCITY", 26, 134, VG_SMALL, dim_text, VG_LEFT);
        vg_text("BEFORE", 300, 134, VG_SMALL, vg_mix(glass, ink, .45f), VG_LEFT);
        vg_text("AFTER", 360, 134, VG_SMALL, ink, VG_LEFT);
        for (unsigned i = 0; i < 16 && first + i < q->length; i++) {
            const md_step *step = &q->steps[first + i];
            const int x = 30 + (int)i * 26;
            if (step->note) {
                const int now = mode == MD_EXPRESSION_GATE ? (int)md_step_gate(step) : step->velocity;
                const int after = md_phrase_value(step, first + i, mode, seq_amount);
                vg_rect(x, 238 - now * 80 / 100, 7, now * 80 / 100, vg_mix(glass, ink, .35f));
                vg_rect(x + 9, 238 - after * 80 / 100, 9, after * 80 / 100, ink);
            } else vg_rect(x + 4, 236, 8, 2, 0x2a403d);
            if (first + i == q->edit) vg_rect(x, 242, 18, 2, 0xf2fbf7);
        }
    } else {
        const unsigned clip = md_sequence_clipboard();
        vg_text(seq_action >= SEQ_ACCENT ? "ENABLE A SYNTH SEQUENCE ON SEQUENCE" :
                clip == 0 ? "CLIPBOARD EMPTY" : clip == 1 ? "A STEP IN THE CLIPBOARD" : "A PHRASE IN THE CLIPBOARD", 26, 140, VG_BOLD, 0xf2fbf7, VG_LEFT);
        /* the scope as a strip: one step or the whole phrase */
        const int len = q->length ? q->length : 16;
        for (int i = 0; i < 32; i++) {
            const int in = i < len, picked = seq_scope ? in : i == q->edit;
            vg_round(26 + i * 13, 190, 10, 28, 3, !in ? 0x142624 : picked ? ink : glass_hi);
        }
    }
    caption(md_journey_active() ? "Stop the journey to edit" : md_record_mode() ? "Turn recording off to edit" : "F1 applies  -  F2 undoes");
}

static void draw_seq_rules(uint32_t ink) {
    panel(ink);
    const md_patch *p = md_state();
    const unsigned fill = md_fill_status();
    const int drums = p->kit.enabled && track == 3;
    const md_sequence *q = &p->sequence[track];
    static const char *const fill_text[] = {"FILL OFF", "FILL PLAYING", "FILL NEXT BAR", "FILL + NEXT BAR"};
    vg_text(fill_text[fill & 3u], 26, 104, VG_BIG, fill ? ink : 0xf2fbf7, VG_LEFT);
    vg_text(drums ? "SELECT A SYNTH TRACK" : q->enabled ? "Rules follow the track's cycle" : "Turn SEQUENCE on to use rules", 26, 134, VG_SMALL, soft_text, VG_LEFT);
    const int len = q->length ? q->length : 16;
    for (int i = 0; i < 32; i++) {
        const int x = 26 + i * 13;
        if (i >= len || drums) { vg_rect(x + 3, 206, 4, 2, 0x22403b); continue; }
        const md_step *st = &q->steps[i];
        const uint32_t c = i == q->edit ? 0xf2fbf7 : st->note ? ink : 0x34514b;
        step_glyph(x, 196, md_step_rule(st), c, 0);
    }
    for (int r = 0; r < 4; r++) {
        step_glyph(30 + r * 108, 230, (unsigned)r, vg_mix(glass, ink, .7f), 0);
        vg_text(seq_rules[r], 46 + r * 108, 232, VG_SMALL, dim_text, VG_LEFT);
    }
}

/* ---- MUTABLE DREAMS FM ------------------------------------------------------------------------------------ */
static void draw_radio(uint32_t ink) {
    panel(ink);
    const float t = seconds_now();
    /* the station's signal: rings leaving the lamp while on air */
    const float lx = 52.f, ly = 132.f;
    for (int r = 0; r < 4 && radio.on; r++) {
        const float age = t * .5f + r * .25f - (int)(t * .5f + r * .25f);
        ring_line(lx, ly, 10.f + age * 46.f, 1.6f, vg_mix(glass, 0xe8705e, 1.f - age), 0.f, 1.f);
    }
    vg_circle(lx, ly, 9.f, radio.on ? 0xe8705e : 0x3c5752);
    vg_text("MUTABLE DREAMS FM", 92, 108, VG_BIG, 0xf2fbf7, VG_LEFT);
    vg_text(radio.on ? "ON AIR" : "OFF AIR - F1 STARTS THE PROGRAMME", 92, 136, VG_BOLD, radio.on ? 0xe8705e : dim_text, VG_LEFT);
    if (radio.on) {
        char line[48];
        radio_line(line, sizeof line);
        vg_text(radio.in_dream ? md_itinera_name((unsigned)radio.world) : md_biome_name(md_state()->biome_id), 26, 176, VG_BIG, ink, VG_LEFT);
        vg_text(line, 26, 204, VG_BOLD, soft_text, VG_LEFT);
        if (!radio.in_dream) vg_text(md_biome_family(md_state()->biome_id), 452, 180, VG_SMALL, dim_text, VG_RIGHT);
        /* the edition: four pieces, this one lit */
        const int stage = (radio.slot + RADIO_SLOTS - 1) % 4;
        for (int i = 0; i < 4; i++)
            vg_round(26 + i * 30, 232, 24, 8, 3, radio.in_dream ? 0x2a403d : i == stage ? ink : i < stage ? vg_mix(glass_hi, ink, .4f) : glass_hi);
        char dream[40] = "ITINERA IN ";
        unsigned k = 11;
        char num[8];
        const int pieces = radio_dreams ? (radio.dream_at - radio.slot + RADIO_SLOTS) % RADIO_SLOTS : -1;
        if (radio.in_dream) { const char *a = "A DREAM IS PLAYING"; k = 0; while (*a) dream[k++] = *a++; dream[k] = 0; }
        else if (pieces < 0) { const char *a = "NO DREAMS IN THIS PROGRAMME"; k = 0; while (*a) dream[k++] = *a++; dream[k] = 0; }
        else { vg_itoa(pieces, num); for (unsigned i = 0; num[i]; i++) dream[k++] = num[i]; const char *a = pieces == 1 ? " PIECE" : " PIECES"; while (*a) dream[k++] = *a++; dream[k] = 0; }
        vg_text(dream, 452, 230, VG_SMALL, dim_text, VG_RIGHT);
    } else {
        vg_text("Editions of four pieces, a new Biome each, the station's tempo", 26, 180, VG_SMALL, soft_text, VG_LEFT);
        vg_text("bands - and every four hours a 30-minute Itinera dream.", 26, 198, VG_SMALL, soft_text, VG_LEFT);
        vg_text("Pads, gesture and REC play along. STOP ends the broadcast.", 26, 222, VG_SMALL, dim_text, VG_LEFT);
        vg_text(radio_chime ? "ID CHIME ON" : "ID CHIME OFF", 452, 244, VG_SMALL, dim_text, VG_RIGHT);
    }
}

/* ---- the directory (MENU) -------------------------------------------------------------------------------- */
/* (0.27, Codex design review) the directory in two banks of 36: three columns of 12 rows, 20 px each – readable on
 * the 4" screen. The bank follows the cursor; a click on the footer turns it. */
enum { MENU_ROWS = 12, MENU_BANK = 36, MENU_Y = 60, MENU_ROW_H = 20 };
static int menu_bank_of(int v) { return v / MENU_BANK; }
static void draw_menu(void) {
    if (device_fb) { nd_pixels(device_fb, 480, 320); nd_glass(6.f, 4.f, 468.f, 312.f, 10.f, ARGB(214, 8, 12, 16)); }
    vg_text("ALL PAGES", 14, 9, VG_BIG, 0xe4f1ea, VG_LEFT);
    char idx[12]; unsigned k = 0; char d[4];
    vg_itoa(menu_cursor + 1, d); if (menu_cursor + 1 < 10) idx[k++] = '0'; for (unsigned q = 0; d[q]; q++) idx[k++] = d[q];
    idx[k++] = ' '; idx[k++] = '/'; idx[k++] = ' '; vg_itoa(V_COUNT, d); for (unsigned q = 0; d[q]; q++) idx[k++] = d[q]; idx[k] = 0;
    vg_text(idx, 466, 14, VG_BOLD, 0xadc7bc, VG_RIGHT);
    /* the seven groups as a colour legend, on its own row */
    int lx = 14;
    for (int g = 0; g < G_COUNT; g++) {
        vg_circle((float)(lx + 3), 47.f, 3.f, group_ink[g]);
        lx += 10 + vg_text(group_names[g], lx + 10, 40, VG_SMALL, 0x87a398, VG_LEFT) + 12;
    }
    const int bank = menu_bank_of(menu_cursor);
    for (int v = bank * MENU_BANK; v < V_COUNT && v < (bank + 1) * MENU_BANK; v++) {
        const int at = v - bank * MENU_BANK, col = at / MENU_ROWS, row = at % MENU_ROWS;
        const int x = 12 + col * 156, y = MENU_Y + row * MENU_ROW_H;
        const uint32_t ink = group_ink[views[v].group];
        if (v == menu_cursor) { vg_round(x, y, 144, MENU_ROW_H, 4, 0x183431); vg_rect(x + 3, y + 4, 3, 12, ink); }
        vg_circle((float)(x + 13), (float)(y + 10), 3.f, ink);
        vg_text(views[v].name, x + 22, y + 3, VG_BOLD, v == menu_cursor ? 0xe4f1ea : v == page ? ink : 0xadc7bc, VG_LEFT);
    }
    char foot[64]; k = 0;
    const char *a = bank ? "2/2" : "1/2"; while (*a) foot[k++] = *a++;
    a = "  -  DATA chooses  -  ENTER opens  -  MENU closes"; while (*a) foot[k++] = *a++; foot[k] = 0;
    /* (0.30) during FIRST STEPS the footer carries the tour's line, in the tour's gold */
    if (tour_step >= 0) { vg_round(14, 300, 452, 18, 6, 0x0a1820); vg_text(tour_texts[tour_step].line1, 240, 302, VG_SMALL, 0xf0c38c, VG_CENTER); }
    else vg_text(foot, 240, 302, VG_SMALL, 0x87a398, VG_CENTER);
}

/* ---- the page ---------------------------------------------------------------------------------------------- */
static void draw_view(void) {
    const view_info *vi = &views[page];
    const uint32_t ink = group_ink[vi->group];
    seq_notice_poll();
    /* header: page title and where it sits */
    vg_text(vi->name, 16, 7, VG_BIG, 0xeef8f3, VG_LEFT);
    int n = 0, at = 0;
    for (int v = 0; v < V_COUNT; v++) if (views[v].group == vi->group) { if (v == page) at = n; n++; }
    const int tw = vg_text_width(vi->name, VG_BIG);
    /* (0.27) where the page sits in its group: a lit capsule among small seeds */
    int px = 16 + tw + 12;
    for (int i = 0; i < n; i++) {
        if (i == at) { vg_round(px, 16, 14, 6, 3, ink); px += 18; }
        else { vg_circle((float)(px + 3), 19.f, 2.2f, 0x34514b); px += 9; }
    }
    if (n > 1) {                                       /* (0.27) and the number: page 2 of 7 in this group */
        char cnt[8]; unsigned c = 0; char d[4];
        vg_itoa(at + 1, d); for (unsigned q = 0; d[q]; q++) cnt[c++] = d[q];
        cnt[c++] = '/'; vg_itoa(n, d); for (unsigned q = 0; d[q]; q++) cnt[c++] = d[q]; cnt[c] = 0;
        vg_text(cnt, px + 4, 12, VG_SMALL, 0x6f8f88, VG_LEFT);
    }
    const float now = md_status()->frames / 44100.f;
    char rec_text[32];
    if (rec_label(rec_text)) {
        vg_circle(462.f - vg_text_width(rec_text, VG_BOLD) - 10.f, 20.f, 4.f, rec_state == REC_TAKING ? 0xe8705e : 0xe8a25e);
        vg_text(rec_text, 462, 14, VG_BOLD, rec_state == REC_TAKING ? 0xe8705e : 0xe8a25e, VG_RIGHT);
    } else if (now < depth_hint_until) {
        char hint[24];
        percent_text(depth_hint_which ? "SPACE DEPTH" : "TEXTURE DEPTH", depth_hint_which ? settings.space_depth : settings.texture_depth, hint);
        vg_text(hint, 462, 14, VG_BOLD, colors[depth_hint_which ? ROW_SPACE : ROW_TEXTURE], VG_RIGHT);
    } else if (now < notice_until) {
        vg_text(notice, 462, 14, VG_BOLD, 0xf0cb86, VG_RIGHT);
    } else {
        char status[40];
        unsigned k = 0;
        const char *a = radio.on ? "ON AIR" : md_playing() ? "PLAYING" : "PRESS PLAY";
        while (*a) status[k++] = *a++;
        status[k++] = ' '; status[k++] = ' ';
        const char *t = track_labels[track];
        while (*t) status[k++] = *t++;
        status[k] = 0;
        /* (0.27) a status pill: its LED green while playing, red on air, grey while stopped */
        const int sw = vg_text_width(status, VG_SMALL);
        const uint32_t led = radio.on ? 0xe8705e : md_playing() ? 0x83d2a6 : 0x5f7a74;
        vg_round(464 - sw - 30, 9, sw + 30, 18, 9, vg_mix(0x0d1a1c, led, .16f));
        {   /* (0.32) a flower that opens on every beat – the Brick's BPM flower – instead of a plain LED */
            const md_stats *st = md_status();
            const float beat = md_playing() ? 1.f - (float)(st->step & 3u) / 4.f : .3f;
            const float cx = (float)(464 - sw - 18), cy = 18.f, rr = 2.2f + 1.6f * beat;
            for (int p = 0; p < 5; p++) {
                const float a = (float)p * .2f + (float)(st->step >> 2) * .02f;
                vg_circle(cx + vd_sin(a + .25f) * rr, cy + vd_sin(a) * rr, 1.7f + .6f * beat, vg_mix(0x0d1a1c, led, .5f + .5f * beat));
            }
            vg_circle(cx, cy, 1.5f, 0xf2e6b0);
        }
        vg_text(status, 456, 11, VG_SMALL, vg_mix(0xdcefe7, ink, .35f), VG_RIGHT);
    }
    vf_begin();
    /* the six house groups on K1-K6; DREAM shows as a seventh chip */
    for (int g = 0; g < 6; g++) {
        const int x = 15 + g * 75, on = g == vi->group;
        ui_tab(x, 36, 71, 20, group_ink[g], on, on && page == V_PADSET ? "PADS" : group_names[g]);   /* (0.27) */
        vf_key(VB_K1 + g, x, 36, 71, 20);
        vibe_led_button(VB_K1 + g, group_ink[g], on ? .8f : .15f);
    }
    if (vi->group == G_DREAM) vg_round(15, 57, 446, 2, 1, vg_mix(0x0d1a1c, group_ink[G_DREAM], .7f));   /* DREAM: the 7th group, below the six */
    /* four values */
    for (unsigned i = 0; i < 4; i++) {
        const int x = 17 + 114 * (int)i;
        Knob kb;
        char text[24];
        if (!knob_of(page, i, &kb)) continue;
        /* (0.27) a glass cell: name, value, a meter with a bead (bipolar values fill from the middle) */
        const int cx = 15 + 113 * (int)i;
        const int bipolar = kb.fmt == F_SIGNED || kb.fmt == F_BIPOLAR || kb.fmt == F_PAN;
        /* meters only for values that run continuously (Codex review: not for names, on/off, choices) */
        const int continuous = kb.fmt == F_PCT || kb.fmt == F_PCT100 || kb.fmt == F_INT || kb.fmt == F_SIGNED || kb.fmt == F_BIPOLAR ||
                               kb.fmt == F_BPM || kb.fmt == F_MS || kb.fmt == F_PAN || kb.fmt == F_NOTE || kb.fmt == F_DIV;
        char shown[24];
        {   /* a long name gets "..." rather than running into the next cell; the page shows it in full */
            const char *v = knob_text(&kb, text);
            unsigned q = 0;
            while (v[q] && q < sizeof shown - 1) { shown[q] = v[q]; q++; }
            shown[q] = 0;
            while (q > 3 && vg_text_width(shown, VG_BOLD) > 96) { q--; shown[q] = 0; shown[q - 1] = '.'; shown[q - 2] = '.'; shown[q - 3] = '.'; }
        }
        ui_cell(cx, 60, 110, 35, kb.label, shown, knob_fill(&kb), bipolar && (kb.lo < 0 || kb.fmt == F_PAN), kb.cmd != L_PADSET && continuous, ink);
        (void)x;
        vf_encoder(VE_1 + i, cx - 1, 59, 112, 35);
    }
    switch (vi->kind) {
    case K_ATLAS: atlas(); break;
    case K_LIST: draw_list(ink); break;
    case K_VOICE: draw_voice(ink); break;
    case K_BARS: draw_bars(ink); break;
    case K_MIXER: draw_mixer(ink); break;
    case K_ENV: draw_envelope(ink); break;
    case K_FILTER: draw_filter(ink); break;
    case K_TIDE: draw_tide(ink); break;
    case K_LINKS: draw_links(ink); break;
    case K_DRIFT: draw_drift(ink); break;
    case K_HARMONY: draw_harmony(ink); break;
    case K_CHORDS: draw_chords(ink); break;
    case K_BASS: draw_bass(ink); break;
    case K_PADS: draw_fx(); break;
    case K_SPACE: draw_space(ink); break;
    case K_GRAIN: if (page == V_CLOUD) draw_cloud(ink); else draw_grain(ink); break;   /* (0.27) CLOUD has its own picture */
    case K_RHYTHM: draw_rhythm(ink); break;
    case K_SEQ: draw_sequence(ink); break;
    case K_KIT: draw_kit(ink); break;
    case K_DREAM: draw_dream(ink); break;
    case K_JOURNEY: draw_journey(ink); break;
    case K_BANKS: draw_banks(ink); break;
    case K_SEQGRID: draw_seq_grid(ink); break;
    case K_SEQTOOLS: draw_seq_tools(ink); break;
    case K_SEQRULES: draw_seq_rules(ink); break;
    case K_RADIO: draw_radio(ink); break;
    case K_LISTEN: draw_listen(ink); break;
    case K_SING: draw_sing(ink); break;
    case K_CV: draw_cv(ink); break;
    default: draw_info(ink); break;
    }
    /* soft keys */
    static const char *const moment_labels[6] = {"M1","M2","M3","M4","M5","M6"};
    for (unsigned i = 0; i < 6; i++) {
        const int x = 15 + 75 * (int)i;
        const char *label = "";
        const int kind = view_soft(i, &label);
        char fav[10];
        int active = 0, lit = kind != 0;
        if (page == V_LIVE) { short_name(md_biome_name(favorites[i]), fav, 7); label = fav; active = (int)i == (int)selected; lit = 1; }
        else if (page == V_PADSET) { label = moment_labels[i]; active = (int)i == moment_active; lit = moments[i].used; }
        else if (kind == 3) active = i == track;
        else if (page == V_LISTEN) active = (i == 0 && lis.on) || (i == 3 && md_state()->freeze) || (i == 4 && lis.moves);
        else if (page == V_SING) active = (int)i == lis.sing || (i == 4 && key_armed);
        else if (page == V_CV) active = (int)i == lis.cv_track;
        else if (page == V_MIDI) active = (i == 0 && settings.notes_out) || (i == 1 && !lis.song_free);
        /* (0.27) keys of glass: off (outline only), normal, lit (an LED), active (filled with the group's colour) */
        const int state = !(kind || page == V_PADSET) ? UI_OFF : active ? UI_ACTIVE : (page == V_PADSET && lit) ? UI_LIT : lit ? UI_NORMAL : UI_OFF;
        ui_key(x, 286, 71, 27, 7, ink, state, label, VG_BOLD);
        vibe_led_button(VB_F1 + (int)i, ink, active ? .8f : kind ? .16f : 0.f);
        if (kind || page == V_PADSET) vf_key(VB_F1 + i, x, 287, 71, 26);
    }
}

/* (0.27) HOME: the help, drawn by the garden itself (the SDK list cut the lines off). On top a card for the open
 * page: what it is, the four encoders, the six soft keys; below the keys that work everywhere, one full line each. */
static int help_wrap(const char *text, int x, int y, int w, int font, uint32_t ink, int max_lines) {
    char line[96];
    int lines = 0;
    const char *p = text;
    while (*p && lines < max_lines) {
        unsigned n = 0, cut = 0;
        while (p[n] && n < sizeof line - 1) {
            line[n] = p[n]; line[n + 1] = 0;
            if (vg_text_width(line, font) > w) break;
            if (p[n] == ' ') cut = n;
            n++;
        }
        if (p[n] && cut && lines < max_lines - 1) n = cut;           /* break at the last space that fits */
        line[n] = 0;
        vg_text(line, x, y + lines * (vg_font_height(font) + 2), font, ink, VG_LEFT);
        p += n;
        while (*p == ' ') p++;
        lines++;
    }
    return lines;
}
static int help_page;                                 /* (0.27) the help's page, LEFT / RIGHT */
static void help_page_turn(int d) { help_page = (help_page + 2 + d) % 2; }
static void draw_help(void) {
    const view_info *vi = &views[page];
    const uint32_t ink = group_ink[vi->group];
    if (device_fb) { nd_pixels(device_fb, 480, 320); nd_glass(8.f, 6.f, 464.f, 308.f, 12.f, ARGB(214, 7, 13, 20)); }
    vg_text("HELP", 20, 13, VG_BOLD, ink, VG_LEFT);
    vg_text(vi->name, 20 + vg_text_width("HELP", VG_BOLD) + 10, 7, VG_BIG, 0xeef8f3, VG_LEFT);
    vg_round(372, 11, 90, 18, 9, 0x13272a);
    vg_text("HOME closes", 417, 13, VG_SMALL, 0x9fc2b8, VG_CENTER);
    /* the page: what it holds, its encoders, its soft keys */
    vg_round(16, 36, 448, 112, 9, vg_mix(0x0e1b1e, ink, .07f));
    vg_rect(26, 36, 428, 1, vg_mix(0x0e1b1e, ink, .5f));
    if (help_key_text[0]) {                          /* (0.30) opened by holding an F key: what that key does, first */
        help_wrap(vi->hint, 26, 42, 428, VG_SMALL, 0xadc7bc, 1);
        vg_round(22, 58, 436, 18, 6, vg_mix(0x0e1b1e, 0xf0c38c, .22f));
        help_wrap(help_key_text, 30, 60, 420, VG_BOLD, 0xffe6a0, 1);
    } else help_wrap(vi->hint, 26, 42, 428, VG_BOLD, 0xe0f5ec, 2);
    for (unsigned i = 0; i < 4; i++) {
        Knob kb;
        const int x = 26 + 108 * (int)i;
        char num[2] = {(char)('1' + i), 0};
        vg_circle((float)(x + 7), 87.f, 7.f, vg_mix(0x0e1b1e, ink, .45f));
        vg_text(num, x + 7, 80, VG_SMALL, 0x0a1418, VG_CENTER);
        const char *label = knob_of(page, i, &kb) ? kb.label : "-";
        if (page == V_SYSTEM && i == 2) label = "DUCK RECOV.";
        vg_text(label, x + 19, 80, VG_SMALL, 0xcfe6dd, VG_LEFT);
    }
    for (unsigned i = 0; i < 6; i++) {
        const char *label = "";
        const int kind = view_soft(i, &label);
        char fav[10];
        if (page == V_LIVE) { short_name(md_biome_name(favorites[i]), fav, 7); label = fav; }
        if (page == V_PADSET) {
            static const char *const moment_names[6] = {"M1","M2","M3","M4","M5","M6"};
            label = moment_names[i];
        }
        const int x = 26 + 72 * (int)i;
        const int used = (kind || page == V_LIVE || page == V_PADSET) && *label;
        if (!used) continue;                            /* (0.30) an unassigned key: an empty place, not a "-" */
        ui_key(x, 104, 66, 22, 6, ink, (int)i == help_key_id ? UI_ACTIVE : UI_NORMAL, label, VG_SMALL);
        char fk[3] = {'F', (char)('1' + i), 0};
        vg_text(fk, x + 33, 128, VG_SMALL, 0x6f8f88, VG_CENTER);
    }
    /* everywhere */
    static const char *const rows[][2] = {
        {"COLOUR KEYS", "A group of pages - the same key again: the next page"},
        {"MENU", "All 50 pages. Encoder 1 chooses, ENTER opens"},
        {"HOLD AN F KEY", "Half a second: this help, and the key does nothing"},
        {"PADS 1-16", "Tone, texture, space, form. SHIFT + pad latches it"},
        {"PAD 16", "Gesture: tap, play pads 5-12, tap - your hands loop"},
        {"SHIFT + PAD 15", "A new variation of the Biome. SHIFT + RETURN undoes"},
        {"ENCODERS A / B", "How strongly texture and space pads act"},
        {"REC", "A take as WAV (length on BANKS). REC again ends it"},
        {"SYSTEM F1  /  ?", "FIRST STEPS: the guided tour, from the start"},
    };
    /* (0.27, Codex design review) five rows of 28 px a page, two lines where needed; LEFT / RIGHT turn the page */
    const unsigned total = sizeof rows / sizeof rows[0], per = 5, pages = (total + per - 1) / per;
    if (help_page >= (int)pages) help_page = 0;
    for (unsigned r = (unsigned)help_page * per, i = 0; r < total && i < per; r++, i++) {
        const int y = 152 + 28 * (int)i;
        if (i & 1) vg_round(16, y - 2, 448, 28, 5, 0x0c171a);
        vg_text(rows[r][0], 24, y + 5, VG_BOLD, vg_mix(0xcfe6dd, ink, .45f), VG_LEFT);
        help_wrap(rows[r][1], 140, y - (vg_text_width(rows[r][1], VG_SMALL) > 316 ? 1 : -5), 316, VG_SMALL, 0xb4cfc6, 2);
    }
    char foot[40]; unsigned k = 0; char d[4];
    vg_itoa(help_page + 1, d); for (unsigned q = 0; d[q]; q++) foot[k++] = d[q];
    foot[k++] = '/'; vg_itoa((int)pages, d); for (unsigned q = 0; d[q]; q++) foot[k++] = d[q];
    const char *t = "  -  LEFT / RIGHT turn the page"; while (*t) foot[k++] = *t++; foot[k] = 0;
    vg_text(foot, 240, 296, VG_SMALL, 0x87a398, VG_CENTER);
}
