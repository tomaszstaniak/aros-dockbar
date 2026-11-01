#include <string.h>
#include "snapshot.h"

/* Wrap-safe "t has come". */
static int reached(unsigned now, unsigned t) { return (int)(now - t) >= 0; }

void dock_snap_start(struct DockSnapshot *s, unsigned gen)
{
    memset(s, 0, sizeof *s);
    s->gen = gen;
}

/* Docks that have just opened (a new screen, or a reload asked for) were
 * read while their windows were still closed, so those reasons only watch.
 * A changed prefs file comes in bursts while it is written, and a moved
 * window reaches its new place a little later; both wait before reading. */
void dock_snap_invalidate(struct DockSnapshot *s, int reason, unsigned now)
{
    unsigned settle = reason == DOCK_SNAP_WALLPAPER ? 300u : reason == DOCK_SNAP_MOVED ? 120u : 0u;
    unsigned watch = reason == DOCK_SNAP_SCREEN ? 30000u :
                     reason == DOCK_SNAP_WALLPAPER ? 20000u :
                     reason == DOCK_SNAP_ASKED ? 5000u : 3000u;
    if (reason == DOCK_SNAP_WALLPAPER || reason == DOCK_SNAP_MOVED) {
        if (!s->due || settle) s->due_at = now + settle;
        s->due = 1;
        s->tries = 0;
    }
    if (!s->watching || !reached(s->watch_until, now + watch)) s->watch_until = now + watch;
    if (!s->watching) s->sample_at = now + DOCK_SNAP_SAMPLE_MS;
    s->watching = 1;
    s->seen_count = 0;
    s->request++;
}

int dock_snap_next(struct DockSnapshot *s, unsigned gen, unsigned now)
{
    if (gen != s->gen) return DOCK_SNAP_IDLE;
    if (s->due && reached(now, s->due_at)) return DOCK_SNAP_CAPTURE;
    if (!s->watching) return DOCK_SNAP_IDLE;
    if (reached(now, s->watch_until)) {
        s->watching = 0;
        /* Still changing when the watch ends: take what is there now. */
        if (s->seen_count) {
            s->seen_count = 0;
            s->due = 1;
            s->due_at = now;
            s->tries = 0;
            return DOCK_SNAP_CAPTURE;
        }
        return DOCK_SNAP_IDLE;
    }
    if (s->ring_known && reached(now, s->sample_at)) return DOCK_SNAP_SAMPLE;
    return DOCK_SNAP_IDLE;
}

static void captured(struct DockSnapshot *s, int result, unsigned long ring, unsigned now);

void dock_snap_captured(struct DockSnapshot *s, unsigned gen, unsigned request, int result,
                        unsigned long ring, unsigned now)
{
    if (gen != s->gen) return;
    int newer = request != s->request, due = s->due;
    unsigned due_at = s->due_at;
    captured(s, result, ring, now);
    /* The picture answers only the requests made before it started. */
    if (newer && due && !s->due) {
        s->due = 1;
        s->due_at = due_at;
    }
}

static void captured(struct DockSnapshot *s, int result, unsigned long ring, unsigned now)
{
    if (result == DOCK_SNAP_BUSY) {
        s->due = 1;
        s->due_at = now + 250u;
        return;
    }
    /* The ring is kept even after a failure, so that a later change of the
     * desktop asks again. */
    s->ring = ring;
    s->ring_known = 1;
    s->seen_count = 0;
    s->sample_at = now + DOCK_SNAP_SAMPLE_MS;
    if (result == DOCK_SNAP_OK) {
        s->valid = 1;
        s->due = 0;
        s->tries = 0;
        return;
    }
    if (++s->tries >= DOCK_SNAP_TRIES) {
        s->due = 0;
        return;
    }
    /* Up to about 20 s in all: a desktop that is still loading its picture
     * after a screen change can take that long on slow hardware. */
    unsigned wait = s->tries < 5 ? 200u << s->tries : 5000u;
    s->due = 1;
    s->due_at = now + (wait > 5000u ? 5000u : wait);
}

/* A change must be seen twice in a row before it counts, so that a desktop
 * caught halfway through painting is not taken. Equal frames say nothing
 * about the desktop being finished; they only say it stopped changing. */
void dock_snap_sampled(struct DockSnapshot *s, unsigned gen, unsigned long ring, unsigned now)
{
    if (gen != s->gen) return;
    s->sample_at = now + DOCK_SNAP_SAMPLE_MS;
    if (ring == s->ring) {
        s->seen_count = 0;
        return;
    }
    if (s->seen_count && ring == s->seen) s->seen_count++;
    else {
        s->seen = ring;
        s->seen_count = 1;
    }
    if (s->seen_count >= 2) {
        s->seen_count = 0;
        s->due = 1;
        s->due_at = now;
        s->tries = 0;
        /* Painting often comes in steps; keep looking a little longer. */
        if (!reached(s->watch_until, now + 3000u)) s->watch_until = now + 3000u;
    }
}

const char *dock_snap_reason(int reason)
{
    switch (reason) {
    case DOCK_SNAP_OPENED: return "opened";
    case DOCK_SNAP_SCREEN: return "screen";
    case DOCK_SNAP_WALLPAPER: return "wallpaper";
    case DOCK_SNAP_MOVED: return "moved";
    case DOCK_SNAP_ASKED: return "asked";
    }
    return "?";
}

const char *dock_snap_result(int result)
{
    switch (result) {
    case DOCK_SNAP_OK: return "ok";
    case DOCK_SNAP_FAILED: return "failed";
    case DOCK_SNAP_UNPAINTED: return "unpainted";
    case DOCK_SNAP_BUSY: return "busy";
    case DOCK_SNAP_COVERED: return "covered";
    }
    return "?";
}
