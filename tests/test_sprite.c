#include "sprite.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) {
 struct DockSprite s={0};struct DockSpriteSpec spec={128,128,11,120,30};
 assert(argc==2);
 assert(dock_sprite_load(&s,argv[1],&spec,32));
 assert(s.size==32 && s.count==120);
 assert(dock_sprite_index(&s,0)==0);
 assert(dock_sprite_index(&s,33)==0);
 assert(dock_sprite_index(&s,34)==1);
 assert(dock_sprite_index(&s,3999)==119);
 assert(dock_sprite_index(&s,4000)==0);
 assert(memcmp(s.pixels,s.pixels+30*32*32*4,32*32*4));
 dock_sprite_free(&s);dock_sprite_free(&s);
 spec.count=122;assert(!dock_sprite_load(&s,argv[1],&spec,32));
 spec.count=120;spec.columns=10;assert(!dock_sprite_load(&s,argv[1],&spec,32));
 spec.columns=11;spec.fps=0;assert(!dock_sprite_load(&s,argv[1],&spec,32));
 assert(!dock_sprite_load(&s,"/missing.png",&spec,32));
 puts("sprite: decode, frame bounds, timing, loop and invalid metadata passed");
 return 0;
}
