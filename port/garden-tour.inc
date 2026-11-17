/* FIRST STEPS (0.26): an interactive tour for someone who plays the garden for the first time – no manual needed.
 *
 * Seven short steps. Each says what to do and moves on by itself as soon as it was done: PLAY, a space pad, a macro,
 * NEXT, the gesture, the pages, the inputs. A card on the display (and on the stage, where the pad or key to press
 * glows; the Dream Console shows the same step). It never takes a key away: HOME ends it (and opens the help as
 * always). It starts by itself on the very first start; SYSTEM F1, the stage's ? and the console's ? start it again.
 * Included by adapter-check.c after garden-views.inc. */

enum { TOUR_PLAY, TOUR_BLOOM, TOUR_MACRO, TOUR_NEXT, TOUR_GESTURE, TOUR_PAGES, TOUR_LISTEN, TOUR_DONE, TOUR_STEPS };
static int tour_step = -1;                            /* -1: no tour */
static float tour_t, tour_hold, tour_flash;
static int tour_mark_biome, tour_mark_macro;
typedef struct { const char *title, *line1, *line2; } tour_text;
static const tour_text tour_texts[TOUR_STEPS] = {
    {"WELCOME TO THE GARDEN", "Press PLAY: a Biome wakes up -", "six tracks of music that never repeat."},
    {"PLAY WITH YOUR HANDS", "Hold pad 9 BLOOM: a room opens.", "Pads 5-12 are textures and spaces."},
    {"SHAPE THE BIOME", "Turn encoder 1 on LIVE: DENSITY.", "Encoders 1-4 are the four macros."},
    {"ANOTHER WORLD", "Tap pad 15 NEXT for the next Biome.", "F1-F6 on LIVE are your favourites."},
    {"YOUR MOVES, LOOPED", "Tap pad 16, play pads 5-12 a while,", "tap 16 again: your hands loop by themselves."},
    {"THE WHOLE INSTRUMENT", "Press MENU: all 50 pages.", "The coloured keys jump into groups."},
    {"THE GARDEN LISTENS", "LISTEN page (FX): F1 lets it hear IN 1.", "Sing on SING - it answers. (Moves on by itself.)"},
    {"YOU KNOW THE GARDEN", "HOME explains every page. Hold an F key:", "it says what it does. Have fun - Kid Bunka"},
};

static int tour_macro_sum(void) { const md_patch *p = md_state(); return p->macro[0] + p->macro[1] + p->macro[2] + p->macro[3]; }
static void tour_enter(int step) {
    tour_step = step; tour_t = 0.f; tour_hold = 0.f;
    tour_mark_biome = md_state()->biome_id;
    tour_mark_macro = tour_macro_sum();
}
static void tour_start(void) { tour_enter(TOUR_PLAY); tour_flash = 0.f; }
static void tour_start_from_page(void) { set_view(V_LIVE); tour_start(); }   /* SYSTEM F1: back to LIVE, step 1 */
static void tour_end(void) { if (tour_step >= 0) { tour_step = -1; tour_done_saved = 1; } }
static void tour_next(void) { tour_flash = 1.f; if (tour_step + 1 >= TOUR_STEPS) tour_end(); else tour_enter(tour_step + 1); }

/* from the audio flow: did the player do what the step asks? (state only, no SDK calls) */
static void tour_tick(float seconds) {
    if (tour_flash > 0.f) tour_flash -= seconds * 1.5f;
    if (tour_step < 0) return;
    tour_t += seconds;
    switch (tour_step) {
    case TOUR_PLAY: if (md_playing()) tour_next(); break;
    case TOUR_BLOOM:
        if (pads.pads[8].held || pads.pads[8].level > .3f) tour_hold += seconds;
        if (tour_hold > .6f) tour_next();
        break;
    case TOUR_MACRO: { const int d = tour_macro_sum() - tour_mark_macro; if (d > 30 || d < -30) tour_next(); break; }
    case TOUR_NEXT: if (md_state()->biome_id != tour_mark_biome) tour_next(); break;
    case TOUR_GESTURE: if (gesture.mode == GESTURE_LOOPING) tour_next(); break;
    case TOUR_PAGES: if (show_menu || page != V_LIVE) tour_next(); break;
    case TOUR_LISTEN: if (lis.on || tour_t > 15.f) tour_next(); break;
    case TOUR_DONE: if (tour_t > 8.f) tour_end(); break;
    }
}

/* the stage glows where the step's control is: 0 none, 1 PLAY, 2.. pad index + 2 */
static int tour_stage_target(void) {
    switch (tour_step) {
    case TOUR_PLAY: return 1;
    case TOUR_BLOOM: return 2 + 8;
    case TOUR_NEXT: return 2 + 14;
    case TOUR_GESTURE: return 2 + 15;
    default: return 0;
    }
}

