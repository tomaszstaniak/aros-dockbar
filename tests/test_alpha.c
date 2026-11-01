#include "alpha.h"
#include "frame.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void set(unsigned char *p, int a, unsigned long c)
{
    p[0] = (unsigned char)a; p[1] = (unsigned char)(c >> 16); p[2] = (unsigned char)(c >> 8); p[3] = (unsigned char)c;
}
static unsigned long rgb(const unsigned char *p) { return (unsigned long)p[1] << 16 | (unsigned long)p[2] << 8 | p[3]; }
static int near(unsigned long a, unsigned long b, int d)
{
    for (int s = 0; s < 24; s += 8)
        if (abs((int)((a >> s) & 0xff) - (int)((b >> s) & 0xff)) > d) return 0;
    return 1;
}

int main(void)
{
    unsigned char p[4];
    /* Nothing over: unchanged. Opaque over: the colour, opaque. */
    set(p, 77, 0x102030); dock_over(p, 0xffffff, 0);
    assert(p[0] == 77 && rgb(p) == 0x102030);
    set(p, 0, 0); dock_over(p, 0x123456, 255);
    assert(p[0] == 255 && rgb(p) == 0x123456);
    /* Over a clear pixel: the colour itself at the given alpha (straight). */
    for (int a = 1; a < 255; a += 7) {
        set(p, 0, 0x000000); dock_over(p, 0xc08040, a);
        assert(p[0] == a && rgb(p) == 0xc08040);
    }
    /* Over an opaque pixel: the same mix the pictured docks draw. */
    for (int a = 1; a < 255; a += 5) {
        set(p, 255, 0x204060); dock_over(p, 0xe0c0a0, a);
        assert(p[0] == 255 && near(rgb(p), dock_mix(0x204060, 0xe0c0a0, a / 255.0f), 1));
    }
    /* An opaque icon over a see-through bar stays opaque: the bar's
     * transparency never makes icons or text fainter. */
    set(p, 0, 0); dock_over(p, 0x303030, 89);      /* bar at 35 % */
    dock_over(p, 0xff0000, 255);
    assert(p[0] == 255 && rgb(p) == 0xff0000);
    /* A half-covered edge over the bar is more opaque than either, and the
     * composite over any desktop equals drawing both in turn onto it. */
    for (unsigned long desk = 0; desk <= 0xffffff; desk += 0x3f3f3f) {
        unsigned char layer[4], direct[4];
        set(layer, 0, 0); dock_over(layer, 0x303030, 89); dock_over(layer, 0xffd000, 128);
        assert(layer[0] > 128 && layer[0] > 89);
        set(direct, 255, desk); dock_over(direct, 0x303030, 89); dock_over(direct, 0xffd000, 128);
        unsigned char shown[4]; set(shown, 255, desk); dock_over(shown, rgb(layer), layer[0]);
        assert(near(rgb(shown), rgb(direct), 2));
    }
    puts("alpha: ok");
    return 0;
}
