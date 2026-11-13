/* Dream as MIDI (0.12): every note the engine plays goes out through the host's MIDI (SDK 9) –
 * tracks 1-6 on channels 1-6, the lo-fi kit on channel 10 (GM drums from note 36). A DAW records the
 * generative music as MIDI, an external synth can double a track. Off by default.
 * Audio side, once per block: the engine's per-track event counters say what was just played. */
enum { NOTE_GATE = 15435 };                   /* 0.35 s at the engine's 44.1 kHz, or until the next note */
static uint32_t notes_seen[MD_TRACKS];
static uint8_t notes_held[MD_TRACKS], notes_channel[MD_TRACKS];
static unsigned notes_age[MD_TRACKS];
static int notes_were_on;

static void notes_off_track(unsigned i) {
    if (!notes_held[i]) return;
    vibe_midi_send((uint8_t)(0x80 | notes_channel[i]), notes_held[i], 0);
    notes_held[i] = 0;
}

static void notes_all_off(void) { for (unsigned i = 0; i < MD_TRACKS; i++) notes_off_track(i); }

static void notes_out_tick(unsigned frames) {
    const md_stats *st = md_status();
    if (!settings.notes_out) {
        if (notes_were_on) { notes_all_off(); notes_were_on = 0; }
        for (unsigned i = 0; i < MD_TRACKS; i++) notes_seen[i] = st->events[i];
        return;
    }
    notes_were_on = 1;
    const md_patch *p = md_state();
    for (unsigned i = 0; i < MD_TRACKS; i++) {
        if (notes_held[i]) { notes_age[i] += frames; if (notes_age[i] >= NOTE_GATE) notes_off_track(i); }
        if (st->events[i] == notes_seen[i]) continue;
        notes_seen[i] = st->events[i];
        const int kit = p->kit.enabled && i == 3;
        int note = st->last_note[i];
        if (note < 1) note = kit ? 36 : 60;
        if (note > 127) note = 127;
        notes_off_track(i);
        notes_channel[i] = (uint8_t)(kit ? 9 : i);
        notes_held[i] = (uint8_t)note;
        notes_age[i] = 0;
        const int velocity = p->track[i].mute ? 0 : 40 + p->track[i].level * 80 / 1000;
        if (velocity > 0) vibe_midi_send((uint8_t)(0x90 | notes_channel[i]), (uint8_t)note, (uint8_t)(velocity > 127 ? 127 : velocity));
        else notes_held[i] = 0;
    }
}
