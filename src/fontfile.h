#ifndef DOCK_FONTFILE_H
#define DOCK_FONTFILE_H
#include <stddef.h>

/* The TrueType file behind an outline font of the system: diskfont's
 * "<name>.otag" is a big-endian tag list; indirect tags point into the file,
 * and one of them names the font file for the engine (for example
 * "Fonts:TrueType/VeraSans.ttf"). Anything damaged or unexpected gives 0,
 * and the caller draws with the system's own text routines. Pure. */
int dock_otag_font_file(const unsigned char *data, size_t size, char *out, size_t capacity);

/* "<name>.font" as named in a TextAttr -> "FONTS:<name>.otag"; 0 when the
 * name does not end in ".font" or does not fit. */
int dock_otag_path(const char *font_name, char *out, size_t capacity);

#endif
