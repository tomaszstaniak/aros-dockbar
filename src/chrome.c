#include "chrome.h"
#include <stdlib.h>
#include <graphics/regions.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <intuition/intuition.h>
#include <string.h>

int dock_corner_radius = DOCK_CORNER_DEFAULT;

struct Region *dock_shape(int width, int height, int radius)
{
    struct Region *region = NewRegion();
    if (!region) return NULL;
    for (int y = 0; y < height; ++y) {
        int edge = dock_row_inset(y, width, height, radius);
        struct Rectangle row = { edge, y, width - edge - 1, y };
        if (!OrRectRegion(region, &row)) { DisposeRegion(region); return NULL; }
    }
    return region;
}

void dock_frame_draw(struct RastPort *rp, const struct DockFrameStyle *style,
                     int width, int height, int radius)
{
    unsigned char *row = malloc((size_t)width * 4);
    if (!row) { FillPixelArray(rp, 0, 0, width, height, style->fill_bottom); return; }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            unsigned long c = dock_frame_pixel(style, x, y, width, height, radius);
            row[x * 4] = 0xff; row[x * 4 + 1] = c >> 16; row[x * 4 + 2] = c >> 8; row[x * 4 + 3] = c;
        }
        WritePixelArray(row, 0, 0, width * 4, rp, 0, y, width, 1, RECTFMT_ARGB);
    }
    free(row);
}

void dock_round_fill(struct RastPort *rp, int x, int y, int width, int height,
                     int radius, unsigned long color, unsigned long behind)
{
    for (int row = 0; row < height; ++row) {
        int edge = 0;
        float c;
        while (edge < width / 2 && (c = dock_round_coverage(edge, row, width, height, radius)) < 1) {
            if (c > 0) {
                unsigned long blended = dock_mix(behind, color, c);
                FillPixelArray(rp, x + edge, y + row, 1, 1, blended);
                FillPixelArray(rp, x + width - 1 - edge, y + row, 1, 1, blended);
            }
            ++edge;
        }
        if (width - edge * 2 > 0)
            FillPixelArray(rp, x + edge, y + row, width - edge * 2, 1, color);
    }
}

int dock_buffer_open(struct DockBuffer *buffer, struct Screen *screen, int width, int height)
{
    memset(buffer, 0, sizeof(*buffer));
    buffer->bitmap = AllocBitMap(width, height, GetBitMapAttr(screen->RastPort.BitMap, BMA_DEPTH),
                                 BMF_CLEAR, screen->RastPort.BitMap);
    if (!buffer->bitmap) return 0;
    InitRastPort(&buffer->rp);
    buffer->rp.BitMap = buffer->bitmap;
    SetFont(&buffer->rp, screen->RastPort.Font);
    buffer->width = width;
    buffer->height = height;
    return 1;
}

void dock_buffer_show(struct DockBuffer *buffer, struct Window *window)
{
    if (buffer->bitmap && window)
        BltBitMapRastPort(buffer->bitmap, 0, 0, window->RPort, 0, 0, buffer->width, buffer->height, 0xc0);
}

void dock_buffer_close(struct DockBuffer *buffer)
{
    if (buffer->bitmap) FreeBitMap(buffer->bitmap);
    memset(buffer, 0, sizeof(*buffer));
}
