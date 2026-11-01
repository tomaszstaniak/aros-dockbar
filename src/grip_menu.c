#include "grip_menu.h"
#include "chrome.h"

#include <string.h>
#include <intuition/intuition.h>
#include <graphics/text.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#define MENU_PAD 6

static const char *const labels[DOCK_MENU_COUNT] = {
    "Preferences...", "Add separator", "Add space", "Add clock", "Add widget...", "Add command...", "Edit items...", "Refresh icons", "About...", "Quit"
};

int dock_menu_open(struct DockMenu *menu, struct Screen *screen, struct TextFont *font,
                   int anchor_x, int anchor_y, int anchor_width, int anchor_height, int vertical,
                   const struct DockColors *colors)
{
    struct RastPort measure;
    int widest = 0, left, top;
    if (!menu || menu->window) return 0;
    memset(menu, 0, sizeof(*menu));
    menu->hover = -1;
    menu->colors = colors ? colors : dock_colors(0);
    InitRastPort(&measure);
    SetFont(&measure, font);
    for (int i = 0; i < DOCK_MENU_COUNT; i++) {
        int w = TextLength(&measure, (CONST_STRPTR)labels[i], (ULONG)strlen(labels[i]));
        if (w > widest) widest = w;
    }
    menu->row = font->tf_YSize + 8;
    menu->width = widest + 36;
    menu->height = DOCK_MENU_COUNT * menu->row + MENU_PAD * 2;
    if (vertical) {
        left = anchor_x + anchor_width + 4;
        if (left + menu->width > screen->Width) left = anchor_x - menu->width - 4;
        top = anchor_y;
    } else {
        left = anchor_x;
        top = anchor_y - menu->height - 4;
        if (top < screen->BarHeight + 4) top = anchor_y + anchor_height + 4;
    }
    if (left + menu->width > screen->Width) left = screen->Width - menu->width;
    if (left < 0) left = 0;
    if (top + menu->height > screen->Height) top = screen->Height - menu->height;
    if (top < screen->BarHeight + 1) top = screen->BarHeight + 1;
    unsigned long t = menu->colors->text;
    menu->pen = ObtainBestPen(screen->ViewPort.ColorMap, ((t >> 16) & 255) * 0x01010101UL,
                              ((t >> 8) & 255) * 0x01010101UL, (t & 255) * 0x01010101UL, TAG_DONE);
    menu->shape = dock_shape(menu->width, menu->height, dock_corner_radius);
    if (menu->pen < 0 || !menu->shape || !dock_buffer_open(&menu->buffer, screen, menu->width, menu->height))
        goto failed;
    menu->window = OpenWindowTags(NULL, WA_PubScreen, (IPTR)screen,
        WA_Left, left, WA_Top, top, WA_Width, menu->width, WA_Height, menu->height,
        WA_ShapeRegion, (IPTR)menu->shape, WA_Borderless, TRUE, WA_Activate, TRUE,
        WA_RMBTrap, TRUE, WA_ReportMouse, TRUE, WA_SimpleRefresh, TRUE,
        WA_IDCMP, IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_RAWKEY |
                  IDCMP_INACTIVEWINDOW | IDCMP_REFRESHWINDOW, TAG_DONE);
    if (!menu->window) goto failed;
    SetFont(&menu->buffer.rp, font);
    dock_menu_draw(menu);
    return 1;
failed:
    dock_menu_close(menu, screen);
    return 0;
}

int dock_menu_hit(const struct DockMenu *menu, int x, int y)
{
    if (!menu || !menu->window || x < 0 || x >= menu->width || y < MENU_PAD) return -1;
    int index = (y - MENU_PAD) / menu->row;
    return index < DOCK_MENU_COUNT ? index : -1;
}

void dock_menu_draw(struct DockMenu *menu)
{
    struct RastPort *rp;
    if (!menu || !menu->window) return;
    rp = &menu->buffer.rp;
    const struct DockColors *c = menu->colors;
    struct DockFrameStyle frame = { c->popup_fill, c->popup_fill, c->border_top, c->border_bottom };
    dock_frame_draw(rp, &frame, menu->width, menu->height, dock_corner_radius);
    for (int i = 0; i < DOCK_MENU_COUNT; i++) {
        int y = MENU_PAD + i * menu->row;
        if (i == menu->hover)
            dock_round_fill(rp, 4, y, menu->width - 8, menu->row, 5, c->popup_hover, c->popup_fill);
        SetAPen(rp, (ULONG)menu->pen);
        SetDrMd(rp, JAM1);
        Move(rp, 16, y + 4 + rp->Font->tf_Baseline);
        Text(rp, (CONST_STRPTR)labels[i], (ULONG)strlen(labels[i]));
    }
    dock_buffer_show(&menu->buffer, menu->window);
}

void dock_menu_close(struct DockMenu *menu, struct Screen *screen)
{
    if (!menu) return;
    if (menu->window) {
        struct IntuiMessage *msg;
        while ((msg = (struct IntuiMessage *)GetMsg(menu->window->UserPort)))
            ReplyMsg((struct Message *)msg);
        CloseWindow(menu->window);
    }
    if (menu->shape) DisposeRegion(menu->shape);
    dock_buffer_close(&menu->buffer);
    /* width is set just before the pen is obtained, so a never-opened menu
     * (zero-initialized) does not release pen 0. */
    if (menu->width && menu->pen >= 0 && screen) ReleasePen(screen->ViewPort.ColorMap, (ULONG)menu->pen);
    memset(menu, 0, sizeof(*menu));
    menu->hover = -1;
}
