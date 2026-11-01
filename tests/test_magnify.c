#include "magnify.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct DockModel m;
static struct DockStyle style(int icon, int orientation, int labels, int spacing)
{
    struct DockStyle s;
    memset(&s, 0, sizeof s);
    s.icon = icon; s.orientation = orientation; s.spacing = spacing; s.labels = labels; s.text_height = 14;
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
static int overlap(struct DockRect a, struct DockRect b)
{
    return a.w > 0 && a.h > 0 && b.w > 0 && b.h > 0 &&
           a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}
static int magnifies(const struct DockLayout *l, int i)
{
    return dock_kind_shown(l->slots[i].kind) && !l->slots[i].button;
}

static void rest_when_off(void)
{
    struct DockMagnifyParams p = {30, 0};
    struct DockMagnifyFrame f;
    for (int o = 0; o < 2; o++) {
        struct DockStyle s = style(48, o, DOCK_LABELS_HOVER, 0);
        struct DockLayout l;
        sample(&s);
        assert(dock_layout(&m, &s, &l));
        for (int strength = 0; strength <= 1000; strength += 1000) {
            p.percent = strength ? 0 : 30;
            for (int ptr = -50; ptr < 900; ptr += 7) {
                assert(dock_magnify_frame(&l, &s, &p, ptr, strength, &f));
                for (int i = 0; i < l.count; i++) {
                    assert(same(f.icon[i], l.slots[i].icon) && same(f.highlight[i], l.slots[i].highlight));
                    assert(!f.grown[i]);
                }
            }
        }
        /* Far from the dock nothing grows, even at full strength. */
        p.percent = 30;
        assert(dock_magnify_frame(&l, &s, &p, -1000, 1000, &f));
        for (int i = 0; i < l.count; i++) assert(!f.grown[i]);
    }
}

/* Every size, orientation, label mode, spacing and pointer position: grown
 * icons stay square, in their room, inside their slot and the bar's
 * margins, off the label; what is drawn under a point is what a click there
 * hits; items that do not magnify keep their rest geometry. */
static void stays_in_slot(void)
{
    int sizes[] = {32, 40, 48, 57, 72};
    for (unsigned z = 0; z < sizeof sizes / sizeof *sizes; z++)
        for (int o = 0; o < 2; o++)
            for (int labels = 0; labels < 3; labels++)
                for (int spacing = 0; spacing <= 8; spacing += 8)
                    for (int percent = 10; percent <= 100; percent += 45) {
                        struct DockStyle s = style(sizes[z], o, labels, spacing);
                        struct DockLayout l;
                        struct DockMagnifyParams p = {percent, 0};
                        sample(&s);
                        assert(dock_layout(&m, &s, &l));
                        int cross = o ? l.width : l.height, len = o ? l.height : l.width;
                        for (int ptr = -20; ptr < len + 20; ptr += 3) {
                            struct DockMagnifyFrame f;
                            assert(dock_magnify_frame(&l, &s, &p, ptr, 1000, &f));
                            assert(f.count == l.count);
                            for (int i = 0; i < l.count; i++) {
                                const struct DockSlot *sl = &l.slots[i];
                                struct DockRect bar = o ? (struct DockRect){DOCK_HIGHLIGHT_MARGIN, sl->slot.y, cross - 2 * DOCK_HIGHLIGHT_MARGIN, sl->slot.h}
                                                        : (struct DockRect){sl->slot.x, DOCK_HIGHLIGHT_MARGIN, sl->slot.w, cross - 2 * DOCK_HIGHLIGHT_MARGIN};
                                if (!magnifies(&l, i)) {
                                    assert(same(f.icon[i], sl->icon) && same(f.highlight[i], sl->highlight) && !f.grown[i]);
                                    assert(!dock_magnify_room(&l, &s, i).w);
                                    continue;
                                }
                                struct DockRect room = dock_magnify_room(&l, &s, i);
                                assert(room.w == room.h && room.w >= sl->icon.w);
                                assert(inside(room, bar));
                                assert(!overlap(room, sl->label));
                                assert(f.icon[i].w == f.icon[i].h);
                                assert(f.icon[i].w == sl->icon.w + f.grown[i]);
                                assert(f.grown[i] % DOCK_MAGNIFY_STEP == 0);
                                assert(f.grown[i] <= sl->icon.w * percent / 100);
                                assert(inside(f.icon[i], room));
                                if (f.grown[i]) assert(inside(f.highlight[i], bar) && inside(f.icon[i], f.highlight[i]));
                                else assert(same(f.highlight[i], sl->highlight));
                                /* An always-shown label is below the icon on both
                                 * bars; the grown highlight reaches no further
                                 * down than the rest one. */
                                if (sl->label.w) assert(f.highlight[i].y + f.highlight[i].h <= sl->highlight.y + sl->highlight.h);
                            }
                            /* Drawn here = hit here, over the whole window. */
                            for (int y = 0; y < l.height; y += 2)
                                for (int x = 0; x < l.width; x += 2)
                                    for (int i = 0; i < l.count; i++) {
                                        struct DockRect r = f.icon[i];
                                        if (f.grown[i] && x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
                                            assert(dock_layout_item(&l, x, y) == i);
                                    }
                        }
                    }
}

/* Default bar of 48 px icons: a slot is 58 long and the bar 64 thick, so an
 * icon can take 10 px; with labels always under a horizontal bar's icons,
 * only 6 (5 to the margin above, 1 to the label). */
static void room_sizes(void)
{
    struct DockStyle s = style(48, DOCK_HORIZONTAL, DOCK_LABELS_HOVER, 0);
    struct DockLayout l;
    struct DockMagnifyParams p = {30, 0};
    struct DockMagnifyFrame f;
    sample(&s);
    assert(dock_layout(&m, &s, &l));
    assert(dock_magnify_room(&l, &s, 0).w == 58);
    int c0 = l.slots[0].icon.x + 24;
    assert(dock_magnify_frame(&l, &s, &p, c0, 1000, &f));
    assert(f.grown[0] == 10);                   /* 30 % wanted, room for 10 */
    assert(f.icon[0].x == l.slots[0].icon.x - 5 && f.icon[0].y == l.slots[0].icon.y - 5);
    p.percent = 10;
    assert(dock_magnify_frame(&l, &s, &p, c0, 1000, &f));
    assert(f.grown[0] == 4);                     /* 4.8 -> whole steps, down */
    assert(dock_magnify_frame(&l, &s, &p, c0, 500, &f));
    assert(f.grown[0] == 2);                     /* half strength */

    s = style(48, DOCK_HORIZONTAL, DOCK_LABELS_ALWAYS, 0);
    sample(&s);
    assert(dock_layout(&m, &s, &l));
    assert(dock_magnify_room(&l, &s, 0).w == 54);
    p.percent = 30;
    assert(dock_magnify_frame(&l, &s, &p, c0, 1000, &f));
    assert(f.grown[0] == 6 && f.icon[0].y + f.icon[0].h <= l.slots[0].label.y);
}

/* The icon under the pointer grows most; growth falls with distance and
 * the same distance on either side gives the same size. */
static void falloff_shape(void)
{
    struct DockStyle s = style(48, DOCK_HORIZONTAL, DOCK_LABELS_HOVER, 0);
    struct DockLayout l;
    struct DockMagnifyParams p = {100, 0};
    struct DockMagnifyFrame f;
    memset(&m, 0, sizeof m);
    for (int i = 0; i < 7; i++) m.items[m.count++].kind = DOCK_APP;
    assert(dock_layout(&m, &s, &l));
    int c3 = l.slots[3].icon.x + 24;
    assert(dock_magnify_frame(&l, &s, &p, c3, 1000, &f));
    assert(f.grown[3] == 10);
    assert(f.grown[2] == f.grown[4] && f.grown[1] == f.grown[5] && f.grown[0] == f.grown[6]);
    assert(f.grown[3] >= f.grown[2] && f.grown[2] >= f.grown[1] && f.grown[1] >= f.grown[0]);
    assert(f.grown[2] > 0 && f.grown[0] == 0);  /* radius 2.5 icon boxes */
    /* A move of one pixel changes at most the icons in reach. */
    struct DockMagnifyFrame g;
    int changed = 0;
    assert(dock_magnify_frame(&l, &s, &p, c3 + 1, 1000, &g));
    for (int i = 0; i < l.count; i++) changed += dock_magnify_changed(&f, &g, i);
    assert(changed <= 5);
    assert(!dock_magnify_changed(&f, &f, 3));
}

static void rejects(void)
{
    struct DockStyle s = style(48, DOCK_HORIZONTAL, DOCK_LABELS_HOVER, 0);
    struct DockLayout l;
    struct DockMagnifyParams p = {30, 0};
    struct DockMagnifyFrame f;
    sample(&s);
    assert(dock_layout(&m, &s, &l));
    assert(!dock_magnify_frame(NULL, &s, &p, 0, 0, &f));
    assert(!dock_magnify_frame(&l, &s, &p, 0, 1001, &f));
    p.percent = 101;
    assert(!dock_magnify_frame(&l, &s, &p, 0, 0, &f));
    assert(!dock_magnify_room(&l, &s, 99).w);
}

int main(void)
{
    rest_when_off();
    stays_in_slot();
    room_sizes();
    falloff_shape();
    rejects();
    puts("magnify: ok");
    return 0;
}
