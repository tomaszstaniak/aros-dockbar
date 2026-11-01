#include "fade.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 struct DockFade f={0};unsigned char red[]={255,255,0,0,77,77,77,77};
 unsigned char blue[]={255,0,0,255,77,77,77,77};uint64_t now=1ULL<<40;
 assert(dock_fade_begin(&f,red,8,1,1,now));
 dock_fade_apply(&f,blue,8,now);assert(!memcmp(blue,red,8));
 unsigned char target[]={255,0,0,255,77,77,77,77};memcpy(blue,target,8);
 dock_fade_apply(&f,blue,8,now+200);assert(blue[0]==255&&blue[1]>=127&&blue[1]<=128&&blue[3]>=127&&blue[3]<=128&&blue[4]==77);
 unsigned char middle[8];memcpy(middle,blue,8);
 assert(dock_fade_begin(&f,blue,8,1,1,now+200));
 memcpy(blue,target,8);dock_fade_apply(&f,blue,8,now+200);assert(!memcmp(blue,middle,8));
 memcpy(blue,target,8);dock_fade_apply(&f,blue,8,now+600);assert(!f.from&&!memcmp(blue,target,8));
 red[0]=0;assert(dock_fade_begin(&f,red,8,1,1,now));memcpy(blue,target,8);
 dock_fade_apply(&f,blue,8,now+200);assert(blue[0]>=127&&blue[0]<=128&&blue[1]==0&&blue[3]==255);
 assert(!dock_fade_begin(&f,red,8,513,1,now));assert(f.from);
 dock_fade_clear(&f);dock_transitions_enabled=0;assert(!dock_fade_begin(&f,red,8,1,1,now)&&!f.from);
 dock_fade_clear(&f);puts("fade: endpoints, premultiplied alpha, stride, interruption, time bounds and disable passed");return 0;
}
