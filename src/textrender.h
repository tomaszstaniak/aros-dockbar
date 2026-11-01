#ifndef DOCK_TEXTRENDER_H
#define DOCK_TEXTRENDER_H
#include <stddef.h>

/* Anti-aliased text from TrueType faces with FreeType, for the short strings
 * a dock draws (labels, tooltips, clocks, text buttons). Faces are read into
 * memory once (FreeType would otherwise read the file lazily per glyph).
 * Several faces form a fallback chain: each character comes from the first
 * face that has it, so a face without Polish letters can be completed by
 * another. Strings are 8-bit in the system code page (AROS file names and
 * labels); only ISO-8859-1 and ISO-8859-2 are decoded. No AROS calls: the
 * caller loads the font files. Rendering gives 8-bit coverage, which the
 * caller colours and blends. */

#define DOCK_TEXT_FACES 3

/* A rendered string: coverage w x h. The pen origin of the string, at the
 * top of its line box, is at (left, top) in the image; ink may reach left
 * of it, past the advance and above or below the line box. advance is the
 * typographic width, the one layouts use (= dock_text_width). */
struct DockTextImage { int w, h, left, top, advance; };

enum DockCodeSet { DOCK_CODESET_LATIN1 = 4, DOCK_CODESET_LATIN2 = 5 };  /* IANA MIBenum */

struct DockTextRender {
    void *library;                       /* FT_Library */
    void *face[DOCK_TEXT_FACES];         /* FT_Face, in fallback order */
    int faces, pixel_size, ascender, descender, codeset;
    /* Small cache of rendered strings: FNV hash to find, text to confirm. */
    struct DockTextEntry {
        unsigned long hash, used;
        char *text;
        unsigned char *coverage;
        struct DockTextImage image;
    } cache[32];
    unsigned long tick;
    /* Tests only: replace the hash (forced collisions), render 1-bit. */
    unsigned long (*hash)(const char *s);
    int test_mono;
};

/* Faces are given as file contents (data, size); the memory must stay valid
 * while the renderer is open. Faces that do not load are skipped. Returns 1
 * when at least one face loaded; 0 means "use the native font". */
int dock_text_open(struct DockTextRender *t, const unsigned char *const *data, const size_t *size,
                   int count, int pixel_size, int codeset);
void dock_text_close(struct DockTextRender *t);

/* Unicode of one 8-bit character in the renderer's code page. */
unsigned dock_text_unicode(int codeset, unsigned char c);

/* Advance width of s in pixels (with kerning inside each face). */
int dock_text_width(struct DockTextRender *t, const char *s);
/* Sets the largest pixel size at which the first face's line height
 * (ascender + descender) is at most 'height', so the text matches a system
 * font of that YSize; dock_text_height may then be larger.
 * 1 on success. Empties the cache. */
int dock_text_fit_height(struct DockTextRender *t, int height);
/* Line height: ascender + descender. */
int dock_text_height(const struct DockTextRender *t);

/* Coverage of s (one byte per pixel, image->w per row), owned by the cache
 * and valid until the next call; NULL on failure. Characters no face can
 * draw become an empty box. */
const unsigned char *dock_text_render(struct DockTextRender *t, const char *s, struct DockTextImage *image);

#endif
