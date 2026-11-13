/* Big pad grids (SDK 20, 0.13): on an 8 x 8 grid (a Launchpad or Push as the VIBE's pads) the garden
 * lays itself out over the whole grid. On the VIBE's own 4 x 4 nothing changes.
 *
 *   rows 1-4, columns 1-4   the 16 performance pads, exactly as on the VIBE
 *   rows 1-4, columns 5-8   T1 T2 T3 T4 / T5 T6 PLAY STOP / M1 M2 M3 M4 / M5 M6 VARIATION REC
 *   rows 5-8                the 32 steps of the selected track: tap = note on / off (kit: the pad's gate)
 *
 * Everything here is also on the display and the controls (SEQ GRID, PADS layer, MIDI page): a shortcut. */
static int grid_cols = 4, grid_rows = 4, grid_big;
static int pad_from_stage;                     /* the stage sends logical pads 0-15, never grid ids */

static void grid_init(void) {
    vibe_device_info d;
    vibe_device(&d);
    grid_cols = d.pad_cols ? d.pad_cols : 4;
    grid_rows = d.pad_rows ? d.pad_rows : 4;
    grid_big = grid_cols >= 8 && grid_rows >= 8;
}

/* grid id -> performance pad 0-15, or -1 for the grid's own functions */
static int grid_logical(int id) {
    if (!grid_big) return id < PAD_COUNT ? id : -1;
    const int r = id / grid_cols, c = id % grid_cols;
    return r < 4 && c < 4 ? r * 4 + c : -1;
}
static int grid_id(int logical) { return grid_big ? (logical / 4) * grid_cols + logical % 4 : logical; }

static void grid_step_toggle(int step) {
    const md_patch *p = md_state();
    if (md_command_space() < 4) return;
    if (p->kit.enabled && track == 3) {
        md_command(MD_KIT_STEP, 3, step);
        md_command(MD_KIT_GATE, 3, !((p->kit.gates[p->kit.pad] >> step) & 1u));
        return;
    }
    const md_sequence *q = &p->sequence[track];
    if (!q->enabled) md_command(MD_SEQ_ENABLE, (int)track, 1);
    if (step >= q->length) md_command(MD_SEQ_LENGTH, (int)track, step + 1);
    md_command(MD_SEQ_STEP, (int)track, step);
    const int note = q->steps[step].note;
    md_command(MD_SEQ_NOTE, (int)track, note ? 23 : clampi(p->track[track].note, 24, 96));
}

static void grid_function(int id, int down) {
    if (!down || !grid_big) return;
    const int r = id / grid_cols, c = id % grid_cols;
    if (r >= 4 && r < 8 && c < 8) { grid_step_toggle((r - 4) * 8 + c); return; }
    if (r >= 4 || c < 4 || c >= 8) return;
    const int k = r * 4 + (c - 4);
    if (k < 6) { if (k < md_active_tracks()) track = (unsigned)k; }
    else if (k == 6) md_command(MD_PLAY, 0, 1);
    else if (k == 7) stop_all();
    else if (k < 14) moment_key((unsigned)(k - 8));
    else if (k == 14) mutate_dream();
    else rec_button();
}

/* The grid's own lights, from vibe_on_draw. */
static void grid_leds(void) {
    if (!grid_big) return;
    const md_patch *p = md_state();
    const int drums = p->kit.enabled && track == 3;
    const md_sequence *q = &p->sequence[track];
    int hit = 0;
    const int head = md_sequence_playhead(track, &hit);
    const unsigned length = drums ? p->kit.length : q->enabled ? q->length : (unsigned)p->track[track].length;
    const uint32_t ink = page_ink[track % 6];
    for (int s = 0; s < 32; s++) {
        const int id = (4 + s / 8) * grid_cols + s % 8;
        const int on = drums ? (int)((p->kit.gates[p->kit.pad] >> s) & 1u) :
                       q->enabled ? q->steps[s].note != 0 : md_hit((unsigned)s, (unsigned)p->track[track].hits, (unsigned)p->track[track].length, (unsigned)p->track[track].rotate);
        if (s == head) vibe_led_pad(id, 0xffffff, .9f);
        else if ((unsigned)s >= length) vibe_led_pad(id, 0x000000, 0.f);
        else vibe_led_pad(id, ink, on ? .75f : .08f);
    }
    for (int k = 0; k < 16; k++) {
        const int id = (k / 4) * grid_cols + 4 + k % 4;
        uint32_t color = 0x83d2a6;
        float level = .12f;
        if (k < 6) { color = page_ink[k]; level = k == (int)track ? .9f : k < md_active_tracks() ? .2f : 0.f; }
        else if (k == 6) { color = 0x83d2a6; level = md_playing() ? .9f : .3f; }
        else if (k == 7) { color = 0xe8705e; level = .25f; }
        else if (k < 14) { color = 0xf0cb86; level = moment_active == k - 8 ? .95f : moments[k - 8].used ? .35f : .05f; }
        else if (k == 14) { color = 0xc0a0ed; level = .4f; }
        else { color = 0xe8705e; level = rec_state == REC_TAKING ? .95f : .15f; }
        vibe_led_pad(id, color, level);
    }
}
