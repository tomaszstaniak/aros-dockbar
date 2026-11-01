#include "layout.h"
#include <string.h>

/* Room along the bar on each side of a widget's text: the highlight pad
 * plus a gap to the next item. 14 keeps 0.1/0.2 clock widths. */
#define WIDGET_SIDE 14

static int text_item(const struct DockItem *item, int index, const struct DockStyle *style)
{
    int p = style->present[index];
    return dock_kind_shown(item->kind) &&
           (p == DOCK_PRESENT_TEXT || p == DOCK_PRESENT_ICON_TEXT);
}

/* Inside a text button: caption, the icon beside it for icon and text, the
 * group mark. */
static void button_inner(const struct DockItem *item, int index, const struct DockStyle *style, int *w, int *h)
{
    int tw = style->text_w[index] + (item->kind == DOCK_GROUP ? DOCK_GROUP_MARK : 0);
    if (style->present[index] == DOCK_PRESENT_ICON_TEXT && !style->beside) {
        *w = style->icon > tw ? style->icon : tw;
        *h = style->icon + DOCK_ICON_TEXT_GAP_UNDER + style->text_height;
    } else if (style->present[index] == DOCK_PRESENT_ICON_TEXT) {
        *w = style->icon + DOCK_ICON_TEXT_GAP + tw;
        *h = style->icon > style->text_height ? style->icon : style->text_height;
    } else { *w = tw; *h = style->text_height; }
}

static void button_size(const struct DockItem *item, int index, const struct DockStyle *style, int *w, int *h)
{
    button_inner(item, index, style, w, h);
    *w += 2 * DOCK_TEXT_PAD_X;
    *h += 2 * DOCK_TEXT_PAD_Y;
}

static int main_extent(const struct DockItem *item, int index, const struct DockStyle *style)
{
    int vertical = style->orientation == DOCK_VERTICAL;
    switch (item->kind) {
    case DOCK_SEPARATOR: return 12;
    case DOCK_SPACER: return style->icon / 2;
    case DOCK_APPLET:
    case DOCK_CLOCK: return (vertical ? style->content_h[index] : style->content_w[index]) + 2 * WIDGET_SIDE;
    default: break;
    }
    if (text_item(item, index, style)) {
        int w, h;
        button_size(item, index, style, &w, &h);
        return (vertical ? h : w) + 2 * DOCK_BUTTON_GAP;
    }
    /* Labels sit under the icon, which lengthens a vertical bar's slots
     * but widens a horizontal bar as a whole (see the cross extent). */
    return style->icon + 10 + (vertical && style->labels == DOCK_LABELS_ALWAYS ? style->text_height + 2 : 0);
}

static struct DockRect map(int vertical, int main, int cross, int main_len, int cross_len)
{
    struct DockRect r;
    if (vertical) { r.x = cross; r.y = main; r.w = cross_len; r.h = main_len; }
    else { r.x = main; r.y = cross; r.w = main_len; r.h = cross_len; }
    return r;
}

int dock_layout(const struct DockModel *model, const struct DockStyle *style,
                struct DockLayout *layout)
{
    int vertical, cross, pos;
    if (!model || !style || !layout || model->count < 0 || model->count > DOCK_MAX_ITEMS ||
        style->icon < 16 || style->icon > 128 || style->spacing < 0 || style->spacing > 64 ||
        style->text_height < 1)
        return 0;
    for (int i = 0; i < model->count; i++)
        if ((model->items[i].kind == DOCK_CLOCK || model->items[i].kind == DOCK_APPLET) && (style->content_w[i] < 1 || style->content_h[i] < 1)) return 0;
    memset(layout, 0, sizeof(*layout));
    vertical = style->orientation == DOCK_VERTICAL;
    layout->orientation = vertical;
    layout->count = model->count;
    for (int i = 0; i < model->count; i++)
        if (text_item(&model->items[i], i, style) && (style->text_w[i] < 1 || style->text_w[i] > DOCK_TEXT_MAX)) return 0;
    /* Across, the bar is as thick as its items need: icons their box, text
     * buttons their height (horizontal) or the widest button (vertical,
     * where all buttons share it), widgets their text with highlight and
     * margin. A dock of text buttons is therefore slimmer than one of icons. */
    int icon_cross = style->icon + 16 + (!vertical && style->labels == DOCK_LABELS_ALWAYS ? style->text_height + 2 : 0);
    int any = 0;
    cross = 0;
    for (int i = 0; i < model->count; i++) {
        const struct DockItem *item = &model->items[i];
        int need = 0;
        if (item->kind == DOCK_CLOCK || item->kind == DOCK_APPLET)
            need = (vertical ? style->content_w[i] : style->content_h[i]) + 2 * (DOCK_HIGHLIGHT_PAD + DOCK_HIGHLIGHT_MARGIN);
        else if (text_item(item, i, style)) {
            int w, h;
            button_size(item, i, style, &w, &h);
            need = (vertical ? w : h) + 2 * DOCK_HIGHLIGHT_MARGIN;
        } else if (dock_kind_shown(item->kind))
            need = icon_cross;
        else continue;
        any = 1;
        if (cross < need) cross = need;
    }
    if (!any) cross = icon_cross;
    layout->start = style->hide_grip ? DOCK_EDGE_PAD : DOCK_GRIP;
    if (!style->hide_grip) layout->grip = map(vertical, 0, 0, DOCK_GRIP, cross);
    pos = layout->start;
    for (int i = 0; i < model->count; i++) {
        const struct DockItem *item = &model->items[i];
        struct DockSlot *s = &layout->slots[i];
        int extent = main_extent(item, i, style);
        s->kind = item->kind;
        s->slot = map(vertical, pos, 0, extent, cross);
        s->hittable = item->kind != DOCK_SEPARATOR && item->kind != DOCK_SPACER;
        if (text_item(item, i, style)) {
            int iw, ih, bw, bh, m = DOCK_HIGHLIGHT_MARGIN;
            button_inner(item, i, style, &iw, &ih);
            button_size(item, i, style, &bw, &bh);
            /* Vertical: every button as wide as the bar allows, text kept
             * horizontal and starting at the same place. */
            struct DockRect b = vertical ? map(1, pos + DOCK_BUTTON_GAP, m, bh, cross - 2 * m)
                                         : map(0, pos + DOCK_BUTTON_GAP, (cross - bh) / 2, bw, bh);
            int x = b.x + DOCK_TEXT_PAD_X, y = b.y + DOCK_TEXT_PAD_Y;
            s->button = 1;
            s->highlight = b;
            if (style->present[i] == DOCK_PRESENT_ICON_TEXT && !style->beside) {
                /* Icon centred over its caption; caption and group mark
                 * centred under it, as one unit. */
                int tw = style->text_w[i] + (item->kind == DOCK_GROUP ? DOCK_GROUP_MARK : 0);
                s->icon = (struct DockRect){ b.x + (b.w - style->icon) / 2, y, style->icon, style->icon };
                x = b.x + (b.w - tw) / 2;
                s->text = (struct DockRect){ x, y + style->icon + DOCK_ICON_TEXT_GAP_UNDER, style->text_w[i], style->text_height };
            } else {
                if (style->present[i] == DOCK_PRESENT_ICON_TEXT) {
                    s->icon = (struct DockRect){ x, y + (ih - style->icon) / 2, style->icon, style->icon };
                    x += style->icon + DOCK_ICON_TEXT_GAP;
                } else if (!vertical) x = b.x + (b.w - iw) / 2;
                s->text = (struct DockRect){ x, y + (ih - style->text_height) / 2, style->text_w[i], style->text_height };
            }
            if (item->kind == DOCK_GROUP)
                s->mark = (struct DockRect){ vertical && (style->beside || style->present[i] != DOCK_PRESENT_ICON_TEXT)
                                             ? b.x + b.w - DOCK_TEXT_PAD_X - DOCK_GROUP_MARK + 4 : s->text.x + style->text_w[i] + 4,
                                             s->text.y, DOCK_GROUP_MARK - 4, style->text_height };
            s->content = b;
        } else if (dock_kind_shown(item->kind)) {
            /* Centred across when text buttons or widgets make the bar
             * thicker; with labels under a horizontal bar's icons, at the top. */
            int along = pos + 5, across = (!vertical && style->labels == DOCK_LABELS_ALWAYS) ? 8 : (cross - style->icon) / 2;
            s->icon = map(vertical, along, across, style->icon, style->icon);
            if (style->labels == DOCK_LABELS_ALWAYS)
                s->label = vertical
                    ? map(1, along + style->icon + 1, 2, style->text_height, cross - 4)
                    : map(0, pos, across + style->icon + 1, extent, style->text_height);
            s->content = s->icon;
        } else if (item->kind == DOCK_CLOCK || item->kind == DOCK_APPLET) {
            int cw = style->content_w[i], ch = style->content_h[i];
            s->content = vertical ? map(1, pos + (extent - ch) / 2, (cross - cw) / 2, ch, cw)
                                  : map(0, pos + (extent - cw) / 2, (cross - ch) / 2, cw, ch);
        }
        if (s->content.w && !s->button) {
            struct DockRect h = s->content;
            int m = DOCK_HIGHLIGHT_MARGIN, p = DOCK_HIGHLIGHT_PAD;
            h.x -= p; h.y -= p; h.w += 2 * p; h.h += 2 * p;
            /* Across the bar the margin wins over the pad. */
            if (vertical) {
                if (h.x < m) { h.w -= m - h.x; h.x = m; }
                if (h.x + h.w > cross - m) h.w = cross - m - h.x;
            } else {
                if (h.y < m) { h.h -= m - h.y; h.y = m; }
                if (h.y + h.h > cross - m) h.h = cross - m - h.y;
            }
            s->highlight = h;
        }
        pos += extent + style->spacing;
    }
    pos += DOCK_EDGE_PAD - style->spacing * (model->count > 0);
    /* An empty dock still needs room to drop the first icon onto. */
    if (pos < layout->start + style->icon + 18) pos = layout->start + style->icon + 18;
    if (vertical) { layout->width = cross; layout->height = pos; }
    else { layout->width = pos; layout->height = cross; }
    return 1;
}

static int along(const struct DockLayout *l, int x, int y) { return l->orientation ? y : x; }
static int rect_main(const struct DockLayout *l, const struct DockRect *r) { return l->orientation ? r->y : r->x; }
static int rect_main_len(const struct DockLayout *l, const struct DockRect *r) { return l->orientation ? r->h : r->w; }

int dock_layout_item(const struct DockLayout *l, int x, int y)
{
    if (!l || x < 0 || y < 0 || x >= l->width || y >= l->height) return -1;
    for (int i = 0; i < l->count; i++) {
        const struct DockRect *r = &l->slots[i].slot;
        if (x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h) return i;
    }
    return -1;
}

int dock_layout_hit(const struct DockLayout *l, int x, int y)
{
    int i = dock_layout_item(l, x, y);
    return i >= 0 && l->slots[i].hittable ? i : -1;
}

int dock_layout_before(const struct DockLayout *l, int x, int y)
{
    int p = along(l, x, y);
    for (int i = 0; i < l->count; i++) {
        const struct DockRect *r = &l->slots[i].slot;
        if (p < rect_main(l, r) + rect_main_len(l, r) / 2) return i;
    }
    return l->count;
}

int dock_layout_center(const struct DockLayout *l, int index, int x, int y)
{
    if (!l || index < 0 || index >= l->count || !l->slots[index].hittable) return 0;
    const struct DockRect *r = &l->slots[index].slot;
    int p = along(l, x, y) - rect_main(l, r), len = rect_main_len(l, r);
    return p >= len / 4 && p < len * 3 / 4;
}

int dock_layout_mark(const struct DockLayout *l, int before)
{
    if (before < l->count) return rect_main(l, &l->slots[before].slot) - 2;
    if (!l->count) return l->start;
    const struct DockRect *r = &l->slots[l->count - 1].slot;
    return rect_main(l, r) + rect_main_len(l, r);
}

int dock_layout_in_grip(const struct DockLayout *l, int x, int y)
{
    const struct DockRect *g = &l->grip;
    return x >= g->x && x < g->x + g->w && y >= g->y && y < g->y + g->h;
}

void dock_layout_shift(struct DockSlot *s, int dx, int dy)
{
    struct DockRect *r[] = { &s->slot, &s->icon, &s->label, &s->content, &s->highlight, &s->text, &s->mark };
    for (unsigned i = 0; i < sizeof r / sizeof *r; i++) { r[i]->x += dx; r[i]->y += dy; }
}
