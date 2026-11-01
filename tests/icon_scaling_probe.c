/* Original finite visual probe: stock icon.library ScaleBox versus the dock's
 * filtered native-icon cache. No system or source icon pixels are modified. */
#include "native_icons.h"
#include <exec/libraries.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/text.h>
#include <proto/cybergraphics.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/icon.h>
#include <proto/intuition.h>
#include <workbench/icon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct Library *CyberGfxBase;

struct Cell {
    struct DiskObject *old;
    DockNativeIcon filtered;
    int old_width, old_height, source_width, source_height, box;
};
static struct Cell cells[4][4];
static const int sizes[4] = { 32, 40, 48, 64 };
static const char *names[4] = { "Shell", "MultiView", "Prefs", "Clock" };
static const char *paths[4] = {
    "SYS:System/Shell", "SYS:Utilities/MultiView", "SYS:Prefs", "SYS:Utilities/Clock"
};

static void label(struct RastPort *rp, int x, int y, const char *value, LONG pen)
{
    SetAPen(rp, (ULONG)pen);
    SetDrMd(rp, JAM1);
    Move(rp, x, y);
    Text(rp, (CONST_STRPTR)value, (ULONG)strlen(value));
}

static void source_size(struct DiskObject *object, int selected, int *w, int *h)
{
    const ULONG *pixels = NULL;
    ULONG width = 0, height = 0;
    struct TagItem tags[] = {
        { selected ? ICONCTRLA_GetARGBImageData2 : ICONCTRLA_GetARGBImageData1,
          (IPTR)&pixels },
        { ICONCTRLA_GetWidth, (IPTR)&width },
        { ICONCTRLA_GetHeight, (IPTR)&height },
        { TAG_DONE, 0 }
    };
    /* Force lazy PNG decoding before reading the original face dimensions. */
    IconControlA(object, tags);
    *w = width ? (int)width : object->do_Gadget.Width;
    *h = height ? (int)height : object->do_Gadget.Height;
}

static int load_reference(struct Cell *cell, const char *path,
                          struct Screen *screen, int size, int selected)
{
    struct TagItem loading[] = {
        { ICONGETA_RemapIcon, FALSE },
        { ICONGETA_GetPaletteMappedIcon, FALSE },
        { ICONGETA_GenerateImageMasks, FALSE }, { TAG_DONE, 0 }
    };
    struct TagItem drawing[] = {
        { ICONDRAWA_Borderless, TRUE }, { TAG_DONE, 0 }
    };
    struct TagItem scaling[] = {
        { ICONCTRLA_SetScaleBox, 0 }, { TAG_DONE, 0 }
    };
    struct Rectangle bounds;
    int selected_width, selected_height, largest;
    cell->old = GetIconTagList((CONST_STRPTR)path, loading);
    if (!cell->old)
        return 0;
    source_size(cell->old, 0, &cell->source_width, &cell->source_height);
    if (cell->source_width < 1 || cell->source_height < 1 ||
        cell->source_width > 1024 || cell->source_height > 1024)
        return 0;
    largest = cell->source_width > cell->source_height ?
              cell->source_width : cell->source_height;
    cell->box = size < largest ? size : largest;
    if (selected) {
        source_size(cell->old, 1, &selected_width, &selected_height);
        if (selected_width < 1 || selected_height < 1 ||
            selected_width > 1024 || selected_height > 1024)
            return 0;
        largest = selected_width > selected_height ? selected_width : selected_height;
        /* The reference must never exercise stock selected-image enlargement:
         * its old sampling path can read beyond the source when scaled up. */
        if (cell->box > largest)
            cell->box = largest;
    }
    scaling[0].ti_Data = PACK_ICON_SCALEBOX(cell->box, cell->box);
    IconControlA(cell->old, scaling);
    if (!LayoutIconA(cell->old, screen, NULL) ||
        !GetIconRectangleA(&screen->RastPort, cell->old, NULL, &bounds, drawing))
        return 0;
    cell->old_width = bounds.MaxX - bounds.MinX + 1;
    cell->old_height = bounds.MaxY - bounds.MinY + 1;
    return cell->old_width > 0 && cell->old_height > 0 &&
           cell->old_width <= 80 && cell->old_height <= 80;
}

static void draw(struct Window *window, int selected, int light, LONG pen)
{
    struct RastPort *rp = window->RPort;
    int left = window->BorderLeft, top = window->BorderTop;
    struct TagItem tags[] = {
        { ICONDRAWA_Borderless, TRUE },
        { ICONDRAWA_EraseBackground, FALSE }, { TAG_DONE, 0 }
    };
    FillPixelArray(rp, left, top,
                   window->Width - window->BorderLeft - window->BorderRight,
                   window->Height - window->BorderTop - window->BorderBottom,
                   light ? 0xffe9edf1UL : 0xff202a38UL);
    label(rp, left + 16, top + 24,
          "Native icon comparison: old = stock ScaleBox; new = dock Lanczos", pen);
    label(rp, left + 16, top + 45,
          selected ? "SELECTED images  |  Escape or close to exit  |  90 second limit" :
                     "NORMAL images  |  Escape or close to exit  |  90 second limit", pen);
    for (int col = 0; col < 4; ++col) {
        char caption[32];
        snprintf(caption, sizeof(caption), "%d px", sizes[col]);
        label(rp, left + 115 + col * 208, top + 76, caption, pen);
    }
    for (int row = 0; row < 4; ++row) {
        int y = top + 96 + row * 126;
        label(rp, left + 10, y + 55, names[row], pen);
        for (int col = 0; col < 4; ++col) {
            struct Cell *cell = &cells[row][col];
            int x = left + 108 + col * 208;
            label(rp, x + 23, y + 13, "old", pen);
            label(rp, x + 119, y + 13, "new", pen);
            if (cell->old_width > 0)
                DrawIconStateA(rp, cell->old, NULL,
                               x + (80 - cell->old_width) / 2,
                               y + 26 + (80 - cell->old_height) / 2,
                               selected ? IDS_SELECTED : IDS_NORMAL, tags);
            else
                label(rp, x + 2, y + 61, "failed", pen);
            if (cell->filtered.object)
                dock_native_draw(&cell->filtered, rp,
                                 x + 96 + (80 - cell->filtered.width) / 2,
                                 y + 26 + (80 - cell->filtered.height) / 2, selected);
            else
                label(rp, x + 98, y + 61, "failed", pen);
        }
    }
}

int main(int argc, char **argv)
{
    struct Screen *screen = NULL;
    struct Window *window = NULL;
    FILE *log;
    LONG pen = -1;
    int selected, light, quit = 0, failures = 0, result = 20;
    time_t started;
    if (argc < 2 || argc > 4) {
        puts("icon_scaling_probe LOGFILE [selected:0|1] [light:0|1]");
        return 20;
    }
    selected = argc > 2 ? atoi(argv[2]) : 0;
    light = argc > 3 ? atoi(argv[3]) : 0;
    if ((selected != 0 && selected != 1) || (light != 0 && light != 1))
        return 20;
    log = fopen(argv[1], "w");
    if (!log)
        return 20;
    fprintf(log, "Icon scaling visual probe: selected=%d light=%d x86_64 ABIv11 SDK 2026.09\n",
            selected, light);
    CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 0);
    screen = LockPubScreen(NULL);
    if (!CyberGfxBase || !screen)
        goto cleanup;
    fprintf(log, "screen=%dx%d icon.library=%u.%u\n", screen->Width, screen->Height,
            IconBase->lib_Version, IconBase->lib_Revision);
    if (screen->Width < 960 || screen->Height < 680) {
        fputs("FAIL screen requires at least 960x680\n", log);
        goto cleanup;
    }
    pen = ObtainBestPen(screen->ViewPort.ColorMap,
                        light ? 0x20202020UL : 0xf1f1f1f1UL,
                        light ? 0x29292929UL : 0xf5f5f5f5UL,
                        light ? 0x34343434UL : 0xf9f9f9f9UL, TAG_DONE);
    if (pen < 0)
        goto cleanup;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            struct Cell *cell = &cells[row][col];
            int old_ok = load_reference(cell, paths[row], screen, sizes[col], selected);
            int new_ok = dock_native_load(&cell->filtered, paths[row], screen, sizes[col]);
            if (!old_ok) cell->old_width = cell->old_height = 0;
            if (!old_ok || !new_ok) ++failures;
            fprintf(log, "%s size=%d old_ok=%d source=%dx%d box=%d target=%dx%d\n",
                    names[row], sizes[col], old_ok, cell->source_width,
                    cell->source_height, cell->box, cell->old_width, cell->old_height);
            fprintf(log, "  new_ok=%d source=%dx%d target=%dx%d mode=%d cache0=%dx%d cache1=%dx%d\n",
                    new_ok, cell->filtered.source_width, cell->filtered.source_height,
                    cell->filtered.width, cell->filtered.height, cell->filtered.mode,
                    cell->filtered.pixel_width[0], cell->filtered.pixel_height[0],
                    cell->filtered.pixel_width[1], cell->filtered.pixel_height[1]);
        }
    }
    fflush(log);
    window = OpenWindowTags(NULL, WA_PubScreen, (IPTR)screen,
                            WA_Left, (screen->Width - 960) / 2, WA_Top, 28,
                            WA_Width, 960, WA_Height, 650,
                            WA_Title, (IPTR)"Dockbar icon scaling: old / new",
                            WA_DragBar, TRUE, WA_DepthGadget, TRUE,
                            WA_CloseGadget, TRUE, WA_Activate, TRUE,
                            WA_SimpleRefresh, TRUE,
                            WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_RAWKEY | IDCMP_REFRESHWINDOW,
                            TAG_DONE);
    if (!window)
        goto cleanup;
    SetFont(window->RPort, screen->RastPort.Font);
    draw(window, selected, light, pen);
    fputs("READY visual matrix; mode 0=planar 1=ARGB 2=palette\n", log);
    fflush(log);
    started = time(NULL);
    while (!quit && time(NULL) - started < 90) {
        struct IntuiMessage *message;
        if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
            break;
        while ((message = (struct IntuiMessage *)GetMsg(window->UserPort))) {
            ULONG cls = message->Class;
            UWORD code = message->Code;
            ReplyMsg((struct Message *)message);
            if (cls == IDCMP_CLOSEWINDOW || (cls == IDCMP_RAWKEY && code == 0x45))
                quit = 1;
            if (cls == IDCMP_REFRESHWINDOW) {
                BeginRefresh(window);
                draw(window, selected, light, pen);
                EndRefresh(window, TRUE);
            }
        }
        Delay(2);
    }
    result = failures ? 10 : 0;
cleanup:
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            dock_native_free(&cells[row][col].filtered);
            if (cells[row][col].old)
                FreeDiskObject(cells[row][col].old);
        }
    }
    if (window) CloseWindow(window);
    if (pen >= 0) ReleasePen(screen->ViewPort.ColorMap, (ULONG)pen);
    if (screen) UnlockPubScreen(NULL, screen);
    if (CyberGfxBase) CloseLibrary(CyberGfxBase);
    fprintf(log, "END result=%d failed_cells=%d; visual quality requires inspection\n", result, failures);
    fclose(log);
    return result;
}
