/* alpha_window_probe: does a composited alpha window show what it holds?
 *
 * Opens a borderless WA_AlphaSurface window with three 96x96 panels written
 * with WritePixelArray(RECTFMT_ARGB): opaque red, half-transparent green and
 * fully transparent. Logs the screen's pixel format, whether the layer got a
 * surface, what ReadPixelArray reads back from the window and from the
 * screen's own RastPort, then stays open for SECONDS so the host can capture
 * the display.
 *
 *   alpha_window_probe LOG/A,X/N,Y/N,SECONDS/N,PRE/N,REDRAW/S,SHAPE/S,
 *                      CARE/S,PLAIN/S
 *
 * PRE waits that many ticks between OpenWindow and the first write; REDRAW
 * writes the panels a second time halfway through the wait; SHAPE opens the
 * window with WA_ShapeRegion covering it, as Dockbar does. CARE drops
 * WA_NoCareRefresh and asks for Dockbar's IDCMP; PLAIN opens the window
 * without WA_AlphaSurface (control).
 */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/layers.h>
#include <cybergraphx/cybergraphics.h>
#include <graphics/regions.h>
#include <stdio.h>
#include <string.h>

#define PW 96
#define PH 96

static ULONG panel[PW * PH];

/* RECTFMT_ARGB is bytes A, R, G, B in memory, so a ULONG 0xAARRGGBB has to
 * be stored big-endian on x86. */
static void fill(ULONG argb)
{
    int i;
    for (i = 0; i < PW * PH; i++) panel[i] = AROS_LONG2BE(argb);
}

static ULONG read1(struct RastPort *rp, int x, int y)
{
    ULONG px = 0xDEADBEEF;
    ReadPixelArray(&px, 0, 0, 4, rp, x, y, 1, 1, RECTFMT_ARGB);
    return AROS_BE2LONG(px);
}

int main(void)
{
    IPTR args[9] = {0};
    struct RDArgs *rd = ReadArgs("LOG/A,X/N,Y/N,SECONDS/N,PRE/N,REDRAW/S,SHAPE/S,CARE/S,PLAIN/S", args, NULL);
    struct Region *shape = NULL;
    int pre = 0;
    FILE *f;
    struct Screen *s;
    struct Window *w;
    IPTR alpha = 0;
    int x = 100, y = 100, secs = 20, i;
    static const ULONG colours[3] = {0xFFFF0000, 0x8000FF00, 0x00000000};
    static const char *names[3] = {"opaque", "half", "clear"};

    if (!rd) { PrintFault(IoErr(), "alpha_window_probe"); return 20; }
    if (args[1]) x = *(LONG *)args[1];
    if (args[2]) y = *(LONG *)args[2];
    if (args[3]) secs = *(LONG *)args[3];
    if (args[4]) pre = *(LONG *)args[4];
    if (args[6])
    {
        struct Rectangle r = {0, 0, 3 * PW - 1, PH - 1};
        shape = NewRegion();
        if (shape) OrRectRegion(shape, &r);
    }
    f = fopen((char *)args[0], "w");
    if (!f) { FreeArgs(rd); return 20; }

    s = LockPubScreen(NULL);
    if (!s) { fprintf(f, "no screen\n"); fclose(f); FreeArgs(rd); return 20; }
    GetAttr(SA_SourceAlphaSupport, (Object *)s, &alpha);
    fprintf(f, "screen %dx%d depth=%d alpha=%ld\n", s->Width, s->Height,
            (int)GetBitMapAttr(s->RastPort.BitMap, BMA_DEPTH), (long)alpha);
    if (CyberGfxBase)
        fprintf(f, "screen bitmap cgx=%ld pixfmt=%ld bppix=%ld bpr=%ld\n",
                (long)GetCyberMapAttr(s->RastPort.BitMap, CYBRMATTR_ISCYBERGFX),
                (long)GetCyberMapAttr(s->RastPort.BitMap, CYBRMATTR_PIXFMT),
                (long)GetCyberMapAttr(s->RastPort.BitMap, CYBRMATTR_BPPIX),
                (long)GetCyberMapAttr(s->RastPort.BitMap, CYBRMATTR_XMOD));

    w = OpenWindowTags(NULL,
        WA_PubScreen, (IPTR)s, WA_Left, x, WA_Top, y,
        WA_Width, 3 * PW, WA_Height, PH,
        WA_Borderless, TRUE, WA_SimpleRefresh, TRUE,
        args[7] ? TAG_IGNORE : WA_NoCareRefresh, TRUE,
        args[7] ? WA_IDCMP : TAG_IGNORE, IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY |
            IDCMP_REFRESHWINDOW | IDCMP_INACTIVEWINDOW | IDCMP_CHANGEWINDOW,
        args[7] ? WA_ReportMouse : TAG_IGNORE, TRUE,
        WA_Activate, FALSE, WA_RMBTrap, TRUE,
        args[8] ? TAG_IGNORE : WA_AlphaSurface, TRUE,
        shape ? WA_ShapeRegion : TAG_IGNORE, (IPTR)shape, TAG_DONE);
    UnlockPubScreen(NULL, s);
    if (!w) { fprintf(f, "window failed\n"); fclose(f); FreeArgs(rd); return 20; }
    fprintf(f, "window at %d,%d composited=%d\n", w->LeftEdge, w->TopEdge,
            (w->WLayer->Flags & LAYERCOMPOSITED) != 0);
    if (CyberGfxBase)
        fprintf(f, "window bitmap pixfmt=%ld bppix=%ld\n",
                (long)GetCyberMapAttr(w->RPort->BitMap, CYBRMATTR_PIXFMT),
                (long)GetCyberMapAttr(w->RPort->BitMap, CYBRMATTR_BPPIX));

    fprintf(f, "pre=%d redraw=%d shape=%d care=%d plain=%d\n", pre, args[5] != 0, shape != NULL,
            args[7] != 0, args[8] != 0);
    {
        struct Layer *l = w->WLayer;
        int n = 0;
        struct Layer *t;
        for (t = l->front; t; t = t->front) n++;
        fprintf(f, "layer flags=%08lx in front of it=%d cliprect=%p\n",
                (unsigned long)l->Flags, n, (void *)l->ClipRect);
    }
    fflush(f);
    if (pre) Delay(pre);
    for (i = 0; i < 3; i++)
    {
        fill(colours[i]);
        WritePixelArray(panel, 0, 0, PW * 4, w->RPort, i * PW, 0, PW, PH, RECTFMT_ARGB);
    }
    Delay(25);
    for (i = 0; i < 3; i++)
    {
        int cx = i * PW + PW / 2, cy = PH / 2;
        fprintf(f, "%s wrote=%08lx window=%08lx screenrp=%08lx\n", names[i],
                (unsigned long)colours[i],
                (unsigned long)read1(w->RPort, cx, cy),
                (unsigned long)read1(&w->WScreen->RastPort, w->LeftEdge + cx, w->TopEdge + cy));
    }
    fprintf(f, "waiting %d s\n", secs);
    fflush(f);
    if (args[5])
    {
        Delay(secs * 25);
        for (i = 0; i < 3; i++)
        {
            fill(colours[i]);
            WritePixelArray(panel, 0, 0, PW * 4, w->RPort, i * PW, 0, PW, PH, RECTFMT_ARGB);
        }
        fprintf(f, "redrawn\n");
        fflush(f);
        Delay(secs * 25);
    }
    else
        Delay(secs * 50);
    CloseWindow(w);
    if (shape) DisposeRegion(shape);
    fprintf(f, "END\n");
    fclose(f);
    FreeArgs(rd);
    return 0;
}
