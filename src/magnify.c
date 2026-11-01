#include "magnify.h"
#include <stdint.h>
#include <string.h>

static int magnifies(const struct DockSlot *s)
{
    return dock_kind_shown(s->kind) && !s->button && s->icon.w > 0;
}

int dock_hover_item(const struct DockLayout *l, int i)
{
    return l && i >= 0 && i < l->count && magnifies(&l->slots[i]);
}

/* Main-axis start and length of a rectangle, and its cross-axis ones. */
static int m0(int v, const struct DockRect *r) { return v ? r->y : r->x; }
static int ml(int v, const struct DockRect *r) { return v ? r->h : r->w; }
static int c0(int v, const struct DockRect *r) { return v ? r->x : r->y; }
static int cl(int v, const struct DockRect *r) { return v ? r->w : r->h; }

static struct DockRect rect(int v, int main, int cross, int main_len, int cross_len)
{
    struct DockRect r;
    if (v) { r.x = cross; r.y = main; r.w = cross_len; r.h = main_len; }
    else { r.x = main; r.y = cross; r.w = main_len; r.h = cross_len; }
    return r;
}

/* Where slot 'i' may draw when magnified: along the bar its own slot (not
 * the spacing between slots, which no click reaches), across the bar the
 * bar less its margins. */
static void bounds(const struct DockLayout *l, int i, int *lo, int *hi, int *clo, int *chi)
{
    int v = l->orientation, cross = v ? l->width : l->height;
    const struct DockSlot *s = &l->slots[i];
    *lo = m0(v, &s->slot);
    *hi = m0(v, &s->slot) + ml(v, &s->slot);
    *clo = DOCK_HIGHLIGHT_MARGIN;
    *chi = cross - DOCK_HIGHLIGHT_MARGIN;
}

/* An icon box of side 'side' centred on the rest icon, moved back inside
 * the room where a label or an edge is closer on one side. */
static struct DockRect place(int v, const struct DockSlot *s, int side,
                             int lo, int hi, int clo, int chi)
{
    int grow = side - ml(v, &s->icon);
    int a = m0(v, &s->icon) - grow / 2, c = c0(v, &s->icon) - grow / 2;
    if (a + side > hi) a = hi - side;
    if (a < lo) a = lo;
    if (c + side > chi) c = chi - side;
    if (c < clo) c = clo;
    return rect(v, a, c, side, side);
}

/* The room's cross-axis end, before an always-shown label. */
static void label_cut(int v, const struct DockSlot *s, int *hi, int *chi)
{
    if (!s->label.w || !s->label.h) return;
    /* Under the icon: across a horizontal bar, along a vertical one. */
    if (v) { if (*hi > m0(v, &s->label)) *hi = m0(v, &s->label); }
    else if (*chi > c0(v, &s->label)) *chi = c0(v, &s->label);
}

static int largest(const struct DockLayout *l, int i,
                   int *lo, int *hi, int *clo, int *chi)
{
    int v = l->orientation, side;
    const struct DockSlot *s = &l->slots[i];
    bounds(l, i, lo, hi, clo, chi);
    label_cut(v, s, hi, chi);
    side = *hi - *lo < *chi - *clo ? *hi - *lo : *chi - *clo;
    /* Never smaller than the rest icon, and growth in whole steps. */
    if (side < ml(v, &s->icon)) side = ml(v, &s->icon);
    side -= (side - ml(v, &s->icon)) % DOCK_MAGNIFY_STEP;
    return side;
}

struct DockRect dock_magnify_room(const struct DockLayout *l, const struct DockStyle *st, int i)
{
    struct DockRect none = {0, 0, 0, 0};
    int lo, hi, clo, chi, side;
    (void)st;
    if (!l || i < 0 || i >= l->count || !magnifies(&l->slots[i])) return none;
    side = largest(l, i, &lo, &hi, &clo, &chi);
    return place(l->orientation, &l->slots[i], side, lo, hi, clo, chi);
}

/* 1000 at distance 0, falling smoothly (smoothstep) to 0 at the radius. */
static int falloff(int distance, int radius)
{
    int64_t t;
    if (distance < 0) distance = -distance;
    if (distance >= radius) return 0;
    t = (int64_t)distance * 1000 / radius;
    return (int)(1000 - (3 * t * t * 1000 - 2 * t * t * t) / 1000000);
}

int dock_magnify_frame(const struct DockLayout *l, const struct DockStyle *st,
                       const struct DockMagnifyParams *p, int pointer, int strength,
                       struct DockMagnifyFrame *f)
{
    int v, radius;
    if (!l || !st || !p || !f || p->percent < 0 || p->percent > 100 || p->radius < 0 ||
        strength < 0 || strength > 1000 || l->count < 0 || l->count > DOCK_MAX_ITEMS)
        return 0;
    v = l->orientation;
    radius = p->radius ? p->radius : st->icon * 5 / 2;
    memset(f, 0, sizeof *f);
    f->count = l->count;
    for (int i = 0; i < l->count; i++) {
        const struct DockSlot *s = &l->slots[i];
        int lo, hi, clo, chi, side, want, grow, centre;
        f->icon[i] = s->icon;
        f->highlight[i] = s->highlight;
        if (!magnifies(s) || !strength || !p->percent) continue;
        side = largest(l, i, &lo, &hi, &clo, &chi);
        want = ml(v, &s->icon) * p->percent / 100;
        if (want > side - ml(v, &s->icon)) want = side - ml(v, &s->icon);
        centre = m0(v, &s->icon) + ml(v, &s->icon) / 2;
        grow = (int)((int64_t)want * falloff(pointer - centre, radius) / 1000 * strength / 1000);
        /* To the nearest step, never past the room. */
        grow = (grow + DOCK_MAGNIFY_STEP / 2) / DOCK_MAGNIFY_STEP * DOCK_MAGNIFY_STEP;
        if (grow > want) grow = want - want % DOCK_MAGNIFY_STEP;
        if (grow <= 0) continue;
        f->grown[i] = grow;
        f->icon[i] = place(v, s, ml(v, &s->icon) + grow, lo, hi, clo, chi);
        /* The highlight around the grown icon, kept inside the slot's room
         * and the bar's margins like the rest highlight. */
        {
            struct DockRect h = f->icon[i];
            int a0, a1, b0, b1;
            h.x -= DOCK_HIGHLIGHT_PAD; h.y -= DOCK_HIGHLIGHT_PAD;
            h.w += 2 * DOCK_HIGHLIGHT_PAD; h.h += 2 * DOCK_HIGHLIGHT_PAD;
            bounds(l, i, &lo, &hi, &clo, &chi);
            a0 = m0(v, &h); a1 = a0 + ml(v, &h); b0 = c0(v, &h); b1 = b0 + cl(v, &h);
            if (a0 < lo) a0 = lo;
            if (a1 > hi) a1 = hi;
            if (b0 < clo) b0 = clo;
            if (b1 > chi) b1 = chi;
            /* Towards an always-shown label, no further than at rest: the
             * label stays readable and in its place. */
            if (s->label.w && s->label.h) {
                const struct DockRect *r = &s->highlight;
                if (v) { if (a1 > r->y + r->h) a1 = r->y + r->h; }
                else if (b1 > r->y + r->h) b1 = r->y + r->h;
            }
            f->highlight[i] = rect(v, a0, b0, a1 - a0, b1 - b0);
        }
    }
    return 1;
}

static int same(const struct DockRect *a, const struct DockRect *b)
{
    return a->x == b->x && a->y == b->y && a->w == b->w && a->h == b->h;
}

int dock_magnify_changed(const struct DockMagnifyFrame *a, const struct DockMagnifyFrame *b, int i)
{
    if (!a || !b || i < 0 || i >= a->count || i >= b->count) return 1;
    return !same(&a->icon[i], &b->icon[i]) || !same(&a->highlight[i], &b->highlight[i]);
}
