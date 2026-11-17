/* MOMENTS and WANDER for the Performance Garden (0.7). Included after garden-state.inc.
 *
 * MOMENTS: six performance snapshots on F1-F6 of the PADS layer (FX pressed twice).
 *   SHIFT + F = store what sounds now: the eight TEXTURE / SPACE lanes, PULSE and the A / B depths.
 *   F = glide there over the pad tail (at least one second); F again = glide back out.
 *   Recallable from the DAW as the parameter "moment".
 * WANDER: when the hands rest, the garden keeps moving by itself: one or two lanes at a time drift
 *   slowly up and down, never above 55 %. Hands, gesture, automation and moments always win.
 *   Device: SHIFT + encoder 3 on PADS. DAW: "wander".
 */
static float moment_layer[MOMENT_LANES];
static float wander_layer[MOMENT_LANES], wander_target[MOMENT_LANES];
static unsigned wander_clock, idle_frames;
static uint32_t wander_seed = 0x2545f491u;
static float sounding[MOMENT_LANES];            /* what the lanes play now, before the depths (audio writes, events read) */

static void moments_clear(void) {
    for (unsigned m = 0; m < MOMENTS; m++) moments[m] = (garden_moment){{0}, 1.f, 1.f, 0};
    moment_active = -1;
    for (unsigned i = 0; i < MOMENT_LANES; i++) moment_layer[i] = wander_layer[i] = wander_target[i] = 0.f;
    wander_clock = idle_frames = 0;
}

static void moment_store(unsigned m) {
    if (m >= MOMENTS) return;
    for (unsigned i = 0; i < MOMENT_LANES; i++) moments[m].lane[i] = sounding[i];
    moments[m].texture_depth = settings.texture_depth;
    moments[m].space_depth = settings.space_depth;
    moments[m].used = 1;
}

/* Recall (or release when it is the active one). An empty slot does nothing. */
static float depth_before[2] = {-1.f, -1.f};          /* A / B before the first recall: gliding out gives them back */
static void moment_recall(int m) {
    if (m < 0 || m >= MOMENTS || m == moment_active) {
        if (moment_active >= 0 && depth_before[0] >= 0.f) {
            settings.texture_depth = depth_before[0]; settings.space_depth = depth_before[1];
        }
        depth_before[0] = depth_before[1] = -1.f;
        moment_active = -1;
        return;
    }
    if (!moments[m].used) return;
    if (moment_active < 0) { depth_before[0] = settings.texture_depth; depth_before[1] = settings.space_depth; }
    moment_active = m;
    settings.texture_depth = moments[m].texture_depth;
    settings.space_depth = moments[m].space_depth;
}

static void moment_key(unsigned m) {
    if (shift_held) moment_store(m);
    else moment_recall((int)m);
}

static float wander_random(void) {
    uint32_t x = wander_seed;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    wander_seed = x;
    return (x >> 8) * (1.f / 16777216.f);
}

/* Audio side, once per block, n engine samples. `hands` = 1 while any pad is held or a gesture plays. */
static void layers_update(unsigned n, int hands) {
    const float glide_time = settings.tail > 1.f ? settings.tail : 1.f;
    const float glide = n / (n + glide_time * (MD_RATE / 3.f));
    for (unsigned i = 0; i < MOMENT_LANES; i++) {
        const float target = moment_active >= 0 ? moments[moment_active].lane[i] : 0.f;
        moment_layer[i] += (target - moment_layer[i]) * glide;
        if (target == 0.f && moment_layer[i] < .0005f) moment_layer[i] = 0.f;
    }
    idle_frames = hands ? 0 : idle_frames + n;
    const int wandering = settings.wander && idle_frames > 2u * MD_RATE;
    wander_clock += n;
    if (wandering && wander_clock > 4u * MD_RATE) {
        wander_clock = 0;
        for (unsigned i = 0; i < MOMENT_LANES; i++) wander_target[i] = 0.f;
        const unsigned picks = 1u + (wander_random() < .45f);
        for (unsigned k = 0; k < picks; k++) {
            const unsigned lane = (unsigned)(wander_random() * 8.f) & 7u;   /* textures and spaces, not PULSE */
            wander_target[lane] = .15f + .4f * wander_random();
        }
    }
    if (!wandering) for (unsigned i = 0; i < MOMENT_LANES; i++) wander_target[i] = 0.f;
    const float drift = n / (n + 3.5f * MD_RATE);
    for (unsigned i = 0; i < MOMENT_LANES; i++) {
        wander_layer[i] += (wander_target[i] - wander_layer[i]) * drift;
        if (wander_target[i] == 0.f && wander_layer[i] < .0005f) wander_layer[i] = 0.f;
    }
}
