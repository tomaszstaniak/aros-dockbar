/* Builds without FreeType (Macaros has none): Smooth text is never
 * available, so every caller keeps the native font. */
#include "textrender.h"
#include <string.h>

int dock_text_open(struct DockTextRender *t, const unsigned char *const *data, const size_t *size,
                   int count, int pixel_size, int codeset)
{
    (void)data; (void)size; (void)count; (void)pixel_size; (void)codeset;
    memset(t, 0, sizeof(*t));
    return 0;
}
void dock_text_close(struct DockTextRender *t) { memset(t, 0, sizeof(*t)); }
unsigned dock_text_unicode(int codeset, unsigned char c) { (void)codeset; return c; }
int dock_text_width(struct DockTextRender *t, const char *s) { (void)t; (void)s; return 0; }
int dock_text_fit_height(struct DockTextRender *t, int height) { (void)t; (void)height; return 0; }
int dock_text_height(const struct DockTextRender *t) { (void)t; return 0; }
const unsigned char *dock_text_render(struct DockTextRender *t, const char *s, struct DockTextImage *image)
{
    (void)t; (void)s; (void)image;
    return NULL;
}
