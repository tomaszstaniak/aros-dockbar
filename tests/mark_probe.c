/* Development probe, not shipped: does a pattern written into a shaped
 * window come back from a read of the screen, with alpha 0 and 255, before
 * and after a ChangeWindowShape cycle?  mark_probe LOG */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/regions.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library *CyberGfxBase;
enum { W = 100, H = 60, N = W * H };
static unsigned char a[N * 4], b[N * 4];

static int changed(const unsigned char *x, const unsigned char *y)
{
    int n = 0;
    for (int i = 0; i < N; i++)
        if (x[4 * i + 1] != y[4 * i + 1] || x[4 * i + 2] != y[4 * i + 2] || x[4 * i + 3] != y[4 * i + 3]) n++;
    return n;
}

static void round_trip(FILE *log, struct Window *w, struct Screen *s, const char *what, int alpha)
{
    ULONG r1 = ReadPixelArray(a, 0, 0, W * 4, &s->RastPort, w->LeftEdge, w->TopEdge, W, H, RECTFMT_ARGB);
    memcpy(b, a, sizeof a);
    for (int i = 0; i < N; i++) {
        b[4 * i + 3] ^= 0x08;
        if (alpha >= 0) b[4 * i] = (unsigned char)alpha;
    }
    ULONG wr = WritePixelArray(b, 0, 0, W * 4, w->RPort, 0, 0, W, H, RECTFMT_ARGB);
    ULONG r2 = ReadPixelArray(b, 0, 0, W * 4, &s->RastPort, w->LeftEdge, w->TopEdge, W, H, RECTFMT_ARGB);
    fprintf(log, "%s alpha=%d read=%lu write=%lu read=%lu changed=%d/%d first_alpha_read=%d\n", what, alpha,
            (unsigned long)r1, (unsigned long)wr, (unsigned long)r2, changed(a, b), N, a[0]);
}

int main(int argc, char **argv)
{
    if (argc < 2) return 20;
    FILE *log = fopen(argv[1], "w");
    if (!log || !(CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 0))) return 20;
    struct Screen *s = LockPubScreen(NULL);
    struct Region *shape = NewRegion(), *empty = NewRegion();
    struct Rectangle r = {0, 0, W - 1, H - 1};
    OrRectRegion(shape, &r);
    struct Window *w = OpenWindowTags(NULL, WA_PubScreen, (IPTR)s, WA_Left, 600, WA_Top, 300, WA_Width, W, WA_Height, H,
                                      WA_Borderless, TRUE, WA_Activate, FALSE, WA_SimpleRefresh, TRUE,
                                      WA_ShapeRegion, (IPTR)shape, TAG_DONE);
    UnlockPubScreen(NULL, s);
    if (!w) { fputs("no window\n", log); return 20; }
    fprintf(log, "screen rastport layer=%p window layer=%p\n", (void *)s->RastPort.Layer, (void *)w->RPort->Layer);
    fprintf(log, "depth=%lu pixfmt=%lu\n", (unsigned long)GetBitMapAttr(s->RastPort.BitMap, BMA_DEPTH),
            (unsigned long)GetCyberMapAttr(s->RastPort.BitMap, CYBRMATTR_PIXFMT));
    FillPixelArray(w->RPort, 0, 0, W, H, 0x336699);
    round_trip(log, w, s, "fresh", -1);
    round_trip(log, w, s, "fresh", 255);
    struct Region *old = ChangeWindowShape(w, empty, NULL);
    fprintf(log, "hide returned %s\n", old == shape ? "shape" : old ? "other" : "NULL");
    old = ChangeWindowShape(w, shape, NULL);
    fprintf(log, "show returned %s\n", old == empty ? "empty" : old ? "other" : "NULL");
    FillPixelArray(w->RPort, 0, 0, W, H, 0x336699);
    round_trip(log, w, s, "after", -1);
    round_trip(log, w, s, "after", 255);
    Delay(100);
    CloseWindow(w);
    DisposeRegion(shape);
    DisposeRegion(empty);
    fclose(log);
    CloseLibrary(CyberGfxBase);
    return 0;
}
