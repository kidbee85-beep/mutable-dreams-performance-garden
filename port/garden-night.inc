/* SPDX-License-Identifier: GPL-3.0-only
 * The night garden (0.19), from Mutable Dreams for Trimui Brick (brick_garden.inc, brick_views_draw.inc), by Kid Bunka.
 * The same picture language as the Brick and the MPC plugin: each Biome its own sky, land and sign in the sky
 * (rule: bh = (biome + 7) * 2654435761), each track a plant whose species follows its sound model (14 kinds), the
 * macros as weather (DENSITY fireflies, MOTION wind, COLOUR the glow, SPACE mist and the pond), the moon crossing
 * with the bar, a butterfly over the selected track, every hit a spark and a ring on the pond. On VIBE FM the pads
 * add their own weather: FOREST gusts, AIR veils, WATER rain on the pond, STARDUST sparks, BLOOM a light opening,
 * ECHO rings that repeat, GRAIN spores, DUSK the sky darkening.
 *
 * The drawing code is the Brick's, in its own units (a garden 720 high, as wide as the place it is drawn in);
 * nd_* below rasterise it with soft, translucent edges into an RGB565 picture at any size: the stage (869 x 400
 * pixels), the device's display (452 x 178) or the Link screen. The sky is kept and drawn anew five times a second. */

#define ARGB(a, r, g, b) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define BK_ALPHA(c, a) (((c) & 0xFFFFFFu) | ((uint32_t)((a) > 255u ? 255u : (a)) << 24))
#define BK_INK ARGB(255, 255, 252, 244)
#define BK_DIM ARGB(255, 178, 188, 200)
#define BK_FAINT ARGB(255, 118, 128, 146)
enum { SP_TULIP, SP_FERN, SP_BELLS, SP_DANDELION, SP_REEDS, SP_MUSHROOM, SP_THISTLE, SP_GRASS, SP_SUNFLOWER, SP_LANTERN,
       SP_LOTUS, SP_SPIRAL, SP_CRYSTAL, SP_ORCHID };

/* ---- maths without libm: the SDK's sine in turns ---- */
static float nd_sinf(float x) { return vd_sin(x * .15915494f); }
static float nd_cosf(float x) { return vd_sin(x * .15915494f + .25f); }
static float nd_fabsf(float x) { return x < 0.f ? -x : x; }
static int nd_absi(int x) { return x < 0 ? -x : x; }
static float nd_fmodf(float x, float m) { return x - m * (float)(int)(x / m) - (x < 0.f && (x - m * (float)(int)(x / m)) != 0.f ? -m : 0.f); }
static float nd_floorf(float x) { const float i = (float)(int)x; return x < i ? i - 1.f : i; }
static float nd_expf(float x) { return vd_exp2(x * 1.4426950f); }
static uint32_t gd_mix(uint32_t a, uint32_t b, float k) {
    k = k < 0 ? 0 : k > 1 ? 1 : k;
    return 0xFF000000u | (uint32_t)((float)(a >> 16 & 255u) * (1 - k) + (float)(b >> 16 & 255u) * k) << 16 |
           (uint32_t)((float)(a >> 8 & 255u) * (1 - k) + (float)(b >> 8 & 255u) * k) << 8 |
           (uint32_t)((float)(a & 255u) * (1 - k) + (float)(b & 255u) * k);
}
/* the design tokens' six track colours (Visual Kit) */
static uint32_t nd_track_col(int i) {
    static const uint32_t c[6] = {0xaecfb6, 0xdcc694, 0x86c2b4, 0xa5bed9, 0xc8b1d8, 0xcca091};
    return 0xFF000000u | c[((i % 6) + 6) % 6];
}

/* ---- the target: an RGB565 picture, a place in it, and garden units -> pixels ---- */
static uint16_t *nd_fb;                          /* the picture */
static int nd_fw, nd_fh;                         /* its size */
static int nd_cx0, nd_cy0, nd_cx1, nd_cy1;       /* the garden's place in it (pixels, clip) */
static float nd_s;                               /* pixels per garden unit */
static float GD_W, GD_H = 720.f, GD_HORIZON = 520.f;   /* the garden in its own units (as wide as the place) */
#define GD_X 0
#define GD_Y 0
static float gd_k = 1.0f;
/* one sky drawn anew per frame at most (0.21): the others keep their last picture, or a plain night until their turn */
static int nd_sky_budget = 1;
static void nd_frame_begin(int skies) { nd_sky_budget = skies; }

static inline void nd_blend(int x, int y, uint32_t col, float cover) {
    if (x < nd_cx0 || y < nd_cy0 || x >= nd_cx1 || y >= nd_cy1 || cover <= 0.f) return;
    const float a = cover * (float)(col >> 24) * (1.f / 255.f);
    if (a <= .004f) return;
    uint16_t *p = &nd_fb[y * nd_fw + x];
    const uint32_t d = *p;
    const int dr = (int)((d >> 11) & 31u) << 3, dg = (int)((d >> 5) & 63u) << 2, db = (int)(d & 31u) << 3;
    const int sr = (int)(col >> 16 & 255u), sg = (int)(col >> 8 & 255u), sb = (int)(col & 255u);
    const int r = dr + (int)((float)(sr - dr) * a), g = dg + (int)((float)(sg - dg) * a), b = db + (int)((float)(sb - db) * a);
    *p = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
static void nd_disc(float x, float y, float r, uint32_t col) {
    const float X = (float)nd_cx0 + x * nd_s, Y = (float)nd_cy0 + y * nd_s;
    float R = r * nd_s;
    float fade = 1.f;
    if (R < .7f) { fade = R / .7f; R = .7f; }                /* (tiny dots: smaller by light, not by pixels) */
    int x0 = (int)(X - R - 1.f), x1 = (int)(X + R + 1.f), y0 = (int)(Y - R - 1.f), y1 = (int)(Y + R + 1.f);
    if (x0 < nd_cx0) x0 = nd_cx0;
    if (y0 < nd_cy0) y0 = nd_cy0;
    if (x1 >= nd_cx1) x1 = nd_cx1 - 1;
    if (y1 >= nd_cy1) y1 = nd_cy1 - 1;
    const float outer = (R + .5f) * (R + .5f), inner = R > .5f ? (R - .5f) * (R - .5f) : -1.f;
    for (int py = y0; py <= y1; py++) {
        const float dy = (float)py + .5f - Y;
        for (int px = x0; px <= x1; px++) {
            const float dx = (float)px + .5f - X, d2 = dx * dx + dy * dy;
            if (d2 > outer) continue;
            if (d2 <= inner) { nd_blend(px, py, col, fade); continue; }   /* (0.30) inside: full cover, no root */
            const float c = R + .5f - vd_sqrt(d2);
            nd_blend(px, py, col, (c > 1.f ? 1.f : c) * fade);
        }
    }
}
static void nd_line(float x0, float y0, float x1, float y1, float w, uint32_t col) {
    const float ax = (float)nd_cx0 + x0 * nd_s, ay = (float)nd_cy0 + y0 * nd_s, bx = (float)nd_cx0 + x1 * nd_s, by = (float)nd_cy0 + y1 * nd_s;
    float h = w * nd_s * .5f, fade = 1.f;
    if (h < .5f) { fade = h / .5f; h = .5f; }
    int lx = (int)((ax < bx ? ax : bx) - h - 1.f), hx = (int)((ax > bx ? ax : bx) + h + 1.f);
    int ly = (int)((ay < by ? ay : by) - h - 1.f), hy = (int)((ay > by ? ay : by) + h + 1.f);
    if (lx < nd_cx0) lx = nd_cx0;
    if (ly < nd_cy0) ly = nd_cy0;
    if (hx >= nd_cx1) hx = nd_cx1 - 1;
    if (hy >= nd_cy1) hy = nd_cy1 - 1;
    const float vx = bx - ax, vy = by - ay, ll = vx * vx + vy * vy;
    for (int py = ly; py <= hy; py++)
        for (int px = lx; px <= hx; px++) {
            const float qx = (float)px + .5f - ax, qy = (float)py + .5f - ay;
            float t = ll > 0.f ? (qx * vx + qy * vy) / ll : 0.f;
            t = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
            const float dx = qx - vx * t, dy = qy - vy * t, d2 = dx * dx + dy * dy;
            if (d2 > (h + .5f) * (h + .5f)) continue;
            const float c = h + .5f - vd_sqrt(d2);
            nd_blend(px, py, col, (c > 1.f ? 1.f : c) * fade);
        }
}
static void nd_rect(float x, float y, float w, float h, uint32_t col) {
    int x0 = (int)((float)nd_cx0 + x * nd_s), y0 = (int)((float)nd_cy0 + y * nd_s);
    int x1 = (int)((float)nd_cx0 + (x + w) * nd_s + .999f), y1 = (int)((float)nd_cy0 + (y + h) * nd_s + .999f);
    for (int py = y0; py < y1; py++) for (int px = x0; px < x1; px++) nd_blend(px, py, col, 1.f);
}
static void nd_fill(uint32_t top, uint32_t bottom, float from, float to, float curve) {   /* a vertical gradient (units) */
    const int y0 = (int)((float)nd_cy0 + from * nd_s), y1 = (int)((float)nd_cy0 + to * nd_s);
    for (int py = y0 < nd_cy0 ? nd_cy0 : y0; py < y1 && py < nd_cy1; py++) {
        const float k = (float)(py - y0) / (float)(y1 - y0 > 0 ? y1 - y0 : 1);
        const uint32_t c = gd_mix(top, bottom, vd_pow(k, curve) * .85f);
        /* ordered dither (4 x 4 Bayer): no steps in the night sky on a 16-bit picture */
        static const uint8_t bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
        const int r = (int)(c >> 16 & 255u), g = (int)(c >> 8 & 255u), b = (int)(c & 255u);
        for (int px = nd_cx0; px < nd_cx1; px++) {
            const int d = bayer[(py & 3) * 4 + (px & 3)];
            const int rr = r + (d >> 1) > 255 ? 255 : r + (d >> 1), gg = g + (d >> 2) > 255 ? 255 : g + (d >> 2), bb = b + (d >> 1) > 255 ? 255 : b + (d >> 1);
            nd_fb[py * nd_fw + px] = (uint16_t)(((rr >> 3) << 11) | ((gg >> 2) << 5) | (bb >> 3));
        }
    }
}

typedef struct { float x, foot, h, sway, bloom, lv; uint32_t col; int muted, i; float tipx, tipy; } gd_plant;

/* ---- the plants and the Biome sign: the Brick's code, unchanged but for the names of the drawing calls ---- */
static void gd_stem(gd_plant *pl, float bend, float th)
{
    float px = pl->x, py = pl->foot;
    for (int s = 1; s <= 14; s++) {
        float f = (float)s / 14.0f, nx = pl->x + (bend + pl->sway) * f * f, ny = pl->foot - pl->h * f;
        nd_line(px, py, nx, ny, th * (0.45f + 0.55f * gd_k) * (1.0f - 0.5f * f), pl->col);
        px = nx, py = ny;
    }
    pl->tipx = px, pl->tipy = py;
}
static void gd_leaf(float x, float y, float side, float len, float rise, uint32_t col, float a)
{
    for (int d = 1; d <= 8; d++) {
        float u = (float)d / 8.0f, w = nd_sinf(u * 3.1416f) * (1.0f - 0.3f * u) * len * 0.17f;
        nd_disc(x + side * len * u, y - rise * u * u - len * 0.12f * nd_sinf(u * 3.1416f), w + 0.6f, BK_ALPHA(col, (uint32_t)(255.0f * a)));
    }
}

static void gd_tulip(gd_plant *pl, float t)
{
    float b = pl->bloom, open = pl->muted ? 0 : 0.25f + 0.75f * b;
    gd_stem(pl, 6, 5.0f);
    gd_leaf(pl->x, pl->foot - 10 * gd_k, -1, pl->h * 0.30f, pl->h * 0.55f, pl->col, 0.45f);
    gd_leaf(pl->x, pl->foot - 24 * gd_k, 1, pl->h * 0.26f, pl->h * 0.45f, pl->col, 0.4f);
    for (int k = -1; k <= 1; k++) {               /* a cup of three petals, opening on the hit */
        float a = (float)k * (0.35f + 0.5f * open);
        nd_disc(pl->tipx + nd_sinf(a) * 15.0f * gd_k, pl->tipy - (nd_cosf(a) * 15.0f + 5.0f) * gd_k, (14.0f + 4.0f * b) * gd_k, BK_ALPHA(pl->col, 150 + (uint32_t)(100 * b)));
    }
    nd_disc(pl->tipx, pl->tipy - 6.0f * gd_k, (5.0f + 3.0f * b) * gd_k, pl->muted ? BK_FAINT : BK_INK);
    (void)t;
}

static void gd_fern(gd_plant *pl, float t)
{
    float unfurl = 0.35f + 0.25f * pl->lv + 0.4f * pl->bloom, a = 0, r = 22.0f * gd_k, cx, cy, px, py;
    gd_stem(pl, -10, 5.0f);
    for (int s = 2; s <= 12; s += 2) {           /* leaflets along the stem */
        float f = (float)s / 14.0f, nx = pl->x + (-10 + pl->sway) * f * f, ny = pl->foot - pl->h * f;
        gd_leaf(nx, ny, (s & 2) ? 1 : -1, 26.0f * gd_k * (1.0f - f * 0.6f), 8 * gd_k, pl->col, 0.5f);
    }
    cx = pl->tipx + 16.0f * gd_k, cy = pl->tipy;         /* the koru: a spiral that opens with sound */
    px = pl->tipx, py = pl->tipy;
    for (int k = 0; k < 40; k++) {
        float nx = cx - nd_cosf(a) * r, ny = cy - nd_sinf(a) * r;
        nd_line(px, py, nx, ny, (3.2f * (1.0f - (float)k / 50.0f)) * (0.5f + 0.5f * gd_k), pl->col);
        px = nx, py = ny;
        a += 0.22f;
        r *= 0.97f - 0.04f * (1.0f - unfurl);
    }
    nd_disc(px, py, (3.0f + 4.0f * pl->bloom) * gd_k, pl->muted ? BK_FAINT : BK_INK);
    pl->tipx = cx, pl->tipy = cy - 10 * gd_k;
    (void)t;
}

static void gd_bells(gd_plant *pl, float t)
{
    float px = pl->x, py = pl->foot;
    gd_leaf(pl->x, pl->foot - 6 * gd_k, -1, 40 * gd_k, 10 * gd_k, pl->col, 0.4f);
    for (int s = 1; s <= 14; s++) {               /* an arch */
        float f = (float)s / 14.0f, nx = pl->x + (46.0f * gd_k + pl->sway) * f, ny = pl->foot - pl->h * nd_sinf(f * 2.2f) / nd_sinf(2.2f * 0.75f) * 0.8f;
        nd_line(px, py, nx, ny, 3.6f * (1.0f - 0.5f * f), pl->col);
        if (s == 6 || s == 10 || s == 14) {       /* bells hang from it, swinging on the hit */
            float sw = nd_sinf(t * 6.0f + (float)s) * 10.0f * gd_k * pl->bloom, bx = nx + sw, by = ny + 30.0f * gd_k;
            nd_line(nx, ny, bx, by - 10.0f * gd_k, 1.5f, pl->col);
            for (int k = 0; k < 6; k++)           /* a bell: narrow at the top, wide at the lip */
                nd_disc(bx, by + (-10.0f + (float)k * 3.6f) * gd_k, (4.0f + (float)k * 1.6f + 1.5f * pl->bloom) * gd_k, BK_ALPHA(pl->col, 210));
            nd_line(bx - (13.0f + pl->bloom) * gd_k, by + 9.0f * gd_k, bx + (13.0f + pl->bloom) * gd_k, by + 9.0f * gd_k, 2.5f * gd_k, pl->col);
            nd_disc(bx + sw * 0.4f, by + 14.0f * gd_k, 3.0f * gd_k, pl->muted ? BK_FAINT : BK_INK);
            pl->tipx = bx, pl->tipy = by;
        }
        px = nx, py = ny;
    }
}

static void gd_dandelion(gd_plant *pl, float t)
{
    float r = (28.0f + 12.0f * pl->lv) * gd_k;
    gd_stem(pl, 4, 3.4f);
    gd_leaf(pl->x, pl->foot - 4, 1, 34 * gd_k, 6 * gd_k, pl->col, 0.4f);
    gd_leaf(pl->x, pl->foot - 4, -1, 30 * gd_k, 6 * gd_k, pl->col, 0.4f);
    for (int k = 0; k < 18; k++) {               /* the clock: rays with seeds */
        float a = (float)k * 0.349f + t * 0.1f, rr = r * (0.85f + 0.15f * nd_sinf((float)k * 2.1f));
        float ex = pl->tipx + nd_cosf(a) * rr, ey = pl->tipy + nd_sinf(a) * rr;
        nd_line(pl->tipx, pl->tipy, ex, ey, 1.0f, BK_ALPHA(pl->col, 140));
        nd_disc(ex, ey, (2.0f + 1.2f * pl->bloom) * (0.5f + 0.5f * gd_k), BK_ALPHA(pl->muted ? BK_FAINT : BK_INK, 200));
    }
    nd_disc(pl->tipx, pl->tipy, 4.0f, pl->col);
}

static void gd_reeds(gd_plant *pl, float t)
{
    float base = pl->x;
    for (int k = 0; k < 5; k++) {                /* blades, bending in the wind */
        float lean = (float)(k - 2) * 9.0f * gd_k, hh = pl->h * (0.55f + 0.1f * (float)((k * 7) % 4)), px = base + (float)(k - 2) * 5.0f * gd_k, py = pl->foot;
        for (int s = 1; s <= 10; s++) {
            float f = (float)s / 10.0f, nx = base + (float)(k - 2) * 5.0f * gd_k + (lean + pl->sway * 1.3f) * f * f, ny = pl->foot - hh * f;
            nd_line(px, py, nx, ny, 3.0f * (1.0f - 0.8f * f) + 0.5f, BK_ALPHA(pl->col, 170));
            px = nx, py = ny;
        }
    }
    gd_stem(pl, 3, 2.4f);
    for (int k = 0; k < 6; k++)                   /* the cattail */
        nd_disc(pl->tipx, pl->tipy + (6.0f + (float)k * 4.0f) * gd_k, (5.0f + 1.5f * pl->bloom) * gd_k, BK_ALPHA(gd_mix(pl->col, ARGB(255, 120, 80, 50), 0.5f), 230));
    (void)t;
}

static void gd_mushroom(gd_plant *pl, float t)
{
    static const float off[3] = {-34, 8, 40}, sz[3] = {1.1f, 1.4f, 0.85f};
    for (int m = 0; m < 3; m++) {                /* a little family; they jump on the hit */
        float s = sz[m] * gd_k * (0.7f + 0.5f * pl->lv), jump = pl->bloom * 18.0f * sz[m] * (m == 1 ? 1.0f : 0.6f);
        float x = pl->x + off[m] * gd_k, stem = 40.0f * s, top = pl->foot - stem - jump, squash = 1.0f + 0.25f * pl->bloom;
        nd_line(x, pl->foot, x, top + 6, 9.0f * s, BK_ALPHA(ARGB(255, 230, 222, 205), 230));
        for (int k = -4; k <= 4; k++)             /* the cap: a dome of discs */
            nd_disc(x + (float)k * 5.0f * s * squash, top - (16.0f - (float)(k * k)) * s * 0.8f, 9.0f * s, pl->col);
        for (int d = 0; d < 3; d++)
            nd_disc(x + (float)(d - 1) * 9.0f * s, top - 8.0f * s, 2.2f * s, BK_ALPHA(BK_INK, 200));
        if (m == 1)
            pl->tipx = x, pl->tipy = top - 14.0f * s;
    }
    (void)t;
}


static void gd_thistle(gd_plant *pl, float t)      /* SNARE: a spiky globe that bristles on the hit */
{
    float r = (13.0f + 4.0f * pl->lv) * gd_k, sp = 1.0f + 0.5f * pl->bloom;
    gd_stem(pl, -5, 4.0f);
    for (int k = 0; k < 4; k++)
        gd_leaf(pl->x, pl->foot - (float)(8 + k * 16) * gd_k, (k & 1) ? 1 : -1, 22 * gd_k, 10 * gd_k, pl->col, 0.45f);
    for (int k = 0; k < 22; k++) {
        float a = (float)k * 0.2856f - 1.57f, len = r * (1.5f + 0.4f * nd_sinf((float)k * 3.7f)) * sp;
        if (nd_sinf(a) > 0.5f)
            continue;
        nd_line(pl->tipx, pl->tipy, pl->tipx + nd_cosf(a) * len, pl->tipy + nd_sinf(a) * len, 1.6f, BK_ALPHA(pl->col, 220));
    }
    nd_disc(pl->tipx, pl->tipy + 3 * gd_k, r, BK_ALPHA(gd_mix(pl->col, ARGB(255, 60, 70, 90), 0.4f), 255));
    nd_disc(pl->tipx - r * 0.3f, pl->tipy, r * 0.35f, BK_ALPHA(BK_INK, 60));
    (void)t;
}

static void gd_grass(gd_plant *pl, float t)        /* HAT: a tuft of fine blades, a spark on each tip */
{
    for (int k = 0; k < 9; k++) {
        float lean = ((float)k - 4.0f) * 6.0f * gd_k, hh = pl->h * (0.5f + 0.5f * nd_fabsf(nd_sinf((float)k * 1.7f + (float)pl->i)));
        float px = pl->x + ((float)k - 4.0f) * 3.0f * gd_k, py = pl->foot, nx = px, ny = py;
        for (int s = 1; s <= 8; s++) {
            float f = (float)s / 8.0f;
            nx = pl->x + ((float)k - 4.0f) * 3.0f * gd_k + (lean + pl->sway * 1.6f + nd_sinf(t * 3.0f + (float)k) * 2.0f) * f * f;
            ny = pl->foot - hh * f;
            nd_line(px, py, nx, ny, 2.2f * (1.0f - 0.7f * f) + 0.4f, BK_ALPHA(pl->col, 190));
            px = nx, py = ny;
        }
        if (pl->bloom > 0.1f && (k % 2 == 0))
            nd_disc(nx, ny, 2.0f + 2.5f * pl->bloom, BK_ALPHA(BK_INK, (uint32_t)(255 * pl->bloom)));
        if (k == 4)
            pl->tipx = nx, pl->tipy = ny;
    }
}

static void gd_sunflower(gd_plant *pl, float t)    /* BASS: a heavy round head, nodding with the low notes */
{
    float r = (16.0f + 5.0f * pl->lv) * gd_k, nod = 6.0f * pl->bloom * gd_k;
    gd_stem(pl, 10, 6.0f);
    gd_leaf(pl->x, pl->foot - 20 * gd_k, -1, 38 * gd_k, 14 * gd_k, pl->col, 0.5f);
    gd_leaf(pl->x, pl->foot - 40 * gd_k, 1, 34 * gd_k, 12 * gd_k, pl->col, 0.45f);
    for (int k = 0; k < 14; k++) {
        float a = (float)k * 0.4488f + t * 0.05f;
        nd_disc(pl->tipx + nd_cosf(a) * r * 1.25f, pl->tipy + nod + nd_sinf(a) * r * 1.25f, r * 0.42f, BK_ALPHA(pl->col, 200));
    }
    nd_disc(pl->tipx, pl->tipy + nod, r, gd_mix(pl->col, ARGB(255, 60, 40, 30), 0.6f));
    for (int k = 0; k < 8; k++) {                 /* its seeds in a spiral */
        float a = (float)k * 2.3999f, rr = r * 0.8f * vd_sqrt((float)k / 8.0f);
        nd_disc(pl->tipx + nd_cosf(a) * rr, pl->tipy + nod + nd_sinf(a) * rr, 1.6f * gd_k + 0.6f, BK_ALPHA(BK_INK, 150));
    }
}

static void gd_lantern(gd_plant *pl, float t)      /* FELT, ELEMENTS: paper lanterns that glow on the hit */
{
    float px = pl->x, py = pl->foot;
    for (int s = 1; s <= 12; s++) {               /* a bowing stem */
        float f = (float)s / 12.0f, nx = pl->x + (30.0f * gd_k + pl->sway) * f * f, ny = pl->foot - pl->h * f;
        nd_line(px, py, nx, ny, 3.0f * (1.0f - 0.5f * f), pl->col);
        if (s == 5 || s == 8 || s == 12) {
            float sw = nd_sinf(t * 1.5f + (float)s) * 3.0f * gd_k, lx = nx + sw, ly = ny + 18.0f * gd_k;
            float g = pl->muted ? 0.0f : 0.3f + 0.7f * pl->bloom, rr = (8.0f + (float)s * 0.4f) * gd_k;
            nd_line(nx, ny, lx, ly - rr, 1.2f, pl->col);
            nd_disc(lx, ly, rr * 2.2f, BK_ALPHA(ARGB(255, 255, 200, 120), (uint32_t)(40 * g)));
            for (int k = -1; k <= 1; k++)         /* ribbed */
                nd_disc(lx + (float)k * rr * 0.45f, ly, rr * (1.0f - 0.25f * nd_fabsf((float)k)), BK_ALPHA(gd_mix(pl->col, ARGB(255, 255, 190, 110), g), 230));
            nd_disc(lx, ly, rr * 0.35f, BK_ALPHA(ARGB(255, 255, 240, 190), (uint32_t)(255 * g)));
            pl->tipx = lx, pl->tipy = ly;
        }
        px = nx, py = ny;
    }
}

static void gd_lotus(gd_plant *pl, float t)        /* MELLOW: a lotus on a pad, opening wide on the hit */
{
    float open = pl->muted ? 0.2f : 0.45f + 0.55f * pl->bloom, cx = pl->x, cy = pl->foot - pl->h * 0.35f;
    nd_line(cx, pl->foot, cx, cy, 3.0f, pl->col);
    for (int k = -6; k <= 6; k++)                 /* the pad */
        nd_disc(cx + (float)k * 4.0f * gd_k, pl->foot - 2.0f, (9.0f - nd_fabsf((float)k)) * gd_k + 1.5f, BK_ALPHA(gd_mix(pl->col, ARGB(255, 60, 120, 90), 0.6f), 200));
    for (int ring = 0; ring < 2; ring++)
        for (int k = 0; k < 7; k++) {
            float a = -1.57f + ((float)k - 3.0f) * (0.28f + 0.22f * open) * (ring ? 0.7f : 1.0f);
            float len = (24.0f - (float)ring * 6.0f) * gd_k;
            for (int d = 1; d <= 5; d++) {
                float u = (float)d / 5.0f;
                nd_disc(cx + nd_cosf(a) * len * u, cy + nd_sinf(a) * len * u, (2.0f + 4.0f * nd_sinf(u * 3.14f)) * gd_k + 0.5f,
                        BK_ALPHA(gd_mix(pl->col, BK_INK, (float)ring * 0.3f), 170));
            }
        }
    nd_disc(cx, cy - 3 * gd_k, 4.0f * gd_k, BK_INK);
    pl->tipx = cx, pl->tipy = cy - 20 * gd_k;
    (void)t;
}

static void gd_spiral(gd_plant *pl, float t)       /* SILK, SATIN: a vine curling up in loops */
{
    float px = pl->x, py = pl->foot;
    for (int s = 1; s <= 60; s++) {
        float f = (float)s / 60.0f, a = f * 12.0f + t * 0.4f, rad = (10.0f + 6.0f * pl->bloom) * gd_k * (1.0f - 0.5f * f);
        float nx = pl->x + nd_sinf(a) * rad + pl->sway * f, ny = pl->foot - pl->h * f - nd_cosf(a) * rad * 0.3f;
        nd_line(px, py, nx, ny, 2.6f * (1.0f - 0.6f * f) + 0.5f, BK_ALPHA(pl->col, (uint32_t)(140 + 100 * (nd_cosf(a) > 0))));
        if (s % 12 == 6)
            gd_leaf(nx, ny, nd_sinf(a) > 0 ? 1 : -1, 14 * gd_k, 4 * gd_k, pl->col, 0.5f);
        px = nx, py = ny;
    }
    nd_disc(px, py, (3.5f + 3.0f * pl->bloom) * gd_k + 1, pl->muted ? BK_FAINT : BK_INK);
    pl->tipx = px, pl->tipy = py;
}

static void gd_crystal(gd_plant *pl, float t)      /* COPPER, SPECTRAL: glass shards growing, glinting on the hit */
{
    static const float ang[5] = {-0.45f, -0.18f, 0.0f, 0.2f, 0.42f}, len[5] = {0.55f, 0.8f, 1.0f, 0.7f, 0.5f};
    for (int k = 0; k < 5; k++) {
        float a = ang[k] + pl->sway * 0.004f, L = pl->h * len[k] * (0.85f + 0.15f * pl->lv), w = (7.0f - (float)nd_absi(k - 2)) * gd_k;
        float bx = pl->x + ((float)k - 2.0f) * 5.0f * gd_k, ex = bx + nd_sinf(a) * L, ey = pl->foot - nd_cosf(a) * L;
        for (int d = 0; d <= 10; d++) {            /* a shard: widest low, a point above */
            float u = (float)d / 10.0f;
            nd_disc(bx + (ex - bx) * u, pl->foot + (ey - pl->foot) * u, w * (1.0f - u) + 0.8f,
                    BK_ALPHA(gd_mix(pl->col, BK_INK, 0.25f * u), (uint32_t)(130 + 80 * u)));
        }
        nd_line(bx, pl->foot, ex, ey, 1.0f, BK_ALPHA(BK_INK, 110));
        if (k == 2) {
            pl->tipx = ex, pl->tipy = ey;
            if (pl->bloom > 0.05f) {               /* a glint */
                float g = pl->bloom * 12.0f * gd_k;
                nd_line(ex - g, ey, ex + g, ey, 1.6f, BK_ALPHA(BK_INK, (uint32_t)(255 * pl->bloom)));
                nd_line(ex, ey - g, ex, ey + g, 1.6f, BK_ALPHA(BK_INK, (uint32_t)(255 * pl->bloom)));
            }
        }
    }
    (void)t;
}

static void gd_orchid(gd_plant *pl, float t)       /* CHOIR, VOWEL: an arching spray of open flowers, singing */
{
    float px = pl->x, py = pl->foot;
    gd_leaf(pl->x, pl->foot - 4, -1, 36 * gd_k, 4 * gd_k, pl->col, 0.5f);
    gd_leaf(pl->x, pl->foot - 4, 1, 32 * gd_k, 4 * gd_k, pl->col, 0.5f);
    for (int s = 1; s <= 12; s++) {
        float f = (float)s / 12.0f, nx = pl->x + (-36.0f * gd_k + pl->sway) * f * f, ny = pl->foot - pl->h * (f * 1.15f - 0.25f * f * f);
        nd_line(px, py, nx, ny, 2.4f * (1.0f - 0.5f * f), pl->col);
        if (s >= 6 && s % 2 == 0) {               /* a flower: five petals and a lip, opening as it sings */
            float o = (0.6f + 0.4f * pl->bloom) * gd_k * (0.8f + 0.04f * (float)s), wob = nd_sinf(t * 2.0f + (float)s) * 0.1f;
            for (int k = 0; k < 5; k++) {
                float a = (float)k * 1.2566f + wob;
                nd_disc(nx + nd_cosf(a) * 9.0f * o, ny + nd_sinf(a) * 9.0f * o, 6.0f * o + 0.5f, BK_ALPHA(pl->col, 200));
            }
            nd_disc(nx, ny + 6.0f * o, 5.0f * o + 0.5f, BK_ALPHA(gd_mix(pl->col, BK_INK, 0.5f), 230));
            nd_disc(nx, ny, 2.2f * o + 0.5f, BK_INK);
            pl->tipx = nx, pl->tipy = ny;
        }
        px = nx, py = ny;
    }
}

static void gd_draw_species(gd_plant *pl, int species, float t)
{
    switch (species) {
    case SP_FERN: gd_fern(pl, t); break;
    case SP_BELLS: gd_bells(pl, t); break;
    case SP_DANDELION: gd_dandelion(pl, t); break;
    case SP_REEDS: gd_reeds(pl, t); break;
    case SP_MUSHROOM: gd_mushroom(pl, t); break;
    case SP_THISTLE: gd_thistle(pl, t); break;
    case SP_GRASS: gd_grass(pl, t); break;
    case SP_SUNFLOWER: gd_sunflower(pl, t); break;
    case SP_LANTERN: gd_lantern(pl, t); break;
    case SP_LOTUS: gd_lotus(pl, t); break;
    case SP_SPIRAL: gd_spiral(pl, t); break;
    case SP_CRYSTAL: gd_crystal(pl, t); break;
    case SP_ORCHID: gd_orchid(pl, t); break;
    default: gd_tulip(pl, t);
    }
}

static void nd_biome_sigil(int id, float cx, float cy, float r, uint32_t c, float t)
{
    uint32_t h = (uint32_t)(id + 11) * 2654435761u, kind = (h >> 5) % 6u, parts = 3u + (h >> 9) % 6u;
    uint32_t c2 = gd_mix(c, nd_track_col((int)((h >> 13) % 6u)), 0.5f);
    float rot = (float)((h >> 17) % 628u) / 100.0f + t * 0.15f, k;
    switch (kind) {
    case 0:                                       /* a bloom: parts petals round a heart */
        for (uint32_t i = 0; i < parts; i++) {
            float a = rot + (float)i * 6.283f / (float)parts;
            nd_disc(cx + nd_cosf(a) * r * 0.5f, cy + nd_sinf(a) * r * 0.5f, r * 0.36f, BK_ALPHA(c, 200));
        }
        nd_disc(cx, cy, r * 0.25f, c2);
        break;
    case 1: {                                     /* a spiral, parts turns */
        float px = cx, py = cy;
        for (int i = 1; i <= 30; i++) {
            float u = (float)i / 30.0f, a = rot + u * (float)parts * 1.8f, rr = u * r;
            float nx = cx + nd_cosf(a) * rr, ny = cy + nd_sinf(a) * rr;
            nd_line(px, py, nx, ny, 2.2f * (1.0f - 0.5f * u), c);
            px = nx, py = ny;
        }
        nd_disc(px, py, r * 0.15f, c2);
        break;
    }
    case 2:                                       /* crystals: parts shards from a point */
        for (uint32_t i = 0; i < parts; i++) {
            float a = -1.57f + ((float)i - (float)(parts - 1) / 2.0f) * 0.35f, L = r * (0.6f + 0.4f * (float)((h >> (i + 3)) & 1u));
            for (int d = 0; d <= 6; d++) {
                float u = (float)d / 6.0f;
                nd_disc(cx + nd_cosf(a) * L * u, cy + r * 0.6f + nd_sinf(a) * L * u * 1.4f, r * 0.14f * (1.0f - u) + 0.6f, BK_ALPHA(i & 1 ? c2 : c, 210));
            }
        }
        break;
    case 3:                                       /* mushrooms: a little group */
        for (uint32_t i = 0; i < (parts > 4 ? 3u : 2u); i++) {
            float mx = cx + ((float)i - 1.0f) * r * 0.55f, hh = r * (0.5f + 0.3f * (float)((h >> (i * 2)) & 1u)), cr = r * (0.42f - 0.08f * (float)i);
            nd_line(mx, cy + r * 0.7f, mx, cy + r * 0.7f - hh, 2.0f, ARGB(230, 230, 220, 200));
            for (int q = -2; q <= 2; q++)
                nd_disc(mx + (float)q * cr * 0.35f, cy + r * 0.7f - hh - (2.0f - nd_fabsf((float)q)) * cr * 0.22f, cr * 0.5f, i & 1 ? c2 : c);
        }
        break;
    case 4: {                                     /* a frond: a curved spine with paired leaflets */
        float px = cx - r * 0.2f, py = cy + r;
        for (int i = 1; i <= 8; i++) {
            float u = (float)i / 8.0f, nx = cx - r * 0.2f + nd_sinf(u * 2.0f + rot * 0.2f) * r * 0.5f, ny = cy + r - u * r * 1.9f;
            nd_line(px, py, nx, ny, 2.0f, c);
            k = r * 0.35f * (1.0f - u * 0.6f);
            nd_disc(nx - k, ny + 2, k * 0.45f, BK_ALPHA(c2, 200));
            nd_disc(nx + k, ny + 2, k * 0.45f, BK_ALPHA(c2, 200));
            px = nx, py = ny;
        }
        break;
    }
    default:                                      /* star-moss: rays and dots */
        for (uint32_t i = 0; i < parts * 2u; i++) {
            float a = rot + (float)i * 3.1416f / (float)parts, L = r * ((i & 1) ? 0.55f : 0.95f);
            nd_line(cx, cy, cx + nd_cosf(a) * L, cy + nd_sinf(a) * L, 1.6f, BK_ALPHA(c, 210));
            nd_disc(cx + nd_cosf(a) * L, cy + nd_sinf(a) * L, (i & 1) ? 1.8f : 2.6f, c2);
        }
        nd_disc(cx, cy, r * 0.2f, c);
    }
}

/* ---- which plant: the species follows the track's sound model (the Brick's gd_species) ---- */
static int gd_species(int track) {
    const md_patch *p = md_state();
    if (p->kit.enabled && track == 3) return SP_MUSHROOM;
    switch (p->track[track].model) {
    case MD_KICK: case MD_SKIN: return SP_MUSHROOM;
    case MD_SNARE: return SP_THISTLE;
    case MD_HAT: return SP_GRASS;
    case MD_FM: return SP_FERN;
    case MD_COPPER: case MD_SPECTRAL: return SP_CRYSTAL;
    case MD_RINGS: case MD_CEDAR: return SP_BELLS;
    case MD_FELT: case MD_ELEMENTS: return SP_LANTERN;
    case MD_HARMONIC: case MD_BREATH: case MD_AERO: return SP_DANDELION;
    case MD_CHOIR: case MD_VOWEL: return SP_ORCHID;
    case MD_REED: case MD_BOWED: case MD_VELVET_KEYS: return SP_REEDS;
    case MD_SILK: case MD_SATIN: return SP_SPIRAL;
    case MD_MELLOW: return SP_LOTUS;
    case MD_BASS: return SP_SUNFLOWER;
    default: return SP_TULIP;
    }
}

/* ---- one place the garden is drawn in: its kept sky, its sparks and rings ---- */
#define ND_SPARKS 64
#define ND_RIPPLES 20
typedef struct {
    uint16_t *sky;                               /* the kept sky (w x h) */
    int w, h, sky_ok, sky_biome, sky_dusk;
    float sky_t, last_t;
    struct { float x, y, vx, vy, life, size; uint32_t col; } spark[ND_SPARKS];
    int spark_n;
    struct { float x, y, t0; uint32_t col; } rip[ND_RIPPLES];
    int rip_n;
    uint32_t last_ev[6];
    float level[6], bloom[6], sway[6], mute[6], bx, by;
    float every;                                 /* seconds between two skies (0: 0.2) */
    float left, step, grow;                      /* where the plants stand (fractions of the width) and their size */
    int plant_px[6];                             /* (where each plant stands, in pixels: labels go under it) */
    int biome;
    float sprout_t;
} nd_scene;

static void nd_emit(nd_scene *s, float x, float y, float vx, float vy, float size, uint32_t col) {
    const int i = s->spark_n++ % ND_SPARKS;
    s->spark[i].x = x, s->spark[i].y = y, s->spark[i].vx = vx, s->spark[i].vy = vy;
    s->spark[i].life = 1.f, s->spark[i].size = size, s->spark[i].col = col;
}
static void nd_ripple(nd_scene *s, float x, float y, float t, uint32_t col) {
    const int i = s->rip_n++ % ND_RIPPLES;
    s->rip[i].x = x, s->rip[i].y = y, s->rip[i].t0 = t, s->rip[i].col = col;
}

/* the Biome drift as the time of day (0 night .. 1 dawn) */
static float gd_dawn(const md_patch *p) {
    if (!p->drift.mode || !p->drift.depth || p->drift.captured != 3) return 0;
    return (float)md_drift_position() / 1000.0f * (float)p->drift.depth / 1000.0f;
}

/* ---- the sky: its own colours, land and sign for each Biome (the Brick's gd_sky); DUSK darkens it ---- */
static void gd_sky(float t, const md_patch *p, float dusk) {
    float colour = (float)p->macro[2] / 1000.0f, space = (float)p->macro[3] / 1000.0f;
    float dawn = gd_dawn(p);
    static const uint32_t SKY[8][2] = {
        {ARGB(255, 4, 6, 16), ARGB(255, 70, 40, 104)},   {ARGB(255, 2, 12, 16), ARGB(255, 16, 96, 92)},
        {ARGB(255, 10, 6, 14), ARGB(255, 130, 64, 48)},  {ARGB(255, 8, 4, 18), ARGB(255, 120, 44, 96)},
        {ARGB(255, 4, 10, 22), ARGB(255, 60, 110, 150)}, {ARGB(255, 3, 12, 8), ARGB(255, 40, 100, 56)},
        {ARGB(255, 12, 8, 6), ARGB(255, 120, 92, 40)},   {ARGB(255, 2, 3, 8), ARGB(255, 34, 46, 90)}};
    uint32_t bh = (uint32_t)(p->biome_id + 7) * 2654435761u, pal = (bh >> 3) % 8, land = (bh >> 9) % 6, extra = (bh >> 15) % 5;
    uint32_t top = gd_mix(SKY[pal][0], ARGB(255, 30, 36, 70), dawn * 0.8f);
    uint32_t glow = gd_mix(SKY[pal][1], colour < 0.5f ? ARGB(255, 20, 70, 86) : ARGB(255, 128, 70, 46), nd_fabsf(colour - 0.5f) * 0.8f);
    glow = gd_mix(glow, ARGB(255, 222, 134, 96), dawn * 0.8f);
    if (p->freeze) glow = gd_mix(glow, ARGB(255, 70, 110, 150), 0.5f);
    glow = gd_mix(glow, top, dusk * 0.85f);                   /* DUSK: the glow sinks */
    nd_fill(top, glow, 0.f, GD_HORIZON, 2.2f);
    for (int x = 0; x < 90; x++) {               /* stars, fewer near the glow */
        uint32_t h = (uint32_t)x * 2654435761u;
        float sx = (float)(h % (uint32_t)GD_W), sy = (float)((h >> 10) % (uint32_t)(GD_HORIZON - 120));
        float tw = 0.5f + 0.5f * nd_sinf(t * (0.6f + (float)(h >> 24 & 7u) * 0.2f) + (float)x);
        nd_disc(sx, sy, 0.7f + (float)(h >> 20 & 3u) * 0.3f,
                BK_ALPHA(ARGB(255, 235, 240, 255), (uint32_t)((40 + 150 * tw * (1.0f - sy / GD_HORIZON)) * (1.0f - 0.85f * dawn) * (1.f + .6f * dusk))));
    }
    {   /* something of its own in the sky */
        float ex = GD_W * .16f + (float)((bh >> 20) % 480u) * GD_W / 720.f, ey = 90.0f + (float)((bh >> 12) % 120u);
        if (extra == 1)
            for (int k = 0; k < 7; k++)
                nd_disc(ex + 60.0f * nd_sinf((float)k * 2.1f), ey + 30.0f * nd_cosf((float)k * 1.7f), 50.0f + (float)(k * 9),
                        BK_ALPHA(gd_mix(glow, ARGB(255, 200, 120, 220), (float)k / 7.0f), 12));
        else if (extra == 2) {
            nd_disc(ex, ey, 16, gd_mix(glow, ARGB(255, 230, 200, 160), 0.5f));
            nd_disc(ex - 5, ey - 5, 7, ARGB(40, 255, 255, 255));
            for (int k = 0; k < 40; k++) {
                float a = (float)k / 40.0f * 6.283f;
                if (nd_sinf(a) > 0 || k % 2 == 0) nd_disc(ex + nd_cosf(a) * 30.0f, ey + nd_sinf(a) * 8.0f, 1.4f, ARGB(170, 230, 220, 200));
            }
        } else if (extra == 3) {
            nd_disc(ex, ey, 9, ARGB(220, 220, 225, 240));
            nd_disc(ex + 3, ey - 2, 8, BK_ALPHA(top, 230));
        } else if (extra == 4)
            for (float x = 0; x < GD_W; x += 6) {
                float u = x / GD_W, ay = ey - 40.0f + 26.0f * nd_sinf(u * 5.0f + t * 0.3f);
                for (int k = 0; k < 5; k++)
                    nd_disc(x, ay + (float)k * 9.0f, 7.0f, BK_ALPHA(gd_mix(ARGB(255, 90, 220, 170), glow, (float)k / 5.0f), (uint32_t)(10 + 4 * (4 - k))));
            }
    }
    const float step = 2.f / nd_s < 2.f ? 2.f : 2.f / nd_s;   /* (a column a pixel wide at most) */
    for (int layer = 0; layer < 3; layer++)       /* its land, far to near */
        for (float x = 0; x < GD_W; x += step) {
            float u = x / 720.f, base = GD_HORIZON - 70.0f + (float)layer * 34.0f, ph = (float)(bh >> 24) * 0.05f, hy;
            switch (land) {
            case 1: hy = base - 40.0f * nd_fabsf(nd_sinf(u * (3.0f + (float)layer) + ph + (float)layer)) + 12.0f * nd_fabsf(nd_sinf(u * 13.0f + ph)) + 20.0f; break;
            case 2: hy = base + 14.0f * nd_sinf(u * (1.4f + 0.5f * (float)layer) + ph + (float)layer * 2.0f) + 10.0f; break;
            case 3: {
                float tri = nd_fabsf(nd_fmodf(u * (34.0f + 10.0f * (float)layer) + ph, 1.0f) - 0.5f) * 2.0f;
                hy = base + 16.0f * nd_sinf(u * 2.5f + (float)layer + ph) - (layer < 2 ? (1.0f - tri) * (26.0f - 6.0f * (float)layer) : 0.0f);
                break;
            }
            case 4: hy = base + 26.0f * nd_floorf(2.0f * nd_sinf(u * (2.5f + (float)layer) + ph + (float)layer)) * 0.5f; break;
            case 5: {
                float sp = nd_sinf(u * (7.0f + 3.0f * (float)layer) + ph * 3.0f);
                hy = base + 12.0f * nd_sinf(u * 3.0f + (float)layer) - (sp > 0.85f ? (sp - 0.85f) * 520.0f : 0.0f);
                break;
            }
            default: hy = base + 22.0f * nd_sinf(u * (2.0f + (float)layer) + (float)layer * 1.7f + ph) + 9.0f * nd_sinf(u * 9.0f + (float)layer);
            }
            if (hy < 40.0f) hy = 40.0f;
            uint32_t c = gd_mix(gd_mix(top, glow, 0.30f - 0.09f * (float)layer), ARGB(255, 6, 10, 18), 0.45f + 0.25f * (float)layer);
            nd_rect(x, hy, step, GD_HORIZON - hy, c);
        }
    for (int b = 0; b < 3 && space > 0.05f; b++) {   /* mist, more with SPACE */
        float my = GD_HORIZON - 60.0f + (float)b * 26.0f;
        for (float x = 0; x <= GD_W; x += 24) {
            float u = x / GD_W, y0 = my + 8.0f * nd_sinf(u * 6.0f + t * 0.2f + (float)b);
            nd_disc(x, y0, 26.0f, BK_ALPHA(ARGB(255, 200, 215, 235), (uint32_t)(10.0f * space)));
        }
    }
    nd_rect(0, GD_HORIZON, GD_W, GD_H - GD_HORIZON, ARGB(255, 5, 9, 16));   /* the pond's bed */
}

/* the pond: the garden mirrored, rippling, clearer with SPACE (in pixels) */
static void nd_pond(float t, float space, float energy) {
    const int hz = nd_cy0 + (int)(GD_HORIZON * nd_s), bottom = nd_cy1, w = nd_cx1 - nd_cx0;
    for (int y = hz; y < bottom; y++) {
        const int d = y - hz, sy = hz - 1 - (int)((float)d * 1.25f);
        if (sy < nd_cy0) continue;
        const float fade = (0.22f + 0.4f * space) * (1.0f - (float)d / (float)(bottom - hz));
        const int shift = (int)(nd_sinf((float)d / nd_s * 0.35f - t * 2.2f) * (1.0f + (float)d * 0.03f) * (1.0f + 3.0f * energy) * nd_s);
        for (int x = 0; x < w; x++) {
            int xs = x + shift;
            xs = xs < 0 ? 0 : xs >= w ? w - 1 : xs;
            const uint16_t c = nd_fb[sy * nd_fw + nd_cx0 + xs];
            nd_blend(nd_cx0 + x, y, 0xFF000000u | ((uint32_t)((c >> 11) & 31u) << 19) | ((uint32_t)((c >> 5) & 63u) << 10) | ((uint32_t)(c & 31u) << 3), fade);
        }
    }
    nd_rect(0, GD_HORIZON, GD_W, 2.f / nd_s, ARGB(60, 200, 220, 255));
}

static void nd_butterfly(float x, float y, float t, uint32_t col) {
    float flap = nd_fabsf(nd_sinf(t * 9.0f)), fx = x + 18.0f * nd_sinf(t * 0.9f), fy = y - 36.0f + 8.0f * nd_sinf(t * 1.7f);
    for (int s = -1; s <= 1; s += 2) {
        nd_disc(fx + (float)s * (6.0f + 4.0f * flap), fy - 3.0f, 6.0f * (0.4f + 0.6f * flap) + 1, BK_ALPHA(col, 220));
        nd_disc(fx + (float)s * (5.0f + 2.0f * flap), fy + 5.0f, 4.0f * (0.4f + 0.6f * flap) + 1, BK_ALPHA(col, 180));
    }
    nd_line(fx, fy - 6, fx, fy + 7, 2.0f, BK_INK);
}

/* ---- the whole garden into a place of a picture. lanes: the eight texture / space lanes as heard (0..1) ---- */
static void nd_draw(nd_scene *s, uint16_t *fb, int fw, int fh, int x, int y, int w, int h, const float lanes[8], int sel, int detail) {
    const md_patch *p = md_state();
    const md_stats *st = md_status();
    const float t = (float)(st->frames % (44100u * 3600u)) / 44100.f;
    float dt = t - s->last_t;
    dt = dt < 0 || dt > .25f ? .033f : dt;
    s->last_t = t;
    nd_fb = fb, nd_fw = fw, nd_fh = fh;
    nd_cx0 = x, nd_cy0 = y, nd_cx1 = x + w, nd_cy1 = y + h;
    nd_s = (float)h / 720.f;
    GD_H = 720.f; GD_HORIZON = 520.f; GD_W = (float)w / nd_s;
    const float forest = lanes[0], air = lanes[1], water = lanes[2], dust = lanes[3];
    const float bloom = lanes[4], echo = lanes[5], grain = lanes[6], dusk = lanes[7];
    const float density = (float)p->macro[0] / 1000.f, motion = (float)p->macro[1] / 1000.f, space = (float)p->macro[3] / 1000.f;
    const int n = md_active_tracks();
    if (p->biome_id != s->biome) {               /* another Biome: its garden sprouts, seeds fly up */
        const int first = s->biome < 0;
        if (!first)
            for (int k = 0; k < 24; k++)
                nd_emit(s, 30.0f + (float)k * GD_W / 26.f, GD_HORIZON - 6, nd_sinf((float)k * 2.3f) * 30.0f, -60.0f - (float)((k * 37) % 50), 2.2f, nd_track_col(k % 6));
        s->biome = p->biome_id;
        s->sprout_t = first ? -10.f : t;            /* (the first Biome stands there already) */
        s->sky_ok = 0;
    }
    float sprout = t - s->sprout_t < 1.8f ? (t - s->sprout_t) / 1.8f : 1.0f;
    sprout = sprout * sprout * (3.0f - 2.0f * sprout);
    /* the sky, kept: drawn anew five times a second (or when the Biome or DUSK changes) */
    const int dq = (int)(dusk * 20.f);
    const int due = !s->sky_ok || t - s->sky_t >= (s->every > 0.f ? s->every : .2f) || t < s->sky_t || dq != s->sky_dusk;
    if (due && !nd_sky_budget && !s->sky_ok) {
        for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) fb[(y + r) * fw + x + c] = 0x0841;   /* (a plain night, one frame) */
    } else if (due && nd_sky_budget) {
        nd_sky_budget--;
        gd_sky(t, p, dusk);
        for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) s->sky[r * w + c] = fb[(y + r) * fw + x + c];
        s->sky_ok = 1, s->sky_t = t, s->sky_dusk = dq;
    } else
        for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) fb[(y + r) * fw + x + c] = s->sky[r * w + c];
    /* BLOOM: a light opening over the garden */
    if (bloom > .01f)                   /* (0.27, Codex design review) finer steps, a smaller reach, a muted warm light: no rings */
        for (int k = 8; k >= 1; k--)
            nd_disc(GD_W * .5f, 270.f, (50.f + 190.f * bloom) * (float)k / 8.f, BK_ALPHA(ARGB(255, 214, 206, 160), (uint32_t)(5.f + 7.f * bloom)));
    {   /* the moon crosses the sky with four bars */
        float u = (float)(st->step % 64u) / 64.0f, a = 3.1416f * (0.1f + 0.8f * u);
        float mx = GD_W * .5f - nd_cosf(a) * GD_W * .42f, my = 340.0f - nd_sinf(a) * 190.0f, dawn = gd_dawn(p);   /* (below the title) */
        uint32_t body = gd_mix(ARGB(255, 250, 246, 228), ARGB(255, 255, 200, 110), dawn);
        nd_disc(mx, my, 54 + 30 * dawn, BK_ALPHA(body, (uint32_t)(8 + 14 * dawn)));
        nd_disc(mx, my, 34 + 10 * dawn, BK_ALPHA(body, (uint32_t)(16 + 20 * dawn)));
        nd_disc(mx, my, 20, BK_ALPHA(body, 245));
        if (dawn < 0.6f) {
            uint32_t sea = ARGB((uint32_t)(40.0f * (1.0f - dawn / 0.6f)), 120, 120, 140);
            nd_disc(mx - 6, my - 5, 4.5f, sea); nd_disc(mx + 7, my + 4, 3.5f, sea); nd_disc(mx + 2, my - 10, 2.5f, sea);
        }
    }
    /* AIR: veils drifting through the sky */
    if (air > .01f)
        for (int i = 0; i < 10; i++) {
            const float span = 140.f + (float)(i % 4) * 70.f, sp = 40.f + (float)i * 9.f;
            const float x0 = nd_fmodf(t * sp + (float)i * 211.f, GD_W + span) - span, yy = 120.f + (float)i * 30.f + 10.f * nd_sinf(t * .3f + (float)i);
            for (float k = 0; k < span; k += 18.f)
                nd_disc(x0 + k, yy + 4.f * nd_sinf(k * .02f + t), 9.f, BK_ALPHA(ARGB(255, 220, 236, 245), (uint32_t)(26.f * air * nd_sinf(3.1416f * k / span))));
        }
    /* the plants: species by model, size by level, place by pan; FOREST adds gusts to the wind */
    gd_plant pl[6];
    for (int i = 0; i < 6; i++) {
        const md_track *tr = &p->track[i];
        const float peak = st->track_peak[i], target = peak > .3f ? 1.f : peak / .3f;
        s->level[i] += (target - s->level[i]) * (1.f - nd_expf(-dt * 6.f));
        if (st->events[i] != s->last_ev[i]) {    /* a hit: it blooms, its stem bends, a spark goes up */
            s->last_ev[i] = st->events[i];
            s->bloom[i] = 1.f;
            s->sway[i] = (i & 1) ? 1.f : -1.f;
        }
        s->bloom[i] *= nd_expf(-dt * 2.2f);
        s->sway[i] *= nd_expf(-dt * 3.f);
        const int muted = i >= n || tr->mute;
        s->mute[i] += ((muted ? 1.f : 0.f) - s->mute[i]) * (1.f - nd_expf(-dt * 2.2f));
        pl[i].i = i;
        pl[i].col = gd_mix(nd_track_col(i), ARGB(255, 70, 78, 92), s->mute[i]);
        pl[i].muted = s->mute[i] > .5f;
        pl[i].x = GD_W * (s->left + s->step * (float)i) + ((float)tr->pan - 500.0f) * 0.05f;
        s->plant_px[i] = nd_cx0 + (int)(pl[i].x * nd_s);
        pl[i].foot = GD_HORIZON - 4.0f - (float)((i * 5) % 3) * 6.0f;
        pl[i].h = (170.0f + 200.0f * (float)tr->level / 1000.0f) * (0.84f + 0.16f * s->level[i]) * (i >= n ? 0.5f : 1.0f) * (1.0f - 0.16f * s->mute[i]);
        float g = sprout * 1.6f - (float)i * 0.1f;
        g = g < 0.02f ? 0.02f : g > 1 ? 1 : g;
        pl[i].h *= g * s->grow;
        const float wind = motion + forest * .8f;
        pl[i].sway = pl[i].muted ? 0 : s->sway[i] * (8.0f + 26.0f * wind) + nd_sinf(t * (0.8f + wind) + (float)i) * 16.0f * wind
                     + forest * 10.f * nd_sinf(t * 3.1f + (float)i * .7f);
        pl[i].sway += s->mute[i] * ((i & 1) ? 22.0f : -22.0f);
        pl[i].bloom = (1.0f - s->mute[i]) * (s->bloom[i] > bloom ? s->bloom[i] : bloom * .8f) * (sprout < 1.f ? sprout : 1.f);
        pl[i].lv = s->level[i];
        if (i >= n) continue;
        gd_draw_species(&pl[i], gd_species(i), t);
        if (s->bloom[i] > .98f && !pl[i].muted && detail) {
            const float note = (float)(st->last_note[i] > 0 ? st->last_note[i] : 60);
            nd_emit(s, pl[i].tipx, pl[i].tipy, nd_sinf(t * 7.0f + (float)i) * 14.0f, -40.0f - (note - 36.0f) * 1.6f, 3.0f, pl[i].col);
            nd_ripple(s, pl[i].x, GD_HORIZON + 22 + (float)((i % 3) * 16), t, pl[i].col);
        }
    }
    /* the butterfly flies to the selected track's plant */
    if (sel >= 0 && sel < n) {
        const float tx = pl[sel].tipx, ty = pl[sel].tipy, k = 1.0f - nd_expf(-dt * 3.5f);
        if (s->bx <= 0) s->bx = tx, s->by = ty;
        const float dist = nd_fabsf(tx - s->bx);
        s->bx += (tx - s->bx) * k;
        s->by += (ty - s->by) * k - dist * 0.04f * k * 6.0f;
        nd_butterfly(s->bx, s->by, t, ARGB(255, 255, 236, 170));
    }
    /* STARDUST: sparks rising from the plants */
    if (dust > .05f && detail)
        for (int i = 0; i < n; i++)
            if ((int)(t * 30.f) % 6 == i && s->level[i] > .1f)
                nd_emit(s, pl[i].tipx, pl[i].tipy, nd_sinf(t * 5.f + (float)i) * 30.f, -50.f - 40.f * dust, 2.f + dust, ARGB(255, 255, 240, 190));
    for (int i = 0; i < ND_SPARKS; i++)
        if (s->spark[i].life > 0) {
            s->spark[i].x += (s->spark[i].vx + 20.0f * motion) * dt;
            s->spark[i].y += s->spark[i].vy * dt;
            s->spark[i].vy *= 1.0f - 0.6f * dt;
            s->spark[i].life -= dt * 0.45f;
            if (s->spark[i].y > 8 && s->spark[i].x > 0 && s->spark[i].x < GD_W) {
                const float L = s->spark[i].life;
                nd_disc(s->spark[i].x, s->spark[i].y, s->spark[i].size * 2.6f, BK_ALPHA(s->spark[i].col, (uint32_t)(40 * L)));
                nd_disc(s->spark[i].x, s->spark[i].y, s->spark[i].size, BK_ALPHA(s->spark[i].col, (uint32_t)(230 * L)));
            }
        }
    /* fireflies: DENSITY (and STARDUST); every bar's beat lights them together */
    {
        const float beat = md_playing() ? nd_expf(-(float)(st->step % 4u) * 1.2f) * .6f : 0.f;
        const int flies = 6 + (int)(density * 40.0f) + (int)(dust * 30.f);
        for (int i = 0; i < flies; i++) {
            uint32_t hh = (uint32_t)i * 2246822519u;
            float fx = (float)(hh % (uint32_t)GD_W) + 30.0f * nd_sinf(t * 0.3f + (float)i);
            float blink = nd_sinf(t * (1.0f + (float)(hh >> 28) * 0.2f) + (float)i);
            float fy = 180.0f + (float)((hh >> 9) % 320u) + 20.0f * nd_sinf(t * 0.5f + (float)i * 1.3f);
            blink = (blink > 0 ? blink * 0.65f : 0) + beat;
            blink = blink > 1 ? 1 : blink;
            if (blink > 0.2f) {
                nd_disc(fx, fy, 6.0f, BK_ALPHA(ARGB(255, 210, 255, 150), (uint32_t)(30 * blink)));
                nd_disc(fx, fy, 1.8f, BK_ALPHA(ARGB(255, 230, 255, 190), (uint32_t)(230 * blink)));
            }
        }
    }
    /* GRAIN: spores drifting up from the meadow */
    if (grain > .01f)
        for (int i = 0; i < (int)(50.f * grain); i++) {
            uint32_t hh = (uint32_t)(i + 3) * 2654435761u;
            const float u = nd_fmodf(t * (.05f + (float)(hh >> 27) * .01f) + (float)(hh % 1000u) / 1000.f, 1.f);
            const float sx = (float)(hh % (uint32_t)GD_W) + 20.f * nd_sinf(t + (float)i), sy = GD_HORIZON - u * 380.f;
            nd_disc(sx, sy, 2.2f, BK_ALPHA(ARGB(255, 240, 220, 170), (uint32_t)(200.f * grain * (1.f - u))));
        }
    /* the pond mirrors it; WATER rains on it, ECHO makes every ring come back */
    float energy = 0;
    for (int i = 0; i < n; i++) energy += s->level[i] / 6.f;
    nd_pond(t, space, energy + water * .5f);
    if (water > .05f && detail && (int)(t * 20.f) % 3 == 0)
        nd_ripple(s, (float)((uint32_t)(t * 977.f) % (uint32_t)GD_W), GD_HORIZON + 30.f + (float)((uint32_t)(t * 313.f) % 150u), t, ARGB(255, 190, 225, 245));
    for (int i = 0; i < ND_RIPPLES; i++) {
        const float life = 1.6f + 1.6f * echo;
        for (int e = 0; e <= (echo > .05f ? 2 : 0); e++) {
            float age = t - s->rip[i].t0 - (float)e * .4f, r, a, px = 0, py = 0;
            if (s->rip[i].t0 <= 0 || age < 0 || age > life) continue;
            r = 8.0f + 90.0f * age / 1.6f;
            a = (1.0f - age / life) * 0.7f * (e ? .6f : 1.f);
            for (int k = 0; k <= 28; k++) {
                float an = (float)k * 0.2244f, xx = s->rip[i].x + nd_cosf(an) * r, yy = s->rip[i].y + nd_sinf(an) * r * 0.22f;
                if (k) nd_line(px, py, xx, yy, 1.4f, BK_ALPHA(s->rip[i].col, (uint32_t)(255 * a)));
                px = xx, py = yy;
            }
        }
    }
}

/* the Biome's sign beside its name (stage, Link, the device's LIVE header): in pixels, any size */
static void nd_sigil_px(uint16_t *fb, int fw, int fh, float cx, float cy, float r, int biome, float t) {
    nd_fb = fb, nd_fw = fw, nd_fh = fh;
    nd_cx0 = 0, nd_cy0 = 0, nd_cx1 = fw, nd_cy1 = fh, nd_s = 1.f;
    nd_disc(cx, cy, r * 1.3f, BK_ALPHA(nd_track_col(biome), 40));
    nd_biome_sigil(biome, cx, cy, r, nd_track_col(biome), t);
}

/* ==== 0.20: the night behind everything, glass, and the pads' signs ============================================ */

/* draw straight into a picture's pixels (scale 1, the whole picture) */
static void nd_pixels(uint16_t *fb, int fw, int fh) {
    nd_fb = fb, nd_fw = fw, nd_fh = fh;
    nd_cx0 = 0, nd_cy0 = 0, nd_cx1 = fw, nd_cy1 = fh, nd_s = 1.f;
}

/* glass: a rounded, translucent panel (pixels; col's alpha is the glass's) with a faint light along its top */
static void nd_glass(float x, float y, float w, float h, float r, uint32_t col) {
    int x0 = (int)x, y0 = (int)y, x1 = (int)(x + w), y1 = (int)(y + h);
    if (x0 < nd_cx0) x0 = nd_cx0;
    if (y0 < nd_cy0) y0 = nd_cy0;
    if (x1 > nd_cx1) x1 = nd_cx1;
    if (y1 > nd_cy1) y1 = nd_cy1;
    for (int py = y0; py < y1; py++)
        for (int px = x0; px < x1; px++) {
            /* distance outside the rounded corner */
            float dx = 0.f, dy = 0.f;
            if ((float)px + .5f < x + r) dx = x + r - ((float)px + .5f); else if ((float)px + .5f > x + w - r) dx = ((float)px + .5f) - (x + w - r);
            if ((float)py + .5f < y + r) dy = y + r - ((float)py + .5f); else if ((float)py + .5f > y + h - r) dy = ((float)py + .5f) - (y + h - r);
            float cover = 1.f;
            if (dx > 0.f && dy > 0.f) { const float d = vd_sqrt(dx * dx + dy * dy) - r; cover = d > .5f ? 0.f : d < -.5f ? 1.f : .5f - d; }
            nd_blend(px, py, col, cover);
        }
    for (int px = x0 + (int)r; px < x1 - (int)r; px++) nd_blend(px, y0, ARGB(255, 210, 240, 230), .10f);   /* (the rim) */
}

/* the night behind a whole page: the Biome's sky, its land low on the horizon, dimmed; kept, drawn anew twice a second */
static void nd_backdrop(nd_scene *s, uint16_t *fb, int fw, int fh, int x, int y, int w, int h, float dim) {
    const md_patch *p = md_state();
    const float t = (float)(md_status()->frames % (44100u * 3600u)) / 44100.f;
    nd_fb = fb, nd_fw = fw, nd_fh = fh;
    nd_cx0 = x, nd_cy0 = y, nd_cx1 = x + w, nd_cy1 = y + h;
    nd_s = (float)h / 720.f;
    GD_H = 720.f; GD_HORIZON = 690.f; GD_W = (float)w / nd_s;
    const int due = !s->sky_ok || t - s->sky_t >= 4.f || t < s->sky_t || p->biome_id != s->biome;   /* (the night behind: every 4 s) */
    if (due && !nd_sky_budget && !s->sky_ok) {
        for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) fb[(y + r) * fw + x + c] = 0x0841;
    } else if (due && nd_sky_budget) {
        nd_sky_budget--;
        gd_sky(t, p, 0.f);
        nd_rect(0, 0, GD_W, GD_H, BK_ALPHA(ARGB(255, 3, 6, 12), (uint32_t)(255.f * dim)));
        for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) s->sky[r * w + c] = fb[(y + r) * fw + x + c];
        s->sky_ok = 1, s->sky_t = t, s->biome = p->biome_id;
    } else
        for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) fb[(y + r) * fw + x + c] = s->sky[r * w + c];
    /* a few fireflies, alive between the panels */
    const float density = (float)p->macro[0] / 1000.f;
    for (int i = 0; i < 4 + (int)(10.f * density); i++) {
        const uint32_t hh = (uint32_t)(i + 5) * 2246822519u;
        const float fx = (float)(hh % (uint32_t)GD_W) + 40.f * nd_sinf(t * .25f + (float)i);
        const float fy = 80.f + (float)((hh >> 9) % 560u) + 30.f * nd_sinf(t * .4f + (float)i * 1.3f);
        const float blink = nd_sinf(t * (.9f + (float)(hh >> 28) * .15f) + (float)i);
        if (blink > .3f) {
            nd_disc(fx, fy, 8.f, BK_ALPHA(ARGB(255, 210, 255, 150), (uint32_t)(28.f * blink)));
            nd_disc(fx, fy, 2.2f, BK_ALPHA(ARGB(255, 230, 255, 190), (uint32_t)(220.f * blink)));
        }
    }
}

/* (0.32) each colour group has its own life behind its pages – the Mutable Dreams night, never the same twice.
   Drawn inside the page's glass (garden-draw.inc panel); positions as shares of the glass (W, H in garden units). */
static void nd_ambience_at(int group, float t) {
    const md_stats *st = md_status();
    const float W = GD_W, H = GD_H;
    switch (group) {
    case 1: /* ENV: three slow breaths of light rising from the ground */
        for (int i = 0; i < 3; i++) {
            const float b = .5f + .5f * nd_sinf(t * 1.05f + (float)i * 2.1f);
            nd_disc(W * (.2f + .3f * (float)i), H * .92f, H * (.22f + .08f * b), ARGB((uint32_t)(18.f + 34.f * b), 240, 190, 140));
        }
        break;
    case 2: /* LFO: the wind – thin streaks drifting across, each on its own wave */
        for (int i = 0; i < 6; i++) {
            const float y0 = H * (.12f + .15f * (float)i), sp = W * (.05f + .01f * (float)i);
            const float x0 = nd_fmodf(t * sp + (float)i * W * .17f, W * 1.3f) - W * .15f;
            float px = x0, py = y0 + H * .03f * nd_sinf(t * .8f + (float)i);
            for (int k = 1; k <= 6; k++) {
                const float x = x0 + (float)k * W * .025f, y = y0 + H * .03f * nd_sinf(t * .8f + (float)i + (float)k * .45f);
                nd_line(px, py, x, y, 2.f, ARGB((uint32_t)(34 + 16 * k), 170, 225, 215));
                px = x, py = y;
            }
        }
        break;
    case 3: { /* HARMONY: a constellation; its stars light when their track plays */
        float lx = 0.f, ly = 0.f;
        for (int i = 0; i < 9; i++) {
            const uint32_t h = (uint32_t)(i + 11) * 2654435761u;
            const float x = W * (.05f + .112f * (float)i), y = H * (.03f + (float)(h % 1000u) / 1000.f * .12f) + H * .015f * nd_sinf(t * .2f + (float)i);
            const int tr = i % 6;
            const float lit = st->track_peak[tr] > .02f ? 1.f : .35f + .25f * nd_sinf(t * 1.3f + (float)i);
            if (i) nd_line(lx, ly, x, y, 1.2f, ARGB(45, 190, 195, 245));
            nd_disc(x, y, 4.f + 5.f * lit, ARGB((uint32_t)(60.f + 150.f * lit), 215, 218, 255));
            lx = x, ly = y;
        }
        break; }
    case 4: /* FX: mist drifting over the water, rings where drops fall */
        for (int i = 0; i < 4; i++) {
            const float x = nd_fmodf(t * W * (.012f + .004f * (float)i) + (float)i * W * .3f, W * 1.4f) - W * .2f;
            nd_disc(x, H * (.6f + .1f * (float)(i & 1)), H * .3f, ARGB(20, 190, 220, 235));
        }
        for (int i = 0; i < 3; i++) {
            const float ph = nd_fmodf(t * .35f + (float)i * .33f, 1.f), r = H * (.04f + .3f * ph);
            const float cx = W * (.2f + .3f * (float)i), cy = H * .88f;
            for (int k = 0; k < 18; k++) {
                const float an = (float)k * .349f;
                nd_disc(cx + nd_cosf(an) * r, cy + nd_sinf(an) * r * .28f, 3.f, ARGB((uint32_t)(170.f * (1.f - ph)), 170, 215, 235));
            }
        }
        break;
    case 5: /* SEQ: sixteen lights along the ground, the step that plays now bright */
        for (int i = 0; i < 16; i++) {
            const int on = (int)(st->step & 15u) == i && md_playing();
            const float x = W * (.06f + .88f * (float)i / 15.f), y = H * .97f - H * .02f * nd_sinf((float)i * .21f);
            if (on) nd_disc(x, y, 30.f, ARGB(50, 250, 215, 140));
            nd_disc(x, y, on ? 12.f : 6.f, on ? ARGB(245, 250, 220, 150) : ARGB(110, 240, 205, 140));
        }
        break;
    case 6: /* DREAM: two ribbons of aurora and, now and then, a falling star */
        for (int r = 0; r < 2; r++)
            for (int i = 0; i < 26; i++) {
                const float x = W * (float)i / 25.f, y = H * (.42f + .1f * (float)r) + H * .05f * nd_sinf((float)i * .38f + t * (.25f + .1f * (float)r));
                nd_disc(x, y, H * .05f, r ? ARGB(16, 200, 150, 235) : ARGB(16, 120, 230, 180));
            }
        {   const float ph = nd_fmodf(t * .12f, 1.f);
            if (ph < .12f) { const float u = ph / .12f, x = W * (.2f + .6f * u), y = H * (.05f + .3f * u);
                for (int k = 0; k < 8; k++) nd_disc(x - (float)k * 18.f, y - (float)k * 5.f, 5.f - (float)k * .5f, ARGB((uint32_t)(230 - 26 * k), 255, 245, 220)); }
        }
        break;
    default:  /* SOUND: fireflies, as many as DENSITY asks for */
        for (int i = 0; i < 5 + (int)(8.f * (float)md_state()->macro[0] / 1000.f); i++) {
            const uint32_t hh = (uint32_t)(i + 23) * 2246822519u;
            const float fx = (float)(hh % (uint32_t)W) + W * .04f * nd_sinf(t * .21f + (float)i);
            const float fy = H * (.2f + (float)((hh >> 9) % 1000u) / 1000.f * .7f) + H * .05f * nd_sinf(t * .37f + (float)i * 1.7f);
            const float blink = nd_sinf(t * (.8f + (float)(hh >> 28) * .12f) + (float)i);
            if (blink > .2f) { nd_disc(fx, fy, 14.f, ARGB((uint32_t)(40.f * blink), 210, 255, 150)); nd_disc(fx, fy, 4.f, ARGB((uint32_t)(235.f * blink), 230, 255, 190)); }
        }
        break;
    }
}

/* the sixteen pads' signs (pixels): a small picture of what each pad does */
static void nd_pad_sign(int pad, float cx, float cy, float r, uint32_t col, float lit, float t) {
    const uint32_t c = BK_ALPHA(col, (uint32_t)(150.f + 105.f * lit)), soft = BK_ALPHA(col, (uint32_t)(60.f + 80.f * lit));
    const float w = r * .14f + .8f;
    switch (pad) {
    case 0:  /* ROOT: one note */
        nd_disc(cx - r * .15f, cy + r * .45f, r * .32f, c); nd_line(cx + r * .14f, cy + r * .42f, cx + r * .14f, cy - r * .7f, w, c); break;
    case 1:  /* FIFTH: two notes a fifth apart */
        nd_disc(cx - r * .35f, cy + r * .45f, r * .26f, c); nd_disc(cx + r * .3f, cy - r * .1f, r * .26f, c);
        nd_line(cx - r * .12f, cy + r * .42f, cx - r * .12f, cy - r * .6f, w, soft); nd_line(cx + r * .53f, cy - r * .14f, cx + r * .53f, cy - r * .9f, w, soft); break;
    case 2:  /* THIRD: a triad */
        for (int k = 0; k < 3; k++) nd_disc(cx, cy + r * (.55f - .5f * (float)k), r * .22f, k == 1 ? c : soft); break;
    case 3:  /* NINTH: an open star of notes */
        for (int k = 0; k < 5; k++) { const float a = (float)k * 1.2566f + t * .2f; nd_disc(cx + nd_cosf(a) * r * .55f, cy + nd_sinf(a) * r * .55f, r * .16f, k & 1 ? soft : c); }
        nd_disc(cx, cy, r * .14f, c); break;
    case 4:  /* FOREST: a leaf */
        for (int d = 0; d <= 10; d++) { const float u = (float)d / 10.f; nd_disc(cx - r * .6f + u * r * 1.2f, cy + r * .5f - u * r * 1.0f, nd_sinf(u * 3.1416f) * r * .32f + .6f, c); }
        nd_line(cx - r * .7f, cy + r * .6f, cx + r * .55f, cy - r * .45f, w * .7f, ARGB(200, 20, 34, 30)); break;
    case 5:  /* AIR: three strokes of wind */
        for (int k = 0; k < 3; k++) {
            float px = cx - r * .7f, py = cy - r * .4f + (float)k * r * .4f;
            for (int d = 1; d <= 10; d++) { const float u = (float)d / 10.f, nx = cx - r * .7f + u * r * (1.3f - .2f * (float)k), ny = cy - r * .4f + (float)k * r * .4f + nd_sinf(u * 5.f + t * 2.f + (float)k) * r * .08f; nd_line(px, py, nx, ny, w, k == 1 ? c : soft); px = nx, py = ny; }
        } break;
    case 6:  /* WATER: a drop */
        nd_disc(cx, cy + r * .25f, r * .45f, c);
        for (int d = 0; d < 8; d++) { const float u = (float)d / 8.f; nd_disc(cx, cy + r * .25f - r * .45f - u * r * .5f, r * .45f * (1.f - u), c); } break;
    case 7:  /* STARDUST: a four-point sparkle and specks */
        nd_line(cx, cy - r * .8f, cx, cy + r * .8f, w, c); nd_line(cx - r * .8f, cy, cx + r * .8f, cy, w, c);
        nd_disc(cx, cy, r * .2f, c);
        for (int k = 0; k < 3; k++) nd_disc(cx + r * (.55f - .5f * (float)k), cy - r * (.6f - .55f * (float)k), r * .08f + .5f, soft); break;
    case 8:  /* BLOOM: a flower opening */
        for (int k = 0; k < 6; k++) { const float a = (float)k * 1.0472f + t * .3f; nd_disc(cx + nd_cosf(a) * r * .42f, cy + nd_sinf(a) * r * .42f, r * (.24f + .08f * lit), soft); }
        nd_disc(cx, cy, r * .22f, c); break;
    case 9:  /* ECHO: rings, fading */
        for (int k = 0; k < 3; k++) {
            const float rr = r * (.25f + .3f * (float)k); float px = cx + rr, py = cy;
            for (int d = 1; d <= 24; d++) { const float a = (float)d * .2618f, nx = cx + nd_cosf(a) * rr, ny = cy + nd_sinf(a) * rr; nd_line(px, py, nx, ny, w * .8f, BK_ALPHA(col, (uint32_t)((200.f - 60.f * (float)k) * (.5f + .5f * lit)))); px = nx, py = ny; }
        } break;
    case 10: /* GRAIN: a cluster of grains */
        for (int k = 0; k < 9; k++) { const uint32_t h = (uint32_t)(k + 1) * 2654435761u; nd_disc(cx + ((float)(h % 100u) / 100.f - .5f) * r * 1.4f, cy + ((float)((h >> 8) % 100u) / 100.f - .5f) * r * 1.4f, r * (.08f + .06f * (float)(k % 3)), k & 1 ? c : soft); } break;
    case 11: /* DUSK: a crescent moon */
        nd_disc(cx, cy, r * .6f, c); nd_disc(cx + r * .28f, cy - r * .18f, r * .52f, ARGB(255, 22, 32, 48)); break;
    case 12: /* THIN: three stems, two fading */
        for (int k = 0; k < 3; k++) nd_line(cx + (float)(k - 1) * r * .45f, cy + r * .7f, cx + (float)(k - 1) * r * .45f, cy - r * .6f, w, k == 1 ? c : BK_ALPHA(col, (uint32_t)(40.f + 40.f * lit))); break;
    case 13: { /* PULSE: a breath on the eighths */
        float px = cx - r * .8f, py = cy;
        for (int d = 1; d <= 16; d++) { const float u = (float)d / 16.f, nx = cx - r * .8f + u * r * 1.6f, ny = cy - nd_fabsf(nd_sinf(u * 6.283f * 2.f)) * r * .6f + r * .25f; nd_line(px, py, nx, ny, w, c); px = nx, py = ny; }
        break; }
    case 14: /* NEXT: a path to the next Biome */
        nd_line(cx - r * .7f, cy, cx + r * .55f, cy, w, c); nd_line(cx + r * .55f, cy, cx + r * .15f, cy - r * .4f, w, c); nd_line(cx + r * .55f, cy, cx + r * .15f, cy + r * .4f, w, c); break;
    default: { /* GESTURE: a loop that runs */
        float px = cx + r * .6f, py = cy;
        for (int d = 1; d <= 30; d++) { const float a = (float)d * .2094f, nx = cx + nd_cosf(a) * r * .6f, ny = cy + nd_sinf(a) * r * .38f; nd_line(px, py, nx, ny, w, d > 24 ? c : soft); px = nx, py = ny; }
        const float a = t * 2.f; nd_disc(cx + nd_cosf(a) * r * .6f, cy + nd_sinf(a) * r * .38f, r * .14f + .5f, c); }
    }
}

/* the four macros as weather (the Brick's K1-K4): FIREFLIES, WIND, GLOW, MIST */
static void nd_weather_sign(int k, float cx, float cy, float r, uint32_t col, float v, float t) {
    switch (k) {
    case 0: for (int i = 0; i < 3 + (int)(v * 4.f); i++) { const float a = (float)i * 2.4f + t * .5f; nd_disc(cx + nd_cosf(a) * r * .6f, cy + nd_sinf(a * 1.3f) * r * .5f, r * .12f + .6f, BK_ALPHA(ARGB(255, 220, 255, 160), 230)); } break;
    case 1: nd_pad_sign(5, cx, cy, r, col, v, t * (.5f + 2.f * v)); break;
    case 2: nd_disc(cx, cy, r * (.5f + .3f * v), BK_ALPHA(ARGB(255, 255, 196, 140), (uint32_t)(60.f + 120.f * v))); nd_disc(cx, cy, r * .3f, ARGB(240, 255, 226, 180)); break;
    default: for (int b = 0; b < 3; b++) for (int i = 0; i < 4; i++) nd_disc(cx - r * .6f + (float)i * r * .4f, cy - r * .3f + (float)b * r * .35f + nd_sinf((float)i + t + (float)b) * r * .05f, r * .22f, BK_ALPHA(ARGB(255, 210, 225, 240), (uint32_t)(50.f + 70.f * v)));
    }
}

/* ==== 0.22: the Brick's places on the FX and DREAM pages (pixels, inside the page's glass) ==================== */

/* THE ECHO CAVE (SPACE): a drop falls into the cave's pool; its echoes ring out ECHO TIME apart, each fainter by
 * FEEDBACK; the mist is the ROOM, thicker with its tail and the return */
static void nd_echo_cave(float x, float y, float w, float h, float delay_ms, float feedback, float room, float ret, int frozen, float t, uint32_t col) {
    const float cx = x + w * .5f, pool = y + h * .80f;
    for (float yy = pool - h * .76f; yy < pool; yy += 1.f) {            /* the dark inside: an arch, darker the deeper */
        const float k = (pool - yy) / (h * .78f), hw = w * .44f * vd_sqrt(1.f - k * k > 0.f ? 1.f - k * k : 0.f);
        nd_rect(cx - hw, yy, 2.f * hw, 1.f, ARGB((uint32_t)(150.f - 90.f * k), 3, 5, 10));
    }
    for (int i = 0; i < 46; i++) {                                    /* the rock around the mouth */
        const float a = (float)i / 45.f * 3.1416f, rx = w * .46f, ry = h * .78f;
        nd_disc(cx - nd_cosf(a) * rx, pool - nd_sinf(a) * ry, 9.f + 4.f * nd_sinf((float)i * 1.7f), ARGB(255, 46, 50, 62));
    }
    for (int i = 0; i < 6; i++) {                                     /* stalactites */
        const float sx = cx - w * .3f + (float)i * w * .12f, top = pool - h * .78f * nd_sinf(3.1416f * (.2f + .12f * (float)i)) + 6.f;
        const float len = 10.f + (float)((i * 37) % 14);
        for (int d = 0; d < 5; d++) nd_disc(sx, top + len * (float)d / 5.f, 4.f * (1.f - (float)d / 5.f) + .6f, ARGB(255, 64, 68, 80));
        if (nd_fmodf(t * .5f + (float)i * .3f, 1.f) < .08f) nd_disc(sx, top + len + 3.f, 1.6f, BK_INK);
    }
    for (int i = 0; i < 5; i++)                                       /* the room's mist */
        nd_disc(x + w * (.18f + .16f * (float)i) + 8.f * nd_sinf(t * .3f + (float)i), pool - h * .35f + 5.f * nd_sinf(t * .4f + (float)i * 2.f),
                w * .035f, BK_ALPHA(ARGB(255, 200, 220, 240), (uint32_t)(2.f + 10.f * room * (.3f + ret))));
    nd_rect(x + w * .1f, pool, w * .8f, 2.f, ARGB(90, 150, 210, 235));   /* the pool's surface */
    const float gap = delay_ms / 1000.f < .08f ? .08f : delay_ms / 1000.f, cycle = gap * 7.f < 2.4f ? 2.4f : gap * 7.f;
    const float ph = frozen ? .55f * cycle : nd_fmodf(t, cycle);
    if (ph < .45f) nd_disc(cx, pool - h * .55f + (h * .55f) * (ph / .45f) * (ph / .45f), 2.6f, ARGB(240, 190, 230, 250));   /* the drop */
    float a = 1.f;
    for (int k = 0; k < 8 && a > .04f; k++, a *= feedback < .05f ? .05f : feedback) {    /* the drop, then its echoes */
        const float age = ph - .45f - (float)k * gap;
        if (age < 0.f || age > 1.6f) continue;
        const float r = 6.f + age * w * .22f, fade = (1.f - age / 1.6f) * a;
        float px = cx + r, py = pool;
        for (int d = 1; d <= 28; d++) {
            const float an = (float)d * .2244f, nx = cx + nd_cosf(an) * r, ny = pool + nd_sinf(an) * r * .2f;
            nd_line(px, py, nx, ny, 1.4f, BK_ALPHA(col, (uint32_t)(230.f * fade)));
            px = nx, py = ny;
        }
    }
}

/* THE SPORE MEADOW (GRAIN): mushrooms on moss let their spores go where the grains read the tape; the cloud of
 * spores is as wide as the grain window; FREEZE holds them in the air, REVERSE lets them fall back */
static void nd_spore_meadow(float x, float y, float w, float h, const uint8_t position[4], const uint8_t window[4], int frozen, int reverse, float t, uint32_t col) {
    const float ground = y + h * .82f;
    for (float gx = x; gx < x + w; gx += 6.f) nd_disc(gx, ground + 3.f * nd_sinf(gx * .08f), 6.f, ARGB(255, 30, 46, 40));   /* moss */
    for (int g = 0; g < 4; g++) {
        const float mx = x + 20.f + (float)position[g] * (w - 40.f) / 255.f, open = window[g] ? 1.f : .35f;
        const float stem = 18.f + 6.f * (float)(g % 2), capr = 9.f + 5.f * open;
        nd_line(mx, ground, mx, ground - stem, 3.f, ARGB(230, 230, 220, 200));
        for (int q = -2; q <= 2; q++) nd_disc(mx + (float)q * capr * .35f, ground - stem - (2.f - nd_fabsf((float)q)) * capr * .2f, capr * .5f, ARGB(255, 214, 160, 120));
        nd_disc(mx - capr * .3f, ground - stem - capr * .3f, 1.6f, ARGB(200, 255, 236, 210));
        if (!window[g]) continue;
        const float spread = 4.f + (float)window[g] * 50.f / 255.f;
        for (int s = 0; s < 14; s++) {                                 /* its spores */
            const uint32_t hh = (uint32_t)(s + g * 31 + 1) * 2654435761u;
            float u = frozen ? (float)(hh % 1000u) / 1000.f : nd_fmodf(t * (.25f + (float)(hh >> 28) * .03f) + (float)(hh % 1000u) / 1000.f, 1.f);
            if (reverse) u = 1.f - u;
            const float sx = mx + ((float)((hh >> 10) % 200u) / 100.f - 1.f) * spread * u, sy = ground - stem - capr - u * h * .55f;
            nd_disc(sx, sy, 1.8f, BK_ALPHA(col, (uint32_t)(220.f * (1.f - u))));
        }
    }
}

/* THE DREAM MAP (ITINERA): twelve stations, each a Biome's sign, along a winding route; a light with a tail walks
 * it with the dream's progress (still: it rests at the start) */
static void nd_dream_map(float x, float y, float w, float h, int preset, float progress, int active, float t, uint32_t col) {
    float pts[12][2];
    for (int i = 0; i < 12; i++) {
        const uint32_t hh = (uint32_t)(preset * 12 + i + 1) * 2654435761u;
        pts[i][0] = x + 14.f + (float)i * (w - 28.f) / 11.f;
        pts[i][1] = y + h * .5f + nd_sinf((float)i * .9f + (float)preset) * h * .28f + ((float)((hh >> 8) % 20u) - 10.f);
    }
    for (int i = 1; i < 12; i++) {                                     /* the route, dotted */
        for (int d = 0; d < 6; d++) {
            const float u = (float)d / 6.f;
            nd_disc(pts[i - 1][0] + (pts[i][0] - pts[i - 1][0]) * u, pts[i - 1][1] + (pts[i][1] - pts[i - 1][1]) * u, .9f, BK_ALPHA(col, 110));
        }
    }
    const float at = progress * 11.f;
    for (int i = 0; i < 12; i++) {
        const int biome = (int)(((uint32_t)(preset * 12 + i + 1) * 2654435761u >> 7) % 271u);
        const int passed = active && (float)i <= at;
        nd_disc(pts[i][0], pts[i][1], 9.f, ARGB(220, 10, 14, 24));
        nd_biome_sigil(biome, pts[i][0], pts[i][1], 6.5f, passed ? col : BK_ALPHA(nd_track_col(biome), 150), passed ? t : 0.f);
    }
    const int seg = (int)at < 11 ? (int)at : 10;
    const float f = at - (float)seg;
    const float lx = pts[seg][0] + (pts[seg + 1][0] - pts[seg][0]) * f, ly = pts[seg][1] + (pts[seg + 1][1] - pts[seg][1]) * f;
    for (int k = 6; k >= 1; k--) nd_disc(lx - (float)k * 3.f, ly, 2.f + (float)k * .3f, BK_ALPHA(ARGB(255, 255, 240, 200), (uint32_t)(20 * (7 - k))));
    nd_disc(lx, ly, 3.5f + .8f * nd_sinf(t * 3.f), ARGB(250, 255, 246, 220));
}

/* THE STREAM (TAPE): water flows the way the grains play – faster with the pitch, backwards with REVERSE; leaves
 * drift on it; FREEZE turns it to ice */
static void nd_stream(float x, float y, float w, float h, int pitch, int reverse, int frozen, float t, uint32_t col) {
    const float speed = (reverse ? -1.f : 1.f) * vd_exp2((float)pitch / 12.f) * 40.f;
    for (int b = 0; b < 5; b++) {                                      /* the bands of water */
        const float by = y + h * (.3f + .12f * (float)b);
        float px = x, py = by;
        for (float xx = x + 6.f; xx <= x + w; xx += 6.f) {
            const float ny = by + 3.f * nd_sinf((xx - (frozen ? 0.f : t * speed)) * .05f + (float)b);
            nd_line(px, py, xx, ny, 2.2f, frozen ? ARGB(150, 200, 235, 255) : BK_ALPHA(col, (uint32_t)(90 + 25 * b)));
            px = xx, py = ny;
        }
    }
    for (int i = 0; i < 6; i++) {                                      /* leaves riding the current */
        const float u = frozen ? (float)i / 6.f : nd_fmodf((float)i / 6.f + t * speed / w, 1.f);
        const float lx = x + (u < 0.f ? u + 1.f : u) * w, ly = y + h * (.36f + .1f * (float)(i % 4)) + 3.f * nd_sinf(t + (float)i);
        for (int d = 0; d <= 5; d++) nd_disc(lx + (float)d * 1.6f, ly - nd_sinf((float)d / 5.f * 3.1416f) * 1.5f, 2.6f * nd_sinf((float)d / 5.f * 3.1416f) + .6f, ARGB(230, 214, 178, 96));
    }
    if (frozen) for (int i = 0; i < 12; i++) { const uint32_t hh = (uint32_t)(i + 7) * 2654435761u; nd_line(x + (float)(hh % (uint32_t)w), y + h * .32f + (float)((hh >> 9) % 60u), x + (float)(hh % (uint32_t)w) + 10.f, y + h * .34f + (float)((hh >> 9) % 60u), 1.f, ARGB(140, 235, 248, 255)); }
}

/* REEDS IN THE WIND (DUCKING): the reeds bow when the ducking track hits, by the amount, and rise with the release */
static void nd_reeds(float x, float y, float w, float h, float bow, float t, uint32_t col) {
    const float ground = y + h * .9f;
    for (int i = 0; i < 11; i++) {
        const float rx = x + w * (.06f + .088f * (float)i), len = h * (.55f + .2f * nd_sinf((float)i * 1.9f)), lean = bow * h * .5f + 4.f * nd_sinf(t * 1.3f + (float)i);
        float px = rx, py = ground;
        for (int s = 1; s <= 10; s++) {
            const float f = (float)s / 10.f, nx = rx + lean * f * f, ny = ground - len * f * (1.f - .3f * bow * f);
            nd_line(px, py, nx, ny, 2.4f * (1.f - .6f * f), BK_ALPHA(col, 200));
            px = nx, py = ny;
        }
        for (int d = 0; d < 4; d++) nd_disc(px - lean * .03f * (float)d, py + (float)d * 3.f, 2.6f - .4f * (float)d, ARGB(230, 140, 104, 70));   /* the head */
    }
    nd_rect(x, ground, w, 2.f, BK_ALPHA(col, 120));
}

/* (0.23) the stage's night at half size: drawn into w/2 x h/2 (a quarter of the work), shown 2 x */
static void nd_backdrop_half(nd_scene *s, uint16_t *fb, int fw, int fh, float dim) {
    static uint16_t half[640 * 360];
    const int hw = fw / 2, hh = fh / 2;
    if (hw > 640 || hh > 360) return;
    const md_patch *p = md_state();
    const float t = (float)(md_status()->frames % (44100u * 3600u)) / 44100.f;
    const int due = !s->sky_ok || t - s->sky_t >= 4.f || t < s->sky_t || p->biome_id != s->biome;
    if (due && nd_sky_budget) {
        nd_sky_budget--;
        nd_fb = half, nd_fw = hw, nd_fh = hh;
        nd_cx0 = 0, nd_cy0 = 0, nd_cx1 = hw, nd_cy1 = hh;
        nd_s = (float)hh / 720.f;
        GD_H = 720.f; GD_HORIZON = 690.f; GD_W = (float)hw / nd_s;
        gd_sky(t, p, 0.f);
        nd_rect(0, 0, GD_W, GD_H, BK_ALPHA(ARGB(255, 3, 6, 12), (uint32_t)(255.f * dim)));
        for (int i = 0; i < hw * hh; i++) s->sky[i] = half[i];
        s->sky_ok = 1, s->sky_t = t, s->biome = p->biome_id;
    }
    if (!s->sky_ok) { for (int i = 0; i < fw * fh; i++) fb[i] = 0x0841; return; }
    for (int y = 0; y < fh; y++) {
        const uint16_t *src = &s->sky[(y >> 1) * hw];
        uint16_t *dst = &fb[y * fw];
        for (int x = 0; x < fw; x++) dst[x] = src[x >> 1];
    }
    /* fireflies at full size, over it */
    nd_fb = fb, nd_fw = fw, nd_fh = fh;
    nd_cx0 = 0, nd_cy0 = 0, nd_cx1 = fw, nd_cy1 = fh;
    nd_s = (float)fh / 720.f; GD_W = (float)fw / nd_s;
    const float density = (float)p->macro[0] / 1000.f;
    for (int i = 0; i < 4 + (int)(10.f * density); i++) {
        const uint32_t h2 = (uint32_t)(i + 5) * 2246822519u;
        const float fx = (float)(h2 % (uint32_t)GD_W) + 40.f * nd_sinf(t * .25f + (float)i);
        const float fy = 80.f + (float)((h2 >> 9) % 560u) + 30.f * nd_sinf(t * .4f + (float)i * 1.3f);
        const float blink = nd_sinf(t * (.9f + (float)(h2 >> 28) * .15f) + (float)i);
        if (blink > .3f) {
            nd_disc(fx, fy, 8.f, BK_ALPHA(ARGB(255, 210, 255, 150), (uint32_t)(28.f * blink)));
            nd_disc(fx, fy, 2.2f, BK_ALPHA(ARGB(255, 230, 255, 190), (uint32_t)(220.f * blink)));
        }
    }
}
