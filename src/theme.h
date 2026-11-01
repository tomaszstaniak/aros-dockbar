#ifndef DOCK_THEME_H
#define DOCK_THEME_H

/* One set of colours for everything a dock draws: bar, frame, text,
 * separators, highlights, insertion mark, grip, arrows, group previews,
 * tooltip, menu and group panel. Drawing code takes every colour from here.
 * Colours are 0xRRGGBB. Application icons are never recoloured. */

enum DockThemeChoice { DOCK_THEME_DARK = 0, DOCK_THEME_LIGHT = 1, DOCK_THEME_AUTO = 2 };

struct DockColors {
    unsigned long fill_top, fill_bottom, border_top, border_bottom;
    unsigned long text, text_muted, outline;  /* outline: around text with no bar behind */
    unsigned long separator, hover, pressed, target, mark;
    unsigned long grip, grip_shadow, arrow;
    unsigned long preview_border, preview_fill;
    unsigned long popup_fill, popup_border, popup_hover;
    unsigned long button;  /* resting plate of a text button on a bar */
};

/* light: 1 for the light variant (dark text), 0 for the dark one. */
const struct DockColors *dock_colors(int light);

/* Which variant to draw. choice is a DockThemeChoice; luminance is the mean
 * of what is behind the dock (0..255), or -1 when nothing is known (an opaque
 * bar), and previous the variant drawn so far (-1 none). Automatic choice
 * has a dead band so that a desktop near the middle does not flip the dock
 * back and forth: light from 150 up, dark from 120 down. */
int dock_theme_light(int choice, int luminance, int previous);

/* Mean perceived luminance (Rec. 709 weights) of ARGB pixels whose mask
 * byte is zero (mask may be NULL: all pixels); -1 if none counted. */
int dock_luminance(const unsigned char *argb, const unsigned char *mask, int count);

/* WCAG contrast ratio of two colours, times 100 (450 = 4.5:1). */
int dock_contrast100(unsigned long a, unsigned long b);

#endif
