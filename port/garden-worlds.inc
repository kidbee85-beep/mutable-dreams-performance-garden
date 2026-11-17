/* Kid Bunka's 48 worlds (0.19): the same worlds as on the Trimui Brick and the MPC. A world is a Biome with its
 * tempo, the four macros, engine settings and the texture / space lanes set (as automation lanes: the DAW sees
 * them, the hands still play over them). On the device: the BROWSER page, F5 switches BIOMES / WORLDS; F1 enters,
 * F2 / F3 or encoder 2 step, F4 or encoder 1 the family. In the Dream Console: the WORLDS tab of the library.
 * (The Brick's Sound Lab and the MPC's POLY layer have no counterpart on VIBE FM: a world here is its garden.) */
typedef struct { int cmd, track, value; } world_engine;
typedef struct { int family; const char *name, *story; int biome, tempo, macros[4], engines; world_engine engine[12]; int lanes[8]; } world_spec;
#include "garden-worlds-data.inc"

static int browse_worlds;                     /* the BROWSER shows the worlds */
static int world_pos, world_family = -1;      /* the cursor and the family filter (-1 all) */
static int world_here = -1;                   /* the world entered last (-1: none, or the Biome changed since) */
static void load_biome_id(int id);

static int world_matches(int i) { return world_family < 0 || worlds[i].family == world_family; }
static void world_step(int delta) {
    int pos = world_pos;
    for (int guard = 0; guard < WORLDS && delta; guard++) {
        pos = (pos + (delta > 0 ? 1 : WORLDS - 1)) % WORLDS;
        if (world_matches(pos)) { delta += delta > 0 ? -1 : 1; world_pos = pos; }
    }
}
static void world_family_set(int f) {
    world_family = f < -1 ? -1 : f >= WORLD_FAMILIES ? WORLD_FAMILIES - 1 : f;
    if (!world_matches(world_pos)) world_step(1);
}

static void enter_world(int i);
/* (0.21, from the MPC) wandering worlds: while a world plays, after world_wander minutes the next world of its
 * family comes. Audio side, once per block (engine frames); it stops when the Biome changes by other means. */
static unsigned world_wander_frames;
static void worlds_tick(unsigned n) {
    if (world_wander <= 0 || world_here < 0 || !md_playing() || md_state()->biome_id != worlds[world_here].biome) { world_wander_frames = 0; return; }
    world_wander_frames += n;
    if (world_wander_frames < (unsigned)world_wander * 60u * 44100u) return;
    world_wander_frames = 0;
    const int family = worlds[world_here].family;
    int next = world_here;
    for (int guard = 0; guard < WORLDS; guard++) { next = (next + 1) % WORLDS; if (worlds[next].family == family) break; }
    enter_world(next);
}
static void world_wander_step(int steps) {
    static const int choices[4] = {0, 5, 10, 20};
    int k = 0;
    for (int i = 0; i < 4; i++) if (choices[i] == world_wander) k = i;
    k += steps; k = k < 0 ? 0 : k > 3 ? 3 : k;
    world_wander = choices[k];
    world_wander_frames = 0;
}

static void enter_world(int i) {
    if (i < 0 || i >= WORLDS) return;
    const world_spec *w = &worlds[i];
    if (w->biome < 0 || w->biome >= MD_BIOMES || md_command_space() < 32) { say("BUSY - TRY AGAIN"); return; }
    load_biome_id(w->biome);
    if (w->tempo > 0) md_command(MD_TEMPO, 0, w->tempo);
    for (int k = 0; k < 4; k++) {
        if (w->macros[k] < 0) continue;
        const int v = w->macros[k] >= 500 ? md_project_state()->macro[k] / 10 + w->macros[k] - 1000 : w->macros[k];
        md_command(MD_MACRO, k, (v < 0 ? 0 : v > 100 ? 100 : v) * 10);
    }
    for (int e = 0; e < w->engines; e++) md_command(w->engine[e].cmd, w->engine[e].track, w->engine[e].value);
    for (int k = 0; k < 8; k++) perf[k] = w->lanes[k] > 0 ? (float)w->lanes[k] / 100.f : 0.f;   /* (reported to the DAW) */
    world_here = i;
    world_pos = i;
    say(w->name);
}
