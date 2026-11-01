/* Paints a solid backdrop window so the dock can be judged on light and dark
   ground without changing the shared machine's Wanderer wallpaper. */
#include <intuition/intuition.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <stdio.h>
#include <stdlib.h>

struct Library *CyberGfxBase;

int main(int argc, char **argv)
{
    if (argc != 7) {
        puts("backdrop_probe LEFT TOP WIDTH HEIGHT RRGGBB SECONDS");
        return 20;
    }
    int left = atoi(argv[1]), top = atoi(argv[2]), width = atoi(argv[3]),
        height = atoi(argv[4]), seconds = atoi(argv[6]);
    unsigned long color = strtoul(argv[5], NULL, 16);
    struct Screen *screen = LockPubScreen(NULL);
    CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 0);
    if (!screen || !CyberGfxBase) return 20;
    struct Window *window = OpenWindowTags(NULL, WA_PubScreen, (IPTR)screen,
        WA_Left, left, WA_Top, top, WA_Width, width, WA_Height, height,
        WA_Backdrop, TRUE, WA_Borderless, TRUE, WA_Activate, FALSE,
        WA_SmartRefresh, TRUE, WA_IDCMP, 0, TAG_DONE);
    int ok = window != NULL;
    if (window) {
        FillPixelArray(window->RPort, 0, 0, window->Width, window->Height, color);
        for (int i = 0; i < seconds * 10; i++) {
            if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) break;
            Delay(5);
        }
        CloseWindow(window);
    }
    CloseLibrary(CyberGfxBase);
    UnlockPubScreen(NULL, screen);
    return ok ? 0 : 20;
}
