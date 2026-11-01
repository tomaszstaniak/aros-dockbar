/* Development probe, not shipped: is the desktop repainted where a small
 * window closes while Wanderer is dragging an icon?   expose_probe LOG
 * After 4 s (time to start a drag in Wanderer) it opens a 120x24 window at
 * ten places on the bare desktop in turn, each for 300 ms, and after each
 * close compares the area with what was there before. 6 s after the last
 * one (the drag is over by then) it compares all ten again. A pixel that
 * still shows the window was never repainted. Run it once during a drag and
 * once without one. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <stdio.h>
#include <string.h>

struct Library *CyberGfxBase;
enum { W = 120, H = 24, N = 10, X = 100, Y0 = 60, STEP = 40 };
static unsigned char before[N][W * H * 4], now[W * H * 4];

static int stale(struct Screen *s, int k)
{
    int count = 0;
    ReadPixelArray(now, 0, 0, W * 4, &s->RastPort, X, Y0 + k * STEP, W, H, RECTFMT_ARGB);
    for (int i = 0; i < W * H; i++)
        if (memcmp(now + 4 * i + 1, before[k] + 4 * i + 1, 3)) count++;
    return count;
}

int main(int argc, char **argv)
{
    FILE *out = argc > 1 ? fopen(argv[1], "w") : NULL;
    struct Screen *s;
    if (!out || !(CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 0))) return 20;
    if (!(s = LockPubScreen(NULL))) return 20;
    Delay(4 * 50);
    for (int k = 0; k < N; k++) {
        int y = Y0 + k * STEP;
        ReadPixelArray(before[k], 0, 0, W * 4, &s->RastPort, X, y, W, H, RECTFMT_ARGB);
        struct Window *w = OpenWindowTags(NULL, WA_PubScreen, (IPTR)s, WA_Left, X, WA_Top, y, WA_Width, W, WA_Height, H,
            WA_Borderless, TRUE, WA_Activate, FALSE, WA_SmartRefresh, TRUE, TAG_DONE);
        if (!w) { fprintf(out, "OPEN failed %d\n", k); continue; }
        FillPixelArray(w->RPort, 0, 0, W, H, 0x303030);
        Delay(15);
        CloseWindow(w);
        Delay(20);
        fprintf(out, "STEP %d y=%d unrepainted=%d of %d\n", k, y, stale(s, k), W * H);
    }
    Delay(6 * 50);
    int total = 0;
    for (int k = 0; k < N; k++) total += stale(s, k) > 0;
    fprintf(out, "LATER areas still unrepainted=%d of %d\n", total, N);
    fclose(out);
    UnlockPubScreen(NULL, s);
    CloseLibrary(CyberGfxBase);
    return 0;
}
