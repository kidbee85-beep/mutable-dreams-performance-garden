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

static void panel(uint32_t ink) {
    vg_round(BOX_X, BOX_Y, BOX_W, BOX_H, 10, glass);
    vg_rect(BOX_X + 10, BOX_Y, BOX_W - 20, 1, vg_mix(glass, ink, .35f));
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

/* ---- per kind -------------------------------------------------------------------------------------------- */
static void draw_list(uint32_t ink) {
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
        vg_text(md_biome_name(ids[i]), 30, y, near ? VG_BOLD : VG_SMALL,
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
        /* the Biome's seed as a small constellation */
        uint32_t s = card.seed | 1u;
        for (int i = 0; i < 9; i++) {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            vg_circle(320.f + (s % 120), 200.f + ((s >> 8) % 52), 1.5f + ((s >> 16) & 1), vg_mix(glass_hi, ink, .6f));
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
    const float spacing = 20.f + p->delay_ms * .9f, fb = p->delay_fb / 1000.f;
    float h = 90.f;
    for (float x = 40.f; x < 440.f && h > 2.f; x += spacing * .35f) {
        vg_rect((int)x, (int)(236.f - h), 4, (int)h, vg_mix(glass, ink, .25f + h / 140.f));
        h *= .25f + fb * .7f;
    }
    float px = 40.f, py = 236.f;
    const float tail = .2f + p->reverb_time / 1000.f;
    for (int i = 0; i <= 60; i++) {
        const float x = 40.f + i * 6.8f, y = 236.f - 70.f * vd_pow(1.f - i / 60.f, 1.f / tail) * (p->fx_level / 1000.f + .1f);
        if (i) vg_line(px, py, x, y, 2.f, vg_mix(glass, 0xf2fbf7, .55f));
        px = x; py = y;
    }
    caption("Bars: echo repeats · line: room tail");
}

static void draw_grain(uint32_t ink) {
    panel(ink);
    const md_patch *p = md_state();
    uint8_t position[4], window[4];
    md_grain_positions(position, window);
    for (int x = 0; x < 420; x += 3) {
        const float y = vd_sin(x * .021f + seconds_now() * (p->freeze ? 0.f : .3f)) * vd_sin(x * .0047f) * 26.f;
        vg_line(30.f + x, 180.f - y, 30.f + x, 180.f + y, 1.f, vg_mix(glass, soft_text, .35f));
    }
    for (int g = 0; g < 4; g++) {
        if (!window[g]) continue;
        const float x = 30.f + position[g] * 420.f / 255.f, w = 4.f + window[g] * 60.f / 255.f;
        vg_round((int)(x - w / 2), 140, (int)w, 80, 6, vg_mix(glass, ink, .45f));
    }
    caption(p->freeze ? "FREEZE: the tape holds still" : p->reverse ? "Grains play backwards" : "Grain windows on the tape");
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
    caption(k->enabled ? md_kit_name(k->bank) : "Kit off - KIT ON replaces track 4");
}

static void draw_dream(uint32_t ink) {
    panel(ink);
    md_itinera_view it;
    md_itinera_read(&it);
    const float time = seconds_now();
    /* a slow horizon of the chosen dream */
    for (int r = 0; r < 4; r++) {
        float px = 20.f, py = 0.f;
        for (int x = 0; x <= 440; x += 10) {
            const float y = 228.f - r * 12.f + vd_sin(x * .006f + r * .3f + time * .015f * (r + 1) + it_preset * .7f) * (6.f + r * 3.f);
            if (x) vg_line(px, py, 20.f + x, y, 1.6f, vg_mix(glass, ink, .2f + r * .15f));
            px = 20.f + x; py = y;
        }
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
        vg_text("Keys and pads always play; MIDI channels pick the track.", 26, 172, VG_SMALL, soft_text, VG_LEFT);
        vg_round(26, 200, 200, 44, 8, settings.notes_out ? vg_mix(glass_hi, ink, .55f) : glass_hi);
        vg_text("DREAM AS MIDI", 38, 206, VG_BOLD, settings.notes_out ? 0x0a1719 : 0xf2fbf7, VG_LEFT);
        vg_text(settings.notes_out ? "ON - tracks ch 1-6, kit ch 10" : "OFF - F1 turns it on", 38, 224, VG_SMALL, settings.notes_out ? 0x0a1719 : dim_text, VG_LEFT);
        vg_text("Record the generative notes in the DAW", 240, 214, VG_SMALL, dim_text, VG_LEFT);
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
static void draw_menu(void) {
    vg_clear(0x0a1719);
    vg_text("ALL PAGES", 17, 9, VG_BIG, 0xe0f5ec, VG_LEFT);
    /* the seven groups as a colour legend */
    int lx = 140;
    for (int g = 0; g < G_COUNT; g++) {
        vg_circle((float)lx, 20.f, 3.f, group_ink[g]);
        lx += 7 + vg_text(group_names[g], lx + 7, 14, VG_SMALL, dim_text, VG_LEFT) + 7;
    }
    for (int v = 0; v < V_COUNT; v++) {
        /* three columns of 16 (0.16: four columns of 15 put LEARN and MAP past the right edge) */
        const int col = v / 16, row = v % 16;
        const int x = 18 + col * 156, y = 40 + row * 15;
        const uint32_t ink = group_ink[views[v].group];
        if (v == menu_cursor) vg_round(x - 6, y - 1, 150, 15, 5, vg_mix(0x0a1719, ink, .32f));
        vg_circle((float)(x + 2), (float)(y + 6), 3.f, ink);
        vg_text(views[v].name, x + 12, y, VG_SMALL, v == menu_cursor ? 0xf2fbf7 : v == page ? ink : 0xc4dfd2, VG_LEFT);
    }
    vg_text("Encoder 1 or DATA chooses  -  ENTER or F1 opens  -  MENU closes", 240, 294, VG_SMALL, dim_text, VG_CENTER);
}

/* ---- the page ---------------------------------------------------------------------------------------------- */
static void draw_view(void) {
    const view_info *vi = &views[page];
    const uint32_t ink = group_ink[vi->group];
    seq_notice_poll();
    /* header: page title and where it sits */
    vg_text(vi->name, 17, 8, VG_BIG, 0xe0f5ec, VG_LEFT);
    int n = 0, at = 0;
    for (int v = 0; v < V_COUNT; v++) if (views[v].group == vi->group) { if (v == page) at = n; n++; }
    const int tw = vg_text_width(vi->name, VG_BIG);
    for (int i = 0; i < n; i++) vg_circle((float)(17 + tw + 14 + i * 9), 20.f, i == at ? 3.2f : 2.f, i == at ? ink : 0x34514b);
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
        vg_text(status, 462, 14, VG_SMALL, ink, VG_RIGHT);
    }
    vf_begin();
    /* the six house groups on K1-K6; DREAM shows as a seventh chip */
    for (int g = 0; g < 6; g++) {
        const int x = 15 + g * 75, on = g == vi->group;
        vg_round(x, 36, 71, 20, 4, on ? 0x34514b : 0x152c2b);
        vg_text(on && page == V_PADSET ? "PADS" : group_names[g], x + 35, 38, VG_BOLD, on ? group_ink[g] : 0x9abcb2, VG_CENTER);
        vf_key(VB_K1 + g, x, 36, 71, 20);
        vibe_led_button(VB_K1 + g, group_ink[g], on ? .8f : .15f);
    }
    if (vi->group == G_DREAM) vg_round(15, 56, 446, 2, 1, group_ink[G_DREAM]);
    /* four values */
    for (unsigned i = 0; i < 4; i++) {
        const int x = 17 + 114 * (int)i;
        Knob kb;
        char text[24];
        if (!knob_of(page, i, &kb)) continue;
        vg_text(kb.label, x, 62, VG_SMALL, 0xaccbc2, VG_LEFT);
        vg_text(knob_text(&kb, text), x, 76, VG_BOLD, ink, VG_LEFT);
        if (kb.cmd != L_PADSET) {
            vg_rect(x, 92, 104, 2, 0x1d3532);
            vg_rect(x, 92, (int)(104.f * knob_fill(&kb)), 2, vg_mix(0x1d3532, ink, .8f));
        }
        vf_encoder(VE_1 + i, x - 2, 60, 109, 32);
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
    case K_GRAIN: draw_grain(ink); break;
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
        vg_round(x, 287, 71, 26, 5, active ? 0x3b6c60 : lit && page == V_PADSET ? 0x2a4440 : 0x192b2d);
        vg_text(label, x + 35, 293, VG_BOLD, kind || page == V_PADSET ? (lit ? 0xe0f5ec : 0x6f8f88) : 0x54756f, VG_CENTER);
        vibe_led_button(VB_F1 + (int)i, ink, active ? .8f : kind ? .16f : 0.f);
        if (kind || page == V_PADSET) vf_key(VB_F1 + i, x, 287, 71, 26);
    }
}

static void draw_help(void) {
    const view_info *vi = &views[page];
    Knob kb[4];
    int have[4];
    for (unsigned i = 0; i < 4; i++) have[i] = knob_of(page, i, &kb[i]);
    static char enc[64];
    unsigned n = 0;
    for (unsigned i = 0; i < 4; i++) {
        const char *s = have[i] ? kb[i].label : "-";
        while (*s && n < 58) enc[n++] = *s++;
        if (i < 3 && n < 60) { enc[n++] = ' '; enc[n++] = '/'; enc[n++] = ' '; }
    }
    enc[n] = 0;
    const char *const rows[] = {
        "THIS PAGE", vi->hint,
        "ENCODERS 1-4", enc,
        "COLOUR KEYS", "SOUND ENV LFO HARMONY FX SEQ - again: next page",
        "MENU", "All 47 pages, also DREAM: Itinera, Journey, Banks",
        "LEFT / RIGHT", "Previous / next page",
        "ENCODER A / B", "Texture depth / space depth",
        "PADS", "Tone, texture, space, form. SHIFT + pad latches",
        "PAD 16", "Gesture: tap, play 5-12, it loops. Tap: stop. SHIFT: layer",
        "SHIFT + PAD 15", "A new variation of the Biome. SHIFT + RETURN undoes",
        "REC", "Record a take as WAV, 30-120 s (BANKS). REC again ends it",
        "MIDI PAGE F1", "Dream as MIDI: every note out on channels 1-6, kit 10",
        "RADIO PAGE F1", "Mutable Dreams FM: the station's programme on air",
        "PADS PAGE", "F1-F6 moments, SHIFT + encoders: tone mode, speed",
        "PLAY / STOP", "Start or stop. STOP lets go of everything",
        "PEDAL", "Switch: gesture - expression: space",
        "MIDI PRESSURE", "Channel 10, notes 36-51",
    };
    vg_controls(vi->name, rows, (int)(sizeof rows / sizeof rows[0] / 2));
}
