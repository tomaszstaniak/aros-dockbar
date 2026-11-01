#include "theme.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    for (int v = 0; v < 2; v++) {
        const struct DockColors *c = dock_colors(v);
        /* Text must read on both ends of the bar's gradient, on popups and
         * on highlights; muted text and outlines a little less. */
        assert(dock_contrast100(c->text, c->fill_top) >= 700);
        assert(dock_contrast100(c->text, c->fill_bottom) >= 700);
        assert(dock_contrast100(c->text, c->popup_fill) >= 700);
        assert(dock_contrast100(c->text, c->hover) >= 450);
        assert(dock_contrast100(c->text, c->button) >= 700);
        assert(c->button != c->fill_top && c->button != c->hover);
        assert(dock_contrast100(c->text, c->pressed) >= 450);
        assert(dock_contrast100(c->text_muted, c->fill_bottom) >= 300);
        assert(dock_contrast100(c->text, c->outline) >= 700);
        assert(dock_contrast100(c->mark, c->fill_bottom) >= 300);
        assert(dock_contrast100(c->arrow, c->fill_bottom) >= 300);
        assert(c->hover != c->fill_top && c->pressed != c->hover && c->target != c->hover);
    }
    /* The dark variant is the colours of 0.2, so existing docks look the same. */
    assert(dock_colors(0)->fill_top == 0x393e42 && dock_colors(0)->border_top == 0x767d82 &&
           dock_colors(0)->hover == 0x485159 && dock_colors(0)->target == 0x48786f);

    assert(dock_theme_light(DOCK_THEME_DARK, 250, 1) == 0);
    assert(dock_theme_light(DOCK_THEME_LIGHT, 10, 0) == 1);
    /* Automatic: nothing known (opaque bar) is dark; a dead band keeps the
     * previous choice so that a mid-grey desktop does not flip the dock. */
    assert(dock_theme_light(DOCK_THEME_AUTO, -1, 1) == 0);
    assert(dock_theme_light(DOCK_THEME_AUTO, 200, -1) == 1);
    assert(dock_theme_light(DOCK_THEME_AUTO, 60, 1) == 0);
    assert(dock_theme_light(DOCK_THEME_AUTO, 135, 1) == 1);
    assert(dock_theme_light(DOCK_THEME_AUTO, 135, 0) == 0);
    assert(dock_theme_light(DOCK_THEME_AUTO, 150, 0) == 1 && dock_theme_light(DOCK_THEME_AUTO, 120, 1) == 0);

    unsigned char px[4 * 4];
    unsigned char mask[4] = {0, 0, 1, 1};
    memset(px, 0, sizeof px);
    for (int i = 0; i < 2; i++) px[4 * i + 1] = px[4 * i + 2] = px[4 * i + 3] = 200;
    for (int i = 2; i < 4; i++) px[4 * i + 1] = px[4 * i + 2] = px[4 * i + 3] = 0;
    assert(dock_luminance(px, NULL, 4) == 100);
    assert(dock_luminance(px, mask, 4) == 200);  /* masked pixels are ours */
    assert(dock_luminance(px, (const unsigned char *)"\1\1\1\1", 4) == -1);
    puts("PASS theme: contrast in both variants, dark = 0.2 colours, automatic choice with dead band, luminance");
    return 0;
}
