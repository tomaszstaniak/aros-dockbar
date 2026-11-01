#include "interaction.h"
#include <string.h>

static void clear_target(struct DockDrag *drag)
{
    drag->target_container = -1;
    drag->target_index = -1;
    drag->armed = 0;
    drag->since = 0;
}

void dock_drag_cancel(struct DockDrag *drag)
{
    if (!drag)
        return;
    memset(drag, 0, sizeof(*drag));
    drag->container = -1;
    drag->index = -1;
    clear_target(drag);
}

void dock_drag_begin(struct DockDrag *drag, int kind, int container,
                     int index, int x, int y)
{
    dock_drag_cancel(drag);
    if (!drag || (kind != 1 && kind != 2))
        return;
    drag->kind = kind;
    drag->container = container;
    drag->index = index;
    drag->start_x = x;
    drag->start_y = y;
}

void dock_drag_update(struct DockDrag *drag, int x, int y,
                      int target_container, int target_index, int center,
                      unsigned int now_ms)
{
    long long dx, dy;
    if (!drag || !drag->kind)
        return;
    dx = (long long)x - drag->start_x;
    dy = (long long)y - drag->start_y;
    if (dx >= 6 || dx <= -6 || dy >= 6 || dy <= -6)
        drag->active = 1;
    if (!drag->active || drag->kind != 1 || !center || target_index < 0 ||
        (target_container == drag->container && target_index == drag->index)) {
        clear_target(drag);
        return;
    }
    if (drag->target_index != target_index ||
        drag->target_container != target_container) {
        drag->target_container = target_container;
        drag->target_index = target_index;
        drag->since = now_ms;
        drag->armed = 0;
    } else if ((unsigned int)(now_ms - drag->since) >= 500u) {
        drag->armed = 1;
    }
}

void dock_drag_state_init(struct DockDragState *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    dock_drag_cancel(&s->own);
    s->spring_view = s->spring_item = -1;
}

int dock_drag_state_poll(struct DockDragState *s, int left_held)
{
    int was;
    if (!s) return 0;
    was = s->foreign;
    s->foreign = left_held && !s->own.kind;
    if (!s->foreign) s->spring_view = s->spring_item = -1;
    return was && !s->foreign;
}

int dock_drag_state_busy(const struct DockDragState *s)
{
    return s && (s->foreign || (s->own.kind && s->own.active));
}

struct DockDrag dock_drag_state_end(struct DockDragState *s)
{
    struct DockDrag was;
    dock_drag_cancel(&was);
    if (!s) return was;
    was = s->own;
    dock_drag_cancel(&s->own);
    s->spring_view = s->spring_item = -1;
    return was;
}

int dock_front_pick(const void *front, const void *const *layers, int count,
                    const void *own)
{
    if (!front) return -1;
    if (own && front == own) return DOCK_FRONT_OWN;
    for (int i = 0; i < count; i++)
        if (layers[i] && layers[i] == front) return i;
    return -1;
}
