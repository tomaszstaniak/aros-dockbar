#include "fade.h"
#include <stdlib.h>
#include <string.h>
int dock_transitions_enabled=1;
void dock_fade_clear(struct DockFade *f){if(f){free(f->from);memset(f,0,sizeof *f);}}
int dock_fade_begin(struct DockFade *f,const unsigned char *pixels,int stride,int w,int h,uint64_t now){
 if(!f)return 0;
 if(!dock_transitions_enabled){dock_fade_clear(f);return 0;}
 if(!pixels||w<1||h<1||w>512||h>512||stride<w*4)return 0;
 unsigned char *copy=malloc((size_t)w*h*4);if(!copy)return 0;
 for(int y=0;y<h;y++)memcpy(copy+(size_t)y*w*4,pixels+(size_t)y*stride,(size_t)w*4);
 dock_fade_clear(f);f->from=copy;f->width=w;f->height=h;f->started=now;return 1;
}
void dock_fade_apply(struct DockFade *f,unsigned char *pixels,int stride,uint64_t now){
 if(!f||!f->from||!pixels||stride<f->width*4)return;
 uint64_t elapsed=now>=f->started?now-f->started:0;
 if(!dock_transitions_enabled||elapsed>=DOCK_FADE_MS){dock_fade_clear(f);return;}
 uint64_t q=elapsed*65535/DOCK_FADE_MS;
 uint64_t weight=q*q*(3*65535-2*q)/(65535ULL*65535); /* smoothstep */
 for(int y=0;y<f->height;y++)for(int x=0;x<f->width;x++){
  const unsigned char *a=f->from+((size_t)y*f->width+x)*4;
  unsigned char *b=pixels+(size_t)y*stride+x*4;
  /* Dock/card buffers are opaque: avoid per-channel variable divisions there. */
  if(a[0]==255&&b[0]==255){
   unsigned w=(unsigned)weight,old=65535-w;
   for(int k=1;k<4;k++)b[k]=(unsigned char)((a[k]*old+b[k]*w+32767)/65535);
   continue;
  }
  uint64_t aa=a[0]*(65535-weight),bb=b[0]*weight,total=aa+bb;
  for(int k=1;k<4;k++)b[k]=total?(unsigned char)((a[k]*aa+b[k]*bb+total/2)/total):0;
  b[0]=(unsigned char)((total+32767)/65535);
 }
}
