/* Garden Listens (0.24): the pictures of LISTEN, SING and CV GATE, in the night garden's hand (garden-night.inc).
 * nd_moon_pond is also a place on the stage (PLACES). Included by adapter-check.c before garden-draw.inc. */

/* THE MOON POND (LISTEN): what comes in at IN 1 / IN 2 falls as rain into a pond under the moon. The wave is the
 * input itself; rings spread with its loudness; three streams of light carry it to the cloud, the echo and the room
 * as far as their sends reach. HOLD freezes the pond to ice. */
static void nd_moon_pond(float x, float y, float w, float h, int on, float level, const float *scope, unsigned scope_at,
                         float cloud, float echo, float room, int frozen, float t, uint32_t col) {
    const float mx = x + w * .82f, my = y + h * .2f, mr = h * .12f;
    /* (0.26) the moon tells the state: dim while the input is off, bright while listening, a red flare at a peak */
    const int peak = on && level > .7f;
    for (int g = 4; g >= 1; g--) nd_disc(mx, my, mr * (1.f + .35f * (float)g), peak ? ARGB(30 + 8 * (5 - g), 255, 120, 90) :
                                         ARGB((on ? 18 : 6) + 4 * (5 - g), 230, 240, 255));
    nd_disc(mx, my, mr, on ? ARGB(235, 240, 244, 228) : ARGB(120, 150, 160, 170));
    nd_disc(mx - mr * .3f, my - mr * .2f, mr * .22f, ARGB(60, 180, 190, 200));
    const float py = y + h * .62f;                                       /* the water line */
    for (int b = 0; b < 6; b++) nd_rect(x, py + (float)b * h * .06f, w, h * .06f, ARGB(70 + 18 * b, 8, 22 + 4 * b, 40 + 5 * b));
    /* the moon's reflection, broken by the water */
    for (int k = 0; k < 7; k++) {
        const float wob = frozen ? 0.f : 4.f * nd_sinf(t * 2.1f + (float)k * 1.3f) * (.3f + level * 2.f);
        nd_rect(mx - mr * (.9f - .1f * (float)k) + wob, py + 4.f + (float)k * 5.f, mr * (1.8f - .2f * (float)k), 2.f, ARGB(150 - 16 * k, 236, 240, 222));
    }
    if (!on) {
        for (int i = 0; i < 3; i++) nd_disc(x + w * (.2f + .2f * (float)i), py + 10.f + 3.f * nd_sinf(t + (float)i), 2.f, ARGB(120, 180, 210, 220));
        return;
    }
    if (frozen) {                                                        /* (0.26) HOLD: the pond is ice, crystals glint */
        nd_rect(x, py - 2.f, w, 3.f, ARGB(160, 220, 245, 255));
        for (int i = 0; i < 14; i++) {
            const uint32_t hh = (uint32_t)(i + 3) * 2654435761u;
            const float cx = x + w * (float)(hh % 1000u) / 1000.f, cy = py + 6.f + (float)((hh >> 10) % 40u) * h * .006f;
            const float tw = .5f + .5f * nd_sinf(t * 2.f + (float)i);
            nd_line(cx - 4.f, cy, cx + 4.f, cy, 1.f, ARGB((uint32_t)(80.f + 140.f * tw), 235, 250, 255));
            nd_line(cx, cy - 3.f, cx, cy + 3.f, 1.f, ARGB((uint32_t)(60.f + 120.f * tw), 235, 250, 255));
        }
    } else if (level < .004f) {                                          /* (0.26) listening, silence: a still pond, one slow ring */
        const float age = nd_fmodf(t * .25f, 1.f), rad = 6.f + age * 40.f;
        for (int sgm = 0; sgm < 18; sgm++) { const float a0 = (float)sgm * .349f, a1 = a0 + .25f;
            nd_line(x + w * .45f + nd_cosf(a0) * rad, py + 8.f + nd_sinf(a0) * rad * .28f, x + w * .45f + nd_cosf(a1) * rad, py + 8.f + nd_sinf(a1) * rad * .28f, 1.f, ARGB((uint32_t)(90.f * (1.f - age)), 200, 230, 240)); }
    }
    /* the input's loudness over the last moments, as a shimmering band on the water (mirrored below the line) */
    float px = x, pu = py, pd = py;
    for (int i = 0; i < SCOPE; i++) {
        float v = scope[(scope_at + (unsigned)i) % SCOPE];
        v = v > 1.f ? 1.f : v;
        const float wob = frozen ? 0.f : 1.5f * nd_sinf(t * 5.f + (float)i * .7f);
        const float nx = x + w * (float)(i + 1) / (float)SCOPE, nu = py - v * h * .34f - wob, nd = py + v * h * .16f + wob * .5f;
        nd_line(px, pu, nx, nu, 1.8f, frozen ? ARGB(200, 210, 240, 255) : BK_ALPHA(col, 230));
        nd_line(px, pd, nx, nd, 1.2f, frozen ? ARGB(120, 210, 240, 255) : BK_ALPHA(col, 110));
        if (v > .02f && (i & 1)) nd_line(nx, nu + 2.f, nx, nd - 1.f, 1.f, BK_ALPHA(col, (uint32_t)(30.f + 90.f * v)));
        px = nx, pu = nu, pd = nd;
    }
    /* rings where the rain falls */
    for (int r = 0; r < 4; r++) {
        const float age = nd_fmodf(t * (.6f + level) + (float)r * .25f, 1.f);
        const float cx = x + w * (.3f + .13f * (float)r), rad = 4.f + age * (12.f + level * 90.f);
        const uint32_t a = (uint32_t)((1.f - age) * (60.f + level * 400.f));
        for (int s = 0; s < 18; s++) {
            const float a0 = (float)s * .349f, a1 = a0 + .25f;
            nd_line(cx + nd_cosf(a0) * rad, py + 8.f + nd_sinf(a0) * rad * .28f, cx + nd_cosf(a1) * rad, py + 8.f + nd_sinf(a1) * rad * .28f, 1.2f, ARGB(a > 230u ? 230u : a, 200, 230, 240));
        }
    }
    /* three streams of light up to the cloud, the echo and the room */
    const float send[3] = {cloud, echo, room};
    for (int s = 0; s < 3; s++) {
        if (send[s] <= .01f) continue;
        const float ex = x + w * (.14f + .2f * (float)s), ey = y + h * .14f;
        for (int k = 0; k < 6; k++) {
            const float u = nd_fmodf(t * (.25f + .1f * (float)s) + (float)k / 6.f, 1.f);
            const float fx = x + w * .38f + (ex - x - w * .38f) * u + 6.f * nd_sinf(u * 9.f + (float)s), fy = py - (py - ey) * u;
            nd_disc(fx, fy, 1.4f + 2.4f * send[s] * (1.f - u) + level * 3.f, ARGB((uint32_t)(80.f + 150.f * send[s] * (1.f - u * .6f)), 255, 236, 170));
        }
        nd_disc(ex, ey, 3.f + 4.f * send[s], ARGB(200, 255, 230, 160));
    }
}

static void listen_meter(int x, int y, int h, float level, float hold, uint32_t ink) {
    vg_round(x, y, 8, h, 3, glass_hi);
    float v = level * 1.6f; v = v > 1.f ? 1.f : v;
    const int f = (int)(v * (float)h);
    if (f > 0) vg_round(x, y + h - f, 8, f, 3, level > .7f ? 0xe8705e : ink);
    float hv = hold * 1.6f; hv = hv > 1.f ? 1.f : hv;
    vg_rect(x - 2, y + h - (int)(hv * (float)h), 12, 2, 0xe0f5ec);
}

static void draw_listen(uint32_t ink) {
    panel(ink);
    const float t = seconds_now();
    if (device_fb) {
        nd_pixels(device_fb, 480, 320);
        nd_moon_pond(48.f, 104.f, 408.f, 142.f, lis.on, lis_level, lis_scope, lis_scope_at, lis.cloud, lis.echo, lis.room,
                     md_state()->freeze, t, ink | 0xFF000000u);
    }
    listen_meter(26, 110, 130, lis.on ? lis_level : 0.f, lis.on ? lis_peak_hold : 0.f, ink);
    static const char *const names[3] = {"CLOUD", "ECHO", "ROOM"};
    for (int s = 0; s < 3; s++) vg_text(names[s], 48 + (int)(408.f * (.14f + .2f * (float)s)), 106, VG_SMALL, soft_text, VG_CENTER);
    char line[48];
    if (!lis.on) caption("Input off - F1 lets the garden listen to IN 1 / IN 2");
    else {
        unsigned n = 0;
        const char *a = lis.stereo ? "IN 1 + 2  " : "IN 1  ";
        while (*a) line[n++] = *a++;
        a = listen_modes[clampi(lis.mode, 0, 2)];
        while (*a) line[n++] = *a++;
        a = md_state()->freeze ? "  -  HOLD: the cloud keeps it" : "  -  rain into the moon pond";
        while (*a && n < sizeof line - 1) line[n++] = *a++;
        line[n] = 0;
        caption(line);
    }
}

static void draw_sing(uint32_t ink) {
    panel(ink);
    const float t = seconds_now();
    if (device_fb) {
        nd_pixels(device_fb, 480, 320);
        nd_moon_pond(48.f, 104.f, 408.f, 142.f, lis.on, lis_level * .5f, lis_scope, lis_scope_at, 0.f, 0.f, 0.f, 0, t, ink | 0xFF000000u);
    }
    /* the note being sung, large, with a needle for its cents */
    const int heard = lis_note;
    char name[8];
    if (heard >= 0 && lis.on) {
        const char *nn = notes[heard % 12];
        unsigned i = 0;
        while (nn[i]) { name[i] = nn[i]; i++; }
        vg_itoa(heard / 12 - 1, name + i);
    } else { name[0] = '-'; name[1] = 0; }
    vg_text("YOU", 92, 108, VG_SMALL, soft_text, VG_CENTER);                 /* (0.26) what you sing, and what the garden answers */
    vg_text(name, 92, 122, VG_HUGE, heard >= 0 ? 0xf2fbf7 : 0x4f6d67, VG_CENTER);
    {
        char an[8];
        const int ans = lis_answer_glow > 0.f ? lis_answer_note : -1;
        if (ans >= 0) { const char *nn = notes[ans % 12]; unsigned i = 0; while (nn[i]) { an[i] = nn[i]; i++; } vg_itoa(ans / 12 - 1, an + i); }
        else { an[0] = '-'; an[1] = 0; }
        vg_text("GARDEN", 300, 108, VG_SMALL, 0xf0c38c, VG_CENTER);
        vg_text(an, 300, 122, VG_BIG, ans >= 0 ? 0xffe6a0 : 0x4f5a4a, VG_CENTER);
    }
    const float cents = heard >= 0 ? lis_heard.cents : 0.f;
    const float a = cents / 50.f * .9f;                                       /* -0.9 .. +0.9 rad */
    ring_line(92.f, 214.f, 30.f, 1.2f, 0x34514b, -.40f, -.10f);
    vg_line(92.f, 214.f, 92.f + vd_sin(a * .15915f) * 30.f, 214.f - vd_sin(a * .15915f + .25f) * 30.f, 2.f,
            cents > 15.f || cents < -15.f ? 0xe8a25e : ink);
    /* the call: the notes of the phrase as seeds on their contour (0.30: high notes sit higher, a thread joins them);
       in ANSWER a line says whose turn it is - your call, the garden listening for the beat of silence, its reply */
    const int count = phrase_n;
    int lo = 127, hi = 0;
    for (int k = 0; k < count && k < PHRASE; k++) { if (phrase[k] < lo) lo = phrase[k]; if (phrase[k] > hi) hi = phrase[k]; }
    const int span = hi - lo < 5 ? 5 : hi - lo;
    int px = 0, py = 0;
    for (int k = 0; k < PHRASE; k++) {
        const int lit = k < count;
        const int x = 172 + k * 17, y = lit ? 236 - (phrase[k] - lo) * 22 / span : 236;
        const int playing = phrase_play && k == phrase_play - 2;
        if (lit && k) vg_line((float)px, (float)py, (float)x, (float)y, 1.f, phrase_play && k <= phrase_play - 2 ? 0xb89a5e : 0x3f6a60);
        vg_circle((float)x, (float)y, playing ? 6.f : lit ? 4.f : 2.f, playing ? 0xffe6a0 : lit ? (phrase_play && k < phrase_play - 2 ? 0xe0c27e : ink) : 0x2c4642);
        px = x, py = y;
    }
    if (lis.on && lis.sing == SING_ANSWER) {
        const char *turn = phrase_play ? "GARDEN REPLY" : count == 0 ? "YOUR CALL - SING A PHRASE" : (lis_note >= 0 ? "YOUR CALL" : "LISTENING...");
        const uint32_t tc = phrase_play ? 0xffe6a0 : count && lis_note < 0 ? 0xadc7bc : ink;
        vg_round(166, 191, vg_text_width(turn, VG_SMALL) + (count && !phrase_play && lis_note < 0 ? 70 : 12), 16, 5, 0x0a1820);   /* readable over a loud wave */
        vg_text(turn, 172, 194, VG_SMALL, tc, VG_LEFT);
        if (count && !phrase_play && lis_note < 0) {                  /* the beat of silence filling up */
            const float tempo = (float)md_state()->tempo, beat = 60.f / (tempo < 30.f ? 30.f : tempo);
            float f = phrase_clock / beat; f = f < 0.f ? 0.f : f > 1.f ? 1.f : f;
            const int bx = 172 + vg_text_width(turn, VG_SMALL) + 8;
            vg_round(bx, 199, 48, 4, 2, 0x183431);
            if (f > .05f) vg_round(bx, 199, (int)(48.f * f), 4, 2, 0xadc7bc);
        }
    }
    if (lis_answer_glow > 0.f && lis_answer_note >= 0 && device_fb) {
        const float g = lis_answer_glow;
        const float fx = 380.f, fy = 150.f;
        for (int p = 0; p < 6; p++) { const float an = (float)p * 1.047f + t; nd_disc(fx + nd_cosf(an) * 12.f * g, fy + nd_sinf(an) * 12.f * g, 5.f * g, ARGB((uint32_t)(200.f * g), 255, 220, 150)); }
        nd_disc(fx, fy, 6.f, ARGB(240, 255, 246, 210));
    }
    static const char *const how[SING_MODES] = {"F2 FOLLOW, F3 HARMONY, F4 ANSWER - then sing or play into IN 1",
        "FOLLOW: the garden plays what you sing", "HARMONY: a third or a fifth above you", "ANSWER: sing a phrase, stop - the garden replies"};
    vg_text(track_labels[clampi(lis.sing_track, 0, 5)], 452, 110, VG_BOLD, ink, VG_RIGHT);
    caption(how[clampi(lis.sing, 0, SING_MODES - 1)]);
}

static void cv_jack(int x, int y, const char *name, const char *value, int lit, uint32_t ink) {
    vg_circle((float)x, (float)y, 15.f, 0x22383a);
    vg_circle((float)x, (float)y, 11.f, lit ? ink : 0x3c5752);
    vg_circle((float)x, (float)y, 5.f, 0x0a1214);
    vg_text(name, x, y + 22, VG_SMALL, soft_text, VG_CENTER);
    vg_text(value, x, y + 36, VG_BOLD, lit ? 0xe0f5ec : dim_text, VG_CENTER);
}

static void draw_cv(uint32_t ink) {
    panel(ink);
    char v1[16], v2[16];
    /* CV OUT: the note's volts */
    if (lis.cv_track >= 0 && cv_note >= 0) {
        float v = (float)(cv_note - 36 + 12 * lis.cv_octave) / 12.f;
        v = v < 0.f ? 0.f : v > 5.f ? 5.f : v;
        const int mv = (int)(v * 100.f + .5f);
        vg_itoa(mv / 100, v1); unsigned n = 0; while (v1[n]) n++;
        v1[n++] = '.'; v1[n++] = (char)('0' + (mv / 10) % 10); v1[n++] = (char)('0' + mv % 10); v1[n++] = ' '; v1[n++] = 'V'; v1[n] = 0;
    } else { v1[0] = '-'; v1[1] = 0; }
    const float vin = cv_in_volts;
    {
        int mv = (int)((vin < 0.f ? -vin : vin) * 100.f + .5f);
        unsigned n = 0;
        if (vin < -.005f) v2[n++] = '-';
        vg_itoa(mv / 100, v2 + n); while (v2[n]) n++;
        v2[n++] = '.'; v2[n++] = (char)('0' + (mv / 10) % 10); v2[n++] = (char)('0' + mv % 10); v2[n++] = ' '; v2[n++] = 'V'; v2[n] = 0;
    }
    /* (0.26) what the track plays and what goes out: the note, and LIMIT when the jack's 0-5 V cannot carry it */
    if (lis.cv_track >= 0 && cv_note >= 0) {
        char line[40]; unsigned n = 0;
        const char *q = track_labels[clampi(lis.cv_track, 0, 5)]; while (*q) line[n++] = *q++;
        line[n++] = ' '; q = notes[cv_note % 12]; while (*q) line[n++] = *q++;
        char oct[6]; vg_itoa(cv_note / 12 - 1, oct); q = oct; while (*q) line[n++] = *q++;
        line[n] = 0;
        const float want = (float)(cv_note - 36 + 12 * lis.cv_octave) / 12.f;
        vg_text(line, 26, 104, VG_BOLD, ink, VG_LEFT);
        if (want < 0.f || want > 5.f) vg_text("LIMIT - OCTAVE?", 26 + vg_text_width(line, VG_BOLD) + 10, 104, VG_SMALL, 0xe8a25e, VG_LEFT);
    }
    cv_jack(80, 140, "CV OUT", v1, lis.cv_track >= 0, ink);
    if (cv_gate_left > 0.f) { vg_circle(180.f, 140.f, 20.f, vg_mix(0x101c20, ink, .35f)); vg_circle(180.f, 140.f, 17.f, vg_mix(0x101c20, ink, .6f)); }   /* (0.30) the gate flashes */
    cv_jack(180, 140, "GATE OUT", cv_gate_left > 0.f ? "+5 V" : "0 V", cv_gate_left > 0.f, ink);
    cv_jack(280, 140, "CV IN", v2, lis.cv_in > 0, ink);
    /* a little keyboard with the CV note */
    for (int k = 0; k < 24; k++) {
        const int x = 32 + k * 8;
        static const int black[12] = {0,1,0,1,0,0,1,0,1,0,1,0};
        const int on = cv_note >= 0 && lis.cv_track >= 0 && (cv_note % 24) == k;
        vg_rect(x, 206, 7, black[k % 12] ? 14 : 22, on ? ink : black[k % 12] ? 0x1b2b2d : 0x9fb8b0);
    }
    /* CV IN: which macro it moves, and where */
    vg_text(cv_in_names[clampi(lis.cv_in, 0, 4)], 380, 118, VG_BOLD, lis.cv_in ? ink : dim_text, VG_CENTER);
    vg_round(340, 140, 80, 10, 4, glass_hi);
    float f = vin / 5.f; f = f < 0.f ? 0.f : f > 1.f ? 1.f : f;
    if (lis.cv_in) vg_round(340, 140, (int)(80.f * f) + 1, 10, 4, ink);
    if (lis.cv_in) {                                                          /* (0.30) the voltage, where it goes, how far */
        char line[32]; unsigned n = 0;
        const char *q = v2; while (*q && n < 10) line[n++] = *q++;
        line[n++] = ' '; line[n++] = '>'; line[n++] = ' ';
        vg_itoa((int)(f * 100.f + .5f), line + n); while (line[n]) n++;
        line[n++] = ' '; line[n++] = '%'; line[n] = 0;
        vg_text(line, 380, 156, VG_SMALL, 0xadc7bc, VG_CENTER);
    }
    caption(lis.cv_track < 0 ? "CV TRACK chooses which track's notes go out (1 V / oct)" : "C2 = 0 V, 1 V per octave; the gate opens on every note");
}

/* THE SINGING FLOWER (SING, a stage place): a night flower that opens while you sing; its heart takes the colour of
 * the note (the twelve notes round the colour wheel); when the garden answers, pollen flies from it. */
static uint32_t nd_note_col(int note, uint32_t a) {
    static const uint32_t wheel[12] = {0xf0a37c, 0xf0b979, 0xe9cf7a, 0xc8d97c, 0x9fd88f, 0x86d3b0, 0x83c9d6, 0x88afe3,
                                      0x9d9ce6, 0xb894e0, 0xd48fcf, 0xe896a6};
    return BK_ALPHA(wheel[((note % 12) + 12) % 12], a);
}
static void nd_singing_flower(float x, float y, float w, float h, int on, int heard, int answer, float glow, float level, float t, uint32_t col) {
    const float cx = x + w * .5f, ground = y + h * .96f, cy = y + h * .42f;
    /* the stem and two leaves */
    float px = cx, py = ground;
    for (int s = 1; s <= 12; s++) {
        const float f = (float)s / 12.f, nx = cx + 6.f * nd_sinf(f * 3.f + t * .7f) * f, ny = ground - (ground - cy) * f;
        nd_line(px, py, nx, ny, 3.f - 1.4f * f, ARGB(220, 92, 150, 110));
        px = nx, py = ny;
    }
    for (int side = -1; side <= 1; side += 2) {
        const float lx = cx + 2.f * (float)side, ly = ground - (ground - cy) * .35f;
        for (int d = 0; d < 8; d++) { const float f = (float)d / 8.f; nd_disc(lx + (float)side * f * w * .16f, ly - f * h * .08f - nd_sinf(f * 3.1416f) * h * .05f, 4.f * nd_sinf(f * 3.1416f) + 1.f, ARGB(200, 80, 140, 100)); }
    }
    /* the petals: closed buds at rest, open while singing */
    static float open;
    const float want = on ? (heard >= 0 ? 1.f : .25f + level * 2.f) : .1f;
    open += (want - open) * .12f;
    const float r = h * (.12f + .2f * open);
    for (int p = 0; p < 7; p++) {
        const float a = (float)p * .8976f + t * .15f, sp = .35f + .65f * open;
        for (int d = 1; d <= 6; d++) {
            const float f = (float)d / 6.f;
            nd_disc(cx + nd_cosf(a) * r * f * sp, cy + nd_sinf(a) * r * f * sp * .8f - r * (1.f - sp) * f * .6f, r * .28f * nd_sinf(f * 3.1416f) + 1.f,
                    BK_ALPHA(col, (uint32_t)(120 + 90 * open)));
        }
    }
    nd_disc(cx, cy, r * .28f + 2.f, heard >= 0 && on ? nd_note_col(heard, 245) : ARGB(200, 230, 210, 150));
    /* pollen when the garden answers */
    if (glow > 0.f && answer >= 0) for (int k = 0; k < 10; k++) {
        const float a = (float)k * .628f + t, d = (1.f - glow) * w * .4f;
        nd_disc(cx + nd_cosf(a) * d, cy + nd_sinf(a) * d * .7f - (1.f - glow) * h * .2f, 2.5f * glow + .5f, nd_note_col(answer, (uint32_t)(250.f * glow)));
    }
}

/* (0.24) while IN 1 / IN 2 is heard: a rain of light over the garden, as much as the input is loud; the note being
 * sung tints it. Drawn over nd_draw on LIVE and on the stage. */
static void nd_input_rain(float x, float y, float w, float h, float level, int note, float t) {
    if (!lis.on || level < .004f) return;
    float l = level * 2.5f; l = l > 1.f ? 1.f : l;
    const int drops = 6 + (int)(l * 34.f);
    for (int i = 0; i < drops; i++) {
        const uint32_t hh = (uint32_t)(i + 1) * 2654435761u;
        const float sp = .35f + (float)((hh >> 8) % 100u) / 160.f;
        const float u = nd_fmodf(t * sp + (float)(hh % 1000u) / 1000.f, 1.f);
        const float dx = x + w * (float)((hh >> 12) % 1000u) / 1000.f, dy = y + h * u;
        const uint32_t c = note >= 0 ? (nd_note_col(note, 0) & 0xFFFFFFu) : 0xd8ecf4u;
        nd_line(dx, dy, dx - 1.5f, dy + 6.f + 8.f * l, 1.2f, BK_ALPHA(c, (uint32_t)(60.f + 150.f * l * (1.f - u * .5f))));
    }
}
