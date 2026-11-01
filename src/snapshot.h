#ifndef DOCK_SNAPSHOT_H
#define DOCK_SNAPSHOT_H

/* When to read the desktop behind one see-through dock again. Pure: main
 * reads pixels; this decides when, and drops answers that belong to an
 * earlier screen. Times are milliseconds and may wrap.
 *
 * Nothing tells a program that the desktop has finished painting: a screen
 * reopens grey and the picture can follow seconds later, and a changed
 * wallpaper is only a changed prefs file. So every event starts a limited
 * watch of the desktop just around the dock; a change there that has stopped
 * changing asks for a new picture. Outside a watch nothing is read. */

enum { DOCK_SNAP_IDLE, DOCK_SNAP_CAPTURE, DOCK_SNAP_SAMPLE };
enum { DOCK_SNAP_OPENED, DOCK_SNAP_SCREEN, DOCK_SNAP_WALLPAPER, DOCK_SNAP_MOVED, DOCK_SNAP_ASKED };
/* OK: picture taken. FAILED: the read did not return every pixel.
 * UNPAINTED: the desktop had not repainted where the dock was.
 * COVERED: another window lies over the dock, so nothing under it can be
 * told apart; retried like a failure, so that a dock left covered is not
 * read again and again.
 * BUSY: not possible now (a drag, a menu over the dock); no retry counted. */
enum { DOCK_SNAP_OK, DOCK_SNAP_FAILED, DOCK_SNAP_UNPAINTED, DOCK_SNAP_BUSY, DOCK_SNAP_COVERED };

#define DOCK_SNAP_SAMPLE_MS 400u
#define DOCK_SNAP_TRIES 8

struct DockSnapshot {
    unsigned gen;             /* screen the state belongs to */
    int valid;                /* a picture of this screen exists */
    int due;                  /* a picture is wanted from due_at on */
    unsigned due_at;
    unsigned watch_until, sample_at;
    int watching;
    unsigned long ring;       /* desktop around the dock when last taken */
    int ring_known;
    unsigned long seen;       /* a differing ring, and how often in a row */
    int seen_count;
    int tries;
    unsigned request;         /* counts invalidations; see dock_snap_captured */
};

void dock_snap_start(struct DockSnapshot *s, unsigned gen);
void dock_snap_invalidate(struct DockSnapshot *s, int reason, unsigned now);
int dock_snap_next(struct DockSnapshot *s, unsigned gen, unsigned now);
/* request: s->request when the picture was started. A picture is taken over
 * some time (the dock window stays closed until the desktop has repainted);
 * an invalidation during that time keeps its own request alive, even when
 * the earlier picture succeeds. */
void dock_snap_captured(struct DockSnapshot *s, unsigned gen, unsigned request, int result,
                        unsigned long ring, unsigned now);
void dock_snap_sampled(struct DockSnapshot *s, unsigned gen, unsigned long ring, unsigned now);
const char *dock_snap_reason(int reason);
const char *dock_snap_result(int result);

#endif
