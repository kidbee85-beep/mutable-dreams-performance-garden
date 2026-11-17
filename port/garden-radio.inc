/* MUTABLE DREAMS FM (0.13): the station's programme inside the instrument.
 *
 * The same rules as the Radio Studio's station_program.js, on the engine itself:
 *   - editions of four pieces; each edition draws one of four character sets
 *     (lullaby groove playful afterglow / pulse lullaby groove duet / playful ensemble groove afterglow /
 *      drift groove playful ensemble), each character with its own tempo band
 *   - pieces of 70-260 s (the first 75 s), scaled by the programme length setting
 *   - every piece a Biome from the whole library: never one of the last 24, another family than the one before,
 *     rhythm where the character asks for it, and a groove at the latest after six quiet minutes
 *   - once in every 72 pieces (about four hours) a 30-minute Itinera dream, the next world five steps on
 *   - halfway through a piece, sometimes a new variation of the same Biome
 * The hands stay free to play: pads, gesture, moments and REC work over the radio. STOP ends the broadcast.
 * Included after garden-grid.inc. Runs from vibe_on_audio, once per block (commands only, no waiting). */

enum { RADIO_RECENT = 24, RADIO_SLOTS = 72, RADIO_QUIET_MAX = 360, RADIO_DREAM_SECONDS = 1800 };
enum { CH_LULLABY, CH_GROOVE, CH_PLAYFUL, CH_AFTERGLOW, CH_PULSE, CH_DUET, CH_ENSEMBLE, CH_DRIFT };
static const char *const radio_characters[8] = {"LULLABY","GROOVE","PLAYFUL","AFTERGLOW","PULSE","DUET","ENSEMBLE","DRIFT"};
static const uint8_t radio_bands[8][2] = {{46,62},{74,92},{72,96},{51,75},{88,116},{65,91},{76,106},{49,68}};
static const uint8_t radio_forms[4][4] = {
    {CH_LULLABY, CH_GROOVE, CH_PLAYFUL, CH_AFTERGLOW}, {CH_PULSE, CH_LULLABY, CH_GROOVE, CH_DUET},
    {CH_PLAYFUL, CH_ENSEMBLE, CH_GROOVE, CH_AFTERGLOW}, {CH_DRIFT, CH_GROOVE, CH_PLAYFUL, CH_ENSEMBLE}};
static const uint16_t radio_seconds[12] = {70,110,140,150,165,180,180,185,195,215,225,260};

static struct {
    int on, character, form, slot, dream_at, world, in_dream, biome, mutated;
    unsigned piece_frames, piece_length, quiet_seconds, frames_total;
    uint16_t recent[RADIO_RECENT];
    unsigned recent_at;
    uint32_t seed;
} radio;
static int radio_wander_before = -1;
static int radio_length_choice = 1;            /* 0 short (x0.5), 1 radio, 2 long (x2) */
static int radio_dreams = 1;                   /* the four-hourly Itinera dream */
static int radio_autostart;                    /* 1 = on air as soon as the instrument starts: a VIBE as a radio */
static int radio_chime = 1;                    /* the station ident between pieces */
static int radio_on_now(void) { return radio.on; }
static md_patch radio_scratch;

static uint32_t radio_rand(void) {
    uint32_t x = radio.seed ? radio.seed : 0x4d444653u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return radio.seed = x;
}
static int radio_is_rhythmic(const md_patch *p) {
    if (p->kit.enabled) return 1;
    for (int i = 0; i < MD_TRACKS; i++) {
        const int m = p->track[i].model;
        if (!p->track[i].mute && p->track[i].level > 0 && (m == MD_KICK || m == MD_SNARE || m == MD_HAT || m == MD_SKIN)) return 1;
    }
    return 0;
}
static int radio_recent(int id) {
    for (int i = 0; i < RADIO_RECENT; i++) if (radio.recent[i] == (uint16_t)(id + 1)) return 1;
    return 0;
}

/* The next piece: its character, length and Biome, following the station's rules. */
static void radio_next_piece(void) {
    const int stage = radio.slot % 4;
    if (stage == 0) { radio.form = (int)(radio_rand() % 4u); }
    int character = radio_forms[radio.form][stage];
    const unsigned base = radio.slot == 0 ? 75u : radio_seconds[radio.slot % 12];
    const unsigned seconds = radio_length_choice == 0 ? base / 2 : radio_length_choice == 2 ? base * 2 : base;
    const int needs_rhythm = radio.quiet_seconds + seconds > RADIO_QUIET_MAX || radio.slot == 1;
    const int wants_rhythm = character == CH_GROOVE || character == CH_PULSE || character == CH_ENSEMBLE;
    const int previous_family = radio.biome >= 0 ? md_biome_family_index(radio.biome) : -1;
    int best = -1, best_rhythm = 0;
    float best_score = -1e9f;
    for (int tries = 0; tries < 28; tries++) {
        const int id = md_biome_at(radio_rand() % MD_BIOMES);
        if (id < 0 || radio_recent(id)) continue;
        if (!md_preview_biome_kit(&radio_scratch, id)) continue;
        const int rhythm = radio_is_rhythmic(&radio_scratch);
        float score = (float)(radio_rand() % 1000u) / 480.f;
        if (md_biome_family_index(id) != previous_family) score += 3.f;
        if ((needs_rhythm || wants_rhythm) && rhythm) score += 3.f;
        if (needs_rhythm && !rhythm) score -= 4.f;
        if (score > best_score) { best_score = score; best = id; best_rhythm = rhythm; }
    }
    if (best < 0) best = md_biome_at(radio_rand() % MD_BIOMES);
    if (needs_rhythm && best_rhythm && !wants_rhythm) character = CH_GROOVE;
    if (wants_rhythm && !best_rhythm) character = CH_DUET;
    radio.recent[radio.recent_at++ % RADIO_RECENT] = (uint16_t)(best + 1);
    radio.quiet_seconds = best_rhythm ? 0u : radio.quiet_seconds + seconds;
    radio.character = character;
    radio.biome = best;
    radio.piece_frames = 0;
    radio.piece_length = seconds * 44100u;
    radio.mutated = 0;
    /* the tempo of the character: the Biome's own, pulled into the band */
    md_biome_card card;
    int tempo = 72;
    if (md_biome_card_info(best, &card)) tempo = card.tempo;
    const int lo = radio_bands[character][0], hi = radio_bands[character][1];
    const int center = tempo < lo ? lo : tempo > hi ? hi : tempo;
    tempo = (center * 45 + (lo + (int)(radio_rand() % (unsigned)(hi - lo + 1))) * 55) / 100;
    if (radio_chime && radio.slot) garden_textures_chime(&textures, md_biome_card_info(best, &card) ? card.root : 0, md_biome_card_info(best, &card) ? card.scale : 1);
    if (md_command_space() >= 4) {
        thin_end();
        latched &= ~(1u << PAD_THIN);
        md_command(MD_SCENE, 0, best);
        md_command(MD_SCENE_CONFIRM, 0, 1);
        md_command(MD_TEMPO, 0, tempo);
        md_command(MD_PLAY, 0, 1);
    }
    radio.slot = (radio.slot + 1) % RADIO_SLOTS;
    if (radio.slot == 0) radio.dream_at = 9 + (int)(radio_rand() % 4u);
}

static void radio_start_dream(void) {
    radio.world = (radio.world + 5) % MD_ITINERA_PRESETS;
    if (md_command_space() < 2) return;
    md_command(MD_PLAY, 0, 0);
    md_command(MD_ITINERA_START, radio.world, 0);         /* 30 minutes, slow, close: the radio's dream */
    radio.in_dream = 1;
    radio.piece_frames = 0;
    radio.piece_length = RADIO_DREAM_SECONDS * 44100u;
    radio.quiet_seconds = RADIO_QUIET_MAX;
}

static void radio_set(int on) {
    if (on == radio.on) return;
    radio.on = on;
    if (on) {
        radio.seed ^= md_status()->frames | 1u;
        radio.slot = 0;
        radio.dream_at = 9 + (int)(radio_rand() % 4u);
        radio.world = (int)(radio_rand() % MD_ITINERA_PRESETS);
        radio.biome = -1;
        radio.in_dream = 0;
        radio.quiet_seconds = 0;
        radio.frames_total = 0;
        radio_wander_before = settings.wander;          /* the station wanders; OFF AIR gives your setting back (0.18) */
        settings.wander = 1;
        radio_next_piece();
        say("MUTABLE DREAMS FM - ON AIR");
    } else {
        if (radio.in_dream) md_command(MD_PLAY, 0, 0);
        radio.in_dream = 0;
        if (radio_wander_before >= 0) settings.wander = radio_wander_before;
        radio_wander_before = -1;
        say("OFF AIR");
    }
}

/* Audio side, once per block: n engine frames have passed. */
static void radio_tick(unsigned n) {
    if (!radio.on) return;
    radio.frames_total += n;
    radio.piece_frames += n;
    if (radio.in_dream) {
        md_itinera_view v;
        md_itinera_read(&v);
        if (radio.piece_frames > 44100u && !v.active) { radio.in_dream = 0; radio_next_piece(); }
        return;
    }
    if (!radio.mutated && radio.piece_frames > radio.piece_length / 2) {
        radio.mutated = 1;
        if (radio_rand() % 3u == 0 && md_command_space()) md_command(MD_MUTATE, 0, 1);
    }
    if (radio.piece_frames >= radio.piece_length) {
        if (radio_dreams && radio.slot == radio.dream_at) radio_start_dream();
        else radio_next_piece();
    }
}

static void radio_skip(void) {
    if (!radio.on) return;
    if (radio.in_dream) { md_command(MD_PLAY, 0, 0); radio.in_dream = 0; }
    radio_next_piece();
}

/* "LULLABY  -  2:14 LEFT" or "ITINERA DREAM  -  27 MIN LEFT" */
static void radio_line(char *out, unsigned max) {
    unsigned k = 0;
    const char *a = radio.in_dream ? "ITINERA DREAM" : radio_characters[radio.character];
    while (*a && k < max - 1) out[k++] = *a++;
    const char *sep = "  -  ";
    while (*sep && k < max - 1) out[k++] = *sep++;
    const unsigned left = radio.piece_frames < radio.piece_length ? (radio.piece_length - radio.piece_frames) / 44100u : 0u;
    char num[8];
    if (radio.in_dream) { vg_itoa((int)(left / 60u), num); for (unsigned i = 0; num[i] && k < max - 1; i++) out[k++] = num[i];
        const char *m = " MIN LEFT"; while (*m && k < max - 1) out[k++] = *m++; }
    else { vg_itoa((int)(left / 60u), num); for (unsigned i = 0; num[i] && k < max - 1; i++) out[k++] = num[i];
        if (k < max - 3) { out[k++] = ':'; out[k++] = (char)('0' + (left % 60u) / 10u); out[k++] = (char)('0' + left % 10u); }
        const char *m = " LEFT"; while (*m && k < max - 1) out[k++] = *m++; }
    out[k] = 0;
}
