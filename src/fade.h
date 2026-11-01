#ifndef DOCK_FADE_H
#define DOCK_FADE_H
#include <stdint.h>
#define DOCK_FADE_MS 400
/* Host presentation policy, independent of provider/package implementations. */
extern int dock_transitions_enabled;
struct DockFade { unsigned char *from; int width,height; uint64_t started; };
/* Capture the currently presented straight ARGB rectangle, not its next target.
 * A new capture can replace an in-flight transition without a visual jump.
 * Bounds 1..512, stride in bytes. Zero-initialize; caller owns the target. */
int dock_fade_begin(struct DockFade *,const unsigned char *,int stride,int width,int height,uint64_t);
void dock_fade_apply(struct DockFade *,unsigned char *,int stride,uint64_t);
void dock_fade_clear(struct DockFade *);
#endif
