#ifndef DOCK_SPRITE_H
#define DOCK_SPRITE_H
#include <stdint.h>
struct DockSpriteSpec { int width,height,columns,count,fps; };
/* Owned straight ARGB frames, resized at load time. Zero-initialize before use. */
struct DockSprite { unsigned char *pixels; int size,count,fps,frame; uint64_t started; };
int dock_sprite_load(struct DockSprite *,const char *,const struct DockSpriteSpec *,int);
int dock_sprite_index(const struct DockSprite *,uint64_t elapsed_ms);
void dock_sprite_free(struct DockSprite *);
#endif
