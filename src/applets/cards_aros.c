#include "cards_aros.h"
#include "worker_aros.h"
#include "view.h"
#include "json.h"
#include "../chrome.h"
#include "../theme.h"
#include "../svg_icons.h"
#include "../textrender.h"
#include "../fade.h"
#include "tabler_masks.h"
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/text.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <cybergraphx/cybergraphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define CARD_W 464
#define CARD_H 488
#define HERO_SIZE 84
#define ROW_ICON 32
#define FADE_X 24
#define FADE_Y 36
#define FADE_W 416
#define FADE_H 406
static struct {
 struct Window *window;struct Screen *screen;struct Region *shape;
 struct DockBuffer buffer;struct DockAppletRuntime *runtime;struct DockAppletView view;
 const struct DockColors *colors;struct DockTextRender regular,large,small;
 unsigned char *controls[DOCK_TABLER_COUNT];
 unsigned char *font,*icons[32];char icon_keys[32][48],hero_key[48];int icon_count;
 struct DockSprite hero;unsigned char *hero_static;
 unsigned long revision;uint64_t started,signature;
 struct DockFade fade;unsigned char *fade_pixels;
 int page,hover,pressed,ax,ay,aw,ah,dirty;
 long pen;
} card;
static struct DockAppletRuntime *last;
static long pen_for(unsigned long rgb){
 return ObtainBestPen(card.screen->ViewPort.ColorMap,((rgb>>16)&255)*0x01010101UL,
                      ((rgb>>8)&255)*0x01010101UL,(rgb&255)*0x01010101UL,TAG_DONE);
}
static void color(unsigned long rgb){
 if(card.pen>=0)ReleasePen(card.screen->ViewPort.ColorMap,card.pen);
 card.pen=pen_for(rgb);if(card.pen>=0)SetAPen(&card.buffer.rp,card.pen);
}
static void clean(char *out,size_t n,const char *s){
 snprintf(out,n,"%s",s?s:"");for(char *p=out;*p;p++)if((unsigned char)*p<32||*p==127)*p=' ';
}
static void text(int x,int y,const char *s,unsigned long rgb,int style,int maxw){
 struct DockTextRender *font=style==2?&card.large:style==1?&card.small:&card.regular;
 char value[200];clean(value,sizeof value,s);
 if(font->faces){
  size_t len=strlen(value);
  while(len&&dock_text_width(font,value)>maxw)value[--len]=0;
  struct DockTextImage im;const unsigned char *mask=dock_text_render(font,value,&im);
  if(!mask||im.w<1||im.h<1)return;
  unsigned char *argb=malloc((size_t)im.w*im.h*4);if(!argb)return;
  for(int i=0;i<im.w*im.h;i++){argb[4*i]=mask[i];argb[4*i+1]=rgb>>16;argb[4*i+2]=rgb>>8;argb[4*i+3]=rgb;}
  WritePixelArrayAlpha(argb,0,0,im.w*4,&card.buffer.rp,x-im.left,y-im.top,im.w,im.h,0xffffffffUL);free(argb);
 }else{
  struct RastPort *rp=&card.buffer.rp;size_t len=strlen(value);
  while(len&&TextLength(rp,(CONST_STRPTR)value,len)>maxw)value[--len]=0;
  color(rgb);SetDrMd(rp,JAM1);Move(rp,x,y+rp->Font->tf_Baseline);Text(rp,(CONST_STRPTR)value,len);
 }
}
/* Align within a shared column, including native-font fallback. */
static int text_width(const char *s,int style){
 struct DockTextRender *font=style==2?&card.large:style==1?&card.small:&card.regular;
 return font->faces?dock_text_width(font,s):(int)TextLength(&card.buffer.rp,(CONST_STRPTR)s,strlen(s));
}
static void aligned(int x,int y,int w,const char *s,unsigned long rgb,int style,int right){
 int width=text_width(s,style);if(width>w)width=w;
 text(x+(right?w-width:(w-width)/2),y,s,rgb,style,w);
}
static void line(int x,int y,int xx,int yy,unsigned long rgb){color(rgb);Move(&card.buffer.rp,x,y);Draw(&card.buffer.rp,xx,yy);}
static void plate(int x,int y,int w,int h,unsigned long rgb){dock_round_fill(&card.buffer.rp,x,y,w,h,6,rgb,card.colors->popup_fill);}
static void image(unsigned char *pixels,int size,int x,int y){if(pixels)WritePixelArrayAlpha(pixels,0,0,size*4,&card.buffer.rp,x,y,size,size,0xffffffffUL);}
/* Tabler outline icons, rasterised from the original SVGs at build time
 * (scripts/tabler-masks.py): the same in every package, no SVG renderer
 * needed. Only their colour follows the theme, set once per card. */
static void control(int id,int x,int y,int enabled){
 enum{N=DOCK_TABLER_SIZE};
 if(!card.controls[id] && (card.controls[id]=malloc(N*N*4))){
  unsigned long rgb=card.colors->text_muted;
  for(int i=0;i<N*N;i++){unsigned char *p=card.controls[id]+4*i;p[0]=dock_tabler_masks[id][i];p[1]=rgb>>16;p[2]=rgb>>8;p[3]=rgb;}
 }
 if(!card.controls[id])return;
 unsigned char dimmed[N*N*4];unsigned char *pixels=card.controls[id];
 if(!enabled){memcpy(dimmed,pixels,sizeof dimmed);for(int i=0;i<N*N;i++)dimmed[i*4]/=3;pixels=dimmed;}
 image(pixels,N,x,y);
}
static unsigned char *icon(const char *key){
 if(!key[0])return NULL;
 for(int i=0;i<card.icon_count;i++)if(!strcmp(key,card.icon_keys[i]))return card.icons[i];
 if(card.icon_count==32)return NULL;
 int at=card.icon_count++;char path[DA_PATH];
 snprintf(card.icon_keys[at],sizeof card.icon_keys[at],"%s",key);
 struct DockSpriteSpec spec={0};struct DockSprite still={0};
 if(dock_applet_asset_path(card.runtime,key,path,sizeof path,&spec)){
  /* Row icons stay still; decode only the first frame of the declared sheet. */
  spec.count=1;
  if(dock_sprite_load(&still,path,&spec,ROW_ICON)){
   card.icons[at]=still.pixels;still.pixels=NULL;dock_sprite_free(&still);
  }
 }
 if(!card.icons[at]&&dock_applet_asset_path(card.runtime,key,path,sizeof path,NULL))dock_svg_render(path,ROW_ICON,&card.icons[at]);
 return card.icons[at];
}
static void hero_load(uint64_t now){
 const char *key=card.runtime->state.snapshot.icon;if(!strcmp(key,card.hero_key))return;
 dock_sprite_free(&card.hero);dock_svg_free(card.hero_static);card.hero_static=NULL;
 char path[DA_PATH];struct DockSpriteSpec spec={0};
 if(dock_applet_asset_path(card.runtime,key,path,sizeof path,&spec)&&dock_sprite_load(&card.hero,path,&spec,HERO_SIZE))card.hero.started=now;
 else if(dock_applet_asset_path(card.runtime,key,path,sizeof path,NULL))dock_svg_render(path,HERO_SIZE,&card.hero_static);
 snprintf(card.hero_key,sizeof card.hero_key,"%s",key);
}
static int rows(const char *section,int *out){
 const struct DockAppletSnapshot *s=&card.runtime->state.snapshot;int n=0;
 if(!section[0])return 0;
 for(int i=0;i<s->row_count;i++)if(!strcmp(s->rows[i].section,section))out[n++]=i;
 return n;
}
static uint64_t visible_signature(void){
 int indices[DA_ROWS],count=rows(card.view.strip,indices);
 if(card.page>=count)card.page=0;
 return dock_applet_view_signature(&card.runtime->state.snapshot,&card.view,card.page);
}
static int read_content(void){
 if(!card.fade_pixels)card.fade_pixels=malloc(FADE_W*FADE_H*4);
 if(!card.fade_pixels || ReadPixelArray(card.fade_pixels,0,0,FADE_W*4,&card.buffer.rp,
  FADE_X,FADE_Y,FADE_W,FADE_H,RECTFMT_ARGB)!=FADE_W*FADE_H)return 0;
 /* The panel is opaque; 24-bit readback need not carry a meaningful alpha. */
 for(int i=0;i<FADE_W*FADE_H;i++)card.fade_pixels[i*4]=255;
 return 1;
}
static void footer(int h){
 const struct DockColors *c=card.colors;const struct DockAppletSnapshot *s=&card.runtime->state.snapshot;
 char age[100];double minutes=difftime(time(NULL),(time_t)s->updated)/60;
 if(card.runtime->state.pending)snprintf(age,sizeof age,"Updating...");
 else if(card.runtime->state.has_data)snprintf(age,sizeof age,"%s | %.0f min ago",s->status==DA_STALE?(card.view.layout_count?"Saved":"Saved forecast"):"Updated",minutes>0?minutes:0);
 else snprintf(age,sizeof age,"%s",s->status==DA_LOADING?"Updating":"Offline");
 text(24,h-39,age,c->text_muted,1,365);
 text(24,h-21,s->attribution,c->text_muted,1,365);
 if(card.hover==5)plate(412,h-44,32,34,c->popup_hover);
 control(4,418,h-38,1);
}
/* ---- a card the widget describes (view.h) ---- */
#define LAY_TOP 18
#define LAY_FOOT 60
#define LAY_ROW 30
#define LAY_GAP 12
static int block_h(const struct DockViewNode *n){
 switch(n->type){
  case DV_TEXT:return n->style==DV_BIG?50:n->style==DV_MUTED?17:22;
  case DV_TREND:return 17;
  case DV_ICON:return n->size<=ROW_ICON?ROW_ICON:HERO_SIZE;
  case DV_SEPARATOR:return 13;
  case DV_SPACER:return n->size;
  default:return 0;
 }
}
static int group_h(const struct DockViewNode *n){
 int h=block_h(n);
 for(int k=1;k<=n->count;k++)if(block_h(n+k)>h)h=block_h(n+k);
 if(n->type==DV_LIST)return n->size*(h>LAY_ROW?h:LAY_ROW);
 return h;
}
static int layout_h(void){
 int h=LAY_TOP+LAY_FOOT;
 for(int i=0;i<card.view.layout_count;i+=1+card.view.layout[i].count)h+=group_h(&card.view.layout[i]);
 return h<140?140:h>CARD_H?CARD_H:h;
}
static int card_h(void){return card.view.layout_count?layout_h():CARD_H;}
static const char *block_text(const struct DockViewNode *n,const struct DockAppletRow *r){
 if(n->literal[0])return n->literal;
 if(!n->source[0])return "";
 if(!r)return dock_applet_field(&card.runtime->state.snapshot,n->source);
 return !strcmp(n->source,"label")?r->label:!strcmp(n->source,"value")?r->value:!strcmp(n->source,"detail")?r->detail:
        !strcmp(n->source,"caption")?r->caption:"";
}
static int block_number(const struct DockViewNode *n,const struct DockAppletRow *r,double *out){
 const struct DockAppletSnapshot *s=&card.runtime->state.snapshot;
 if(r){if(!r->has_plot)return 0;*out=r->plot;return 1;}
 for(int i=0;i<s->field_count;i++)if(!strcmp(s->fields[i].key,n->number)&&s->fields[i].numeric){*out=s->fields[i].number;return 1;}
 return 0;
}
static int block_style(const struct DockViewNode *n){return n->type==DV_TREND||n->style==DV_MUTED?1:n->style==DV_BIG?2:0;}
static int block_w(const struct DockViewNode *n,const struct DockAppletRow *r){
 char value[200];
 switch(n->type){
  case DV_TEXT:clean(value,sizeof value,block_text(n,r));return text_width(value,block_style(n));
  case DV_TREND:clean(value,sizeof value,block_text(n,r));return text_width(value,1)+13;
  case DV_ICON:return block_h(n);
  case DV_SPACER:return n->size;
  default:return 0;
 }
}
/* Draws one block in the box x,y,w,h; text keeps to its side of the box. */
static void block_draw(const struct DockViewNode *n,const struct DockAppletRow *r,int x,int y,int w,int h){
 const struct DockColors *c=card.colors;int own=block_h(n),top=y+(h-own)/2;
 if(n->type==DV_TEXT){
  unsigned long tone=n->style==DV_MUTED?c->text_muted:c->text;int ty=top+(n->style==DV_BIG?2:3);
  if(n->right)aligned(x,ty,w,block_text(n,r),tone,block_style(n),1);
  else text(x,ty,block_text(n,r),tone,block_style(n),w);
 }
 if(n->type==DV_TREND){
  double v=0;int known=block_number(n,r,&v);const char *t=block_text(n,r);
  /* readable on the dark and on the light card */
  unsigned long tone=!known||v==0?c->text_muted:v>0?0x35a96bUL:0xdc5b52UL;
  char value[200];clean(value,sizeof value,t);
  int tw=text_width(value,1);if(tw>w-13)tw=w-13;
  int tx=n->right?x+w-tw:x+13;
  text(tx,top+3,value,tone,1,tw);
  if(known&&v!=0){struct RastPort *rp=&card.buffer.rp;int ax=tx-8,ay=top+6;color(tone);
   for(int i=0;i<5;i++){int half=v>0?i:4-i;Move(rp,ax-half,ay+i);Draw(rp,ax+half,ay+i);}}
 }
 if(n->type==DV_ICON){
  const char *key=r?r->icon:!strcmp(n->source,"icon")?card.runtime->state.snapshot.icon:block_text(n,NULL);
  if(own==ROW_ICON)image(icon(key),ROW_ICON,x,top);
  else{unsigned char *hero=card.hero.pixels?card.hero.pixels+(size_t)card.hero.frame*HERO_SIZE*HERO_SIZE*4:card.hero_static;image(hero,HERO_SIZE,x,top);}
 }
}
/* One line of blocks between x0 and x1: left ones from the left, right ones
 * from the right; widths[] gives each block's column (shared by list rows). */
static void line_draw(const struct DockViewNode *n,int count,const struct DockAppletRow *r,const int *widths,int x0,int x1,int y,int h){
 for(int k=0;k<count;k++)if(!n[k].right){int w=widths[k];if(x0+w>x1)w=x1-x0;if(w>0)block_draw(&n[k],r,x0,y,w,h);x0+=w+LAY_GAP;}
 for(int k=count-1;k>=0;k--)if(n[k].right){int w=widths[k];if(x1-w<x0)w=x1-x0;if(w>0)block_draw(&n[k],r,x1-w,y,w,h);x1-=w+LAY_GAP;}
}
static void draw_layout(void){
 const struct DockColors *c=card.colors;struct RastPort *rp=&card.buffer.rp;
 const struct DockAppletSnapshot *s=&card.runtime->state.snapshot;
 struct DockFrameStyle frame={c->popup_fill,c->popup_fill,c->popup_border,c->popup_border};
 int height=card_h(),y=LAY_TOP;const int margin=24;
 unsigned long separator=dock_mix(c->popup_fill,c->separator,0.4f);
 dock_frame_draw(rp,&frame,CARD_W,height,dock_corner_radius);
 if(card.hover==1)plate(382,9,32,30,c->popup_hover);
 if(card.hover==2)plate(420,9,30,30,c->popup_hover);
 control(0,388,14,1);
 control(1,425,15,1);
 if(!card.runtime->state.has_data){
  text(margin,18,card.runtime->package.name,c->text,0,326);
  text(margin,55,s->status==DA_LOADING?"Loading...":"Data unavailable",c->text,0,310);
  text(margin,80,"Choose Settings to check this widget",c->text_muted,1,360);
 }else for(int i=0;i<card.view.layout_count&&y<height-LAY_FOOT;i+=1+card.view.layout[i].count){
  const struct DockViewNode *n=&card.view.layout[i];
  int h=group_h(n),x1=CARD_W-margin,widths[8];
  /* beside the header controls a line is shorter */
  if(y<42)x1=370;
  if(n->type==DV_SEPARATOR)line(margin,y+6,x1,y+6,separator);
  else if(n->type==DV_ROW){
   for(int k=0;k<n->count;k++)widths[k]=block_w(n+1+k,NULL);
   line_draw(n+1,n->count,NULL,widths,margin,x1,y,h);
  }else if(n->type==DV_LIST){
   int entries[DA_ROWS],count=rows(n->source,entries),rh=h/n->size;
   if(count>n->size)count=n->size;
   for(int k=0;k<n->count;k++){widths[k]=0;for(int e=0;e<count;e++){int w=block_w(n+1+k,&s->rows[entries[e]]);if(w>widths[k])widths[k]=w;}}
   for(int e=0;e<count&&y+e*rh+rh<=height-LAY_FOOT;e++){
    if(e)line(margin,y+e*rh,x1,y+e*rh,separator);
    line_draw(n+1,n->count,&s->rows[entries[e]],widths,margin,x1,y+e*rh,rh);
   }
  }else{widths[0]=n->type==DV_TEXT||n->type==DV_TREND?x1-margin:block_w(n,NULL);line_draw(n,1,NULL,widths,margin,x1,y,h);}
  y+=h;
 }
 footer(height);
 dock_buffer_show(&card.buffer,card.window);card.dirty=0;
}
static void draw(void){
 const struct DockColors *c=card.colors;struct RastPort *rp=&card.buffer.rp;
 const struct DockAppletSnapshot *s=&card.runtime->state.snapshot;
 if(card.view.layout_count){draw_layout();return;}
 struct DockFrameStyle frame={c->popup_fill,c->popup_fill,c->popup_border,c->popup_border};
 dock_frame_draw(rp,&frame,CARD_W,CARD_H,dock_corner_radius);
 const int margin=24,inner=CARD_W-2*margin;
 unsigned long inset=dock_mix(c->popup_fill,c->preview_fill,0.38f);
 unsigned long separator=dock_mix(c->popup_fill,c->separator,0.4f);
 text(margin,18,dock_applet_field(s,card.view.title),c->text,0,326);
 if(!s->title[0])text(margin,18,card.runtime->package.name,c->text,0,326);
 if(card.hover==1)plate(382,9,32,30,c->popup_hover);
 if(card.hover==2)plate(420,9,30,30,c->popup_hover);
 control(0,388,14,1);
 control(1,425,15,1);
 if(card.runtime->state.has_data){
  text(margin,42,dock_applet_field(s,card.view.value),c->text,2,300);
  text(margin,87,dock_applet_field(s,card.view.subtitle),c->text,0,300);
  text(margin,108,dock_applet_field(s,card.view.range),c->text_muted,1,300);
  unsigned char *hero=card.hero.pixels?card.hero.pixels+(size_t)card.hero.frame*HERO_SIZE*HERO_SIZE*4:card.hero_static;
  image(hero,HERO_SIZE,348,36);
 }else{
  text(margin,55,s->status==DA_LOADING?"Loading...":"Data unavailable",c->text,0,310);
  text(margin,94,"Choose Settings to check this widget",c->text_muted,1,360);
 }
 for(int i=0;i<2;i++)if(card.view.metric[i][0]){
  int x=margin+i*216;plate(x,128,200,30,inset);
  text(x+12,138,card.view.metric_label[i],c->text_muted,1,88);
  aligned(x+96,136,92,dock_applet_field(s,card.view.metric[i]),c->text,0,1);
 }
 text(margin,170,card.view.strip_title,c->text_muted,1,300);
 int hourly[DA_ROWS],n=rows(card.view.strip,hourly);if(card.page>=n)card.page=0;
 if(card.hover==3)plate(383,162,28,27,c->popup_hover);
 if(card.hover==4)plate(420,162,28,27,c->popup_hover);
 control(2,387,165,card.page>0);
 control(3,424,165,card.page+6<n);
 /* The hourly icons sit on the card's own fill inside an inset outline:
  * the light cloud tone of the weather artwork (about 220,227,233) is the
  * inset colour itself, so on a filled inset plate clouds vanished and
  * two-tone clouds showed only their dark half. */
 plate(margin,192,inner,86,inset);
 dock_round_fill(rp,margin+1,193,inner-2,84,5,c->popup_fill,inset);
 for(int k=0;k<6&&card.page+k<n;k++){
  const struct DockAppletRow *r=&s->rows[hourly[card.page+k]];
  int x=margin+k*inner/6,w=(k+1)*inner/6-k*inner/6;
  aligned(x,199,w,r->caption[0]?r->caption:r->label,c->text_muted,1,0);
  image(icon(r->icon),ROW_ICON,x+(w-ROW_ICON)/2,214);
  aligned(x,246,w,r->value,c->text,0,0);
  aligned(x,265,w,r->detail,c->text_muted,1,0);
 }
 text(margin,290,card.view.list_title,c->text_muted,1,390);
 int daily[DA_ROWS],days=rows(card.view.list,daily);
 for(int k=0;k<5&&k<days;k++){
  const struct DockAppletRow *r=&s->rows[daily[k]];int y=312+k*26;
  if(k)line(margin,y-4,CARD_W-margin,y-4,separator);
  text(margin,y+3,r->caption[0]?r->caption:r->label,c->text,0,88);
  image(icon(r->icon),ROW_ICON,120,y-5);
  aligned(170,y+5,70,r->detail,c->text_muted,1,1);
  aligned(272,y+3,168,r->value,c->text,0,1);
 }
 footer(CARD_H);
 if(card.fade.from){
  if(read_content()){
   dock_fade_apply(&card.fade,card.fade_pixels,FADE_W*4,dock_applets_now());
   WritePixelArray(card.fade_pixels,0,0,FADE_W*4,&card.buffer.rp,FADE_X,FADE_Y,FADE_W,FADE_H,RECTFMT_ARGB);
  }else dock_fade_clear(&card.fade);
 }
 dock_buffer_show(&card.buffer,card.window);card.dirty=0;
}
void dock_applet_cards_close(void){
 dock_fade_clear(&card.fade);free(card.fade_pixels);
 if(card.window){struct Message *m;while((m=GetMsg(card.window->UserPort)))ReplyMsg(m);CloseWindow(card.window);}
 if(card.shape)DisposeRegion(card.shape);
 if(card.screen&&card.pen>=0)ReleasePen(card.screen->ViewPort.ColorMap,card.pen);
 for(int i=0;i<DOCK_TABLER_COUNT;i++)free(card.controls[i]);
 dock_buffer_close(&card.buffer);dock_sprite_free(&card.hero);dock_svg_free(card.hero_static);
 for(int i=0;i<card.icon_count;i++)dock_svg_free(card.icons[i]);
 dock_text_close(&card.regular);dock_text_close(&card.large);dock_text_close(&card.small);free(card.font);
 memset(&card,0,sizeof card);card.pen=-1;
}
struct Window *dock_applet_cards_window(void){return card.window;}
struct DockAppletRuntime *dock_applet_cards_instance(void){return card.runtime?card.runtime:last;}
/* r is about to be freed (no window shows it): no pointer to it may remain. */
void dock_applet_cards_forget(const struct DockAppletRuntime *r){if(last==r)last=NULL;if(card.runtime==r)card.runtime=NULL;}
int dock_applet_cards_open(struct Screen *screen,struct DockAppletRuntime *r,int ax,int ay,int aw,int ah,int vertical,const struct DockColors *colors){
 if(card.window&&card.runtime==r){dock_applet_cards_close();return 1;}
 dock_applet_cards_close();char path[DA_PATH];size_t n;char *b=NULL;
 if(!dock_applet_path(r->package.root,"views.json",path,sizeof path)||(b=dock_applet_read(path,65536,&n))==NULL)return 0;
 int valid=dock_applet_view_parse(b,n,&card.view);free(b);if(!valid)return 0;
 int h=card_h();
 if(screen->Width<CARD_W+16||screen->Height-screen->BarHeight<h+16)return 0;
 card.screen=screen;card.runtime=r;last=r;card.colors=colors?colors:dock_colors(0);
 card.ax=ax;card.ay=ay;card.aw=aw;card.ah=ah;card.pen=-1;
 size_t size=0;
 FILE *f=fopen("FONTS:TrueType/VeraSans.ttf","rb");
 if(f){if(!fseek(f,0,SEEK_END)){long length=ftell(f);if(length>0&&length<4*1024*1024&&!fseek(f,0,SEEK_SET)){
  card.font=malloc((size_t)length);if(card.font&&fread(card.font,1,length,f)==(size_t)length)size=length;
 }}fclose(f);}
 if(size){const unsigned char *font=card.font;dock_text_open(&card.regular,&font,&size,1,14,DOCK_CODESET_LATIN1);dock_text_open(&card.small,&font,&size,1,11,DOCK_CODESET_LATIN1);dock_text_open(&card.large,&font,&size,1,40,DOCK_CODESET_LATIN1);}
 int x,y;dock_applet_anchor(screen->Width,screen->Height,screen->BarHeight,CARD_W,h,ax,ay,aw,ah,vertical,&x,&y);
 card.shape=dock_shape(CARD_W,h,dock_corner_radius);
 if(!card.shape||!dock_buffer_open(&card.buffer,screen,CARD_W,h))goto fail;
 SetFont(&card.buffer.rp,screen->RastPort.Font);
 card.window=OpenWindowTags(NULL,WA_PubScreen,(IPTR)screen,WA_Left,x,WA_Top,y,WA_Width,CARD_W,WA_Height,h,
  WA_ShapeRegion,(IPTR)card.shape,WA_Borderless,TRUE,WA_Activate,TRUE,WA_RMBTrap,TRUE,WA_ReportMouse,TRUE,WA_SimpleRefresh,TRUE,
  WA_IDCMP,IDCMP_MOUSEBUTTONS|IDCMP_MOUSEMOVE|IDCMP_RAWKEY|IDCMP_INACTIVEWINDOW|IDCMP_REFRESHWINDOW,TAG_DONE);
 if(!card.window)goto fail;
 card.started=dock_applets_now();hero_load(card.started);card.revision=r->state.view_revision;card.signature=visible_signature();draw();return 1;
 fail:dock_applet_cards_close();return 0;
}
static int hit(int x,int y){
 if(x>=382&&x<414&&y>=9&&y<39)return 1;
 if(x>=420&&x<450&&y>=9&&y<39)return 2;
 int h=card_h(),full=!card.view.layout_count;
 if(full&&x>=383&&x<411&&y>=162&&y<189)return 3;
 if(full&&x>=420&&x<448&&y>=162&&y<189)return 4;
 if(x>=412&&x<444&&y>=h-44&&y<h-10)return 5;
 return 0;
}
int dock_applet_cards_poll(uint64_t now){
 if(!card.window)return 0;
 struct IntuiMessage *m;int close=0,action=0;
 while((m=(struct IntuiMessage *)GetMsg(card.window->UserPort))){
  ULONG cls=m->Class;UWORD code=m->Code;int x=m->MouseX,y=m->MouseY;ReplyMsg((struct Message *)m);
  if(cls==IDCMP_REFRESHWINDOW){BeginRefresh(card.window);EndRefresh(card.window,TRUE);card.dirty=1;}
  if(cls==IDCMP_RAWKEY&&code==0x45)close=1;
  if(cls==IDCMP_INACTIVEWINDOW){int mx=card.screen->MouseX,my=card.screen->MouseY;
   if(mx<card.ax||mx>=card.ax+card.aw||my<card.ay||my>=card.ay+card.ah)close=1;
  }
  if(cls==IDCMP_MOUSEMOVE){int h=hit(x,y);if(h!=card.hover){card.hover=h;card.dirty=1;}}
  if(cls==IDCMP_MOUSEBUTTONS&&code==SELECTDOWN)card.pressed=hit(x,y);
  if(cls==IDCMP_MOUSEBUTTONS&&code==SELECTUP){if(card.pressed&&card.pressed==hit(x,y))action=card.pressed;card.pressed=0;}
 }
 if(action==1){last=card.runtime;dock_applet_cards_close();return 6;}
 if(action==2)close=1;
 if(close){dock_applet_cards_close();return 0;}
 if(action==3&&card.page>=6){card.page-=6;card.dirty=1;dock_fade_clear(&card.fade);card.signature=visible_signature();}
 if(action==4){int entries[DA_ROWS],n=rows(card.view.strip,entries);if(card.page+6<n){card.page+=6;card.dirty=1;dock_fade_clear(&card.fade);card.signature=visible_signature();}}
 if(action==5){dock_applet_refresh(&card.runtime->state,now);card.dirty=1;}
 if(card.revision!=card.runtime->state.view_revision){
  uint64_t signature=visible_signature();
  hero_load(now);
  if(signature!=card.signature){
   /* the cross-fade region belongs to the full card */
   if(card.view.layout_count || !dock_transitions_enabled || !read_content() ||
      !dock_fade_begin(&card.fade,card.fade_pixels,FADE_W*4,FADE_W,FADE_H,dock_applets_now()))dock_fade_clear(&card.fade);
  }
  card.signature=signature;card.revision=card.runtime->state.view_revision;card.dirty=1;
 }
 if(card.fade.from)card.dirty=1;
 if(card.hero.pixels){int frame=dock_sprite_index(&card.hero,now-card.hero.started);if(frame!=card.hero.frame){card.hero.frame=frame;card.dirty=1;}}
 if(card.dirty)draw();
 return 0;
}
