#include "snapshot.h"
#include <assert.h>
#include <stdio.h>

/* Runs the schedule like the main loop does, every 40 ms, answering samples
 * with an unchanged desktop, until it asks for a picture or the time is up. */
static int run(struct DockSnapshot *s, unsigned gen, unsigned *now, unsigned until)
{
    for (; (int)(until - *now) >= 0; *now += 40) {
        int a = dock_snap_next(s, gen, *now);
        if (a == DOCK_SNAP_SAMPLE) dock_snap_sampled(s, gen, s->ring, *now);
        else if (a != DOCK_SNAP_IDLE) return a;
    }
    return DOCK_SNAP_IDLE;
}

int main(void)
{
    struct DockSnapshot s;
    /* Close to the wrap of a 32-bit millisecond clock on purpose. */
    unsigned t0 = 0xfffff000u, now;

    /* Screen reopened: the dock read the (grey) desktop while opening; it
     * then watches. The ring stays the same: nothing more is read, and
     * after 30 s it stops sampling altogether. */
    dock_snap_start(&s, 2);
    now = t0;
    dock_snap_captured(&s, 2, s.request, DOCK_SNAP_OK, 111, now);
    dock_snap_invalidate(&s, DOCK_SNAP_SCREEN, now);
    int samples = 0;
    for (; (int)(t0 + 31000 - now) >= 0; now += 40) {
        int a = dock_snap_next(&s, 2, now);
        assert(a != DOCK_SNAP_CAPTURE);
        if (a == DOCK_SNAP_SAMPLE) { samples++; dock_snap_sampled(&s, 2, 111, now); }
    }
    assert(samples > 60 && samples < 80 && !s.watching);
    for (int i = 0; i < 1000; i++, now += 40) assert(dock_snap_next(&s, 2, now) == DOCK_SNAP_IDLE);

    /* Wallpaper painted 8 s after the reset: one changed sample is not
     * enough (it may be half painted), the second equal one is. */
    dock_snap_start(&s, 3);
    now = t0;
    dock_snap_captured(&s, 3, s.request, DOCK_SNAP_OK, 111, now);
    dock_snap_invalidate(&s, DOCK_SNAP_SCREEN, now);
    int captures = 0;
    for (; (int)(t0 + 12000 - now) >= 0; now += 40) {
        int a = dock_snap_next(&s, 3, now);
        unsigned long ring = (int)(now - t0) < 8000 ? 111 : (int)(now - t0) < 8400 ? 150 : 222;
        if (a == DOCK_SNAP_SAMPLE) dock_snap_sampled(&s, 3, ring, now);
        if (a == DOCK_SNAP_CAPTURE) {
            captures++;
            assert((int)(now - t0) >= 8400);
            dock_snap_captured(&s, 3, s.request, DOCK_SNAP_OK, ring, now);
        }
    }
    assert(captures == 1 && s.ring == 222);

    /* A burst of wallpaper notices makes one picture, 300 ms after the last. */
    dock_snap_start(&s, 4);
    now = t0;
    dock_snap_captured(&s, 4, s.request, DOCK_SNAP_OK, 5, now);
    for (int i = 0; i < 5; i++, now += 100) dock_snap_invalidate(&s, DOCK_SNAP_WALLPAPER, now);
    unsigned last = now - 100;
    assert(run(&s, 4, &now, now + 1000) == DOCK_SNAP_CAPTURE);
    assert((int)(now - last) >= 300 && (int)(now - last) < 340);
    dock_snap_captured(&s, 4, s.request, DOCK_SNAP_OK, 5, now);
    assert(!s.due);

    /* Failed reads and unpainted desktops are retried with growing waits,
     * then given up; the earlier picture of this screen stays valid. */
    dock_snap_invalidate(&s, DOCK_SNAP_MOVED, now);
    int tries = 0;
    unsigned prev = now;
    while (run(&s, 4, &now, now + 10000) == DOCK_SNAP_CAPTURE) {
        if (tries > 1) assert(now - prev >= (200u << (tries > 4 ? 4 : tries)) - 40 && now - prev <= 5040);
        prev = now;
        dock_snap_captured(&s, 4, s.request, tries % 3 == 2 ? DOCK_SNAP_COVERED : tries % 3 ? DOCK_SNAP_UNPAINTED : DOCK_SNAP_FAILED, 5, now);
        tries++;
    }
    assert(tries == DOCK_SNAP_TRIES && s.valid && !s.due);

    /* A first picture that fails leaves nothing valid: main shows the
     * opaque dock. A busy moment is retried without counting. */
    dock_snap_start(&s, 5);
    now = t0;
    dock_snap_captured(&s, 5, s.request, DOCK_SNAP_FAILED, 9, now);
    assert(!s.valid && s.due);
    assert(run(&s, 5, &now, now + 1000) == DOCK_SNAP_CAPTURE);
    for (int i = 0; i < 20; i++) {
        dock_snap_captured(&s, 5, s.request, DOCK_SNAP_BUSY, 9, now);
        assert(s.tries == 1);
        assert(run(&s, 5, &now, now + 1000) == DOCK_SNAP_CAPTURE);
    }
    dock_snap_captured(&s, 5, s.request, DOCK_SNAP_OK, 9, now);
    assert(s.valid && !s.due);

    /* Answers for an earlier screen change nothing, and an old schedule
     * asks nothing of a new screen. */
    dock_snap_start(&s, 6);
    now = t0;
    dock_snap_captured(&s, 5, s.request, DOCK_SNAP_OK, 1, now);
    dock_snap_sampled(&s, 5, 2, now);
    assert(!s.valid && !s.ring_known);
    dock_snap_invalidate(&s, DOCK_SNAP_MOVED, now);
    now += 120;
    assert(dock_snap_next(&s, 5, now) == DOCK_SNAP_IDLE);
    assert(dock_snap_next(&s, 6, now) == DOCK_SNAP_CAPTURE);

    /* Desktop still changing when the watch ends: the last state is taken. */
    dock_snap_start(&s, 7);
    now = t0;
    dock_snap_captured(&s, 7, s.request, DOCK_SNAP_OK, 1, now);
    dock_snap_invalidate(&s, DOCK_SNAP_MOVED, now);
    assert(run(&s, 7, &now, now + 200) == DOCK_SNAP_CAPTURE);
    dock_snap_captured(&s, 7, s.request, DOCK_SNAP_OK, 1, now);
    unsigned long ring = 1;
    int a;
    for (;; now += 40) {
        a = dock_snap_next(&s, 7, now);
        if (a == DOCK_SNAP_SAMPLE) dock_snap_sampled(&s, 7, ++ring, now);
        else if (a != DOCK_SNAP_IDLE) break;
    }
    assert(a == DOCK_SNAP_CAPTURE && !s.watching);
    dock_snap_captured(&s, 7, s.request, DOCK_SNAP_OK, ring, now);
    assert(run(&s, 7, &now, now + 20000) == DOCK_SNAP_IDLE);

    /* A wallpaper notice while a picture is being taken (windows closed):
     * the picture succeeds, but it answers only the earlier request; the
     * newer one still asks for its own picture after its settle time. */
    dock_snap_start(&s, 8);
    now = t0;
    dock_snap_captured(&s, 8, s.request, DOCK_SNAP_OK, 1, now);
    dock_snap_invalidate(&s, DOCK_SNAP_WALLPAPER, now);
    assert(run(&s, 8, &now, now + 1000) == DOCK_SNAP_CAPTURE);
    unsigned started = s.request;
    now += 200;
    dock_snap_invalidate(&s, DOCK_SNAP_WALLPAPER, now);  /* arrives mid-picture */
    unsigned second = now;
    now += 100;
    dock_snap_captured(&s, 8, started, DOCK_SNAP_OK, 1, now);
    assert(s.valid && s.due);
    assert(run(&s, 8, &now, now + 1000) == DOCK_SNAP_CAPTURE && (int)(now - second) >= 300);
    dock_snap_captured(&s, 8, s.request, DOCK_SNAP_OK, 1, now);
    assert(!s.due);
    /* Several notices during one picture still make one more picture. */
    dock_snap_invalidate(&s, DOCK_SNAP_WALLPAPER, now);
    assert(run(&s, 8, &now, now + 1000) == DOCK_SNAP_CAPTURE);
    started = s.request;
    for (int i = 0; i < 3; i++) dock_snap_invalidate(&s, DOCK_SNAP_WALLPAPER, now += 50);
    dock_snap_captured(&s, 8, started, DOCK_SNAP_FAILED, 1, now);
    assert(s.due);
    assert(run(&s, 8, &now, now + 6000) == DOCK_SNAP_CAPTURE);
    dock_snap_captured(&s, 8, s.request, DOCK_SNAP_OK, 1, now);
    assert(!s.due);

    puts("PASS snapshot: requests during a picture kept, watch ends, settled change recaptures, bursts coalesce, retries bounded, stale screens ignored");
    return 0;
}
