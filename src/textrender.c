#include "textrender.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdlib.h>
#include <string.h>

/* ISO-8859-2, 0xA0..0xFF; below 0xA0 both code pages are ASCII/Latin-1. */
static const unsigned short latin2[96] = {
    0x00A0, 0x0104, 0x02D8, 0x0141, 0x00A4, 0x013D, 0x015A, 0x00A7, 0x00A8, 0x0160, 0x015E, 0x0164, 0x0179, 0x00AD, 0x017D, 0x017B,
    0x00B0, 0x0105, 0x02DB, 0x0142, 0x00B4, 0x013E, 0x015B, 0x02C7, 0x00B8, 0x0161, 0x015F, 0x0165, 0x017A, 0x02DD, 0x017E, 0x017C,
    0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7, 0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E,
    0x0110, 0x0143, 0x0147, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x00D7, 0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF,
    0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7, 0x010D, 0x00E9, 0x0119, 0x00EB, 0x011B, 0x00ED, 0x00EE, 0x010F,
    0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7, 0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9,
};

unsigned dock_text_unicode(int codeset, unsigned char c)
{
    if (codeset == DOCK_CODESET_LATIN2 && c >= 0xA0) return latin2[c - 0xA0];
    return c;
}

int dock_text_open(struct DockTextRender *t, const unsigned char *const *data, const size_t *size,
                   int count, int pixel_size, int codeset)
{
    FT_Library library;
    memset(t, 0, sizeof *t);
    if (pixel_size < 6 || pixel_size > 96 || FT_Init_FreeType(&library)) return 0;
    t->library = library;
    t->pixel_size = pixel_size;
    t->codeset = codeset == DOCK_CODESET_LATIN2 ? DOCK_CODESET_LATIN2 : DOCK_CODESET_LATIN1;
    for (int i = 0; i < count && t->faces < DOCK_TEXT_FACES; i++) {
        FT_Face face;
        if (!data[i] || !size[i] || FT_New_Memory_Face(library, data[i], (FT_Long)size[i], 0, &face)) continue;
        if (FT_Set_Pixel_Sizes(face, 0, (FT_UInt)pixel_size)) { FT_Done_Face(face); continue; }
        int asc = (int)((face->size->metrics.ascender + 63) >> 6), desc = (int)((-face->size->metrics.descender + 63) >> 6);
        if (asc > t->ascender) t->ascender = asc;
        if (desc > t->descender) t->descender = desc;
        t->face[t->faces++] = face;
    }
    if (!t->faces) { dock_text_close(t); return 0; }
    return 1;
}

void dock_text_close(struct DockTextRender *t)
{
    for (int i = 0; i < 32; i++) { free(t->cache[i].coverage); free(t->cache[i].text); }
    for (int i = 0; i < t->faces; i++) FT_Done_Face((FT_Face)t->face[i]);
    /* FT_Done_FreeType, not FT_Done_Library: only the former frees the
     * memory pool (the freetype2.library engine leak in the working notes). */
    if (t->library) FT_Done_FreeType((FT_Library)t->library);
    memset(t, 0, sizeof *t);
}

int dock_text_height(const struct DockTextRender *t) { return t->ascender + t->descender; }

static int set_size(struct DockTextRender *t, int pixel_size)
{
    t->ascender = t->descender = 0;
    for (int i = 0; i < t->faces; i++) {
        FT_Face face = (FT_Face)t->face[i];
        if (FT_Set_Pixel_Sizes(face, 0, (FT_UInt)pixel_size)) return 0;
        int asc = (int)((face->size->metrics.ascender + 63) >> 6), desc = (int)((-face->size->metrics.descender + 63) >> 6);
        if (asc > t->ascender) t->ascender = asc;
        if (desc > t->descender) t->descender = desc;
    }
    t->pixel_size = pixel_size;
    return 1;
}

int dock_text_fit_height(struct DockTextRender *t, int height)
{
    if (!t || !t->faces || height < 6) return 0;
    for (int i = 0; i < 32; i++) { free(t->cache[i].coverage); free(t->cache[i].text); t->cache[i].coverage = NULL; t->cache[i].text = NULL; }
    /* Judged on the first face, the one chosen to match the system font;
     * fallback faces may reach higher or lower, and the line box grows. */
    for (int size = height; size >= 6; size--) {
        FT_Face face = (FT_Face)t->face[0];
        if (!set_size(t, size)) continue;
        int line = (int)((face->size->metrics.ascender + 63) >> 6) + (int)((-face->size->metrics.descender + 63) >> 6);
        if (line <= height) return 1;
    }
    return set_size(t, 6);
}

/* The glyph for one character: from the first face that has it and gives an
 * image that can be drawn (8-bit grey or 1-bit mono; an empty one is a
 * space). Embedded bitmaps are not used (FT_LOAD_NO_BITMAP): FreeType then
 * rasterises the outline, so a face's bitmap strikes cannot hide a glyph.
 * -1 when no face can draw it. */
static int load(struct DockTextRender *t, unsigned code, int render, FT_UInt *glyph)
{
    FT_Int32 flags = FT_LOAD_NO_BITMAP | (t->test_mono ? FT_LOAD_TARGET_MONO : FT_LOAD_TARGET_LIGHT) | (render ? FT_LOAD_RENDER : 0);
    for (int i = 0; i < t->faces; i++) {
        FT_Face face = (FT_Face)t->face[i];
        FT_UInt g = FT_Get_Char_Index(face, code);
        if (!g || FT_Load_Glyph(face, g, flags)) continue;
        FT_Bitmap *b = &face->glyph->bitmap;
        if (render && b->rows && b->width && b->pixel_mode != FT_PIXEL_MODE_GRAY && b->pixel_mode != FT_PIXEL_MODE_MONO) continue;
        *glyph = g;
        return i;
    }
    return -1;
}

static int bitmap_value(const FT_Bitmap *b, unsigned r, unsigned c)
{
    const unsigned char *row = b->pitch >= 0 ? b->buffer + r * b->pitch : b->buffer + (b->rows - 1 - r) * -b->pitch;
    if (b->pixel_mode == FT_PIXEL_MODE_MONO) return row[c >> 3] & (0x80 >> (c & 7)) ? 255 : 0;
    return row[c];
}

/* A string's advance (return value, pixels) and, with box, the bounds of
 * its ink relative to the pen origin at the top of the line box; with cov,
 * draws into a w x h image whose (0,0) is at (-ox, -oy) from that origin. */
struct Box { int x0, y0, x1, y1; };
static int walk(struct DockTextRender *t, const char *s, struct Box *box, unsigned char *cov, int w, int h, int ox, int oy)
{
    FT_Long pen = 0;  /* 26.6 */
    int previous_face = -1, render = box || cov;
    FT_UInt previous_glyph = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        FT_UInt glyph = 0;
        int f = load(t, dock_text_unicode(t->codeset, *p), render, &glyph);
        if (f < 0) {
            /* No face can draw it: an empty box, half an em wide, so a
             * missing letter is visible and never an invisible gap. */
            int bw = t->pixel_size / 2, x0 = (int)((pen + 32) >> 6) + 1, top = t->ascender - t->pixel_size * 7 / 10;
            if (box) {
                if (x0 < box->x0) box->x0 = x0;
                if (x0 + bw - 2 > box->x1) box->x1 = x0 + bw - 2;
                if (top < box->y0) box->y0 = top;
                if (t->ascender > box->y1) box->y1 = t->ascender;
            }
            if (cov)
                for (int y = top; y < t->ascender; y++)
                    for (int x = x0; x < x0 + bw - 2; x++) {
                        int cx = x + ox, cy = y + oy;
                        if (cy >= 0 && cy < h && cx >= 0 && cx < w &&
                            (y == top || y == t->ascender - 1 || x == x0 || x == x0 + bw - 3))
                            cov[cy * w + cx] = 255;
                    }
            pen += (FT_Long)bw << 6;
            previous_face = -1;
            continue;
        }
        FT_Face face = (FT_Face)t->face[f];
        if (f == previous_face && FT_HAS_KERNING(face)) {
            FT_Vector k;
            if (!FT_Get_Kerning(face, previous_glyph, glyph, FT_KERNING_DEFAULT, &k)) pen += k.x;
        }
        FT_GlyphSlot g = face->glyph;
        int gx = (int)((pen + 32) >> 6) + g->bitmap_left, gy = t->ascender - g->bitmap_top;
        if (box && g->bitmap.rows && g->bitmap.width) {
            if (gx < box->x0) box->x0 = gx;
            if (gy < box->y0) box->y0 = gy;
            if (gx + (int)g->bitmap.width > box->x1) box->x1 = gx + (int)g->bitmap.width;
            if (gy + (int)g->bitmap.rows > box->y1) box->y1 = gy + (int)g->bitmap.rows;
        }
        if (cov)
            for (unsigned r = 0; r < g->bitmap.rows; r++) {
                int y = gy + (int)r + oy;
                if (y < 0 || y >= h) continue;
                for (unsigned c = 0; c < g->bitmap.width; c++) {
                    int x = gx + (int)c + ox;
                    if (x < 0 || x >= w) continue;
                    int v = cov[y * w + x] + bitmap_value(&g->bitmap, r, c);
                    cov[y * w + x] = (unsigned char)(v > 255 ? 255 : v);
                }
            }
        pen += g->advance.x;
        previous_face = f;
        previous_glyph = glyph;
    }
    return (int)((pen + 63) >> 6);
}

int dock_text_width(struct DockTextRender *t, const char *s)
{
    return t && t->faces && s ? walk(t, s, NULL, NULL, 0, 0, 0, 0) : 0;
}

static unsigned long fnv(const char *s)
{
    unsigned long h = 2166136261u;
    for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; }
    return h & 0xffffffffu;
}

const unsigned char *dock_text_render(struct DockTextRender *t, const char *s, struct DockTextImage *image)
{
    if (!t || !t->faces || !s || !image) return NULL;
    unsigned long hash = t->hash ? t->hash(s) : fnv(s);
    int oldest = 0;
    for (int i = 0; i < 32; i++) {
        struct DockTextEntry *e = &t->cache[i];
        /* The hash only finds candidates: the text decides. */
        if (e->coverage && e->hash == hash && !strcmp(e->text, s)) {
            e->used = ++t->tick;
            *image = e->image;
            return e->coverage;
        }
        if (!e->coverage) { oldest = i; break; }
        if (e->used < t->cache[oldest].used) oldest = i;
    }
    /* The image covers the line box and all ink, which may reach left of
     * the pen origin, past the advance, above or below the line. */
    struct Box box = { 0, 0, 0, dock_text_height(t) };
    int advance = walk(t, s, &box, NULL, 0, 0, 0, 0);
    if (advance > box.x1) box.x1 = advance;
    int w = box.x1 - box.x0, h = box.y1 - box.y0;
    size_t length = strlen(s);
    if (w < 1 || h < 1) return NULL;
    unsigned char *cov = calloc((size_t)w * h, 1);
    char *text = malloc(length + 1);
    if (!cov || !text) { free(cov); free(text); return NULL; }
    memcpy(text, s, length + 1);
    walk(t, s, NULL, cov, w, h, -box.x0, -box.y0);
    struct DockTextEntry *e = &t->cache[oldest];
    free(e->coverage);
    free(e->text);
    e->coverage = cov;
    e->text = text;
    e->hash = hash;
    e->image = (struct DockTextImage){ w, h, -box.x0, -box.y0, advance };
    e->used = ++t->tick;
    *image = e->image;
    return cov;
}
