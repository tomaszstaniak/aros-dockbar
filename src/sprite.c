#include "sprite.h"
#include "resample.h"
#include "pak.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef DOCK_NO_PNG
#include <png.h>
/* A sheet is a PNG file, or an entry of a resource pack ("x.pak#name"):
 * licensed artwork ships packed, never as loose files (src/pak.h). */
struct Decode { FILE *file; unsigned char *memory; size_t size,at; unsigned char *rgba; png_bytep *rows; };
static void read_png(png_structp png,png_bytep data,png_size_t size) {
 struct Decode *d=png_get_io_ptr(png);
 if(d->memory){
  if(size>d->size-d->at)png_error(png,"Truncated sprite PNG");
  memcpy(data,d->memory+d->at,size);d->at+=size;return;
 }
 if(fread(data,1,size,d->file)!=size)png_error(png,"Truncated sprite PNG");
}
/* libpng part only: setjmp is safe here because nothing it needs after a
 * longjmp is changed in between (the caller owns the source). */
static unsigned char *decode_png(struct Decode *d,int *width,int *height) {
 unsigned char *volatile result=NULL;  /* set after setjmp */
 png_structp png=NULL;png_infop info=NULL;
 png=png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
 if(!png)return NULL;
 info=png_create_info_struct(png);if(!info)goto done;
 if(setjmp(png_jmpbuf(png)))goto done;
 png_set_read_fn(png,d,read_png);
 png_set_user_limits(png,4096,4096);
 png_read_info(png,info);
 png_uint_32 w=png_get_image_width(png,info),h=png_get_image_height(png,info);
 int color=png_get_color_type(png,info),depth=png_get_bit_depth(png,info);
 if(!w||!h||w>4096||h>4096)goto done;
 if(depth==16)png_set_strip_16(png);
 if(color==PNG_COLOR_TYPE_PALETTE)png_set_palette_to_rgb(png);
 if(color==PNG_COLOR_TYPE_GRAY&&depth<8)png_set_expand_gray_1_2_4_to_8(png);
 int alpha=png_get_valid(png,info,PNG_INFO_tRNS);
 if(alpha)png_set_tRNS_to_alpha(png);
 if(color==PNG_COLOR_TYPE_GRAY||color==PNG_COLOR_TYPE_GRAY_ALPHA)png_set_gray_to_rgb(png);
 if(!(color&PNG_COLOR_MASK_ALPHA)&&!alpha)png_set_add_alpha(png,255,PNG_FILLER_AFTER);
 png_set_interlace_handling(png);png_read_update_info(png,info);
 if(png_get_rowbytes(png,info)!=w*4)goto done;
 d->rgba=malloc((size_t)w*h*4);d->rows=malloc(h*sizeof *d->rows);
 if(!d->rgba||!d->rows)goto done;
 for(unsigned y=0;y<h;y++)d->rows[y]=d->rgba+(size_t)y*w*4;
 png_read_image(png,d->rows);png_read_end(png,info);
 for(size_t i=0;i<(size_t)w*h;i++){
  unsigned char *p=d->rgba+i*4,a=p[3];p[3]=p[2];p[2]=p[1];p[1]=p[0];p[0]=a;
 }
 *width=w;*height=h;result=d->rgba;d->rgba=NULL;
 done:
 png_destroy_read_struct(&png,info?&info:NULL,NULL);
 return result;
}
static unsigned char *decode(const char *path,int *width,int *height) {
 struct Decode *d=calloc(1,sizeof *d);unsigned char *result=NULL;
 char pak[600],entry[256];
 if(!d)return NULL;
 if(dock_pak_split(path,pak,sizeof pak,entry,sizeof entry))
  d->memory=dock_pak_read(pak,entry,4u*1024*1024,&d->size);
 else d->file=fopen(path,"rb");
 if(d->memory||d->file)result=decode_png(d,width,height);
 if(d->file)fclose(d->file);
 free(d->memory);free(d->rgba);free(d->rows);free(d);return result;
}
#endif
int dock_sprite_load(struct DockSprite *s,const char *path,const struct DockSpriteSpec *spec,int size) {
#ifdef DOCK_NO_PNG
 (void)s;(void)path;(void)spec;(void)size;return 0;
#else
 int w=0,h=0,ok=0;unsigned char *sheet=NULL,*cell=NULL,*frames=NULL;
 if(!s||s->pixels||!path||!spec||size<1||size>128||spec->width<1||spec->width>512||
    spec->height<1||spec->height>512||spec->columns<1||spec->columns>32||
    spec->count<1||spec->count>256||spec->fps<1||spec->fps>60)return 0;
 sheet=decode(path,&w,&h);if(!sheet)goto done;
 if(w!=spec->width*spec->columns||h%spec->height||
    spec->count>spec->columns*(h/spec->height))goto done;
 frames=malloc((size_t)spec->count*size*size*4);
 cell=malloc((size_t)spec->width*spec->height*4);if(!frames||!cell)goto done;
 for(int f=0;f<spec->count;f++){
  unsigned char *scaled=NULL;
  for(int y=0;y<spec->height;y++)memcpy(cell+(size_t)y*spec->width*4,
    sheet+((size_t)((f/spec->columns)*spec->height+y)*w+(f%spec->columns)*spec->width)*4,
    (size_t)spec->width*4);
  if(!dock_resample_argb(cell,spec->width,spec->height,size,size,&scaled))goto done;
  memcpy(frames+(size_t)f*size*size*4,scaled,(size_t)size*size*4);free(scaled);
 }
 s->pixels=frames;frames=NULL;s->size=size;s->count=spec->count;s->fps=spec->fps;s->frame=0;s->started=0;ok=1;
 done:free(sheet);free(cell);free(frames);return ok;
#endif
}
int dock_sprite_index(const struct DockSprite *s,uint64_t ms) {
 if(!s||!s->pixels||s->count<1||s->fps<1)return 0;
 /* Split seconds to avoid overflow on long-running hosts. */
 return (int)(((ms/1000%(unsigned)s->count)*s->fps+(ms%1000)*s->fps/1000)%s->count);
}
void dock_sprite_free(struct DockSprite *s) {
 if(s){free(s->pixels);memset(s,0,sizeof *s);}
}
