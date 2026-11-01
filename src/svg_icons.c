#include "svg_icons.h"

#include <exec/libraries.h>
#include <proto/exec.h>
#include <proto/svgicon.h>
#include <stdio.h>
#include <stdlib.h>

struct Library *SvgIconBase;
static unsigned long render_count;

int dock_svg_open(const char *library_path)
{
    dock_svg_close();
    render_count = 0;
    if (!library_path || !*library_path)
        return 0;
    SvgIconBase = OpenLibrary((CONST_STRPTR)library_path, 0);
    return SvgIconBase != NULL;
}

int dock_svg_render(const char *svg_path, int size, unsigned char **argb)
{
    FILE *file = NULL;
    unsigned char *source = NULL;
    unsigned char *pixels = NULL;
    APTR document = NULL;
    size_t length;
    int ok = 0;

    if (!argb)
        return 0;
    *argb = NULL;
    if (!SvgIconBase || !svg_path || !*svg_path ||
        size < 1 || size > DOCK_SVG_MAX_SIZE)
        return 0;

    file = fopen(svg_path, "rb");
    if (!file)
        goto done;
    /* One extra byte detects oversize input without relying on seek support
     * or a file length that could change between the check and the read. */
    source = malloc((size_t)DOCK_SVG_MAX_SOURCE_BYTES + 1);
    if (!source)
        goto done;
    length = fread(source, 1, (size_t)DOCK_SVG_MAX_SOURCE_BYTES + 1, file);
    if (ferror(file) || length == 0 || length > DOCK_SVG_MAX_SOURCE_BYTES)
        goto done;
    fclose(file);
    file = NULL;

    if (SVGI_Load(source, (ULONG)length, 0, &document) != 0 || !document)
        goto done;
    free(source);
    source = NULL;
    pixels = malloc((size_t)size * (size_t)size * 4);
    if (!pixels)
        goto done;
    /* The library converts its native premultiplied surface to straight
     * A,R,G,B bytes. Do not apply a second conversion here. */
    if (SVGI_RenderARGB(document, (ULONG)size, (ULONG)size,
                        pixels, (ULONG)size * 4) != 0)
        goto done;
    *argb = pixels;
    pixels = NULL;
    ++render_count;
    ok = 1;

done:
    if (document)
        SVGI_Free(document);
    free(pixels);
    free(source);
    if (file)
        fclose(file);
    return ok;
}

void dock_svg_free(unsigned char *argb)
{
    free(argb);
}

void dock_svg_close(void)
{
    if (SvgIconBase) {
        CloseLibrary(SvgIconBase);
        SvgIconBase = NULL;
    }
}

unsigned long dock_svg_render_count(void)
{
    return render_count;
}
