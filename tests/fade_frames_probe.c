/* Offline recording of the actual fade primitive over saved native ARGB frames.
 * This is not a recording of the application event loop. */
#include "fade.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc!=6)return 5;
 int w=atoi(argv[1]),h=atoi(argv[2]);if(w<1||w>512||h<1||h>512)return 5;
 size_t size=(size_t)w*h*4;unsigned char *a=malloc(size),*b=malloc(size),*p=malloc(size);
 if(!a||!b||!p)return 5;
 FILE *f=fopen(argv[3],"rb");if(!f)return 5;int ok=fread(a,1,size,f)==size;fclose(f);if(!ok)return 5;
 f=fopen(argv[4],"rb");if(!f)return 5;ok=fread(b,1,size,f)==size;fclose(f);if(!ok)return 5;
 struct DockFade fade={0};memcpy(p,a,size);
 for(int frame=0;frame<100;frame++){
  uint64_t now=(uint64_t)frame*50;
  if(frame==20||frame==60){if(!dock_fade_begin(&fade,p,w*4,w,h,now))return 5;}
  memcpy(p,frame>=20&&frame<60?b:a,size);dock_fade_apply(&fade,p,w*4,now);
  char path[1024];if(snprintf(path,sizeof path,"%s/frame-%03d.ppm",argv[5],frame)>=(int)sizeof path)return 5;
  f=fopen(path,"wb");if(!f)return 5;
  fprintf(f,"P6\n%d %d\n255\n",w,h);
  for(size_t i=0;i<(size_t)w*h;i++)if(fwrite(p+i*4+1,1,3,f)!=3)return 5;
  if(fclose(f))return 5;
 }
 dock_fade_clear(&fade);free(a);free(b);free(p);return 0;
}
