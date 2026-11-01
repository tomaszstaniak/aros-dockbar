#include "hover.h"
#include "docks.h"
#include <stdint.h>
#include <string.h>

int dock_lift_sign(int placement, int orientation)
{
    if (orientation == DOCK_VERTICAL) {
        if (placement == DOCK_PLACE_RIGHT) return -1;
        return 1;
    }
    if (placement == DOCK_PLACE_TOP) return 1;
    return -1;
}

int dock_lift_room(const struct DockLayout *l, int i, int sign)
{
    if (!l || i < 0 || i >= l->count || (sign != 1 && sign != -1) || !dock_hover_item(l, i)) return 0;
    const struct DockSlot *s = &l->slots[i];
    int v = l->orientation, cross = v ? l->width : l->height;
    int start = v ? s->icon.x : s->icon.y, len = v ? s->icon.w : s->icon.h, room;
    if (sign < 0) room = start - DOCK_HIGHLIGHT_MARGIN;
    else {
        int end = cross - DOCK_HIGHLIGHT_MARGIN;
        /* A label always shown under a horizontal bar's icon is across it. */
        if (!v && s->label.w && s->label.h && s->label.y < end) end = s->label.y;
        room = end - (start + len);
    }
    if (room > DOCK_LIFT_MAX) room = DOCK_LIFT_MAX;
    return room > 0 ? room : 0;
}

int dock_lift_frame(const struct DockLayout *l, int sign, const int progress[DOCK_MAX_ITEMS],
                    struct DockMagnifyFrame *f)
{
    if (!l || !progress || !f || l->count < 0 || l->count > DOCK_MAX_ITEMS || (sign != 1 && sign != -1)) return 0;
    memset(f, 0, sizeof *f);
    f->count = l->count;
    for (int i = 0; i < l->count; i++) {
        int p = progress[i] < 0 ? 0 : progress[i] > 1000 ? 1000 : progress[i];
        int by = (dock_lift_room(l, i, sign) * p + 500) / 1000;
        f->icon[i] = l->slots[i].icon;
        f->highlight[i] = l->slots[i].highlight;
        if (l->orientation) f->icon[i].x += sign * by;
        else f->icon[i].y += sign * by;
    }
    return 1;
}

int dock_hover_ramp(int current, int target, unsigned elapsed_ms)
{
    /* Rounded up, so that short frames add up to no more than the duration. */
    int64_t step = ((int64_t)elapsed_ms * 1000 + DOCK_HOVER_RAMP_MS - 1) / DOCK_HOVER_RAMP_MS;
    if (current < 0) current = 0;
    if (current > 1000) current = 1000;
    if (target > current) return current + step >= target ? target : current + (int)step;
    if (target < current) return current - step <= target ? target : current - (int)step;
    return current;
}

int dock_hover_ease(int progress)
{
    int64_t t = progress < 0 ? 0 : progress > 1000 ? 1000 : progress;
    return (int)((3 * t * t * 1000 - 2 * t * t * t) / 1000000);
}

static void join(struct DockRect *u, const struct DockRect *r)
{
    if (r->w <= 0 || r->h <= 0) return;
    if (u->w <= 0 || u->h <= 0) { *u = *r; return; }
    int x0 = u->x < r->x ? u->x : r->x, y0 = u->y < r->y ? u->y : r->y;
    int x1 = u->x + u->w > r->x + r->w ? u->x + u->w : r->x + r->w;
    int y1 = u->y + u->h > r->y + r->h ? u->y + u->h : r->y + r->h;
    u->x = x0; u->y = y0; u->w = x1 - x0; u->h = y1 - y0;
}

struct DockRect dock_hover_damage(const struct DockMagnifyFrame *a, const struct DockMagnifyFrame *b, int i)
{
    struct DockRect u = {0, 0, 0, 0};
    if (!a || !b || i < 0 || i >= a->count || i >= b->count || !dock_magnify_changed(a, b, i)) return u;
    join(&u, &a->icon[i]);
    join(&u, &a->highlight[i]);
    join(&u, &b->icon[i]);
    join(&u, &b->highlight[i]);
    return u;
}
