#include "elapsed.h"
#include "applets/instance.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* A raw counter that is reset backwards (PC EClock after TR_SETSYSTIME) or
 * jumps forwards must not stall or hurry elapsed time. */
int main(void)
{
    struct DockElapsed e;
    memset(&e, 0, sizeof e);
    assert(dock_elapsed_feed(&e, 4005000, 0) == 0);
    assert(dock_elapsed_feed(&e, 4006000, 0) == 1000);
    /* Back by an hour: no time added, and counting goes on from there. */
    assert(dock_elapsed_feed(&e, 406000, 0) == 1000);
    assert(dock_elapsed_feed(&e, 406000 + 250, 0) == 1250);
    /* Forward step within the limit counts; beyond it, not. */
    assert(dock_elapsed_feed(&e, 406000 + 1250, 2000) == 2250);
    assert(dock_elapsed_feed(&e, 4006000 + 3600000, 2000) == 2250);
    assert(dock_elapsed_feed(&e, 4006000 + 3600000 + 10, 2000) == 2260);
    /* Without a limit a forward step is real elapsed time. */
    assert(dock_elapsed_feed(&e, 4006000 + 7200000, 0) == 2260 + 3599990);
    /* Same value: nothing added. */
    assert(dock_elapsed_feed(&e, 4006000 + 7200000, 0) == 2260 + 3599990);

    /* A widget refreshing every 60 s, its clock reset back one hour in the
     * middle of a request: the request is answered and the next refresh
     * comes 60 s later, not an hour later. Raw values in ms. */
    struct DockElapsed c;
    struct DockAppletInstance s;
    struct DockAppletRequest q;
    struct DockAppletSnapshot r;
    memset(&c, 0, sizeof c);
    dock_applet_instance_init(&s, "org.aros.weather", "i-1", "s-1", 60);
    uint64_t raw = 10000000, now = dock_elapsed_feed(&c, raw, 0);
    assert(dock_applet_tick(&s, now, &q) == 1);
    raw -= 3600000;                 /* timesync during the fetch */
    now = dock_elapsed_feed(&c, raw, 0);
    raw += 2000; now = dock_elapsed_feed(&c, raw, 0);
    memset(&r, 0, sizeof r);
    snprintf(r.package, sizeof r.package, "%s", q.package);
    snprintf(r.instance, sizeof r.instance, "%s", q.instance);
    snprintf(r.session, sizeof r.session, "%s", q.session);
    r.generation = q.generation;
    r.status = DA_READY;
    assert(dock_applet_accept(&s, &r, now) == 1);
    raw += 59000; now = dock_elapsed_feed(&c, raw, 0);
    assert(dock_applet_tick(&s, now, &q) == 0);
    raw += 1000; now = dock_elapsed_feed(&c, raw, 0);
    assert(dock_applet_tick(&s, now, &q) == 1);
    /* Forward an hour during the next fetch, the answer never comes: the
     * caller's timeout counts elapsed time, so it is not taken as expired
     * at once by a calendar step (raw stays monotonic here; the step is in
     * the calendar clock only). */
    uint64_t asked = now;
    raw += 5000; now = dock_elapsed_feed(&c, raw, 0);
    assert(now - asked == 5000);
    puts("elapsed: ok");
    return 0;
}
