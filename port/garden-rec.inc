/* REC (0.11): the performance as a WAV take, written into "VIBE FM/mutable-dreams-garden/" by the host.
 *
 * REC starts a take, REC again ends it early. The audio keeps the finished mix at the engine's own
 * 44.1 kHz (textures and pulse included) in memory, up to the take length (30 / 60 / 90 / 120 s, 90 by
 * default: the length of a competition video). When the take ends, the file is written from
 * vibe_on_draw and the media events – never from vibe_on_audio – with an exact WAV header.
 * Included by adapter-check.c after garden-views.inc. */

enum { REC_MAX_FRAMES = 120 * 44100 };
static int16_t rec_pcm[REC_MAX_FRAMES * 2];
enum { REC_IDLE, REC_TAKING, REC_WRITING };
static int rec_state, rec_handle, rec_header_done;   /* take length: rec_seconds_choice (garden-views.inc) */
static unsigned rec_frames, rec_target, rec_written;
static char rec_path[VIBE_MEDIA_PATH];
static unsigned rec_takes;
static const int rec_choices[4] = {30, 60, 90, 120};

static void rec_start(void) {
    if (rec_state != REC_IDLE) return;
    rec_frames = 0;
    rec_target = (unsigned)rec_choices[rec_seconds_choice & 3] * 44100u;
    rec_state = REC_TAKING;
    say("RECORDING A TAKE");
}

static void rec_stop(void) {
    if (rec_state != REC_TAKING) return;
    if (rec_frames < 4410) { rec_state = REC_IDLE; say("TAKE TOO SHORT - NOT KEPT"); return; }
    rec_state = REC_WRITING;
    rec_handle = 0;
    rec_header_done = 0;
    rec_written = 0;
}

static void rec_button(void) { if (rec_state == REC_IDLE) rec_start(); else if (rec_state == REC_TAKING) rec_stop(); }

/* Audio side: n engine frames of the finished stereo mix. */
static void rec_capture(const float *lr, unsigned n) {
    if (rec_state != REC_TAKING) return;
    for (unsigned i = 0; i < n && rec_frames < rec_target; i++, rec_frames++) {
        for (unsigned c = 0; c < 2; c++) {
            float v = lr[2 * i + c] * 32767.f;
            v = v > 32767.f ? 32767.f : v < -32768.f ? -32768.f : v;
            rec_pcm[2 * rec_frames + c] = (int16_t)v;
        }
    }
    if (rec_frames >= rec_target) {
        rec_state = REC_WRITING;
        rec_handle = 0;
        rec_header_done = 0;
        rec_written = 0;
    }
}

static void put_le32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

/* Writing side: as far as the host takes it now; VIBE_MEDIA_PENDING waits for the next call. */
static void rec_pump(void) {
    if (rec_state != REC_WRITING) return;
    for (int guard = 0; guard < 64; guard++) {
        if (!rec_handle) {
            char name[32] = "dream take ";
            unsigned k = 11;
            char num[8];
            vg_itoa((int)rec_takes + 1, num);
            for (unsigned i = 0; num[i]; i++) name[k++] = num[i];
            name[k++] = '.'; name[k++] = 'w'; name[k++] = 'a'; name[k++] = 'v'; name[k] = 0;
            const int h = vibe_media_create(name, rec_path, sizeof rec_path);
            if (h == VIBE_MEDIA_PENDING) return;
            if (h <= 0) { rec_state = REC_IDLE; say("NO PLACE TO WRITE THE TAKE"); return; }
            rec_handle = h;
        }
        if (!rec_header_done) {
            const uint32_t bytes = rec_frames * 4u;
            uint8_t hd[44] = {'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,2,0,0,0,0,0,0,0,0,0,4,0,16,0,'d','a','t','a',0,0,0,0};
            put_le32(hd + 4, 36u + bytes);
            put_le32(hd + 24, 44100u);
            put_le32(hd + 28, 44100u * 4u);
            put_le32(hd + 40, bytes);
            const int r = vibe_media_append(rec_handle, hd, 44);
            if (r == VIBE_MEDIA_PENDING) return;
            if (r < 0) { vibe_media_close(rec_handle, 0); rec_state = REC_IDLE; say("WRITING THE TAKE FAILED"); return; }
            rec_header_done = 1;
        }
        if (rec_written >= rec_frames) {
            vibe_media_close(rec_handle, 1);
            rec_takes++;
            rec_state = REC_IDLE;
            say("TAKE SAVED IN VIBE FM / MUTABLE DREAMS");
            return;
        }
        unsigned n = rec_frames - rec_written;
        if (n > 16384) n = 16384;                                  /* 64 KB a piece */
        const int r = vibe_media_append(rec_handle, rec_pcm + 2 * rec_written, (int)(n * 4u));
        if (r == VIBE_MEDIA_PENDING) return;
        if (r < 0) { vibe_media_close(rec_handle, 0); rec_state = REC_IDLE; say("WRITING THE TAKE FAILED"); return; }
        rec_written += n;
    }
}

static float rec_seconds(void) { return (rec_state == REC_WRITING ? rec_written : rec_frames) / 44100.f; }

/* "REC 0:42 / 1:30" or "SAVING 64 %" */
static int rec_label(char *out) {
    if (rec_state == REC_IDLE) return 0;
    unsigned k = 0;
    const char *head = rec_state == REC_TAKING ? "REC " : "SAVING ";
    while (*head) out[k++] = *head++;
    if (rec_state == REC_TAKING) {
        const int s = (int)(rec_frames / 44100u), t = (int)(rec_target / 44100u);
        out[k++] = (char)('0' + s / 60); out[k++] = ':'; out[k++] = (char)('0' + (s % 60) / 10); out[k++] = (char)('0' + s % 10);
        out[k++] = ' '; out[k++] = '/'; out[k++] = ' ';
        out[k++] = (char)('0' + t / 60); out[k++] = ':'; out[k++] = (char)('0' + (t % 60) / 10); out[k++] = (char)('0' + t % 10);
    } else {
        char pct[8];
        vg_itoa(rec_frames ? (int)(rec_written * 100u / rec_frames) : 0, pct);
        for (unsigned i = 0; pct[i]; i++) out[k++] = pct[i];
        out[k++] = ' '; out[k++] = '%';
    }
    out[k] = 0;
    return 1;
}
