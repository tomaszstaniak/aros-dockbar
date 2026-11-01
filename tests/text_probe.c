/* Development prototype, not shipped: the same dock strings drawn three
 * ways, to compare on screen and in time.   text_probe LOG
 *   System: the screen font with Text(), as Dockbar draws today.
 *   Outline: the screen font opened through diskfont, if it is an outline
 *            font (else "Vera Sans.font"), drawn with Text().
 *   Smooth:  the TrueType file behind the screen font (its .otag), with
 *            FreeType from memory, Open Sans after it for missing letters.
 * Each on the dark and the light dock colours. Strings in ISO-8859-2 when
 * ENV:CHARSET names 8859-2, else Latin-1 (the Polish line then shows what a
 * Latin-1 system makes of those bytes). The window stays open 60 s. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/text.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/diskfont.h>
#include <proto/cybergraphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "textrender.h"
#include "fontfile.h"

struct Library *CyberGfxBase, *DiskfontBase;
enum { W = 820, ROW = 34, SAMPLES = 4, METHODS = 3 };
static const char *samples[SAMPLES] = { "MultiView", "Za\xbf\xf3\xb3\xe6 g\xea\xb6l\xb1 ja\xbc\xf1", "12:34:56", "Preferences..." };
static const char *method_names[METHODS] = { "System", "Outline", "Smooth" };
static unsigned char *canvas;
static int CH;

static unsigned char *slurp(const char *path, LONG *size)
{
    BPTR f = Open((CONST_STRPTR)path, MODE_OLDFILE);
    if (!f) return NULL;
    Seek(f, 0, OFFSET_END);
    LONG n = Seek(f, 0, OFFSET_BEGINNING);
    unsigned char *d = n > 0 ? malloc((size_t)n) : NULL;
    if (d && Read(f, d, n) != n) { free(d); d = NULL; }
    Close(f);
    *size = n;
    return d;
}

static void fill(int x, int y, int w, int h, ULONG rgb)
{
    for (int j = y; j < y + h && j < CH; j++)
        for (int i = x; i < x + w && i < W; i++) {
            unsigned char *p = canvas + 4 * (j * W + i);
            p[0] = 255; p[1] = rgb >> 16; p[2] = rgb >> 8; p[3] = rgb;
        }
}
static void blend(int x, int y, const unsigned char *cov, int cw, int chh, int stride, ULONG rgb)
{
    for (int j = 0; j < chh; j++)
        for (int i = 0; i < cw; i++) {
            int a = cov[j * stride + i], px = x + i, py = y + j;
            if (!a || px >= W || py >= CH) continue;
            unsigned char *p = canvas + 4 * (py * W + px);
            p[1] = (unsigned char)(p[1] + ((int)((rgb >> 16) & 255) - p[1]) * a / 255);
            p[2] = (unsigned char)(p[2] + ((int)((rgb >> 8) & 255) - p[2]) * a / 255);
            p[3] = (unsigned char)(p[3] + ((int)(rgb & 255) - p[3]) * a / 255);
        }
}

/* Text() into a scratch bitmap, white on black; coverage = brightest channel. */
static struct RastPort scratch_rp;
static struct BitMap *scratch;
static unsigned char scratch_px[600 * 40 * 4], scratch_cov[600 * 40];
static LONG white_pen;
static int text_cover(struct TextFont *font, const char *s, int *w, int *h)
{
    SetFont(&scratch_rp, font);
    *w = TextLength(&scratch_rp, (CONST_STRPTR)s, strlen(s));
    *h = font->tf_YSize + 2;
    if (*w > 600) *w = 600;
    if (*h > 40) *h = 40;
    FillPixelArray(&scratch_rp, 0, 0, *w, *h, 0);
    SetAPen(&scratch_rp, (ULONG)white_pen);
    SetDrMd(&scratch_rp, JAM1);
    Move(&scratch_rp, 0, font->tf_Baseline + 1);
    Text(&scratch_rp, (CONST_STRPTR)s, strlen(s));
    ReadPixelArray(scratch_px, 0, 0, *w * 4, &scratch_rp, 0, 0, *w, *h, RECTFMT_ARGB);
    for (int i = 0; i < *w * *h; i++) {
        unsigned char *p = scratch_px + 4 * i;
        int c = p[1] > p[2] ? p[1] : p[2];
        scratch_cov[i] = (unsigned char)(c > p[3] ? c : p[3]);
    }
    return 1;
}

static ULONG ms(void)
{
    struct DateStamp d;
    DateStamp(&d);
    return (ULONG)(d.ds_Minute * 60000 + d.ds_Tick * 20);
}

int main(int argc, char **argv)
{
    FILE *out = argc > 1 ? fopen(argv[1], "w") : NULL;
    if (!out) return 20;
    if (!(CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 0))) return 20;
    DiskfontBase = OpenLibrary((CONST_STRPTR)"diskfont.library", 0);
    struct Screen *s = LockPubScreen(NULL);
    if (!s) return 20;
    struct TextAttr *ta = s->Font;
    fprintf(out, "SCREENFONT %s %d\n", ta && ta->ta_Name ? (const char *)ta->ta_Name : "?", ta ? ta->ta_YSize : 0);
    char charset[40] = "";
    GetVar((CONST_STRPTR)"CHARSET", (STRPTR)charset, sizeof charset, 0);
    int codeset = strstr(charset, "8859-2") ? DOCK_CODESET_LATIN2 : DOCK_CODESET_LATIN1;
    fprintf(out, "CHARSET '%s' -> %s\n", charset, codeset == DOCK_CODESET_LATIN2 ? "ISO-8859-2" : "ISO-8859-1");

    struct TextFont *system = s->RastPort.Font, *outline = NULL;
    /* Outline: the screen font if it has an .otag, else Vera Sans. */
    char otag[300], file[256];
    struct TextAttr attr = { (STRPTR)"Vera Sans.font", ta ? ta->ta_YSize : 13, 0, 0 };
    LONG size = 0;
    unsigned char *otag_data = ta && dock_otag_path((const char *)ta->ta_Name, otag, sizeof otag) ? slurp(otag, &size) : NULL;
    int system_outline = otag_data != NULL;
    if (!otag_data) { dock_otag_path("Vera Sans.font", otag, sizeof otag); otag_data = slurp(otag, &size); }
    else attr.ta_Name = ta->ta_Name;
    if (DiskfontBase) outline = OpenDiskFont(&attr);
    fprintf(out, "OUTLINE %s %s (screen font is %san outline font)\n", (const char *)attr.ta_Name, outline ? "opened" : "FAILED",
            system_outline ? "" : "not ");
    /* Smooth: the file named in that .otag, then Open Sans. */
    const unsigned char *faces[2] = { NULL, NULL };
    size_t sizes[2] = { 0, 0 };
    int found = otag_data && dock_otag_font_file(otag_data, (size_t)size, file, sizeof file);
    fprintf(out, "OTAG %s -> %s\n", otag, found ? file : "(no font file)");
    LONG n;
    if (found && (faces[0] = slurp(file, &n))) sizes[0] = (size_t)n;
    if ((faces[1] = slurp("FONTS:TrueType/OpenSans-Regular.ttf", &n))) sizes[1] = (size_t)n;
    struct DockTextRender tr;
    struct DockTextImage im;
    ULONG t0 = ms();
    /* Same line height as the system font (system->tf_YSize). */
    int smooth = dock_text_open(&tr, faces, sizes, 2, system->tf_YSize, codeset) && dock_text_fit_height(&tr, system->tf_YSize + 2);
    if (smooth) fprintf(out, "SMOOTH pixel size %d for system YSize %d\n", tr.pixel_size, system->tf_YSize);
    fprintf(out, "SMOOTH %s faces=%d open_ms=%lu ascender=%d descender=%d\n", smooth ? "ok" : "FAILED (would use System)",
            smooth ? tr.faces : 0, (unsigned long)(ms() - t0), tr.ascender, tr.descender);

    white_pen = ObtainBestPen(s->ViewPort.ColorMap, 0xffffffff, 0xffffffff, 0xffffffff, TAG_DONE);
    scratch = AllocBitMap(600, 40, 24, BMF_CLEAR, s->RastPort.BitMap);
    InitRastPort(&scratch_rp);
    scratch_rp.BitMap = scratch;
    CH = 20 + SAMPLES * METHODS * ROW;
    canvas = calloc((size_t)W * CH, 4);
    if (!scratch || !canvas) return 20;
    fill(0, 0, W, CH, 0xd0d4d8);
    for (int sm = 0; sm < SAMPLES; sm++)
        for (int m = 0; m < METHODS; m++) {
            int y = 10 + (sm * METHODS + m) * ROW;
            const unsigned char *cov = NULL;
            int cw = 0, chh = 0, stride = 0;
            if (m < 2) {
                struct TextFont *f = m == 0 ? system : outline;
                if (f && text_cover(f, samples[sm], &cw, &chh)) { cov = scratch_cov; stride = cw; }
            } else if (smooth && (cov = dock_text_render(&tr, samples[sm], &im))) { cw = im.w; chh = im.h; stride = cw; }
            /* Label column, then the string on the dark and the light bar. */
            int lw, lh;
            if (text_cover(system, method_names[m], &lw, &lh)) blend(10, y + 8, scratch_cov, lw, lh, lw, 0x202020);
            if (m < 2 && cov) text_cover(m == 0 ? system : outline, samples[sm], &cw, &chh);
            fill(100, y, 340, ROW - 4, 0x393e42);
            fill(450, y, 340, ROW - 4, 0xeef1f4);
            if (cov) {
                blend(110, y + (ROW - 4 - chh) / 2, cov, cw, chh, stride, 0xedf3fa);
                blend(460, y + (ROW - 4 - chh) / 2, cov, cw, chh, stride, 0x1d2328);
            }
            fprintf(out, "SAMPLE %d %s width=%d height=%d\n", sm, method_names[m], cw, chh);
        }
    /* Cost: 200 renders of each sample; Smooth both cold (cache dropped) and
     * cached. Text() timing includes the read-back that Dockbar does too. */
    for (int m = 0; m < METHODS; m++) {
        if ((m == 1 && !outline) || (m == 2 && !smooth)) continue;
        ULONG t = ms();
        int cw, chh;
        for (int k = 0; k < 200; k++)
            for (int sm = 0; sm < SAMPLES; sm++) {
                if (m < 2) text_cover(m == 0 ? system : outline, samples[sm], &cw, &chh);
                else {
                    if (k % 2 == 0)
                        for (int c = 0; c < 32; c++) { free(tr.cache[c].coverage); free(tr.cache[c].text); tr.cache[c].coverage = NULL; tr.cache[c].text = NULL; }
                    dock_text_render(&tr, samples[sm], &im);
                }
            }
        fprintf(out, "TIME %s 800 strings ms=%lu%s\n", method_names[m], (unsigned long)(ms() - t), m == 2 ? " (half of them from the cache)" : "");
    }
    struct Window *w = OpenWindowTags(NULL, WA_PubScreen, (IPTR)s, WA_Left, 100, WA_Top, 30, WA_InnerWidth, W, WA_InnerHeight, CH,
                                      WA_Title, (IPTR)"Dockbar text prototype", WA_DragBar, TRUE, WA_DepthGadget, TRUE, TAG_DONE);
    if (w) {
        WritePixelArray(canvas, 0, 0, W * 4, w->RPort, w->BorderLeft, w->BorderTop, W, CH, RECTFMT_ARGB);
        Delay(60 * 50);
        CloseWindow(w);
    }
    fclose(out);
    if (smooth) dock_text_close(&tr);
    if (outline) CloseFont(outline);
    FreeBitMap(scratch);
    ReleasePen(s->ViewPort.ColorMap, (ULONG)white_pen);
    UnlockPubScreen(NULL, s);
    if (DiskfontBase) CloseLibrary(DiskfontBase);
    CloseLibrary(CyberGfxBase);
    return 0;
}
