#include "textrender.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = malloc((size_t)n);
    if (data && fread(data, 1, (size_t)n, f) != (size_t)n) { free(data); data = NULL; }
    fclose(f);
    *size = (size_t)n;
    return data;
}
static unsigned long same_hash(const char *s) { (void)s; return 42; }
static int ink(const unsigned char *cov, int w, int h, int x0, int x1)
{
    int n = 0;
    for (int y = 0; y < h; y++)
        for (int x = x0; x < x1 && x < w; x++) n += cov[y * w + x] > 64;
    return n;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    char vera_path[600], open_path[600];
    snprintf(vera_path, sizeof vera_path, "%s/VeraSans.ttf", dir);
    snprintf(open_path, sizeof open_path, "%s/OpenSans-Regular.ttf", dir);
    size_t sizes[2];
    const unsigned char *faces[2] = { slurp(vera_path, &sizes[0]), slurp(open_path, &sizes[1]) };
    if (!faces[0] || !faces[1]) { puts("SKIP textrender: fonts not found"); return 0; }
    struct DockTextRender t;

    /* No usable face: the caller falls back to the native font. */
    const unsigned char *none[1] = { (const unsigned char *)"not a font" };
    size_t none_size[1] = { 10 };
    assert(!dock_text_open(&t, none, none_size, 1, 13, DOCK_CODESET_LATIN2));

    /* "Zażółć gęślą jaźń" in ISO-8859-2. */
    const char *polish = "Za\xbf\xf3\xb3\xe6 g\xea\xb6l\xb1 ja\xbc\xf1";
    assert(dock_text_unicode(DOCK_CODESET_LATIN2, 0xb3) == 0x142 && dock_text_unicode(DOCK_CODESET_LATIN1, 0xb3) == 0xb3);

    /* Vera alone lacks some Polish letters: they become boxes, the string
     * still has a width and ink. */
    assert(dock_text_open(&t, faces, sizes, 1, 13, DOCK_CODESET_LATIN2));
    int w_vera = dock_text_width(&t, polish);
    assert(w_vera > 60);
    dock_text_close(&t);

    /* Vera then Open Sans: every letter has a glyph. */
    assert(dock_text_open(&t, faces, sizes, 2, 13, DOCK_CODESET_LATIN2));
    assert(t.faces == 2 && t.ascender > 8 && t.descender > 1 && dock_text_height(&t) >= 13);
    struct DockTextImage im;
    const unsigned char *cov = dock_text_render(&t, polish, &im);
    assert(cov && im.advance == dock_text_width(&t, polish) && im.h >= dock_text_height(&t));
    assert(im.left >= 0 && im.top >= 0 && im.w >= im.left + im.advance);
    assert(ink(cov, im.w, im.h, 0, im.w) > 100);
    /* Every letter draws something: no invisible character taking room. */
    for (const char *c = polish; *c; c++) {
        char one[2] = { *c, 0 };
        if (*c == ' ') continue;
        const unsigned char *g = dock_text_render(&t, one, &im);
        assert(g && ink(g, im.w, im.h, 0, im.w) > 0);
    }
    /* Cached: the same string returns the same pixels without rendering. */
    cov = dock_text_render(&t, polish, &im);
    assert(dock_text_render(&t, polish, &im) == cov);
    /* Proportional: "iiii" is narrower than "WWWW". */
    assert(dock_text_width(&t, "iiii") * 2 < dock_text_width(&t, "WWWW"));
    /* Baseline: "x" has no ink below it, "g" does. */
    const unsigned char *x = dock_text_render(&t, "x", &im);
    int below = 0;
    for (int y = im.top + t.ascender + 1; y < im.h; y++) for (int i = 0; i < im.w; i++) below += x[y * im.w + i] > 64;
    assert(below == 0);
    const unsigned char *g = dock_text_render(&t, "g", &im);
    below = 0;
    for (int y = im.top + t.ascender + 1; y < im.h; y++) for (int i = 0; i < im.w; i++) below += g[y * im.w + i] > 64;
    assert(below > 0);
    /* Sized by line height to match a system font of that height. */
    assert(dock_text_fit_height(&t, 15) && t.pixel_size == 12);  /* Vera: 12 px is 12 + 3 */
    assert(dock_text_render(&t, "x", &im) && im.h >= dock_text_height(&t));
    /* More strings than the cache holds: all still render correctly. */
    char buf[16];
    for (int i = 0; i < 100; i++) {
        snprintf(buf, sizeof buf, "item %d", i);
        assert(dock_text_render(&t, buf, &im) && im.advance == dock_text_width(&t, buf));
    }
    /* Forced hash collisions: every string hashes alike, and each still gets
     * its own pixels and size. */
    t.hash = same_hash;
    struct DockTextImage a, b;
    const unsigned char *pa = dock_text_render(&t, "MultiView", &a);
    unsigned char copy[4096];
    assert(pa && (size_t)a.w * a.h <= sizeof copy);
    memcpy(copy, pa, (size_t)a.w * a.h);
    const unsigned char *pb = dock_text_render(&t, "Prefs", &b);
    assert(pb && b.advance != a.advance && b.advance == dock_text_width(&t, "Prefs"));
    const unsigned char *pa2 = dock_text_render(&t, "MultiView", &a);
    assert(pa2 && a.advance == dock_text_width(&t, "MultiView") && !memcmp(pa2, copy, (size_t)a.w * a.h));
    t.hash = NULL;
    /* 1-bit (mono) images are drawn too, as full coverage. */
    t.test_mono = 1;
    for (int i = 0; i < 32; i++) { free(t.cache[i].coverage); free(t.cache[i].text); t.cache[i].coverage = NULL; t.cache[i].text = NULL; }
    const unsigned char *m = dock_text_render(&t, "Mono", &im);
    int on = 0, other = 0;
    for (int i = 0; i < im.w * im.h; i++) { on += m[i] == 255; other += m[i] && m[i] != 255; }
    assert(m && on > 20 && other == 0);
    t.test_mono = 0;
    dock_text_close(&t);

    /* Ink beyond the advance and left of the origin: an italic "f" leans
     * over both sides; the image keeps all of it, with the origin inside. */
    char italic_path[600];
    size_t isize;
    snprintf(italic_path, sizeof italic_path, "%s/VeraSansItalic.ttf", dir);
    const unsigned char *it[1] = { slurp(italic_path, &isize) };
    if (it[0]) {
        assert(dock_text_open(&t, it, &isize, 1, 24, DOCK_CODESET_LATIN1));
        const unsigned char *f = dock_text_render(&t, "f", &im);
        assert(f && im.w > im.advance);  /* wider than its advance */
        int right = 0;
        for (int y = 0; y < im.h; y++) right += f[y * im.w + im.w - 1] > 0;
        assert(right > 0);               /* and the overhang is kept */
        dock_text_close(&t);
        free((void *)it[0]);
    }
    printf("PASS textrender: no-font fallback, ISO-8859-2, face fallback for Polish (Vera alone %d px), every letter inked, cache, forced collisions, mono, overhang, proportional, baseline\n",
           w_vera);
    free((void *)faces[0]);
    free((void *)faces[1]);
    return 0;
}
