#include "interaction.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static void threshold_and_click(void)
{
    struct DockDrag d;
    dock_drag_begin(&d,1,0,2,100,100);
    assert(d.kind==1 && d.container==0 && d.index==2 && !d.active);
    assert(d.target_container==-1 && d.target_index==-1 && !d.armed);
    dock_drag_update(&d,105,105,0,3,1,0);
    assert(!d.active && !d.armed && d.target_index==-1);
    dock_drag_update(&d,106,100,0,3,1,10);
    assert(d.active && !d.armed && d.target_index==3 && d.since==10);
    dock_drag_update(&d,100,100,0,3,1,510);
    assert(d.active && d.armed);
    dock_drag_begin(&d,1,0,2,100,100);
    dock_drag_update(&d,100,94,0,2,1,10);
    assert(d.active && !d.armed && d.target_index==-1);
}

static void dwell_resets_and_cancel(void)
{
    struct DockDrag d;
    dock_drag_begin(&d,1,0,2,0,0);
    dock_drag_update(&d,8,0,0,3,1,0);
    assert(d.since==0 && !d.armed);
    dock_drag_update(&d,9,0,0,3,1,499);
    assert(!d.armed);
    dock_drag_update(&d,9,0,0,3,1,500);
    assert(d.armed);
    dock_drag_update(&d,9,0,0,3,0,501);
    assert(!d.armed && d.target_container==-1 && d.target_index==-1);
    dock_drag_update(&d,9,0,0,3,1,600);
    dock_drag_update(&d,9,0,0,4,1,1099);
    assert(!d.armed && d.target_index==4 && d.since==1099);
    dock_drag_update(&d,9,0,1,4,1,1599);
    assert(!d.armed && d.target_container==1 && d.since==1599);
    dock_drag_update(&d,9,0,1,4,1,2099);
    assert(d.armed);
    dock_drag_update(&d,9,0,-1,-1,1,2100);
    assert(!d.armed && d.target_index==-1);
    dock_drag_cancel(&d);
    assert(!d.kind && !d.active && !d.armed && d.target_index==-1);
    dock_drag_update(&d,100,100,0,3,1,5000);
    assert(!d.active && !d.armed);
}

static void wrap_and_bar(void)
{
    struct DockDrag d;
    unsigned int since=UINT_MAX-100u;
    dock_drag_begin(&d,1,0,0,0,0);
    dock_drag_update(&d,6,0,-1,0,1,since);
    assert(d.active && !d.armed && d.target_container==-1 && d.target_index==0);
    dock_drag_update(&d,6,0,-1,0,1,since+499u);
    assert(!d.armed);
    dock_drag_update(&d,6,0,-1,0,1,since+500u);
    assert(d.armed);
    dock_drag_begin(&d,2,0,0,0,0);
    dock_drag_update(&d,-6,0,0,1,1,0);
    dock_drag_update(&d,-6,0,0,1,1,1000);
    assert(d.active && !d.armed && d.target_index==-1);
    dock_drag_begin(&d,1,0,0,INT_MIN,INT_MIN);
    dock_drag_update(&d,INT_MAX,INT_MAX,0,1,1,0);
    assert(d.active);
}

/* One record for own and foreign drags: a press is no drag until it moves;
 * the button held without an own drag is foreign; its end is reported once;
 * ending clears the own drag and the spring dwell and hands back what was. */
static void one_state(void)
{
    struct DockDragState s;
    dock_drag_state_init(&s);
    assert(!s.own.kind && !s.foreign && s.spring_view == -1 && !dock_drag_state_busy(&s));
    assert(!dock_drag_state_poll(&s, 0));
    dock_drag_begin(&s.own, 1, 0, 2, 10, 10);
    assert(!dock_drag_state_poll(&s, 1) && !s.foreign);
    assert(!dock_drag_state_busy(&s));          /* pressed, not moved */
    dock_drag_update(&s.own, 30, 10, -1, -1, 0, 0);
    assert(dock_drag_state_busy(&s));
    struct DockDrag was = dock_drag_state_end(&s);
    assert(was.kind == 1 && was.active && was.index == 2);
    assert(!s.own.kind && !s.own.active && s.own.index == -1 && !dock_drag_state_busy(&s));
    /* Button still held after a cancel: from now on it is not ours. */
    assert(!dock_drag_state_poll(&s, 1) && s.foreign && dock_drag_state_busy(&s));
    s.spring_view = 1; s.spring_item = 3; s.spring_since = 5;
    assert(!dock_drag_state_poll(&s, 1) && s.spring_view == 1);
    assert(dock_drag_state_poll(&s, 0) == 1);   /* ended: paint once */
    assert(!dock_drag_state_poll(&s, 0));       /* reported once only */
    assert(s.spring_view == -1 && s.spring_item == -1 && !dock_drag_state_busy(&s));
    dock_drag_begin(&s.own, 2, 0, -1, 0, 0);
    s.spring_view = 0;
    was = dock_drag_state_end(&s);
    assert(was.kind == 2 && !was.active && s.spring_view == -1);
    was = dock_drag_state_end(NULL);
    assert(!was.kind);
}

static void front_pick(void)
{
    int a = 0, b = 0, other = 0, tip = 0;
    const void *layers[3] = {&a, NULL, &b};
    assert(dock_front_pick(&a, layers, 3, &tip) == 0);
    assert(dock_front_pick(&b, layers, 3, &tip) == 2);
    /* Another window in front, or nothing there */
    assert(dock_front_pick(&other, layers, 3, &tip) == -1);
    assert(dock_front_pick(NULL, layers, 3, &tip) == -1);
    /* A dock without a window never matches a missing layer */
    assert(dock_front_pick(NULL, layers, 3, NULL) == -1);
    assert(dock_front_pick(&tip, layers, 3, &tip) == DOCK_FRONT_OWN);
    assert(dock_front_pick(&tip, layers, 3, NULL) == -1);
}

int main(void)
{
    threshold_and_click();
    dwell_resets_and_cancel();
    wrap_and_bar();
    one_state();
    front_pick();
    puts("PASS interaction: click threshold, persistent drag, dwell reset, cancellation, time wrap, one drag state, front layer");
    return 0;
}
