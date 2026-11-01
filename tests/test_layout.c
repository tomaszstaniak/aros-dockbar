#include "layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static struct DockModel m;
static struct DockStyle style(int orientation, int labels, int spacing)
{
    struct DockStyle s;
    memset(&s, 0, sizeof s);
    s.icon = 40; s.orientation = orientation; s.spacing = spacing; s.labels = labels; s.text_height = 14;
    /* the sample's clock (index 5): "00:00" over a date, as 0.1 measured */
    s.content_w[5] = 50; s.content_h[5] = 30;
    return s;
}
static void sample(void)
{
    enum DockKind kinds[] = {DOCK_APP, DOCK_GROUP, DOCK_SEPARATOR, DOCK_DRAWER, DOCK_SPACER, DOCK_CLOCK};
    memset(&m, 0, sizeof m);
    for (int i = 0; i < 6; i++) m.items[m.count++].kind = kinds[i];
    m.items[1].group = 1;
}
static void horizontal_matches_0_1_geometry(void)
{
    struct DockLayout l;
    struct DockStyle s = style(DOCK_HORIZONTAL, DOCK_LABELS_HOVER, 0);
    sample();
    m.items[4].kind = DOCK_APP;  /* 0.1 had no spacers */
    assert(dock_layout(&m, &s, &l));
    assert(l.width == dock_width(&m, 40) && l.height == 56);
    for (int i = 0; i < m.count; i++) {
        assert(l.slots[i].slot.x == dock_item_x(&m, i, 40));
        assert(l.slots[i].slot.w == dock_item_width(&m.items[i], 40));
    }
    assert(l.slots[0].icon.x == 25 && l.slots[0].icon.y == 8);
}
static void hit_agrees_with_painted_slots(void)
{
    for (int o = 0; o < 2; o++)
        for (int labels = 0; labels < 3; labels++) {
            struct DockLayout l;
            struct DockStyle s = style(o, labels, 4);
            sample();
            assert(dock_layout(&m, &s, &l));
            for (int y = 0; y < l.height; y++)
                for (int x = 0; x < l.width; x++) {
                    int hit = dock_layout_hit(&l, x, y), owner = -1;
                    for (int i = 0; i < l.count; i++) {
                        const struct DockRect *r = &l.slots[i].slot;
                        if (x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h) owner = i;
                    }
                    if (owner >= 0 && l.slots[owner].hittable) assert(hit == owner);
                    else assert(hit == -1);
                    /* Separators and spaces are found for editing and
                     * moving, though they launch nothing. */
                    assert(dock_layout_item(&l, x, y) == owner);
                    if (hit >= 0) assert(!dock_layout_in_grip(&l, x, y));
                }
            /* Icons stay inside their slot and the window. */
            for (int i = 0; i < l.count; i++) {
                const struct DockRect *ic = &l.slots[i].icon, *r = &l.slots[i].slot;
                if (!ic->w) continue;
                assert(ic->x >= 0 && ic->y >= 0 && ic->x + ic->w <= l.width && ic->y + ic->h <= l.height);
                if (o) assert(ic->y >= r->y && ic->y + ic->h <= r->y + r->h);
                else assert(ic->x >= r->x && ic->x + ic->w <= r->x + r->w);
            }
        }
}
static void vertical_runs_down_the_screen(void)
{
    struct DockLayout l;
    struct DockStyle s = style(DOCK_VERTICAL, DOCK_LABELS_HOVER, 0);
    sample();
    assert(dock_layout(&m, &s, &l));
    /* Wide enough for the clock's text with its highlight and margins. */
    assert(l.width == 50 + 2 * (DOCK_HIGHLIGHT_PAD + DOCK_HIGHLIGHT_MARGIN) && l.height > l.width);
    assert(l.grip.y == 0 && l.grip.h == DOCK_GRIP && l.grip.w == l.width);
    for (int i = 1; i < l.count; i++) assert(l.slots[i].slot.y > l.slots[i - 1].slot.y);
    assert(dock_layout_before(&l, 5, 0) == 0);
    assert(dock_layout_before(&l, 5, l.height - 1) == l.count);
    int y = l.slots[3].slot.y + l.slots[3].slot.h / 2;
    assert(dock_layout_center(&l, 3, 5, y) && !dock_layout_center(&l, 3, 5, l.slots[3].slot.y));
}
static void labels_and_spacing_grow_the_right_axis(void)
{
    struct DockLayout a, b;
    struct DockStyle s = style(DOCK_HORIZONTAL, DOCK_LABELS_HOVER, 0), t = s;
    sample();
    t.labels = DOCK_LABELS_ALWAYS;
    assert(dock_layout(&m, &s, &a) && dock_layout(&m, &t, &b));
    assert(b.height == a.height + 16 && b.width == a.width);
    assert(b.slots[0].label.y == b.slots[0].icon.y + 41);
    t = s; t.spacing = 6;
    assert(dock_layout(&m, &t, &b) && b.width == a.width + 6 * (m.count - 1));
    for (int i = 0; i <= m.count; i++) assert(i == 0 || dock_layout_mark(&b, i) > dock_layout_mark(&b, i - 1));
}
static void empty_dock_keeps_a_drop_area(void)
{
    struct DockLayout l;
    struct DockStyle s = style(DOCK_VERTICAL, DOCK_LABELS_HOVER, 0);
    memset(&m, 0, sizeof m);
    assert(dock_layout(&m, &s, &l) && l.height >= DOCK_GRIP + 40 && l.width == 56);
    assert(dock_layout_before(&l, 10, l.height - 1) == 0);
}
static void invalid_input_is_rejected(void)
{
    struct DockLayout l;
    struct DockStyle s = style(DOCK_HORIZONTAL, DOCK_LABELS_HOVER, 0);
    sample();
    s.icon = 2;
    assert(!dock_layout(&m, &s, &l));
    s = style(DOCK_HORIZONTAL, 0, 0);
    m.count = DOCK_MAX_ITEMS + 1;
    assert(!dock_layout(&m, &s, &l));
}
/* The 0.2 clock highlight covered its whole slot and touched the frame.
 * Every highlight now surrounds its content by the pad, keeps the margin
 * to the bar's edge and stays inside its own slot, for all sizes,
 * orientations, label modes and clock texts. */
static void highlights_fit_their_content(void)
{
    static const int clocks[][2] = {{35, 14}, {50, 30}, {62, 30}, {80, 46}};
    for (int o = 0; o < 2; o++)
        for (int labels = 0; labels < 3; labels++)
            for (int icon = 32; icon <= 72; icon += 8)
                for (int c = 0; c < 4; c++) {
                    struct DockLayout l;
                    struct DockStyle s = style(o, labels, 2);
                    s.icon = icon;
                    s.content_w[5] = clocks[c][0];
                    s.content_h[5] = clocks[c][1];
                    sample();
                    assert(dock_layout(&m, &s, &l));
                    int cross = o ? l.width : l.height, prev_end = -1;
                    for (int i = 0; i < l.count; i++) {
                        const struct DockSlot *sl = &l.slots[i];
                        if (!sl->content.w) { assert(!sl->highlight.w); continue; }
                        const struct DockRect *h = &sl->highlight, *ct = &sl->content;
                        if (i == 5) assert(ct->w == (o ? clocks[c][1] : clocks[c][0]) || ct->w == clocks[c][0]);
                        assert(h->x <= ct->x && h->y <= ct->y && h->x + h->w >= ct->x + ct->w && h->y + h->h >= ct->y + ct->h);
                        int lo = o ? h->x : h->y, len = o ? h->w : h->h;
                        assert(lo >= DOCK_HIGHLIGHT_MARGIN && lo + len <= cross - DOCK_HIGHLIGHT_MARGIN);
                        int along = o ? h->y : h->x, along_len = o ? h->h : h->w;
                        int slot = o ? sl->slot.y : sl->slot.x, slot_len = o ? sl->slot.h : sl->slot.w;
                        assert(along >= slot && along + along_len <= slot + slot_len);
                        assert(along > prev_end);
                        prev_end = along + along_len - 1;
                    }
                    /* The clock's text block is centred in its slot. */
                    const struct DockRect *ct = &l.slots[5].content, *sl = &l.slots[5].slot;
                    assert(abs((ct->x + ct->w / 2) - (sl->x + sl->w / 2)) <= 1 && abs((ct->y + ct->h / 2) - (sl->y + sl->h / 2)) <= 1);
                    assert(ct->w == (o ? clocks[c][0] : clocks[c][0]) && ct->h == clocks[c][1]);
                }
}
static void hidden_grip_leaves_a_margin(void)
{
    struct DockLayout a, b;
    struct DockStyle s = style(DOCK_HORIZONTAL, DOCK_LABELS_HOVER, 0);
    sample();
    assert(dock_layout(&m, &s, &a));
    s.hide_grip = 1;
    assert(dock_layout(&m, &s, &b));
    assert(b.start == DOCK_EDGE_PAD && b.width == a.width - (DOCK_GRIP - DOCK_EDGE_PAD));
    assert(!b.grip.w && !dock_layout_in_grip(&b, 2, 10) && dock_layout_in_grip(&a, 2, 10));
    assert(b.slots[0].slot.x == DOCK_EDGE_PAD && dock_layout_mark(&b, 0) == DOCK_EDGE_PAD - 2);
}
static int inside(const struct DockRect *in, const struct DockRect *out)
{
    return in->x >= out->x && in->y >= out->y && in->x + in->w <= out->x + out->w && in->y + in->h <= out->y + out->h;
}
static void text_buttons(void)
{
    for (int o = 0; o < 2; o++) {
        struct DockLayout icons, text, both;
        struct DockStyle s = style(o, DOCK_LABELS_HOVER, 0);
        int widths[] = {40, 0, 0, 150, 0, 0};
        sample();
        assert(dock_layout(&m, &s, &icons));
        for (int i = 0; i < 6; i++) { s.present[i] = DOCK_PRESENT_TEXT; s.text_w[i] = widths[i] ? widths[i] : 30; }
        assert(dock_layout(&m, &s, &text));
        /* Captions longer than the limit are the caller's to shorten. */
        s.text_w[0] = DOCK_TEXT_MAX + 1;
        assert(!dock_layout(&m, &s, &both));
        s.text_w[0] = 40;
        for (int i = 0; i < 6; i++) {
            const struct DockSlot *t = &text.slots[i];
            if (i == 2 || i == 4 || i == 5) {  /* separator, space, clock: unchanged kinds */
                assert(!t->button && !t->text.w);
                continue;
            }
            /* One geometry: the caption inside the button, the button inside
             * the slot (which is what clicks and drops use), clear of the
             * bar's edge across. */
            assert(t->button && t->highlight.w && inside(&t->text, &t->highlight) && inside(&t->highlight, &t->slot));
            assert(dock_layout_hit(&text, t->text.x + 1, t->text.y + 1) == i);
            assert(dock_layout_hit(&text, t->highlight.x, t->highlight.y) == i);
            if (o) assert(t->highlight.x >= DOCK_HIGHLIGHT_MARGIN && t->highlight.x + t->highlight.w <= text.width - DOCK_HIGHLIGHT_MARGIN);
            else assert(t->highlight.y >= DOCK_HIGHLIGHT_MARGIN && t->highlight.y + t->highlight.h <= text.height - DOCK_HIGHLIGHT_MARGIN);
            assert(t->text.w == s.text_w[i]);
        }
        /* The group carries a mark beside its caption, inside its button. */
        assert(text.slots[1].mark.w && inside(&text.slots[1].mark, &text.slots[1].highlight) && !text.slots[0].mark.w);
        if (o) {
            /* Vertical: text stays horizontal, all buttons share the widest
             * button's width, and the bar grows to it. */
            assert(text.slots[0].highlight.w == text.slots[3].highlight.w && text.slots[1].highlight.w == text.slots[3].highlight.w);
            assert(text.width >= 150 + 2 * DOCK_TEXT_PAD_X && text.slots[0].text.x == text.slots[3].text.x);
        } else {
            /* Horizontal: each button as wide as its caption needs, and a
             * dock of text is slimmer than one of icons. */
            assert(text.slots[0].highlight.w == 40 + 2 * DOCK_TEXT_PAD_X && text.slots[3].highlight.w == 150 + 2 * DOCK_TEXT_PAD_X);
            assert(text.height < icons.height);
        }
        /* Icon and text beside: the icon box sits in the button before the caption. */
        for (int i = 0; i < 6; i++) s.present[i] = DOCK_PRESENT_ICON_TEXT;
        s.beside = 1;
        assert(dock_layout(&m, &s, &both));
        const struct DockSlot *b = &both.slots[0];
        assert(b->button && b->icon.w == s.icon && inside(&b->icon, &b->highlight) && b->text.x >= b->icon.x + b->icon.w);
        /* Under: icon centred over the caption, both inside the button, which
         * is as wide as the wider of the two; the group mark beside the
         * caption, inside too. */
        s.beside = 0;
        assert(dock_layout(&m, &s, &both));
        for (int i = 0; i < 6; i++) {
            const struct DockSlot *u = &both.slots[i];
            if (!u->button) continue;
            assert(inside(&u->icon, &u->highlight) && inside(&u->text, &u->highlight) && u->text.y >= u->icon.y + u->icon.h);
            assert(abs((u->icon.x + u->icon.w / 2) - (u->highlight.x + u->highlight.w / 2)) <= 1);
            if (u->mark.w) assert(inside(&u->mark, &u->highlight) && u->mark.x >= u->text.x + u->text.w);
            if (!o) assert(u->highlight.w == (s.text_w[i] + (i == 1 ? DOCK_GROUP_MARK : 0) > s.icon ? s.text_w[i] + (i == 1 ? DOCK_GROUP_MARK : 0) : s.icon) + 2 * DOCK_TEXT_PAD_X);
            assert(dock_layout_hit(&both, u->text.x + 1, u->text.y + 1) == i && dock_layout_hit(&both, u->icon.x + 1, u->icon.y + 1) == i);
        }
        /* Mixed: an icon item next to text buttons keeps its icon geometry
         * and is centred across. */
        for (int i = 0; i < 6; i++) s.present[i] = DOCK_PRESENT_TEXT;
        s.present[0] = DOCK_PRESENT_ICON;
        assert(dock_layout(&m, &s, &both));
        assert(!both.slots[0].button && both.slots[0].icon.w == s.icon && both.slots[3].button);
    }
}
/* A scrolled dock draws each slot moved along the bar: every rectangle of
 * the slot must move together, or captions stay behind their icons (seen
 * on hardware with "Icon and text"). The size check fails when a rectangle
 * is added to DockSlot, as a reminder to shift it too. */
static void scrolled_slots_move_whole(void)
{
    assert(sizeof(struct DockSlot) == 7 * sizeof(struct DockRect) + 3 * sizeof(int));
    for (int o = 0; o < 2; o++) {
        struct DockLayout l;
        struct DockStyle s = style(o, DOCK_LABELS_HOVER, 0);
        sample();
        for (int i = 0; i < 6; i++) { s.present[i] = DOCK_PRESENT_ICON_TEXT; s.text_w[i] = 60 + 10 * i; }
        assert(dock_layout(&m, &s, &l));
        for (int i = 0; i < 6; i++) {
            struct DockSlot a = l.slots[i], b = a;
            int dx = o ? 0 : -137, dy = o ? -137 : 0;
            dock_layout_shift(&b, dx, dy);
            const struct DockRect *ra[] = { &a.slot, &a.icon, &a.label, &a.content, &a.highlight, &a.text, &a.mark };
            const struct DockRect *rb[] = { &b.slot, &b.icon, &b.label, &b.content, &b.highlight, &b.text, &b.mark };
            for (int k = 0; k < 7; k++)
                assert(rb[k]->x == ra[k]->x + dx && rb[k]->y == ra[k]->y + dy && rb[k]->w == ra[k]->w && rb[k]->h == ra[k]->h);
            /* Icon and caption keep their places inside the button. */
            if (b.button) {
                assert(b.icon.x - b.highlight.x == a.icon.x - a.highlight.x && b.text.x - b.highlight.x == a.text.x - a.highlight.x);
                assert(b.text.y >= b.icon.y + b.icon.h);  /* caption under the icon, the default */
            }
        }
    }
}
int main(void)
{
    scrolled_slots_move_whole();
    text_buttons();
    hidden_grip_leaves_a_margin();
    highlights_fit_their_content();
    horizontal_matches_0_1_geometry();
    hit_agrees_with_painted_slots();
    vertical_runs_down_the_screen();
    labels_and_spacing_grow_the_right_axis();
    empty_dock_keeps_a_drop_area();
    invalid_input_is_rejected();
    puts("PASS layout: 0.1 horizontal geometry, hit equals painted slots, vertical, labels, spacing, empty drop area, text buttons, scrolled slots move whole");
    return 0;
}
