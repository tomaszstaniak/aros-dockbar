#ifndef DOCK_ALPHA_H
#define DOCK_ALPHA_H

/* One ARGB pixel (A,R,G,B bytes, straight alpha) with 'color' (0xRRGGBB)
 * laid over it at coverage 'alpha' (0..255): Porter-Duff source over, the
 * blend the compositor itself does (gfx.hidd PutAlphaImage, not
 * premultiplied). Over an opaque pixel it is the plain mix the pictured
 * docks use; over a clear one it is the colour at that alpha. Rounded to
 * nearest; an opaque source gives an opaque pixel whatever is under it. */
void dock_over(unsigned char *pixel, unsigned long color, int alpha);

#endif
