#include "hover.h"
#include "docks.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct DockModel m;
static struct DockStyle style(int icon, int orientation, int labels)
{
    struct DockStyle s;
    memset(&s, 0, sizeof s);
    s.icon = icon; s.orientation = orientation; s.labels = labels; s.text_height = 14;
    s.content_w[5] = 50; s.content_h[5] = 30;   /* the clock */
    return s;
}
/* Icons, a group, a separator, a drawer, a space, a clock, a text button. */
static void sample(struct DockStyle *s)
{
    enum DockKind kinds[] = {DOCK_APP, DOCK_GROUP, DOCK_SEPARATOR, DOCK_DRAWER, DOCK_SPACER, DOCK_CLOCK, DOCK_APP, DOCK_COMMAND, DOCK_APP};
    memset(&m, 0, sizeof m);
    for (int i = 0; i < 9; i++) m.items[m.count++].kind = kinds[i];
    m.items[1].group = 1;
    s->present[6] = DOCK_PRESENT_TEXT; s->text_w[6] = 60;
}
static int same(struct DockRect a, struct DockRect b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }
static int inside(struct DockRect in, struct DockRect out)
{
    return in.x >= out.x && in.y >= out.y && in.x + in.w <= out.x + out.w && in.y + in.h <= out.y + out.h;
}

static void direction(void)
{
    assert(dock_lift_sign(DOCK_PLACE_BOTTOM, DOCK_HORIZONTAL) == -1);  /* up */
    assert(dock_lift_sign(DOCK_PLACE_TOP, DOCK_HORIZONTAL) == 1);      /* down */
    assert(dock_lift_sign(DOCK_PLACE_LEFT, DOCK_VERTICAL) == 1);       /* right */
    assert(dock_lift_sign(DOCK_PLACE_RIGHT, DOCK_VERTICAL) == -1);     /* left */
    assert(dock_lift_sign(DOCK_PLACE_FREE, DOCK_HORIZONTAL) == -1);
    assert(dock_lift_sign(DOCK_PLACE_FREE, DOCK_VERTICAL) == 1);
    /* A bar that does not lie along its edge lifts as a free one. */
    assert(dock_lift_sign(DOCK_PLACE_LEFT, DOCK_HORIZONTAL) == -1);
    assert(dock_lift_sign(DOCK_PLACE_TOP, DOCK_VERTICAL) == 1);
}

/* Every size, orientation, label mode, direction and progress: the icon
 * keeps its size, moves across the bar only, by at most DOCK_LIFT_MAX,
 * stays inside its slot and the bar's margins and off its label; the
 * highlight does not move; what is drawn is what a click there hits; other
 * items keep their rest geometry. */
static void stays_in_slot(void)
{
    int sizes[] = {32, 40, 48, 57, 72};
    for (unsigned z = 0; z < sizeof sizes / sizeof *sizes; z++)
        for (int o = 0; o < 2; o++)
            for (int labels = 0; labels < 3; labels++)
                for (int sign = -1; sign <= 1; sign += 2)
                    for (int p = 0; p <= 1000; p += 125) {
                        struct DockStyle s = style(sizes[z], o, labels);
                        struct DockLayout l;
                        struct DockMagnifyFrame f;
                        int progress[DOCK_MAX_ITEMS];
                        sample(&s);
                        assert(dock_layout(&m, &s, &l));
                        for (int i = 0; i < DOCK_MAX_ITEMS; i++) progress[i] = p;
                        assert(dock_lift_frame(&l, sign, progress, &f) && f.count == l.count);
                        int cross = o ? l.width : l.height;
                        for (int i = 0; i < l.count; i++) {
                            const struct DockSlot *sl = &l.slots[i];
                            assert(same(f.highlight[i], sl->highlight) && !f.grown[i]);
                            if (!dock_hover_item(&l, i)) {
                                assert(same(f.icon[i], sl->icon) && !dock_lift_room(&l, i, sign));
                                continue;
                            }
                            struct DockRect bar = o ? (struct DockRect){DOCK_HIGHLIGHT_MARGIN, sl->slot.y, cross - 2 * DOCK_HIGHLIGHT_MARGIN, sl->slot.h}
                                                    : (struct DockRect){sl->slot.x, DOCK_HIGHLIGHT_MARGIN, sl->slot.w, cross - 2 * DOCK_HIGHLIGHT_MARGIN};
                            int dx = f.icon[i].x - sl->icon.x, dy = f.icon[i].y - sl->icon.y;
                            assert(f.icon[i].w == sl->icon.w && f.icon[i].h == sl->icon.h);
                            assert(o ? dy == 0 : dx == 0);
                            int by = o ? dx : dy;
                            assert(by * sign >= 0 && by * sign <= DOCK_LIFT_MAX);
                            assert(by * sign == (dock_lift_room(&l, i, sign) * p + 500) / 1000);
                            assert(inside(f.icon[i], bar) && inside(f.icon[i], sl->slot));
                            if (sl->label.w) assert(f.icon[i].y + f.icon[i].h <= sl->label.y || o);
                            for (int y = f.icon[i].y; y < f.icon[i].y + f.icon[i].h; y += 3)
                                for (int x = f.icon[i].x; x < f.icon[i].x + f.icon[i].w; x += 3)
                                    assert(dock_layout_item(&l, x, y) == i);
                        }
                    }
}

/* The default dock: 4 px up on a bottom bar of 48 px icons, and only 1 px
 * down on a top bar with labels always under the icons. */
static void rooms(void)
{
    struct DockStyle s = style(48, DOCK_HORIZONTAL, DOCK_LABELS_HOVER);
    struct DockLayout l;
    sample(&s);
    assert(dock_layout(&m, &s, &l));
    assert(dock_lift_room(&l, 0, -1) == 4 && dock_lift_room(&l, 0, 1) == 4);
    assert(dock_lift_room(&l, 5, -1) == 0 && dock_lift_room(&l, 6, -1) == 0);  /* clock, text button */
    assert(dock_lift_room(&l, 2, -1) == 0 && dock_lift_room(&l, 4, -1) == 0);  /* separator, space */
    assert(dock_lift_room(&l, 0, 0) == 0 && dock_lift_room(&l, 99, 1) == 0);
    s = style(48, DOCK_HORIZONTAL, DOCK_LABELS_ALWAYS);
    sample(&s);
    assert(dock_layout(&m, &s, &l));
    assert(dock_lift_room(&l, 0, -1) == 4 && dock_lift_room(&l, 0, 1) == 1);
    s = style(48, DOCK_VERTICAL, DOCK_LABELS_ALWAYS);
    sample(&s);
    assert(dock_layout(&m, &s, &l));
    assert(dock_lift_room(&l, 0, -1) == 4 && dock_lift_room(&l, 0, 1) == 4);
    /* Only the item asked for moves. */
    int progress[DOCK_MAX_ITEMS] = {0};
    struct DockMagnifyFrame f;
    progress[3] = 1000;
    assert(dock_lift_frame(&l, 1, progress, &f));
    for (int i = 0; i < l.count; i++) assert(same(f.icon[i], l.slots[i].icon) == (i != 3));
    assert(f.icon[3].x == l.slots[3].icon.x + 4);
    assert(!dock_lift_frame(&l, 0, progress, &f) && !dock_lift_frame(NULL, 1, progress, &f));
}

/* About 130 ms in or out whatever the frame rate; a reversal continues from
 * where it is; the ease is symmetric and stays in range. */
static void ramp(void)
{
    int p = 0, frames = 0;
    while (p < 1000) { p = dock_hover_ramp(p, 1000, 20); frames++; }
    assert(frames == (DOCK_HOVER_RAMP_MS + 19) / 20);
    p = 0;
    int ms = 0;
    while (p < 1000) { p = dock_hover_ramp(p, 1000, 1); ms++; }
    assert(ms >= 120 && ms <= DOCK_HOVER_RAMP_MS);
    p = dock_hover_ramp(0, 1000, 40);
    int back = dock_hover_ramp(p, 0, 20);
    assert(back > 0 && back < p);             /* turns back from where it was */
    assert(dock_hover_ramp(500, 500, 40) == 500);
    assert(dock_hover_ramp(0, 1000, 100000) == 1000 && dock_hover_ramp(1000, 0, 100000) == 0);
    assert(dock_hover_ramp(0, 1000, 0) == 0);
    assert(dock_hover_ease(0) == 0 && dock_hover_ease(1000) == 1000 && dock_hover_ease(500) == 500);
    for (int t = 0; t <= 1000; t++) {
        assert(dock_hover_ease(t) + dock_hover_ease(1000 - t) == 1000 || dock_hover_ease(t) + dock_hover_ease(1000 - t) == 999);
        if (t) assert(dock_hover_ease(t) >= dock_hover_ease(t - 1));
    }
    assert(dock_hover_ease(-5) == 0 && dock_hover_ease(2000) == 1000);
}

/* The repaint box covers the old and new icon and highlight of the item
 * that changed, nothing for one that did not. */
static void damage(void)
{
    struct DockStyle s = style(48, DOCK_HORIZONTAL, DOCK_LABELS_HOVER);
    struct DockLayout l;
    struct DockMagnifyFrame a, b;
    int rest[DOCK_MAX_ITEMS] = {0}, up[DOCK_MAX_ITEMS] = {0};
    struct DockMagnifyParams p = {30, 0};
    sample(&s);
    assert(dock_layout(&m, &s, &l));
    up[0] = 1000;
    assert(dock_lift_frame(&l, -1, rest, &a) && dock_lift_frame(&l, -1, up, &b));
    struct DockRect d = dock_hover_damage(&a, &b, 0);
    assert(inside(a.icon[0], d) && inside(b.icon[0], d) && inside(a.highlight[0], d) && inside(b.highlight[0], d));
    assert(inside(d, l.slots[0].slot));
    assert(!dock_hover_damage(&a, &b, 3).w);
    /* Magnify into rest: the grown icon and highlight are inside it too. */
    assert(dock_magnify_frame(&l, &s, &p, l.slots[0].icon.x + 24, 1000, &b));
    d = dock_hover_damage(&a, &b, 0);
    assert(b.grown[0] && inside(b.icon[0], d) && inside(b.highlight[0], d) && inside(d, l.slots[0].slot));
}

int main(void)
{
    direction();
    stays_in_slot();
    rooms();
    ramp();
    damage();
    puts("hover: ok");
    return 0;
}
