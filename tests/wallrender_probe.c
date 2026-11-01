/* Development prototype, not shipped: draws Wanderer's desktop background
 * from its prefs instead of reading the screen, then compares the result
 * with the screen pixel by pixel.
 *
 *   wallrender_probe LOG [PPMPREFIX]     (from Wanderer: ENV:wallrender)
 *
 * Mirrors workbench/system/Wanderer/iconwindowbackfill.c of the ABIv11 tree:
 * the picture is loaded with the same DataTypes tags and PDTM_SCALE to the
 * screen size; "scaled" mode then BitMapScale()s it to the screen bitmap
 * width and the height below the title bar, "tiled" mode repeats it from
 * the prefs offsets. Colour specs ("2:") are filled. Anything else (MUI
 * patterns, gradients, brushes) is reported as not supported. Only public
 * DataTypes/graphics calls are used; no Zune or Wanderer internals. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <datatypes/datatypes.h>
#include <datatypes/pictureclass.h>
#include <graphics/scale.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/datatypes.h>
#include <proto/cybergraphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library *CyberGfxBase, *DataTypesBase;
static FILE *out;

#include "wallprefs.h"

/* The full-width backdrop window is Wanderer's desktop. */
static int find_desktop(struct Screen *s, struct Window **desk)
{
    ULONG lock = LockIBase(0);
    *desk = NULL;
    for (struct Window *w = s->FirstWindow; w; w = w->NextWindow)
        if ((w->Flags & WFLG_BACKDROP) && w->Width == s->Width) *desk = w;
    UnlockIBase(lock);
    return *desk != NULL;
}

static int covered(struct Screen *s, struct Window *desk, int x, int y)
{
    for (struct Window *w = s->FirstWindow; w; w = w->NextWindow)
        if (w != desk && x >= w->LeftEdge && y >= w->TopEdge && x < w->LeftEdge + w->Width && y < w->TopEdge + w->Height)
            return 1;
    return 0;
}

static void ppm(const char *name, const unsigned char *argb, int w, int h)
{
    FILE *f = fopen(name, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) fwrite(argb + 4 * i + 1, 1, 3, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    static char line[200], *words[4];
    if (argc == 0) {
        argc = 1;
        if (GetVar((CONST_STRPTR)"wallrender", (STRPTR)line, sizeof line, 0) > 0)
            for (char *p = strtok(line, " \n"); p && argc < 4; p = strtok(NULL, " \n")) words[argc++] = p;
        argv = words;
    }
    if (argc < 2 || !(out = fopen(argv[1], "a"))) return 20;
    const char *ppmprefix = argc > 2 ? argv[2] : NULL;
    CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 0);
    DataTypesBase = OpenLibrary((CONST_STRPTR)"datatypes.library", 0);
    struct Screen *s = LockPubScreen(NULL);
    if (!CyberGfxBase || !DataTypesBase || !s) { fputs("FAIL libraries/screen\n", out); return 20; }

    static unsigned char raw[65536];
    struct DockWallPrefs wp;
    struct Window *desk;
    FILE *pf = fopen("ENV:SYS/Wanderer/global.prefs", "rb");
    size_t rawn = pf ? fread(raw, 1, sizeof raw, pf) : 0;
    if (pf) fclose(pf);
    if (!dock_wallprefs_parse(raw, rawn, &wp)) {
        /* Unknown or damaged: the dock would read the screen instead. */
        fprintf(out, "FALLBACK prefs not understood (%lu bytes): screen picture would be used\nEND\n", (unsigned long)rawn);
        fclose(out); UnlockPubScreen(NULL, s); return 5;
    }
    unsigned mode = (unsigned)wp.mode;
    int ox = wp.x, oy = wp.y;
    char spec[300];
    if (wp.kind == DOCK_WALL_COLOUR) snprintf(spec, sizeof spec, "2:%06lx", wp.rgb);
    else snprintf(spec, sizeof spec, "5:%s", wp.path);
    if (!find_desktop(s, &desk)) { fputs("FAIL no desktop window\n", out); UnlockPubScreen(NULL, s); return 20; }
    int ax = desk->LeftEdge + desk->BorderLeft, ay = desk->TopEdge + desk->BorderTop;
    int aw = desk->Width - desk->BorderLeft - desk->BorderRight, ah = desk->Height - desk->BorderTop - desk->BorderBottom;
    fprintf(out, "SCREEN %dx%d bar=%d bitmap=%lux%lu desktop=%d,%d %dx%d\n", s->Width, s->Height, s->BarHeight,
            (unsigned long)GetBitMapAttr(s->RastPort.BitMap, BMA_WIDTH), (unsigned long)GetBitMapAttr(s->RastPort.BitMap, BMA_HEIGHT),
            ax, ay, aw, ah);
    fprintf(out, "PREFS spec=%s mode=%u tile=%d offset=%d,%d\n", spec, mode, wp.tile, ox, oy);

    unsigned char *mine = calloc((size_t)aw * ah, 4), *real = calloc((size_t)aw * ah, 4);
    if (!mine || !real) { fputs("FAIL memory\n", out); return 20; }
    ULONG t0 = 0, t1 = 0, us;
    CurrentTime(&t0, &us); t0 = t0 * 1000 + us / 1000;
    int supported = 1;

    if (wp.kind == DOCK_WALL_COLOUR) {
        for (int i = 0; i < aw * ah; i++) {
            mine[4 * i] = 255; mine[4 * i + 1] = (unsigned char)(wp.rgb >> 16);
            mine[4 * i + 2] = (unsigned char)(wp.rgb >> 8); mine[4 * i + 3] = (unsigned char)wp.rgb;
        }
    } else if (wp.kind == DOCK_WALL_PICTURE) {
        Object *o = NewDTObject((APTR)wp.path, DTA_SourceType, DTST_FILE, DTA_GroupID, GID_PICTURE,
                                PDTA_DestMode, PMODE_V43, PDTA_Remap, TRUE, PDTA_Screen, (IPTR)s,
                                PDTA_FreeSourceBitMap, TRUE, OBP_Precision, PRECISION_IMAGE,
                                PDTA_DitherQuality, 2, TAG_DONE);
        struct BitMapHeader *bmh = NULL;
        struct BitMap *bm = NULL;
        if (o) {
            DoMethod(o, PDTM_SCALE, s->Width, s->Height, 0);
            if (DoMethod(o, DTM_PROCLAYOUT, NULL, 1)) {
                GetDTAttrs(o, PDTA_BitMapHeader, (IPTR)&bmh, TAG_DONE);
                GetDTAttrs(o, PDTA_DestBitMap, (IPTR)&bm, TAG_DONE);
                if (!bm) GetDTAttrs(o, PDTA_BitMap, (IPTR)&bm, TAG_DONE);
            }
        }
        if (!bm || !bmh) { fprintf(out, "FAIL picture %s\n", wp.path); supported = 0; }
        else {
            int sw = bmh->bmh_Width, sh = bmh->bmh_Height;
            fprintf(out, "PICTURE %dx%d (after PDTM_SCALE)\n", sw, sh);
            if (mode == 2) {
                /* Wanderer's root view: screen bitmap width, height below the bar. */
                int cw = (int)GetBitMapAttr(s->RastPort.BitMap, BMA_WIDTH);
                int ch = (int)GetBitMapAttr(s->RastPort.BitMap, BMA_HEIGHT) - (s->BarHeight + 1);
                struct BitMap *buf = AllocBitMap(cw, ch, GetBitMapAttr(bm, BMA_DEPTH), 0, s->RastPort.BitMap);
                if (buf) {
                    struct BitScaleArgs a = {0};
                    a.bsa_SrcX = 0; a.bsa_SrcY = 0; a.bsa_SrcWidth = sw; a.bsa_SrcHeight = sh;
                    a.bsa_XSrcFactor = sw; a.bsa_YSrcFactor = sh;
                    a.bsa_DestX = 0; a.bsa_DestY = 0; a.bsa_XDestFactor = cw; a.bsa_YDestFactor = ch;
                    a.bsa_SrcBitMap = bm; a.bsa_DestBitMap = buf;
                    BitMapScale(&a);
                    struct RastPort rp;
                    InitRastPort(&rp); rp.BitMap = buf;
                    ReadPixelArray(mine, 0, 0, aw * 4, &rp, 0, 0, aw < cw ? aw : cw, ah < ch ? ah : ch, RECTFMT_ARGB);
                    FreeBitMap(buf);
                }
            } else {
                struct RastPort rp;
                unsigned char *tile = malloc((size_t)sw * sh * 4);
                InitRastPort(&rp); rp.BitMap = bm;
                if (tile && ReadPixelArray(tile, 0, 0, sw * 4, &rp, 0, 0, sw, sh, RECTFMT_ARGB)) {
                    for (int y = 0; y < ah; y++)
                        for (int x = 0; x < aw; x++)
                            memcpy(mine + 4 * (y * aw + x), tile + 4 * (((y + oy) % sh) * sw + (x + ox) % sw), 4);
                }
                free(tile);
            }
        }
        if (o) DisposeDTObject(o);
    } else {
        fprintf(out, "UNSUPPORTED spec type %c\n", spec[0]);
        supported = 0;
    }
    CurrentTime(&t1, &us); t1 = t1 * 1000 + us / 1000;
    fprintf(out, "RENDER %s ms=%lu\n", supported ? "ok" : "none", (unsigned long)(t1 - t0));

    if (supported) {
        ReadPixelArray(real, 0, 0, aw * 4, &s->RastPort, ax, ay, aw, ah, RECTFMT_ARGB);
        /* Pixels under other windows (and Wanderer's icons, which are part of
         * the desktop window) cannot match; count them separately. */
        long same = 0, near = 0, compared = 0, skipped = 0;
        ULONG lock = LockIBase(0);
        for (int y = 0; y < ah; y++)
            for (int x = 0; x < aw; x++) {
                if (covered(s, desk, ax + x, ay + y)) { skipped++; continue; }
                const unsigned char *a = mine + 4 * (y * aw + x), *b = real + 4 * (y * aw + x);
                int d = abs(a[1] - b[1]) + abs(a[2] - b[2]) + abs(a[3] - b[3]);
                compared++;
                if (!d) same++;
                else if (d <= 6) near++;
            }
        UnlockIBase(lock);
        fprintf(out, "COMPARE pixels=%ld exact=%ld (%.2f%%) near=%ld other=%ld (%.2f%%) under-windows=%ld\n",
                compared, same, compared ? 100.0 * same / compared : 0.0, near,
                compared - same - near, compared ? 100.0 * (compared - same - near) / compared : 0.0, skipped);
        /* Where the differences are: 32x32 blocks, the worst first, and
         * small crops of both images around the worst three. */
        enum { B = 32 };
        int bw = (aw + B - 1) / B, bh = (ah + B - 1) / B;
        int *blocks = calloc((size_t)bw * bh, sizeof *blocks);
        if (blocks) {
            lock = LockIBase(0);
            for (int y = 0; y < ah; y++)
                for (int x = 0; x < aw; x++) {
                    if (covered(s, desk, ax + x, ay + y)) continue;
                    const unsigned char *a = mine + 4 * (y * aw + x), *b = real + 4 * (y * aw + x);
                    if (abs(a[1] - b[1]) + abs(a[2] - b[2]) + abs(a[3] - b[3]) > 6) blocks[(y / B) * bw + x / B]++;
                }
            UnlockIBase(lock);
            for (int k = 0; k < 8; k++) {
                int best = -1;
                for (int i = 0; i < bw * bh; i++) if (blocks[i] > 0 && (best < 0 || blocks[i] > blocks[best])) best = i;
                if (best < 0) break;
                int bx = (best % bw) * B, by = (best / bw) * B;
                fprintf(out, "BLOCK %d,%d (screen %d,%d) differing=%d of %d\n", bx, by, ax + bx, ay + by, blocks[best], B * B);
                if (ppmprefix && k < 3) {
                    char name[200];
                    static unsigned char crop[64 * 64 * 4], cr[64 * 64 * 4];
                    int cx = bx - 16 < 0 ? 0 : bx - 16, cy = by - 16 < 0 ? 0 : by - 16;
                    if (cx + 64 > aw) cx = aw - 64;
                    if (cy + 64 > ah) cy = ah - 64;
                    for (int y = 0; y < 64; y++) {
                        memcpy(crop + 4 * y * 64, mine + 4 * ((cy + y) * aw + cx), 64 * 4);
                        memcpy(cr + 4 * y * 64, real + 4 * ((cy + y) * aw + cx), 64 * 4);
                    }
                    snprintf(name, sizeof name, "%s-b%d-mine.ppm", ppmprefix, k); ppm(name, crop, 64, 64);
                    snprintf(name, sizeof name, "%s-b%d-real.ppm", ppmprefix, k); ppm(name, cr, 64, 64);
                    fprintf(out, "CROP %d at %d,%d\n", k, cx, cy);
                }
                blocks[best] = 0;
            }
            free(blocks);
        }
        if (ppmprefix && 0) {
            char name[200];
            snprintf(name, sizeof name, "%s-mine.ppm", ppmprefix); ppm(name, mine, aw, ah);
            snprintf(name, sizeof name, "%s-real.ppm", ppmprefix); ppm(name, real, aw, ah);
        }
    }
    fputs("END\n", out);
    fclose(out);
    UnlockPubScreen(NULL, s);
    CloseLibrary(DataTypesBase);
    CloseLibrary(CyberGfxBase);
    return 0;
}
