#include "group_panel.h"
#include "svg_icons.h"
#include "chrome.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/text.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

static long pen_for(struct Screen *screen, unsigned long rgb)
{
    return ObtainBestPen(screen->ViewPort.ColorMap, ((rgb >> 16) & 255) * 0x01010101UL,
                         ((rgb >> 8) & 255) * 0x01010101UL, (rgb & 255) * 0x01010101UL, TAG_DONE);
}


static int clamp(int value, int low, int high)
{
    if (value < low) return low;
    return value > high ? high : value;
}

static int svg_path(const char *path)
{
    size_t length = strlen(path);
    const char *suffix;
    if (length < 4) return 0;
    suffix = path + length - 4;
    return suffix[0] == '.' && tolower((unsigned char)suffix[1]) == 's' &&
           tolower((unsigned char)suffix[2]) == 'v' &&
           tolower((unsigned char)suffix[3]) == 'g';
}

static void text(struct RastPort *rp, int x, int y, const char *value, long pen)
{
    SetAPen(rp, (ULONG)pen);
    SetDrMd(rp, JAM1);
    Move(rp, x, y);
    Text(rp, (CONST_STRPTR)value, (ULONG)strlen(value));
}

static void fit_label(struct RastPort *rp, char *out, size_t capacity,
                      const char *value, int width)
{
    size_t length;
    snprintf(out, capacity, "%s", value);
    length = strlen(out);
    if (TextLength(rp, (CONST_STRPTR)out, (ULONG)length) <= width) return;
    while (length > 0) {
        --length;
        out[length] = '\0';
        if (length + 3 < capacity) {
            memcpy(out + length, "...", 4);
            if (TextLength(rp, (CONST_STRPTR)out, (ULONG)(length + 3)) <= width)
                return;
            out[length] = '\0';
        }
    }
    out[0] = '\0';
}

void dock_panel_close(struct DockPanel *panel)
{
    if (!panel) return;
    /* Drain the AppWindow before its Intuition window disappears. */
    dock_drop_close(&panel->drop);
    if (panel->window) {
        struct Message *message;
        /* This is an owned UserPort. Keep it attached for CloseWindow's final
         * shutdown, after explicitly replying to messages already queued. */
        while ((message = GetMsg(panel->window->UserPort))) ReplyMsg(message);
        CloseWindow(panel->window);
    }
    if (panel->shape) DisposeRegion(panel->shape);
    dock_buffer_close(&panel->buffer);
    for (int i = 0; i < DOCK_MAX_ITEMS; ++i) {
        dock_native_free(&panel->icons[i]);
        dock_svg_free(panel->svg[i]);
    }
    if (panel->screen) {
        if (panel->text_pen >= 0)
            ReleasePen(panel->screen->ViewPort.ColorMap, (ULONG)panel->text_pen);
        if (panel->muted_pen >= 0)
            ReleasePen(panel->screen->ViewPort.ColorMap, (ULONG)panel->muted_pen);
    }
    memset(panel, 0, sizeof(*panel));
    panel->text_pen = panel->muted_pen = -1;
    panel->hover = panel->pressed = -1;
}

int dock_panel_open(struct DockPanel *panel, struct Screen *screen,
                    struct DockModel *model, int container, int size,
                    int anchor_x, int anchor_y, int anchor_width, int anchor_height,
                    int vertical, int activate, const struct DockColors *colors)
{
    struct DockItem *items;
    int count, font_height, header, footer, left, top, max_width, max_height;
    if (!panel || !screen || !model || container < 1 || container > DOCK_MAX_GROUPS ||
        model->count < 0 || model->count > DOCK_MAX_ITEMS || size < 16 || size > 128) return 0;
    items = dock_items(model, container, &count);
    if (!items || count < 0 || count > DOCK_MAX_ITEMS) return 0;
    dock_panel_close(panel);
    panel->screen = screen;
    panel->container = container;
    panel->count = count;
    panel->size = size;
    panel->columns = count ? (count < 4 ? count : 4) : 2;
    panel->rows = count ? (count + panel->columns - 1) / panel->columns : 1;
    font_height = screen->RastPort.Font->tf_YSize;
    header = font_height + 24;
    footer = 8;
    max_width = screen->Width - 16;
    max_height = screen->Height - screen->BarHeight - 20;
    /* On a small screen reduce the cached icon size, never clip off a row. */
    for (;;) {
        panel->cell_width = panel->size + 32;
        if (panel->cell_width < 76) panel->cell_width = 76;
        panel->cell_height = panel->size + font_height + 20;
        panel->width = panel->columns * panel->cell_width + 24;
        if (panel->width < 208) panel->width = 208;
        panel->height = header + panel->rows * panel->cell_height + footer;
        if (panel->width <= max_width && panel->height <= max_height) break;
        if (panel->size <= 16) goto failed;
        --panel->size;
    }
    panel->grid_x = (panel->width - panel->columns * panel->cell_width) / 2;
    panel->grid_y = header;
    snprintf(panel->title, sizeof(panel->title), "Group");
    for (int i = 0; i < model->count; ++i)
        if (model->items[i].kind == DOCK_GROUP && model->items[i].group == container) {
            snprintf(panel->title, sizeof(panel->title), "%s", model->items[i].label);
            break;
        }
    /* The panel keeps its own background in every mode of the bar (also
     * without a bar), in the dock's colours. */
    panel->colors = colors ? colors : dock_colors(0);
    panel->text_pen = pen_for(screen, panel->colors->text);
    panel->muted_pen = pen_for(screen, panel->colors->text_muted);
    if (panel->text_pen < 0 || panel->muted_pen < 0) goto failed;
    for (int i = 0; i < count; ++i) {
        const char *source = items[i].icon[0] ? items[i].icon : items[i].path;
        snprintf(panel->labels[i], sizeof(panel->labels[i]), "%s", items[i].label);
        if (svg_path(source)) {
            if (dock_svg_render(source, panel->size, &panel->svg[i])) continue;
            source = items[i].path;
        }
        dock_native_load(&panel->icons[i], source, screen, panel->size);
    }
    if (vertical) {
        /* Beside a vertical bar: to the right, or to the left at the right edge. */
        left = anchor_x + anchor_width + 8;
        if (left + panel->width > screen->Width - 8) left = anchor_x - panel->width - 8;
        top = anchor_y + anchor_height / 2 - panel->height / 2;
    } else {
        left = anchor_x + anchor_width / 2 - panel->width / 2;
        top = anchor_y - panel->height - 8;
        if (top < screen->BarHeight + 8) top = anchor_y + anchor_height + 8;
    }
    left = clamp(left, 8, screen->Width - panel->width - 8);
    top = clamp(top, screen->BarHeight + 8, screen->Height - panel->height - 8);
    panel->shape = dock_shape(panel->width, panel->height, dock_corner_radius);
    if (!panel->shape || !dock_buffer_open(&panel->buffer, screen, panel->width, panel->height))
        goto failed;
    panel->window = OpenWindowTags(NULL, WA_PubScreen, (IPTR)screen,
                                   WA_Left, left, WA_Top, top,
                                   WA_Width, panel->width, WA_Height, panel->height,
                                   WA_ShapeRegion, (IPTR)panel->shape,
                                   WA_Borderless, TRUE, WA_Activate, activate ? TRUE : FALSE,
                                   WA_RMBTrap, TRUE, WA_ReportMouse, TRUE,
                                   WA_SimpleRefresh, TRUE,
                                   WA_IDCMP, IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE |
                                   IDCMP_RAWKEY | IDCMP_INACTIVEWINDOW |
                                   IDCMP_ACTIVEWINDOW | IDCMP_REFRESHWINDOW,
                                   TAG_DONE);
    if (!panel->window || !dock_drop_open(&panel->drop, panel->window)) goto failed;
    SetFont(panel->window->RPort, screen->RastPort.Font);
    dock_panel_draw(panel);
    return 1;
failed:
    dock_panel_close(panel);
    return 0;
}

int dock_panel_hit(const struct DockPanel *panel, int x, int y)
{
    int column, row, index;
    if (!panel || !panel->window || x < panel->grid_x || y < panel->grid_y ||
        x >= panel->grid_x + panel->columns * panel->cell_width ||
        y >= panel->grid_y + panel->rows * panel->cell_height) return -1;
    column = (x - panel->grid_x) / panel->cell_width;
    row = (y - panel->grid_y) / panel->cell_height;
    index = row * panel->columns + column;
    return index < panel->count ? index : -1;
}

int dock_panel_before(const struct DockPanel *panel, int x, int y)
{
    int index = dock_panel_hit(panel, x, y);
    if (!panel) return 0;
    if (index < 0) return panel->count;
    return index + ((x - panel->grid_x) % panel->cell_width >= panel->cell_width / 2);
}

void dock_panel_draw(struct DockPanel *panel)
{
    struct RastPort *rp;
    char caption[DOCK_LABEL];
    int baseline;
    if (!panel || !panel->window) return;
    rp = &panel->buffer.rp;
    baseline = rp->Font->tf_Baseline;
    const struct DockColors *c = panel->colors;
    struct DockFrameStyle frame = { c->popup_fill, c->popup_fill, c->border_top, c->border_bottom };
    dock_frame_draw(rp, &frame, panel->width, panel->height, dock_corner_radius);
    fit_label(rp, caption, sizeof(caption), panel->title, panel->width - 82);
    text(rp, 16, 11 + baseline, caption, panel->text_pen);
    text(rp, panel->width - 18 - TextLength(rp, (CONST_STRPTR)"Edit", 4),
         11 + baseline, "Edit", panel->muted_pen);
    for (int i = 0; i < panel->count; ++i) {
        int x = panel->grid_x + (i % panel->columns) * panel->cell_width;
        int y = panel->grid_y + (i / panel->columns) * panel->cell_height;
        if (i == panel->hover || i == panel->pressed)
            dock_round_fill(rp, x + 3, y + 2, panel->cell_width - 6,
                           panel->cell_height - 4, 6,
                           i == panel->pressed ? c->pressed : c->hover, c->popup_fill);
        if (panel->svg[i]) {
            WritePixelArrayAlpha(panel->svg[i], 0, 0, panel->size * 4, rp,
                                 x + (panel->cell_width - panel->size) / 2, y + 7,
                                 panel->size, panel->size, 0xffffffffUL);
        } else if (panel->icons[i].object) {
            dock_native_draw(&panel->icons[i], rp,
                             x + (panel->cell_width - panel->icons[i].width) / 2,
                             y + 7 + (panel->size - panel->icons[i].height) / 2,
                             i == panel->pressed);
        } else {
            FillPixelArray(rp, x + (panel->cell_width - 24) / 2,
                           y + 7 + (panel->size - 24) / 2, 24, 24, c->preview_border);
        }
        fit_label(rp, caption, sizeof(caption), panel->labels[i], panel->cell_width - 10);
        text(rp, x + (panel->cell_width - TextLength(rp, (CONST_STRPTR)caption,
                                                   (ULONG)strlen(caption))) / 2,
             y + panel->size + 13 + baseline, caption, panel->text_pen);
    }
    if (!panel->count) {
        text(rp, 18, panel->grid_y + 22 + baseline, "This group is empty", panel->text_pen);
        text(rp, 18, panel->grid_y + 28 + baseline + rp->Font->tf_YSize,
             "Drop icons here", panel->muted_pen);
    }
    dock_buffer_show(&panel->buffer, panel->window);
}
