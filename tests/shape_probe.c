/* Development probe, not shipped: can a dock window change between
 * non-empty shapes at run time, does it keep drawing afterwards, is the
 * desktop repainted where the shape opens, and do clicks in a hole reach
 * the window behind?   shape_probe LOG
 * The window is 200x80 at 400,300. Phases (each logged with pixel counts):
 *  A full rectangle; B two blocks with a hole at x 60..139 (the old region
 *  object passed back later, as a reused region); C back to A's object;
 *  D a new region object again with the hole. During B and D the probe waits
 *  20 s and logs every click it receives, with its position. */
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
#include <string.h>

struct Library *CyberGfxBase;
enum { W = 200, H = 80, X = 400, Y = 300 };
static unsigned char px[W * H * 4];
static FILE *out;

static struct Region *holed(void)
{
    struct Region *r = NewRegion();
    struct Rectangle a = {0, 0, 59, H - 1}, b = {140, 0, W - 1, H - 1};
    OrRectRegion(r, &a);
    OrRectRegion(r, &b);
    return r;
}

static void fill(struct Window *w, struct Screen *s, ULONG rgb, const char *what)
{
    ULONG wrote = FillPixelArray(w->RPort, 0, 0, W, H, rgb);
    ReadPixelArray(px, 0, 0, W * 4, &s->RastPort, X, Y, W, H, RECTFMT_ARGB);
    int ours = 0, hole_ours = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            unsigned char *p = px + 4 * (y * W + x);
            int match = p[1] == ((rgb >> 16) & 255) && p[2] == ((rgb >> 8) & 255) && p[3] == (rgb & 255);
            ours += match;
            if (x >= 60 && x < 140) hole_ours += match;
        }
    fprintf(out, "%s fill=%06lx returned=%lu on-screen=%d hole-still-ours=%d\n", what, (unsigned long)rgb,
            (unsigned long)wrote, ours, hole_ours);
    fflush(out);
}

/* Where the hole is, how many pixels still show our earlier colour. */
static void hole_report(struct Screen *s, ULONG rgb, const char *what)
{
    ReadPixelArray(px, 0, 0, W * 4, &s->RastPort, X, Y, W, H, RECTFMT_ARGB);
    int left = 0;
    for (int y = 0; y < H; y++)
        for (int x = 60; x < 140; x++) {
            unsigned char *p = px + 4 * (y * W + x);
            left += p[1] == ((rgb >> 16) & 255) && p[2] == ((rgb >> 8) & 255) && p[3] == (rgb & 255);
        }
    fprintf(out, "%s hole pixels still showing the old fill=%d of %d\n", what, left, 80 * H);
    fflush(out);
}

static void listen(struct Window *w, const char *what)
{
    fprintf(out, "%s listening 20 s for clicks\n", what);
    fflush(out);
    for (int t = 0; t < 400; t++) {
        struct IntuiMessage *m;
        while ((m = (struct IntuiMessage *)GetMsg(w->UserPort))) {
            if (m->Class == IDCMP_MOUSEBUTTONS)
                fprintf(out, "%s click code=%u at %d,%d\n", what, m->Code, m->MouseX, m->MouseY);
            ReplyMsg((struct Message *)m);
        }
        fflush(out);
        Delay(2);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2 || !(out = fopen(argv[1], "w"))) return 20;
    if (!(CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 0))) return 20;
    struct Screen *s = LockPubScreen(NULL);
    struct Region *full = NewRegion();
    struct Rectangle r = {0, 0, W - 1, H - 1};
    OrRectRegion(full, &r);
    struct Window *w = OpenWindowTags(NULL, WA_PubScreen, (IPTR)s, WA_Left, X, WA_Top, Y, WA_Width, W, WA_Height, H,
                                      WA_Borderless, TRUE, WA_Activate, FALSE, WA_SimpleRefresh, TRUE, WA_RMBTrap, TRUE,
                                      WA_ShapeRegion, (IPTR)full, WA_IDCMP, IDCMP_MOUSEBUTTONS, TAG_DONE);
    UnlockPubScreen(NULL, s);
    if (!w) { fputs("no window\n", out); return 20; }
    fill(w, s, 0xc02020, "A");

    struct Region *b = holed();
    struct Region *old = ChangeWindowShape(w, b, NULL);
    fprintf(out, "B returned %s\n", old == full ? "A's region" : old ? "other" : "NULL");
    hole_report(s, 0xc02020, "B");
    fill(w, s, 0x2020c0, "B");
    listen(w, "B");

    old = ChangeWindowShape(w, full, NULL);
    fprintf(out, "C returned %s\n", old == b ? "B's region" : old ? "other" : "NULL");
    fill(w, s, 0x20c020, "C");
    if (old) DisposeRegion(old);

    struct Region *d = holed();
    old = ChangeWindowShape(w, d, NULL);
    fprintf(out, "D returned %s\n", old == full ? "A's region" : old ? "other" : "NULL");
    hole_report(s, 0x20c020, "D");
    fill(w, s, 0xc0c020, "D");
    listen(w, "D");

    CloseWindow(w);
    DisposeRegion(d);
    if (old) DisposeRegion(old);
    fputs("END\n", out);
    fclose(out);
    CloseLibrary(CyberGfxBase);
    return 0;
}
