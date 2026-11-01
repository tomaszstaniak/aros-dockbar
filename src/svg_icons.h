#ifndef DOCK_SVG_ICONS_H
#define DOCK_SVG_ICONS_H

#define DOCK_SVG_MAX_SOURCE_BYTES (1024u * 1024u)
#define DOCK_SVG_MAX_SIZE 512

/* Single-task adapter. Open/render return 1 on success, 0 on failure.
 * Pass an explicit application-local path, such as PROGDIR:libs/svgicon.library.
 * Opening again closes the prior library and resets the local render counter.
 */
int dock_svg_open(const char *library_path);

/* On success, owns size*size*4 tightly packed straight A,R,G,B bytes.
 * The renderer preserves aspect ratio and centres in the transparent square.
 * On failure *argb is NULL. Documents never outlive this call. The caller
 * caches the buffer and releases it with dock_svg_free, even after close.
 */
int dock_svg_render(const char *svg_path, int size, unsigned char **argb);
void dock_svg_free(unsigned char *argb);
void dock_svg_close(void);

/* Successful renders by this adapter since the last open, not other clients. */
unsigned long dock_svg_render_count(void);

#endif
