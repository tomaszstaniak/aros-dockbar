/* Development prototype, not shipped: a drop shadow under a dock on stock
 * AROS, to judge the look before deciding on it.   shadow_probe LOG
 * ENV:shadow holds "X Y W H RADIUS" of a dock window (from Dockbar's log,
 * VIEW lines). The probe reads the desktop around that rectangle, opens a
 * borderless window behind the dock, a margin larger on each side, and
 * fills it with the desktop picture darkened by a soft rounded shadow,
 * offset downwards. The dock is moved in front of it. Stays 60 s.
 * What it shows about the platform: the shadow is a picture of the
 * desktop (like a see-through dock), and the shadow window takes the
 * clicks on its pixels; stock AROS has no window that is visible but lets
 * clicks through. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

struct Library *CyberGfxBase;
enum { MARGIN = 14, OFFSET = 4, BLUR = 9 };
#define OPACITY 0.42f

/* Signed distance from (px,py) to a rounded rectangle; negative inside. */
static float sdf(float px, float py, float x, float y, float w, float h, float r)
{
    float cx = x + w / 2, cy = y + h / 2, qx = fabsf(px - cx) - (w / 2 - r), qy = fabsf(py - cy) - (h / 2 - r);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    float in = qx > qy ? qx : qy;
    return sqrtf(ox * ox + oy * oy) + (in < 0 ? in : 0) - r;
}

int main(int argc, char **argv)
{
    FILE *out = argc > 1 ? fopen(argv[1], "w") : NULL;
    char spec[64] = "";
    int x, y, w, h, r;
    if (!out || GetVar((CONST_STRPTR)"shadow", (STRPTR)spec, sizeof spec, 0) <= 0 ||
        sscanf(spec, "%d %d %d %d %d", &x, &y, &w, &h, &r) != 5) return 20;
    if (!(CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 0))) return 20;
    struct Screen *s = LockPubScreen(NULL);
    if (!s) return 20;
    /* The dock's own window: the one at exactly that rectangle. */
    struct Window *dock = NULL;
    ULONG lock = LockIBase(0);
    for (struct Window *win = s->FirstWindow; win; win = win->NextWindow)
        if (win->LeftEdge == x && win->TopEdge == y && win->Width == w && win->Height == h) dock = win;
    UnlockIBase(lock);
    int sx = x - MARGIN, sy = y - MARGIN + OFFSET, sw = w + 2 * MARGIN, sh = h + 2 * MARGIN;
    if (sx < 0) { sw += sx; sx = 0; }
    if (sy < 0) { sh += sy; sy = 0; }
    if (sx + sw > s->Width) sw = s->Width - sx;
    if (sy + sh > s->Height) sh = s->Height - sy;
    unsigned char *px = malloc((size_t)sw * sh * 4);
    if (!px || ReadPixelArray(px, 0, 0, sw * 4, &s->RastPort, sx, sy, sw, sh, RECTFMT_ARGB) != (ULONG)(sw * sh)) return 20;
    for (int j = 0; j < sh; j++)
        for (int i = 0; i < sw; i++) {
            /* The shadow of the dock rectangle moved down by OFFSET, softened
             * over BLUR pixels outside its edge. */
            float d = sdf(sx + i + 0.5f, sy + j + 0.5f, x, y + OFFSET, w, h, r);
            float t = d <= 0 ? 1 : d >= BLUR ? 0 : 1 - d / BLUR;
            float a = OPACITY * t * t * (3 - 2 * t);
            unsigned char *p = px + 4 * (j * sw + i);
            for (int c = 1; c < 4; c++) p[c] = (unsigned char)(p[c] * (1 - a));
        }
    struct Window *win = OpenWindowTags(NULL, WA_PubScreen, (IPTR)s, WA_Left, sx, WA_Top, sy, WA_Width, sw, WA_Height, sh,
                                        WA_Borderless, TRUE, WA_Activate, FALSE, WA_SmartRefresh, TRUE, WA_IDCMP, IDCMP_MOUSEBUTTONS, TAG_DONE);
    fprintf(out, "SHADOW dock=%s window=%dx%d at %d,%d margin=%d offset=%d blur=%d opacity=%.2f\n",
            dock ? "found" : "NOT FOUND", sw, sh, sx, sy, MARGIN, OFFSET, BLUR, OPACITY);
    if (win) {
        WritePixelArray(px, 0, 0, sw * 4, win->RPort, 0, 0, sw, sh, RECTFMT_ARGB);
        if (dock) MoveWindowInFrontOf(dock, win);
        /* Clicks that land on the shadow, to show that they do not reach the
         * desktop behind it. */
        for (int t = 0; t < 60 * 10; t++) {
            struct IntuiMessage *m;
            while ((m = (struct IntuiMessage *)GetMsg(win->UserPort))) {
                if (m->Class == IDCMP_MOUSEBUTTONS && m->Code == SELECTDOWN)
                    fprintf(out, "CLICK on shadow at %d,%d (taken by the shadow window)\n", m->MouseX + sx, m->MouseY + sy);
                ReplyMsg((struct Message *)m);
            }
            Delay(5);
        }
        CloseWindow(win);
    }
    fclose(out);
    free(px);
    UnlockPubScreen(NULL, s);
    CloseLibrary(CyberGfxBase);
    return 0;
}
