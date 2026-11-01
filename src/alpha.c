#include "alpha.h"

void dock_over(unsigned char *p, unsigned long color, int alpha)
{
    if (alpha <= 0) return;
    if (alpha >= 255) {
        p[0] = 255; p[1] = (unsigned char)(color >> 16); p[2] = (unsigned char)(color >> 8); p[3] = (unsigned char)color;
        return;
    }
    /* Weights in 255ths squared: the source's alpha, and what of the pixel
     * below still shows through it. */
    long ws = (long)alpha * 255, wd = (long)p[0] * (255 - alpha), sum = ws + wd;
    for (int c = 1; c < 4; c++) {
        long s = (long)(color >> (8 * (3 - c))) & 0xff;
        p[c] = (unsigned char)((s * ws + (long)p[c] * wd + sum / 2) / sum);
    }
    p[0] = (unsigned char)((sum + 127) / 255);
}
