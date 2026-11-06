#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/notify.h>
#include <proto/input.h>
#include <exec/io.h>
#include <devices/inputevent.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <graphics/clip.h>
#include <proto/icon.h>
#include <proto/cybergraphics.h>
#include <workbench/startup.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "model.h"
#include "config.h"
#include "docks.h"
#include "layout.h"
#include "magnify.h"
#include "hover.h"
#include "resample.h"
#include "elapsed.h"
#include "widget.h"
#include "applets/worker_aros.h"
#include "applets/panel_aros.h"
#include "applets/install.h"
#include "../include/dockbar/widgets.h"
#include <stddef.h>
#include "interaction.h"
#include "native_icons.h"
#include "sprite.h"
#include "fade.h"
#include "applets/view.h"
#include "svg_icons.h"
#include "group_panel.h"
#include "drawer_panel.h"
#include "dirlist.h"
#include "chrome.h"
#include "prefs.h"
#include "prefs_window.h"
#include "prefs_strings.h"
#include "grip_menu.h"
#include "about.h"
#include "item_list.h"
#include "snapshot.h"
#include "theme.h"
#include "textrender.h"
#include "fontfile.h"
#include "command.h"
#include "version.h"
#include "migrate.h"
#include "present.h"
#include "alpha.h"

/* Version(1) reads this; keep the date in the AmigaOS dd.mm.yyyy form. */
static const char version_tag[] __attribute__((used))="$VER: Dockbar " DOCK_VERSION " (" DOCK_DATE ")";
struct Library *CyberGfxBase;
struct Device *InputBase;

/* argb: the picture as drawn by icon.library or the SVG renderer, read back
 * once with its alpha (normal and selected state), so the dock can compose
 * it like everything else. The artwork itself is never changed. */
struct DockImage { DockNativeIcon native; unsigned char *svg; int size; unsigned char *argb[2]; };
/* Magnify: the icon loaded once more at the largest size its slot allows
 * (source), and copies scaled down from it for the sizes on the way (step,
 * by growth step), each made when first needed and kept. A copy is never
 * made from another copy or from the small resting picture. All of it counts
 * against one budget (see mag_budget_bytes). */
#define DOCK_MAG_STEPS 16
struct DockMag { struct DockImage source; unsigned char *step[DOCK_MAG_STEPS][2]; int size[DOCK_MAG_STEPS]; unsigned used[DOCK_MAG_STEPS][2]; };
struct Visual { struct DockImage image, mini[4]; struct DockMag mag; struct DockSprite sprite; struct DockFade fade; int fade_pending; int applet_dx; uint64_t signature; int minis, use_minis; struct DockWidget widget; struct DockAppletRuntime *applet; char applet_icon[48]; };

/* The window side of one dock. Index i belongs to app.set.docks[i]; any
 * change to the set closes and reopens every view. */
struct DockView {
 struct Window *window;
 struct Region *shape;
 struct BitMap *bitmap;
 struct RastPort rp;
 /* desk: the desktop as read, never processed; background: desk with the
  * effect and the frame, rebuilt from desk every time. */
 unsigned char *pixels,*background,*desk;
 struct DockSnapshot snap;
 int samples;  /* desktop reads around the dock in the current watch */
 int x,y;
 struct DockStyle style;
 struct DockLayout layout;
 struct Visual visual[DOCK_MAX_ITEMS];
 DockDropTarget drop;
 int hover,pressed,dirty,corner,transparent;
 /* hover, pressed and drop target as last drawn whole: an animation frame
  * redraws only the icon, which is right only while they are unchanged. */
 int drawn_marks;
 /* Effective shared settings, and the colours drawn with. nobg: icons and
  * text only, the window shaped to them (mask, one byte per pixel). lum is
  * the brightness behind the dock for automatic colours (-1 unknown). */
 struct DockAppearance look;
 const struct DockColors *c;
 int light,nobg,lum;
 unsigned char *mask,*mask_shown;
 /* Without a bar: pixels of the fixed shape that only the other state of an
  * icon (normal or selected) covers; they show the desktop picture. */
 int extra;
 /* Window size. It equals the layout unless the dock is longer than the
  * screen; then the window scrolls along the main axis (overflow). */
 int ww,wh,overflow,scroll,scroll_max;
 unsigned int scroll_at;
 /* A resize in place in progress (see resize_window): the window still has
  * the old box and outline (old_shape) until Intuition reports the change. */
 int resizing,resize_seen,from_x,from_y,deferred_lost;unsigned resize_at;struct Region *old_shape;
 /* Only part of the dock needs drawing (a widget's next animation frame):
  * part_dirty with the box, in window coordinates. dirty means everything. */
 int part_dirty;struct DockRect part;
 /* Composed (present.h): the window has an alpha surface, v->pixels carry
  * real alpha, and nothing of the desktop is read or pictured. */
 int compose;
 /* The pending part (part_dirty) covers a hover box that moved: once it is
  * drawn the window shows the current marks, as after a whole drawing. */
 int hover_part;
 /* Composed with automatic colours: when to measure the brightness behind
  * again (a new wallpaper paints a while after its notice), and how often. */
 unsigned lum_due;int lum_reads;
 /* Hover effect (hover.h): the one this dock shows (None without a bar),
  * which way Lift goes, each icon's progress 0..1000 (Lift) or the whole
  * dock's with the pointer position it follows (Magnify), when the effect
  * last moved, and the frame drawn, in layout coordinates. */
 int effect,lift_sign,progress[DOCK_MAX_ITEMS],strength,along;unsigned effect_at;
 struct DockMagnifyFrame fx;
};

struct App {
 struct DockSet set;
 struct DockView views[DOCK_MAX_DOCKS];
 struct Screen *screen;
 struct TextFont *font;
 /* Pens for text drawn by the system (tooltip): per colour variant. white
  * draws text masks that are coloured by the dock itself. */
 long pen_text[2],pen_muted[2],pen_white;
 struct BitMap *scratch;struct RastPort scratch_rp;int scratch_w,scratch_h;unsigned char *scratch_px;

 struct DockPanel panel; int panel_view,panel_dirty;
 /* An item that took dropped files shows pressed for a moment. */
 int flash_view,flash_index;unsigned int flash_until;
 /* The process's requester window before Dockbar turned DOS requesters off;
  * put back for commands it starts, and at exit. */
 APTR requesters;
 /* A drawer item's contents (Open on click: Panel). */
 struct DockDrawerPanel drawers;int drawers_view,drawers_index;unsigned int drawers_closed_at,drawers_moved_at;
 char drawers_last[DOCK_PATH];int drawers_seen;  /* last drawer shown, for the timing log */
 int log_entries;  /* --log-entries: list a drawer panel's entries when it closes */
 struct DockMenu menu; int menu_view;
 struct Window *tooltip; char tooltip_text[500];
 struct DockEditor editor; int edit_view,edit_container,edit_index,edit_new;
 struct DockPrefsWindow prefs;
 struct DockAbout about; int about_again; /* reopen after a screen change */
 struct DockItemList items;
 /* One drag state for all docks (own drags, so items can move between them,
  * and foreign ones; see DockDragState). Containers in the dwell logic are
  * coded view*16+container. */
 struct DockDragState drag; int drag_view,drag_left,drag_top,drag_x,drag_y,grip_click;
 int rebuild,reopen_view,reopen_container,quit,readonly,rebuild_asked;
 int all_selected;  /* render every icon in its selected state (shape) */
 /* Text "Smooth" (DockAppearance.text): FreeType on the system's font
  * files. smooth is 1 only while it is set up; otherwise the screen font. */
 struct DockTextRender textr; int smooth; unsigned char *font_data[2];
 /* The text setting smooth_open last tried (-1: none on this screen).
  * Smooth may be asked for and still unavailable (no font); that is not a
  * change that needs every dock rebuilt again. */
 int smooth_asked;
 /* Coverage of the last text_cover: pointer, row stride, height. */
 const unsigned char *cover; int cover_stride,cover_h,cover_left,cover_top;
 int panel_pending,pending_x,pending_y,pending_moved;
 int drop_view,drop_container;
 int cli_size,cli_corner;
 /* The screen composes windows (present.h), unless --no-compose; and the
  * docks whose window was refused a surface on this screen (by index). */
 int compose_ok,no_compose,compose_refused[DOCK_MAX_DOCKS];
 char status[100]; unsigned status_until; int status_view;
 /* Quiet ports (no signal bit, see dock_quiet_port): the loop polls them. */
 struct MsgPort *input_port,*instance_port,*notify_port,*widgets_port;
 struct IOStdReq *input_request;
 /* Screen lifecycle: the docks live on the Workbench screen only while it
  * is attached. Settings, ids and the process outlive any screen. */
 APTR notify;
 int suspended,detaches,attaches;
 /* Bumped on every attach and detach: work for an older screen is void. */
 unsigned screen_gen;
 /* Wanderer rewrites this file when its wallpaper changes. */
 struct MsgPort *wall_port; struct NotifyRequest wall_note; int wall_active;
 FILE *log;
 const char *config,*legacy_config,*legacy_prefs,*pins;
};
/* Both are far too large for the small default AROS stack. */
static struct App app;
static struct DockAppletRuntime *configure_after_rebuild;
static struct DockSet next_set;

static void note(const char *format,...) __attribute__((format(printf,1,2)));
static unsigned int milliseconds(void);
static void note(const char *format,...) {
 if(!app.log)return;
 /* Milliseconds since start: the order and spacing of screen, wallpaper
  * and picture events is what a log of this program is read for. */
 static unsigned int started;static int known;if(!known){started=milliseconds();known=1;}
 fprintf(app.log,"%u ",milliseconds()-started);
 va_list args;va_start(args,format);vfprintf(app.log,format,args);va_end(args);
 fputc('\n',app.log);fflush(app.log);
}
/* Elapsed time (elapsed.h): every interval, delay and animation below is
 * counted with it, never with the calendar clock. */
static unsigned int milliseconds(void) {
 return (unsigned int)dock_elapsed_ms();
}
/* --trace-present (diagnostics): every write of a dock's pixels to its
 * window, every window opened or closed, every refresh request, so that a
 * picture seen on screen can be matched with what Dockbar did. --dump-frames N
 * also writes the first N finished pictures of the first dock to RAM:dbf/. */
static int trace_present,dump_frames,dumped;
/* Time spent handing finished pixels to the system (the pixel transfer and
 * what the compositor does for it), apart from composing them. */
static uint64_t present_us;
static void dump_buffer(int index,const unsigned char *px,int w,int h,int x,int y,int pw,int ph) {
 if(index!=0 || dumped>=dump_frames)return;
 char name[40];snprintf(name,sizeof name,"RAM:dbf/%04d.argb",dumped++);
 FILE *f=fopen(name,"wb");if(!f)return;
 /* Header: window size, then the part written, then the whole buffer. */
 int head[6]={w,h,x,y,pw,ph};fwrite(head,sizeof head,1,f);fwrite(px,4,(size_t)w*h,f);fclose(f);
}
/* Free signal bits of this task: Zune needs several for each window it
 * opens, and every IDCMP port takes one. */
static int free_signals(void) {
 ULONG used=FindTask(NULL)->tc_SigAlloc;int n=0;
 for(int i=16;i<32;i++)if(!(used&(1UL<<i)))n++;
 return n;
}
static void feedback(int view,const char *message) {
 snprintf(app.status,sizeof app.status,"%s",message);app.status_until=milliseconds()+3000u;
 app.status_view=view>=0 && view<app.set.count?view:0;app.views[app.status_view].dirty=1;
}
static struct DockModel *model_of(int view){return &app.set.docks[view].model;}
/* A locked dock keeps its place and its contents: no adding, removing,
 * reordering, grouping or editing. Launching and opening groups stay. */
static int dock_locked(int view){return view>=0 && view<app.set.count && app.set.docks[view].place.locked;}
static struct DockItem *item_at(int view,int container,int index) {
 int n=0;struct DockItem *items=dock_items(model_of(view),container,&n);
 return items && index>=0 && index<n?&items[index]:NULL;
}
static int code(int view,int container){return view*16+container;}
static int inside(struct Window *w,int x,int y) {
 return w && x>=w->LeftEdge && y>=w->TopEdge && x<w->LeftEdge+w->Width && y<w->TopEdge+w->Height;
}
/* By geometry alone: the first dock whose window holds the point. Without a
 * bar only the drawn pixels belong to the dock; clicks between them go to
 * what is behind. A pictured dock takes clicks where its shape is
 * (mask_shown); a composed one has no shape and the system decides by the
 * pixels' alpha, so ask it. Used only where our own tooltip is in front. */
static int view_in_box(int sx,int sy) {
 for(int i=0;i<app.set.count;i++){
  struct DockView *v=&app.views[i];
  if(!inside(v->window,sx,sy))continue;
  int x=sx-v->window->LeftEdge,y=sy-v->window->TopEdge;
  if(v->nobg && v->compose){if(!dock_present_hit(v->window,sx,sy))continue;}
  else if(v->nobg && v->mask_shown && (x>=v->ww || y>=v->wh || !v->mask_shown[y*v->ww+x]))continue;
  return i;
 }
 return -1;
}
/* The dock the point is really on: the frontmost layer there, found the way
 * Intuition finds where a click goes (WhichLayer() under the Layer_Info
 * lock). So a window over a dock, or another dock in front, takes the point,
 * and so do the system's own rules for shaped windows, input regions and (on
 * mainline) fully transparent pixels of composed windows. Our tooltip never
 * takes a point from a dock: a tooltip over the pointer would otherwise close
 * and open again. Takes the lock, so never call it while a layer of ours is
 * locked (BeginRefresh); drawing uses the answer pointer_update kept. */
static int view_at(int sx,int sy) {
 const void *layers[DOCK_MAX_DOCKS];int any=0;
 for(int i=0;i<app.set.count;i++){
  struct Window *w=app.views[i].window;
  layers[i]=w?w->WLayer:NULL;any|=inside(w,sx,sy);
 }
 if(!any || !app.screen)return -1;
 struct Layer_Info *li=&app.screen->LayerInfo;
 LockLayerInfo(li);
 struct Layer *front=WhichLayer(li,sx,sy);
 UnlockLayerInfo(li);
 int at=dock_front_pick(front,layers,app.set.count,app.tooltip?app.tooltip->WLayer:NULL);
 return at==DOCK_FRONT_OWN?view_in_box(sx,sy):at;
}
/* The dock under the pointer as of the last pointer_update (this pass). */
static int under_view=-1;

/* The left button held for something that is not our own drag: typically an
 * icon dragged from Wanderer. Wanderer does not repaint its desktop where a
 * window closes during its drag (tests/expose_probe.c), so while it lasts
 * Dockbar closes none of its own windows over the desktop and opens no
 * tooltips; what is open stays until the button is released. As of the last
 * poll of the drag state (once per pass of the main loop). */
static int foreign_drag(void){return app.drag.foreign;}
/* Development builds only (scripts/build.sh dev/Dockbar-dragdiag*): 1 logs
 * how the screen inside each dock differs from what Dockbar last drew,
 * during and after a foreign drag; 2 also draws during such a drag, as
 * 0.4-test1 did, to reproduce the fragments. 0 in every shipped build. */
#ifndef DOCK_DRAGDIAG
#define DOCK_DRAGDIAG 0
#endif
static int hold_drawing(void){return DOCK_DRAGDIAG<2 && foreign_drag();}

/* ---- images ---------------------------------------------------------- */

static void image_free(struct DockImage *i) {
 dock_native_free(&i->native);dock_svg_free(i->svg);free(i->argb[0]);free(i->argb[1]);
 memset(i,0,sizeof *i);
}
static int svg_name(const char *path) {
 size_t n=path?strlen(path):0;return n>=4 && !strcasecmp(path+n-4,".svg");
}
/* An SVG that cannot be rendered (no renderer in the .info-only package, or a
 * bad file) falls back to the target's own icon rather than a placeholder. */
static void image_bake(struct DockImage *i);
static void add_command(int view);
static void image_load(struct DockImage *i,const char *path,const char *fallback,int size) {
 i->size=size;
 if(svg_name(path)){if(dock_svg_render(path,size,&i->svg)){image_bake(i);return;}if(!fallback)return;path=fallback;}
 dock_native_load(&i->native,path,app.screen,size);
 image_bake(i);
}
static void image_draw(struct DockImage *i,struct RastPort *rp,int x,int y,int selected) {
 if(i->svg)WritePixelArrayAlpha(i->svg,0,0,i->size*4,rp,x,y,i->size,i->size,0xffffffffUL);
 else if(i->native.object)dock_native_draw(&i->native,rp,x+(i->size-i->native.width)/2,y+(i->size-i->native.height)/2,selected);
}
/* Draws the picture once over black and once over white and reads both
 * back: from the difference follows each pixel's alpha, from the black
 * pass its colour. This works the same for SVG, PNG/ARGB and planar icons
 * and leaves their artwork as icon.library draws it. */
static void image_bake(struct DockImage *i) {
 int n=i->size;
 if(n<=0 || (!i->svg && !i->native.object))return;
 struct BitMap *bm=AllocBitMap(n,n,GetBitMapAttr(app.screen->RastPort.BitMap,BMA_DEPTH),BMF_CLEAR,app.screen->RastPort.BitMap);
 unsigned char *black=malloc((size_t)n*n*4),*white=malloc((size_t)n*n*4);
 if(bm && black && white){
  struct RastPort rp;InitRastPort(&rp);rp.BitMap=bm;
  for(int state=0;state<(i->native.object?2:1);state++){
   unsigned char *out=malloc((size_t)n*n*4);if(!out)break;
   FillPixelArray(&rp,0,0,(UWORD)n,(UWORD)n,0x000000);image_draw(i,&rp,0,0,state);
   ReadPixelArray(black,0,0,(UWORD)(n*4),&rp,0,0,(UWORD)n,(UWORD)n,RECTFMT_ARGB);
   FillPixelArray(&rp,0,0,(UWORD)n,(UWORD)n,0xffffff);image_draw(i,&rp,0,0,state);
   ReadPixelArray(white,0,0,(UWORD)(n*4),&rp,0,0,(UWORD)n,(UWORD)n,RECTFMT_ARGB);
   for(int k=0;k<n*n;k++){
    int spread=0;
    for(int c=1;c<4;c++){int d=white[4*k+c]-black[4*k+c];if(d>spread)spread=d;}
    int a=255-spread;if(a<0)a=0;
    out[4*k]=(unsigned char)a;
    for(int c=1;c<4;c++){int v=a?black[4*k+c]*255/a:0;out[4*k+c]=(unsigned char)(v>255?255:v);}
   }
   i->argb[state]=out;
  }
 }
 if(bm)FreeBitMap(bm);
 free(black);free(white);
}

/* ---- drawing helpers on a view's ARGB buffer ------------------------- */

static void text(struct RastPort *rp,int x,int y,const char *s,long pen) {
 SetAPen(rp,(ULONG)pen);SetDrMd(rp,JAM1);Move(rp,x,y);Text(rp,(CONST_STRPTR)s,(ULONG)strlen(s));
}
static int text_width(const char *s){
 if(app.smooth)return dock_text_width(&app.textr,s);
 return app.screen?TextLength(&app.screen->RastPort,(CONST_STRPTR)s,(ULONG)strlen(s)):0;
}
/* Longest prefix of s, with "..." when cut, that fits width pixels. */
static void fit(char *out,size_t capacity,const char *s,int width) {
 snprintf(out,capacity,"%s",s);
 if(text_width(out)<=width)return;
 for(size_t n=strlen(s);n>0;n--){snprintf(out,capacity,"%.*s...",(int)n-1,s);if(text_width(out)<=width)return;}
 out[0]=0;
}

/* ---- pens and the text scratch --------------------------------------- */

static long pen_of(struct Screen *s,unsigned long rgb) {
 ULONG r=(ULONG)((rgb>>16)&255)*0x01010101u,g=(ULONG)((rgb>>8)&255)*0x01010101u,b=(ULONG)(rgb&255)*0x01010101u;
 return ObtainBestPen(s->ViewPort.ColorMap,r,g,b,TAG_DONE);
}
/* A whole file in memory: FreeType reads faces lazily otherwise, one DOS
 * read per uncached glyph (Limpet's experience). */
static unsigned char *load_file(const char *path,size_t *size) {
 BPTR f=Open((CONST_STRPTR)path,MODE_OLDFILE);unsigned char *d=NULL;
 if(!f)return NULL;
 if(Seek(f,0,OFFSET_END)>=0){LONG n=Seek(f,0,OFFSET_BEGINNING);
  if(n>0 && n<16*1024*1024 && (d=malloc((size_t)n)) && Read(f,d,n)!=n){free(d);d=NULL;}
  *size=(size_t)n;}
 Close(f);return d;
}
static void smooth_close(void) {
 if(app.smooth)dock_text_close(&app.textr);
 app.smooth=0;free(app.font_data[0]);free(app.font_data[1]);app.font_data[0]=app.font_data[1]=NULL;
}
/* Smooth text: the TrueType file behind the screen font when that is an
 * outline font (its .otag names it), else the system's Vera Sans; Open Sans
 * of the system after it for letters the first lacks. Same line height as
 * the screen font, so layouts do not change size. Any failure leaves the
 * screen font in use. */
static void smooth_open(struct Screen *s) {
 char otag[300],file[256]="",charset[40]="";size_t sizes[2]={0,0},n=0;
 smooth_close();
 app.smooth_asked=app.set.defaults.text;
 if(!app.set.defaults.text)return;
 struct TextAttr *ta=s->Font;
 unsigned char *tag=ta && dock_otag_path((const char *)ta->ta_Name,otag,sizeof otag)?load_file(otag,&n):NULL;
 if(tag && !dock_otag_font_file(tag,n,file,sizeof file))file[0]=0;
 free(tag);
 app.font_data[0]=load_file(file[0]?file:"FONTS:TrueType/VeraSans.ttf",&sizes[0]);
 app.font_data[1]=load_file("FONTS:TrueType/OpenSans-Regular.ttf",&sizes[1]);
 GetVar((CONST_STRPTR)"CHARSET",(STRPTR)charset,sizeof charset,0);
 const unsigned char *faces[2]={app.font_data[0],app.font_data[1]};
 int height=app.font->tf_YSize+2;
 app.smooth=dock_text_open(&app.textr,faces,sizes,2,height,strstr(charset,"8859-2")?DOCK_CODESET_LATIN2:DOCK_CODESET_LATIN1);
 if(app.smooth && !dock_text_fit_height(&app.textr,height)){dock_text_close(&app.textr);app.smooth=0;}
 note("TEXT %s file=%s faces=%d size=%d charset=%s",app.smooth?"smooth":"system (smooth unavailable)",
      file[0]?file:"FONTS:TrueType/VeraSans.ttf",app.smooth?app.textr.faces:0,app.smooth?app.textr.pixel_size:0,charset[0]?charset:"default");
 if(!app.smooth)smooth_close();
}
static void release_screen_things(struct Screen *s) {
 smooth_close();app.smooth_asked=-1;
 long *pens[]={&app.pen_text[0],&app.pen_text[1],&app.pen_muted[0],&app.pen_muted[1],&app.pen_white};
 for(unsigned i=0;i<sizeof pens/sizeof *pens;i++){if(*pens[i]>=0)ReleasePen(s->ViewPort.ColorMap,(ULONG)*pens[i]);*pens[i]=-1;}
 if(app.scratch){FreeBitMap(app.scratch);app.scratch=NULL;}
 free(app.scratch_px);app.scratch_px=NULL;
}
/* Text for the dock is drawn white on black into a scratch line, read back
 * as coverage and coloured by the dock: one path for every colour variant,
 * outlines and the shape of a dock without a bar. */
static int get_screen_things(struct Screen *s) {
 for(int v=0;v<2;v++){app.pen_text[v]=pen_of(s,dock_colors(v)->text);app.pen_muted[v]=pen_of(s,dock_colors(v)->text_muted);}
 app.pen_white=pen_of(s,0xffffff);
 app.scratch_w=s->Width;app.scratch_h=s->RastPort.Font->tf_YSize+2;
 app.scratch=AllocBitMap(app.scratch_w,app.scratch_h,GetBitMapAttr(s->RastPort.BitMap,BMA_DEPTH),BMF_CLEAR,s->RastPort.BitMap);
 app.scratch_px=malloc((size_t)app.scratch_w*app.scratch_h*4);
 if(app.scratch){InitRastPort(&app.scratch_rp);app.scratch_rp.BitMap=app.scratch;SetFont(&app.scratch_rp,s->RastPort.Font);}
 int ok=app.scratch && app.scratch_px && app.pen_white>=0;
 for(int v=0;v<2;v++)ok=ok && app.pen_text[v]>=0 && app.pen_muted[v]>=0;
 if(!ok){release_screen_things(s);note("FAIL pens or text scratch");}
 if(ok)smooth_open(s);
 return ok;
}

/* ---- composing into a view's ARGB buffer ------------------------------ */

/* In a dock without a bar only what is drawn belongs to the window: every
 * drawing call marks its opaque pixels in v->mask. */
/* Drawing is limited to this box while a part of a dock is redrawn. */
static int clip_x0,clip_y0,clip_x1=1<<30,clip_y1=1<<30;
static void put(struct DockView *v,int x,int y,unsigned long color,int alpha) {
 if(x<0 || x>=v->ww || y<0 || y>=v->wh || alpha<=0)return;
 if(x<clip_x0 || x>=clip_x1 || y<clip_y0 || y>=clip_y1)return;
 unsigned char *p=v->pixels+4*(y*v->ww+x);
 if(v->compose){
  /* The compositor blends what is under the window: real alpha, edges
   * included. The mask still makes the fixed shape of a dock without a bar. */
  if(v->nobg && alpha>=128)v->mask[y*v->ww+x]=1;
  if(alpha>255)alpha=255;
  dock_over(p,color,alpha);return;
 }
 if(v->nobg){
  /* Nothing of ours is behind: half-covered pixels are either in or out. */
  if(alpha<128)return;
  alpha=255;v->mask[y*v->ww+x]=1;
 }
 unsigned long behind=((unsigned long)p[1]<<16)|((unsigned long)p[2]<<8)|p[3];
 unsigned long c=alpha>=255?color:dock_mix(behind,color,alpha/255.0f);
 p[0]=255;p[1]=(unsigned char)(c>>16);p[2]=(unsigned char)(c>>8);p[3]=(unsigned char)c;
}
/* Text coverage of s in app.cover (one byte per pixel, app.cover_stride
 * per row, app.cover_h rows); returns its width, 0 when nothing can be
 * drawn. The text's origin (left end, top of the line box) is at
 * (cover_left, cover_top) in it: smooth text may have ink before and above
 * it; the screen font's has none. */
static int text_cover(const char *s) {
 if(app.smooth){
  struct DockTextImage im;const unsigned char *c=dock_text_render(&app.textr,s,&im);
  if(!c)return 0;
  app.cover=c;app.cover_stride=im.w;app.cover_h=im.h;app.cover_left=im.left;app.cover_top=im.top;return im.w;
 }
 int w=text_width(s),h=app.scratch_h;
 app.cover=app.scratch_px;app.cover_stride=app.scratch_w;app.cover_h=h;app.cover_left=app.cover_top=0;
 if(!app.scratch || w<=0)return 0;
 if(w>app.scratch_w)w=app.scratch_w;
 FillPixelArray(&app.scratch_rp,0,0,(UWORD)w,(UWORD)h,0);
 SetAPen(&app.scratch_rp,(ULONG)app.pen_white);SetDrMd(&app.scratch_rp,JAM1);
 Move(&app.scratch_rp,0,app.font->tf_Baseline+1);Text(&app.scratch_rp,(CONST_STRPTR)s,(ULONG)strlen(s));
 if(ReadPixelArray(app.scratch_px,0,0,(UWORD)(app.scratch_w*4),&app.scratch_rp,0,0,(UWORD)w,(UWORD)h,RECTFMT_ARGB)!=(ULONG)(w*h))return 0;
 for(int j=0;j<h;j++)for(int i=0;i<w;i++){
  unsigned char *p=app.scratch_px+4*(j*app.scratch_w+i);
  int c=p[1]>p[2]?p[1]:p[2];if(p[3]>c)c=p[3];
  app.scratch_px[j*app.scratch_w+i]=(unsigned char)c;  /* safe: byte i <= pixel 4*i */
 }
 return w;
}
/* Text with its top-left at x,y. Without a bar it gets an outline in the
 * opposite tone, so it reads on any desktop. */
static void put_text(struct DockView *v,int x,int y,const char *s,unsigned long color) {
 int w=text_cover(s),h=app.cover_h;
 if(!w)return;
 /* Smooth text has its own line box: centre it on the screen font's,
  * then step back to where its image starts. */
 x-=app.cover_left;
 y+=(app.smooth?(app.scratch_h-dock_text_height(&app.textr))/2-1:-1)-app.cover_top;
 #define COVER(i,j) ((i)<0 || (j)<0 || (i)>=w || (j)>=h?0:app.cover[(j)*app.cover_stride+(i)])
 if(v->nobg)for(int j=-1;j<=h;j++)for(int i=-1;i<=w;i++){
  int around=0;
  for(int b=-1;b<=1;b++)for(int a=-1;a<=1;a++){int c=COVER(i+a,j+b);if(c>around)around=c;}
  if(around>=128 && COVER(i,j)<128)put(v,x+i,y+j,v->c->outline,255);
 }
 for(int j=0;j<h;j++)for(int i=0;i<w;i++)put(v,x+i,y+j,color,COVER(i,j));
 #undef COVER
}
/* Rounded box; edges blend with the bar, or are cut without one. */
static void box(struct DockView *v,int x,int y,int w,int h,int radius,unsigned long color) {
 int j0=clip_y0>y?clip_y0-y:0,j1=clip_y1-y<h?clip_y1-y:h,i0=clip_x0>x?clip_x0-x:0,i1=clip_x1-x<w?clip_x1-x:w;
 for(int j=j0;j<j1;j++)for(int i=i0;i<i1;i++){
  float c=dock_round_coverage(i,j,w,h,radius);
  if(c>0)put(v,x+i,y+j,color,(int)(c*255+0.5f));
 }
}
/* Icons were read back with alpha (see image_bake). */
static void put_image(struct DockView *v,struct DockImage *img,int x,int y,int selected) {
 unsigned char *a=img->argb[selected && img->argb[1]?1:0];
 if(!a)return;
 for(int j=0;j<img->size;j++)for(int i=0;i<img->size;i++){
  unsigned char *p=a+4*(j*img->size+i);
  if(p[0])put(v,x+i,y+j,((unsigned long)p[1]<<16)|((unsigned long)p[2]<<8)|p[3],p[0]);
 }
}

/* ---- Magnify pictures ----------------------------------------------- */

/* Everything Magnify keeps, in all docks together: the large sources take at
 * most three quarters, the scaled copies the rest; the copy used longest ago
 * goes first. A source that does not fit leaves its icon unmagnified. */
#define DOCK_MAG_BUDGET (4u<<20)
static size_t mag_bytes;static unsigned mag_clock;
static size_t image_bytes(const struct DockImage *i){return (size_t)i->size*i->size*4*((i->argb[0]?1:0)+(i->argb[1]?1:0));}
static void mag_free(struct DockMag *m) {
 for(int k=0;k<DOCK_MAG_STEPS;k++)for(int s=0;s<2;s++)if(m->step[k][s]){free(m->step[k][s]);mag_bytes-=(size_t)m->size[k]*m->size[k]*4;}
 mag_bytes-=image_bytes(&m->source);image_free(&m->source);
 memset(m,0,sizeof *m);
}
/* Frees the copy used longest ago, other than 'keep'; 0 when there is none. */
static int mag_evict(const unsigned char *keep) {
 struct DockMag *best=NULL;int bk=0,bs=0;unsigned oldest=0;
 for(int v=0;v<DOCK_MAX_DOCKS;v++)for(int i=0;i<DOCK_MAX_ITEMS;i++){
  struct DockMag *m=&app.views[v].visual[i].mag;
  for(int k=0;k<DOCK_MAG_STEPS;k++)for(int s=0;s<2;s++)
   if(m->step[k][s] && m->step[k][s]!=keep && (!best || mag_clock-m->used[k][s]>oldest)){best=m;bk=k;bs=s;oldest=mag_clock-m->used[k][s];}
 }
 if(!best)return 0;
 free(best->step[bk][bs]);best->step[bk][bs]=NULL;mag_bytes-=(size_t)best->size[bk]*best->size[bk]*4;
 return 1;
}
/* The picture for an icon box of 'box' grown by 'grow': the source itself at
 * the largest size, else a copy scaled down from it once and kept. NULL
 * (draw the resting picture) without a source or when scaling fails. */
static unsigned char *mag_picture(struct DockMag *m,int box,int grow,int selected,int *size) {
 int k=grow/DOCK_MAGNIFY_STEP,s=selected && m->source.argb[1]?1:0,want=box+grow;
 if(k<=0 || k>=DOCK_MAG_STEPS || !m->source.argb[s] || want>m->source.size)return NULL;
 *size=want;
 if(want==m->source.size)return m->source.argb[s];
 if(!m->step[k][s] || m->size[k]!=want){
  size_t need=(size_t)want*want*4;
  if(m->step[k][s]){free(m->step[k][s]);m->step[k][s]=NULL;mag_bytes-=(size_t)m->size[k]*m->size[k]*4;}
  /* The other state's copy of this step is of the old size too. */
  if(m->step[k][!s] && m->size[k]!=want){free(m->step[k][!s]);m->step[k][!s]=NULL;mag_bytes-=(size_t)m->size[k]*m->size[k]*4;}
  while(mag_bytes+need>DOCK_MAG_BUDGET && mag_evict(NULL)){}
  if(mag_bytes+need>DOCK_MAG_BUDGET)return NULL;
  if(!dock_resample_argb(m->source.argb[s],m->source.size,m->source.size,want,want,&m->step[k][s]))return NULL;
  m->size[k]=want;mag_bytes+=need;
 }
 m->used[k][s]=++mag_clock;
 return m->step[k][s];
}
static void put_argb(struct DockView *v,const unsigned char *a,int size,int x,int y) {
 for(int j=0;j<size;j++)for(int i=0;i<size;i++){
  const unsigned char *p=a+4*(j*size+i);
  if(p[0])put(v,x+i,y+j,((unsigned long)p[1]<<16)|((unsigned long)p[2]<<8)|p[3],p[0]);
 }
}

/* ---- background: frame, optional see-through desktop picture --------- */

/* Box blur, rows then columns, run twice: close enough to soft glass. */
static void blur(unsigned char *p,int w,int h,int radius) {
 unsigned char *line=malloc((size_t)(w>h?w:h)*4);if(!line)return;
 for(int pass=0;pass<2;pass++)for(int axis=0;axis<2;axis++){
  int n=axis?h:w,lines=axis?w:h;
  for(int l=0;l<lines;l++){
   for(int c=1;c<4;c++){
    int sum=0,count=0;
    for(int k=0;k<=radius && k<n;k++){sum+=p[4*(axis?k*w+l:l*w+k)+c];count++;}
    for(int k=0;k<n;k++){
     line[4*k+c]=(unsigned char)(sum/count);
     int out=k-radius,in=k+radius+1;
     if(out>=0){sum-=p[4*(axis?out*w+l:l*w+out)+c];count--;}
     if(in<n){sum+=p[4*(axis?in*w+l:l*w+in)+c];count++;}
    }
   }
   for(int k=0;k<n;k++)for(int c=1;c<4;c++)p[4*(axis?k*w+l:l*w+k)+c]=line[4*k+c];
  }
 }
 free(line);
}
/* Docks that need the desktop behind them: a see-through bar shows a
 * picture of it, a dock without a bar only needs its brightness for
 * automatic colours. */
static int view_transparent(int index){struct DockView *v=&app.views[index];return !v->compose && v->desk && (v->nobg || v->look.transparency>0);}
/* Colours follow the theme choice and, when automatic, the brightness
 * behind the dock, measured when the dock opens and after each refresh
 * event, never per pointer move. An opaque bar has nothing behind it that
 * shows, so automatic is dark there. */
static void choose_colors(int index) {
 struct DockView *v=&app.views[index];
 int was=v->light;
 v->light=dock_theme_light(v->look.theme,v->lum,v->light);
 v->c=dock_colors(v->light);
 if(was!=v->light && was>=0)note("COLORS dock=%d %s luminance=%d",app.set.docks[index].id,v->light?"light":"dark",v->lum);
}
/* The frame over the desktop picture, or the plain frame while there is no
 * usable picture of this screen (none yet, or the read failed). A dock
 * without a bar has no background at all. */
static void compose_background(int index) {
 struct DockView *v=&app.views[index];const struct DockAppearance *look=&v->look;
 int w=v->ww,h=v->wh;
 float fill_alpha=1.0f-look->transparency/100.0f;
 if(v->compose){
  /* The desktop shows through the window itself: the bar is drawn once with
   * its own alpha, and a dock without a bar starts clear. The brightness for
   * automatic colours is read once (compose_luminance), not pictured. */
  v->transparent=!v->nobg && look->transparency>0;
  choose_colors(index);
  v->dirty=1;
  if(v->nobg){memset(v->background,0,(size_t)w*h*4);return;}
  struct DockFrameStyle style={v->c->fill_top,v->c->fill_bottom,v->c->border_top,v->c->border_bottom};
  for(int j=0;j<h;j++)for(int i=0;i<w;i++){
   unsigned char *p=v->background+4*(j*w+i);
   unsigned long c=dock_frame_pixel(&style,i,j,w,h,v->corner);
   /* The border stays visible however clear the fill is; the rounded
    * corners are the alpha's, smooth, since the window is not shaped. */
   float a=dock_frame_border(i,j,w,h,v->corner)*0.85f;if(a<fill_alpha)a=fill_alpha;
   a*=dock_round_coverage(i,j,w,h,v->corner);
   p[0]=(unsigned char)(a*255+0.5f);p[1]=(unsigned char)(c>>16);p[2]=(unsigned char)(c>>8);p[3]=(unsigned char)c;
  }
  return;
 }
 v->transparent=!v->nobg && look->transparency>0 && v->snap.valid;
 if(!v->nobg)v->lum=v->transparent?dock_luminance(v->desk,NULL,w*h):-1;
 choose_colors(index);
 v->dirty=1;
 if(v->nobg)return;
 struct DockFrameStyle style={v->c->fill_top,v->c->fill_bottom,v->c->border_top,v->c->border_bottom};
 if(v->transparent){
  memcpy(v->background,v->desk,(size_t)w*h*4);
  if(look->glass){
   blur(v->background,w,h,5);
   for(int i=0;i<w*h;i++)for(int c=1;c<4;c++)v->background[4*i+c]=(unsigned char)(v->background[4*i+c]+(255-v->background[4*i+c])/8);
  }
 }
 for(int j=0;j<h;j++)for(int i=0;i<w;i++){
  unsigned char *p=v->background+4*(j*w+i);
  unsigned long c=dock_frame_pixel(&style,i,j,w,h,v->corner);
  if(v->transparent){
   /* The border stays visible however clear the fill is. */
   float a=dock_frame_border(i,j,w,h,v->corner)*0.85f;if(a<fill_alpha)a=fill_alpha;
   unsigned long behind=((unsigned long)p[1]<<16)|((unsigned long)p[2]<<8)|p[3];
   c=dock_mix(behind,c,a);
  }
  p[0]=255;p[1]=(unsigned char)(c>>16);p[2]=(unsigned char)(c>>8);p[3]=(unsigned char)c;
 }
}

/* ---- views ------------------------------------------------------------ */

static int effective_icon(int index){return app.cli_size?app.cli_size:app.set.docks[index].look.icon;}
static int screen_top(void){return app.screen->BarHeight+1;}
/* LockPubScreen(NULL) opens the Workbench screen when there is none, and
 * OpenWorkBench then sends SNOTIFY_AFTER_OPENWB and waits for every
 * subscriber's reply (intuition_misc.c, WaitPort without a time limit).
 * From our own task that waits for us: Dockbar started from S:User-Startup
 * before Wanderer stopped there for good. So once subscribed, Dockbar only
 * locks the default public screen when it is already open; a closed one is
 * waited for through its open notice. Public screens do not finish closing
 * meanwhile: CloseWorkBench waits for our reply to its notice first. */
static struct Screen *lock_open_screen(void) {
 char name[MAXPUBSCREENNAME+1];int open=0;
 GetDefaultPubScreen((UBYTE*)name);
 struct List *list=LockPubScreenList();
 for(struct Node *n=list->lh_Head;n->ln_Succ;n=n->ln_Succ)
  if(n->ln_Name && !strcasecmp(n->ln_Name,name) && !(((struct PubScreenNode*)n)->psn_Flags&PSNF_PRIVATE)){open=1;break;}
 UnlockPubScreenList();
 return open?LockPubScreen(NULL):NULL;
}
/* WA_PubScreen windows must be opened with the public screen locked; the
 * lock is dropped once the window exists, which keeps the screen alive. */
static int lock_screen(void) {
 if(!app.screen)return 0;
 struct Screen *s=lock_open_screen();
 if(s && s!=app.screen){UnlockPubScreen(NULL,s);return 0;}
 return s!=NULL;
}
static void unlock_screen(void){UnlockPubScreen(NULL,app.screen);}
/* The text block a clock draws: its widest possible line, one or two lines. */
static void widget_size(const char *settings,int *w,int *h) {
 struct DockWidget wd;int th=app.font->tf_YSize,lines=1;
 /* Measured with the widest digit and the real month names: the shape of a
  * dock without a bar is fixed, so no later text may be wider. */
 char digit='0',text[DOCK_WIDGET_TEXT];int widest=0;
 for(char c='0';c<='9';c++){char one[2]={c,0};int t=text_width(one);if(t>widest){widest=t;digit=c;}}
 snprintf(text,sizeof text,"%c%c:%c%c",digit,digit,digit,digit);
 *w=text_width(text);
 if(dock_widget_start(&wd,settings)){
  if(wd.options&DOCK_CLOCK_SECONDS){snprintf(text,sizeof text,"%c%c:%c%c:%c%c",digit,digit,digit,digit,digit,digit);*w=text_width(text);}
  if(wd.options&DOCK_CLOCK_DATE){
   for(int m=0;m<12;m++){
    struct tm tm;memset(&tm,0,sizeof tm);tm.tm_mday=28;tm.tm_mon=m;tm.tm_year=126;
    char month[16];
    if(!strftime(month,sizeof month,"%b",&tm))continue;
    snprintf(text,sizeof text,"%c%c %s",digit,digit,month);
    int t=text_width(text);if(t>*w)*w=t;
   }
   lines=2;
  }
 }
 dock_widget_stop(&wd);
 *h=lines*th+(lines-1)*2;
}
/* How an item shows: its own choice, else its dock's. Only launchers,
 * drawers and groups can be text. */
static int item_present(const struct DockInstance *d,const struct DockItem *item) {
 if(!dock_kind_shown(item->kind))return DOCK_PRESENT_ICON;
 return item->present?item->present:d->look.present+1;
}
/* The caption of a text button, shortened to DOCK_TEXT_MAX; returns 1 when
 * it had to be shortened (the full name then shows as a tooltip). */
static int item_caption(const struct DockItem *item,char *out,size_t capacity) {
 const char *name=item->label;
 if(!name[0]){name=strrchr(item->path,'/');if(!name)name=strrchr(item->path,':');name=name?name+1:item->path;}
 if(!name[0])name=item->kind==DOCK_GROUP?"Group":item->path;
 fit(out,capacity,name,DOCK_TEXT_MAX);
 return strcmp(out,name)!=0;
}
/* A widget item: its 32 px icon, a gap, then its short value. The room for
 * the value is fixed (one temperature with a minus sign, three digits and
 * the unit), so neither new data nor a state word changes the dock's size;
 * the pair is centred in it, so the highlight has the same margin on both
 * sides. Longer texts are cut; the tooltip has them in full. */
#define APPLET_TEXT_X 38
static int applet_text_room(void) {
 int digit=0,unit=text_width(" C"),f=text_width(" F");
 for(char d[2]="0";d[0]<='9';d[0]++){int w=text_width(d);if(w>digit)digit=w;}
 if(f>unit)unit=f;
 int minus=text_width("-")+2*digit,three=3*digit;
 return (minus>three?minus:three)+unit;
}
static void style_for(const struct DockSet *set,int index,int icon,struct DockStyle *s) {
 const struct DockInstance *d=&set->docks[index];struct DockAppearance a;
 dock_appearance_of(set,index,&a);
 memset(s,0,sizeof *s);
 s->icon=icon;s->orientation=d->look.orientation;s->spacing=d->look.spacing;s->labels=d->look.labels;
 s->text_height=app.font->tf_YSize;s->hide_grip=!a.grip;s->beside=d->look.beside;
 for(int i=0;i<d->model.count;i++){
  const struct DockItem *item=&d->model.items[i];
  if(item->kind==DOCK_CLOCK)widget_size(item->path,&s->content_w[i],&s->content_h[i]);
  if(item->kind==DOCK_APPLET){
   /* An action widget shows its icon only (no value beside it). */
   const struct DockAppletPackage *pkg=dock_applet_find(dock_applets_catalog(),item->path);
   s->content_w[i]=pkg && pkg->available && pkg->manual?32:APPLET_TEXT_X+applet_text_room();
   s->content_h[i]=app.font->tf_YSize;if(s->content_h[i]<32)s->content_h[i]=32;}
  s->present[i]=item_present(d,item);
  if(s->present[i]!=DOCK_PRESENT_ICON){
   char caption[DOCK_LABEL+4];item_caption(item,caption,sizeof caption);
   int w=text_width(caption);s->text_w[i]=w<1?1:w>DOCK_TEXT_MAX?DOCK_TEXT_MAX:w;
  }
 }
}
/* Layout results are large; keep this one off the stack too. */
static struct DockLayout fit_layout;
static int fits(const struct DockSet *set,int index,int icon) {
 struct DockStyle s;const struct DockInstance *d=&set->docks[index];
 style_for(set,index,icon,&s);
 if(!dock_layout(&d->model,&s,&fit_layout))return 0;
 return fit_layout.width<=app.screen->Width-16 && fit_layout.height<=app.screen->Height-screen_top()-8;
}
static void close_tooltip(void) {
 if(app.tooltip){if(trace_present)note("WIN close tooltip %p",(void*)app.tooltip);CloseWindow(app.tooltip);app.tooltip=NULL;}app.tooltip_text[0]=0;
}
static void close_drawers(void) {
 if(!app.drawers.window)return;
 if(app.drawers_view>=0 && app.drawers_view<app.set.count)app.views[app.drawers_view].dirty=1;
 if(app.log){
  const struct DockDrawerPanel *p=&app.drawers;
  note("DRAWER closing path=%s count=%d reads=%lu stale_reads=%lu scanning=%d",p->path,p->count,p->reads,p->stale_reads,p->scanning);
  if(app.log_entries)for(int i=0;i<p->count;i++)note("DRAWER entry %s%s",p->entries[i].name,p->entries[i].is_dir?"/":"");
 }
 dock_drawer_close(&app.drawers);app.drawers_closed_at=milliseconds();
}
static void close_panel(void) {
 close_drawers();
 app.panel_pending=0;
 if(app.panel.window && app.panel_view>=0 && app.panel_view<app.set.count)app.views[app.panel_view].dirty=1;
 dock_panel_close(&app.panel);app.panel_view=-1;
}
static void close_window_of(struct DockView *v);
static void free_view(struct DockView *v);
/* Buttons pressed while a dock was being resized, by screen position; they
 * are handled once the dock has its new box and picture. When they do not
 * fit, all of that dock's are dropped and the interaction is cancelled after
 * the resize: replaying a press without its release would leave an item
 * pressed or a drag running. */
static struct {int view;UWORD code,qualifier;int sx,sy;} deferred[8];static int deferred_count;
static void close_view(int index) {
 if(app.panel_view==index)close_panel();
 if(app.menu_view==index){dock_menu_close(&app.menu,app.screen);app.menu_view=-1;}
 free_view(&app.views[index]);
}
/* Everything a view owns, the window included. */
static void free_view(struct DockView *v) {
 close_window_of(v);
 if(v->shape){DisposeRegion(v->shape);v->shape=NULL;}
 if(v->old_shape){DisposeRegion(v->old_shape);v->old_shape=NULL;}
 if(v->resizing){int index=(int)(v-app.views),kept=0;
  for(int k=0;k<deferred_count;k++)if(deferred[k].view!=index)deferred[kept++]=deferred[k];
  deferred_count=kept;}
 v->resizing=v->deferred_lost=0;
 for(int i=0;i<DOCK_MAX_ITEMS;i++){
  struct Visual *vis=&v->visual[i];
  dock_fade_clear(&vis->fade);vis->fade_pending=0;vis->signature=0;dock_sprite_free(&vis->sprite);image_free(&vis->image);for(int j=0;j<4;j++)image_free(&vis->mini[j]);
  mag_free(&vis->mag);
  dock_widget_stop(&vis->widget);vis->applet=NULL;vis->applet_icon[0]=0;vis->minis=vis->use_minis=0;
 }
 if(v->bitmap){FreeBitMap(v->bitmap);v->bitmap=NULL;}
 free(v->pixels);v->pixels=NULL;free(v->background);v->background=NULL;free(v->desk);v->desk=NULL;
 free(v->mask);v->mask=NULL;free(v->mask_shown);v->mask_shown=NULL;
}
static void applet_image_load(struct Visual *vis) {
 char asset[1024];struct DockSpriteSpec spec={0};
 dock_sprite_free(&vis->sprite);image_free(&vis->image);
 if(dock_applet_sprite_path(vis->applet,asset,sizeof asset,&spec)&&
    dock_sprite_load(&vis->sprite,asset,&spec,32)){
  vis->sprite.started=dock_applets_now();
  note("SPRITE loaded icon=%s frames=%d fps=%d size=32",vis->applet->state.snapshot.icon,spec.count,spec.fps);
 }else if(dock_applet_icon_path(vis->applet,asset,sizeof asset))image_load(&vis->image,asset,NULL,32);
 snprintf(vis->applet_icon,sizeof vis->applet_icon,"%s",vis->applet->state.snapshot.icon);
}
static void draw_view(int index);
static void render_view(int index);
static struct Region *mask_region(struct DockView *v,const unsigned char *mask);
static unsigned long desk_ring(int x,int y,int w,int h);
/* Everything but the window: geometry, the desktop picture and the icons.
 * open_all reads every desktop picture before any dock window opens, so
 * that docks do not end up in each other's background. */
/* Views being replaced in an update (update_docks): their loaded pictures
 * are moved to the new view instead of loading them again, and a dock that
 * keeps its size reuses its desktop picture. Empty outside an update. */
static struct DockView *donor_view[DOCK_MAX_DOCKS];
static const struct DockModel *donor_model[DOCK_MAX_DOCKS];
static const struct DockView *desk_donor;
static int no_desk_read;
static int take_visual(const struct DockItem *item,int size,struct Visual *vis) {
 for(int d=0;d<DOCK_MAX_DOCKS;d++){
  struct DockView *old=donor_view[d];const struct DockModel *m=donor_model[d];
  if(!old || !m || old->style.icon!=size)continue;
  for(int k=0;k<m->count;k++){
   const struct DockItem *o=&m->items[k];struct Visual *ov=&old->visual[k];
   if(o->kind!=item->kind || strcmp(o->path,item->path) || strcmp(o->icon,item->icon) || o->run!=item->run)continue;
   if(item->kind==DOCK_APPLET){
    if(strcmp(o->dir,item->dir) || !ov->applet)continue;
    vis->sprite=ov->sprite;vis->image=ov->image;memcpy(vis->applet_icon,ov->applet_icon,sizeof vis->applet_icon);
    memset(&ov->sprite,0,sizeof ov->sprite);memset(&ov->image,0,sizeof ov->image);ov->applet=NULL;
    return 1;
   }
   if(!ov->image.svg && !ov->image.native.object)continue;
   vis->image=ov->image;memset(&ov->image,0,sizeof ov->image);
   /* Magnify's pictures too; prepare_view checks that they still fit. */
   vis->mag=ov->mag;memset(&ov->mag,0,sizeof ov->mag);
   return 1;
  }
 }
 return 0;
}
/* Where an item's picture comes from, and what to show when that fails.
 * Only an icon the user chose (.info or .svg) replaces an item's own: drawers
 * show their own icon, SYS:Tools included, so installing the SVG renderer
 * never changes how the user's items look. A command shows its own icon,
 * its script's, or the Shell's. A group's own picture has no fallback (its
 * children are previewed instead). */
static void picture_of(const struct DockItem *item,const char **path,const char **fallback) {
 if(item->kind==DOCK_GROUP){*path=item->icon;*fallback=NULL;return;}
 *path=item->icon[0]?item->icon:
  item->kind==DOCK_COMMAND?(item->run&DOCK_RUN_SCRIPT?item->path:"SYS:System/Shell"):item->path;
 *fallback=item->kind==DOCK_COMMAND?"SYS:System/Shell":item->path;
}
/* Magnify's sources: each plain icon once more, at the largest size its slot
 * allows, loaded here with the dock and never while the pointer moves. One
 * kept from the old view is used when it has that size. */
static void mag_prepare(int index) {
 struct DockView *v=&app.views[index];struct DockModel *m=model_of(index);int loaded=0,kept=0,skipped=0;
 for(int i=0;i<m->count;i++){
  struct Visual *vis=&v->visual[i];struct DockMag *mag=&vis->mag;
  int side=dock_magnify_room(&v->layout,&v->style,i).w;
  int wanted=v->effect==DOCK_HOVER_MAGNIFY && side>v->style.icon && !vis->use_minis && (vis->image.svg || vis->image.native.object);
  if(mag->source.size && (!wanted || mag->source.size!=side))mag_free(mag);
  if(!wanted)continue;
  if(mag->source.size){kept++;continue;}
  size_t need=(size_t)side*side*4*2;
  if(mag_bytes+need>DOCK_MAG_BUDGET/4*3){skipped++;continue;}
  const char *path,*fallback;picture_of(&m->items[i],&path,&fallback);
  image_load(&mag->source,path,fallback,side);
  mag_bytes+=image_bytes(&mag->source);loaded++;
 }
 if(v->effect==DOCK_HOVER_MAGNIFY)note("MAGNIFY dock=%d sources loaded=%d kept=%d over_budget=%d bytes=%lu",app.set.docks[index].id,loaded,kept,skipped,(unsigned long)mag_bytes);
}
/* Automatic colours of a composed see-through dock need the brightness
 * behind it. The screen's own bitmap holds the desktop without composed
 * windows, ours included, so one read where the dock goes is enough; it is
 * only measured, never shown. */
static void compose_luminance(int index) {
 struct DockView *v=&app.views[index];
 if(v->look.theme!=DOCK_THEME_AUTO || (!v->nobg && v->look.transparency<=0))return;
 /* Where the window is now: a dock moved by its grip keeps v->x/v->y of
  * its placement until it is closed. Read one row band at a time into a
  * small buffer; the average is all that is kept. */
 int x=v->window?v->window->LeftEdge:v->x,y=v->window?v->window->TopEdge:v->y;
 unsigned char row[4*256];double sum=0;long n=0;ULONG got=0;
 for(int j=0;j<v->wh;j+=4)for(int i=0;i<v->ww;i+=256){
  int w=v->ww-i<256?v->ww-i:256;
  ULONG r=ReadPixelArray(row,0,0,(UWORD)(w*4),&app.screen->RastPort,x+i,y+j,(UWORD)w,1,RECTFMT_ARGB);
  got+=r;
  if(r==(ULONG)w){int l=dock_luminance(row,NULL,w);sum+=(double)l*w;n+=w;}
 }
 if(n)v->lum=(int)(sum/n+0.5);
 note("COMPOSE dock=%d luminance=%d at=%d,%d pixels=%lu",app.set.docks[index].id,v->lum,x,y,(unsigned long)got);
}
/* The outline of an ordinary (pictured) dock window: rounded for a bar;
 * without a bar what it can draw, in either state of its icons, fixed while
 * the dock is open. */
static struct Region *pictured_shape(int index) {
 struct DockView *v=&app.views[index];
 if(!v->nobg)return dock_shape(v->ww,v->wh,v->corner);
 int n=v->ww*v->wh;
 v->extra=0;
 render_view(index);memcpy(v->mask_shown,v->mask,(size_t)n);
 app.all_selected=1;render_view(index);app.all_selected=0;
 for(int k=0;k<n;k++)if(v->mask[k]!=v->mask_shown[k]){v->mask_shown[k]=1;v->extra++;}
 if(v->extra)note("NOBG dock=%d %d pixels only in one state (icon or button)",app.set.docks[index].id,v->extra);
 return mask_region(v,v->mask_shown);
}
static int prepare_view(int index) {
 struct DockView *v=&app.views[index];struct DockInstance *d=&app.set.docks[index];
 int icon=effective_icon(index),x,y;
 memset(v,0,sizeof *v);v->hover=v->pressed=-1;
 v->corner=app.cli_corner>=0?app.cli_corner:d->look.corner;
 /* Policy for a dock that does not fit: smaller icons, down to the minimum,
  * for this run only; the saved size is kept. */
 dock_appearance_of(&app.set,index,&v->look);
 v->nobg=v->look.background==DOCK_BG_NONE;v->light=-1;v->lum=-1;
 v->compose=app.compose_ok && !app.compose_refused[index];
 while(icon>DOCK_ICON_MIN && !fits(&app.set,index,icon))icon--;
 if(icon!=effective_icon(index))feedback(index,"Icons made smaller to fit the screen");
 style_for(&app.set,index,icon,&v->style);
 if(!dock_layout(&d->model,&v->style,&v->layout))return 0;
 /* Still too long at the smallest icons: show a scrolling window onto the
  * whole dock rather than cut items off. The saved layout is unchanged. */
 v->ww=v->layout.width;v->wh=v->layout.height;
 int avail=v->layout.orientation?app.screen->Height-screen_top()-8:app.screen->Width-16;
 int content=v->layout.orientation?v->layout.height:v->layout.width;
 if(content>avail){
  v->overflow=1;v->scroll_max=content-avail;
  if(v->layout.orientation)v->wh=avail;else v->ww=avail;
  note("OVERFLOW dock=%d content=%d window=%d",d->id,content,avail);
  /* Scrolling would change which pixels a dock without a bar shows, and
   * its shape must stay fixed: a scrolling dock keeps its bar. */
  if(v->nobg){v->nobg=0;feedback(index,"Too long to show without a bar");note("NOBG off dock=%d: overflow",d->id);}
 }
 dock_place_resolve(&d->place,app.screen->Width,app.screen->Height,screen_top(),v->ww,v->wh,&x,&y);
 v->x=x;v->y=y;
 size_t bytes=(size_t)v->ww*v->wh*4;
 /* A composed dock never holds a picture of the desktop (uncompose makes
  * one when it has to become pictured). */
 v->pixels=malloc(bytes);v->background=malloc(bytes);v->desk=v->compose?NULL:calloc(bytes,1);
 v->mask=calloc((size_t)v->ww*v->wh,1);v->mask_shown=calloc((size_t)v->ww*v->wh,1);
 v->bitmap=AllocBitMap(v->ww,v->wh,GetBitMapAttr(app.screen->RastPort.BitMap,BMA_DEPTH),BMF_CLEAR,app.screen->RastPort.BitMap);
 if(!v->pixels || !v->background || (!v->desk && !v->compose) || !v->mask || !v->mask_shown || !v->bitmap){close_view(index);return 0;}
 note("MEMORY dock=%d buffers=%lu (pixels, background%s, masks, bitmap)",d->id,(unsigned long)(bytes*(v->desk?4:3)+2*(size_t)v->ww*v->wh),v->desk?", desktop picture":"");
 InitRastPort(&v->rp);v->rp.BitMap=v->bitmap;SetFont(&v->rp,app.font);
 dock_snap_start(&v->snap,app.screen_gen);
 if(view_transparent(index) && desk_donor && desk_donor->desk && desk_donor->x==x && desk_donor->y==y &&
    desk_donor->ww==v->ww && desk_donor->wh==v->wh && desk_donor->snap.valid){
  /* Same place and size: the desktop behind it is the one already read. */
  memcpy(v->desk,desk_donor->desk,bytes);v->snap=desk_donor->snap;
  if(v->nobg)v->lum=dock_luminance(v->desk,NULL,v->ww*v->wh);
 } else if(view_transparent(index) && no_desk_read){
  /* An update with our windows still on screen: reading now would take
   * them (an old dock's icons) for desktop. The picture is taken later the
   * usual way, with the docks in the way closed (snapshot hold). */
  note("SNAP dock=%d deferred: windows of ours are on screen",d->id);
 } else if(view_transparent(index)){
  /* No dock window is open here, so the desktop shows as it is now. Its
   * picture may not be painted yet; the watch started by the caller sees
   * that later. */
  /* The ring first: a desktop that changes between the two reads then
   * shows up as a change, never as a settled picture. */
  unsigned long ring=desk_ring(x,y,v->ww,v->wh);
  ULONG got=ReadPixelArray(v->desk,0,0,(UWORD)(v->ww*4),&app.screen->RastPort,x,y,(UWORD)v->ww,(UWORD)v->wh,RECTFMT_ARGB);
  int result=got==(ULONG)v->ww*v->wh?DOCK_SNAP_OK:DOCK_SNAP_FAILED;
  if(v->nobg && result==DOCK_SNAP_OK)v->lum=dock_luminance(v->desk,NULL,v->ww*v->wh);
  dock_snap_captured(&v->snap,app.screen_gen,v->snap.request,result,ring,milliseconds());
  note("SNAP dock=%d gen=%u closed result=%s pixels=%lu/%d size=%dx%d at=%d,%d",d->id,app.screen_gen,dock_snap_result(result),(unsigned long)got,v->ww*v->wh,v->ww,v->wh,x,y);
 }
 if(v->compose)compose_luminance(index);
 compose_background(index);
 for(int i=0;i<d->model.count;i++){
  struct DockItem *item=&d->model.items[i];struct Visual *vis=&v->visual[i];
  if(item->kind==DOCK_SEPARATOR || item->kind==DOCK_SPACER)continue;
  if(item->kind==DOCK_CLOCK){if(dock_widget_start(&vis->widget,item->path))dock_widget_update(&vis->widget,time(NULL));continue;}
  if(item->kind==DOCK_APPLET){vis->applet=dock_applets_get(item->path,item->dir);if(vis->applet){
    if(!take_visual(item,icon,vis))applet_image_load(vis);
    vis->signature=dock_applet_compact_signature(&vis->applet->state.snapshot,vis->applet->package.primary);}continue;}
  if(item->kind!=DOCK_GROUP && take_visual(item,icon,vis))continue;
  if(item->kind==DOCK_GROUP && item->icon[0])image_load(&vis->image,item->icon,NULL,icon);
  /* A group without its own picture, or whose picture cannot be shown,
   * previews its first children instead. */
  if(item->kind==DOCK_GROUP && !vis->image.svg && !vis->image.native.object){
   int n=0;struct DockItem *children=dock_items(&d->model,item->group,&n);if(n>4)n=4;
   for(int j=0;children && j<n;j++)image_load(&vis->mini[j],children[j].icon[0]?children[j].icon:children[j].path,children[j].path,icon/2-4);
   vis->minis=n;vis->use_minis=1;
  } else if(item->kind!=DOCK_GROUP){
   const char *path,*fallback;picture_of(item,&path,&fallback);
   image_load(&vis->image,path,fallback,icon);
   note("ICON dock=%d index=%d origin=%s override=%d svg=%d source=%dx%d size=%dx%d path=%s",d->id,i,
    dock_icon_origin_name(vis->image.native.origin),item->icon[0]!=0,vis->image.svg!=NULL,
    vis->image.native.source_width,vis->image.native.source_height,vis->image.native.width,vis->image.native.height,item->path);
  }
 }
 /* Without a bar the window is shaped to what it can draw: an icon's
  * selected image (an open drawer, say) may reach where the normal one does
  * not, and the shape stays fixed while the dock is open. */
 /* A composed window needs no shape: where it is clear, the desktop shows
  * and clicks go through (layers' hit test skips alpha 0), so edges keep
  * their smooth alpha instead of a cut at half coverage. */
 v->shape=v->compose?dock_shape(v->ww,v->wh,0):pictured_shape(index);
 if(!v->shape){close_view(index);return 0;}
 /* A dock without a bar is shaped to its icons at rest: no hover effect
  * there for now (the shape cannot follow, see draw_view). */
 v->effect=v->nobg?DOCK_HOVER_NONE:v->look.hover;
 v->lift_sign=dock_lift_sign(d->place.mode,v->layout.orientation);
 {int rest[DOCK_MAX_ITEMS]={0};dock_lift_frame(&v->layout,v->lift_sign,rest,&v->fx);}
 v->effect_at=milliseconds();
 mag_prepare(index);
 return 1;
}
/* A composed dock that lost or never got its surface becomes a pictured one:
 * the plain frame now, the desktop picture through the usual watch. It stays
 * pictured until the screen changes. */
static void uncompose(int index) {
 struct DockView *v=&app.views[index];
 app.compose_refused[index]=1;v->compose=0;
 if(!v->desk)v->desk=calloc((size_t)v->ww*v->wh,4);
 /* Without memory for the picture the bar is drawn plain (snap stays
  * invalid, and view_transparent docks without a desk are never read). */
 if(v->desk)memset(v->desk,0,(size_t)v->ww*v->wh*4);
 compose_background(index);
 /* An ordinary window needs the outline a composed one did without. */
 struct Region *shape=pictured_shape(index);
 if(shape){if(v->shape)DisposeRegion(v->shape);v->shape=shape;}
 if(view_transparent(index))dock_snap_invalidate(&v->snap,DOCK_SNAP_MOVED,milliseconds());
}
/* The window alone, at v->x/v->y; everything it shows is already prepared. */
static int open_window(int index) {
 struct DockView *v=&app.views[index];
 struct TagItem tags[]={{WA_ShapeRegion,(IPTR)v->shape},{WA_PubScreen,(IPTR)app.screen},{WA_Left,(IPTR)v->x},{WA_Top,(IPTR)v->y},
  {WA_Width,(IPTR)v->ww},{WA_Height,(IPTR)v->wh},{WA_Borderless,TRUE},{WA_RMBTrap,TRUE},{WA_ReportMouse,TRUE},{WA_Activate,FALSE},
  /* A composed window is smart refresh whatever is asked; the others
   * repaint from v->pixels on IDCMP_REFRESHWINDOW. */
  {WA_SimpleRefresh,TRUE},
  {WA_IDCMP,IDCMP_MOUSEMOVE|IDCMP_MOUSEBUTTONS|IDCMP_RAWKEY|IDCMP_REFRESHWINDOW|IDCMP_INACTIVEWINDOW|IDCMP_CHANGEWINDOW},{TAG_DONE,0}};
 v->window=dock_present_open(v->compose,tags);
 if(trace_present)note("WIN open dock=%d %p composed=%d",app.set.docks[index].id,(void*)v->window,dock_present_composed(v->window));
 if(v->window && v->compose && !dock_present_composed(v->window)){
  /* No surface for it (memory, the screen's budget): the dock is pictured
   * instead, on this screen. */
  note("COMPOSE dock=%d refused: ordinary window; using the desktop picture",app.set.docks[index].id);
  CloseWindow(v->window);v->window=NULL;
  uncompose(index);
  tags[0].ti_Data=(IPTR)v->shape;
  v->window=dock_present_open(0,tags);
 }
 return v->window && dock_drop_open(&v->drop,v->window);
}
static void close_window_of(struct DockView *v) {
 dock_drop_close(&v->drop);
 /* A dock moved by its grip is reopened where it is now, not where it was
  * first placed. */
 if(v->window){v->x=v->window->LeftEdge;v->y=v->window->TopEdge;}
 if(v->window){
  if(trace_present)note("WIN close %p",(void*)v->window);
  struct IntuiMessage *msg;while((msg=(struct IntuiMessage*)GetMsg(v->window->UserPort)))ReplyMsg((struct Message*)msg);CloseWindow(v->window);v->window=NULL;}
}
static int show_view(int index) {
 struct DockView *v=&app.views[index];struct DockInstance *d=&app.set.docks[index];
 if(!v->bitmap)return 0;
 if(!open_window(index)){close_view(index);return 0;}
 note("VIEW dock=%d name=%s left=%d top=%d width=%d height=%d items=%d orientation=%d transparency=%d glass=%d",d->id,d->name,
  v->window->LeftEdge,v->window->TopEdge,v->ww,v->wh,d->model.count,d->look.orientation,d->look.transparency,d->look.glass);
 /* The surface is the system's memory, counted against the screen's budget. */
 note("PRESENT dock=%d %s surface_bytes=%d",d->id,v->compose?"composed":"pictured",v->compose?v->ww*v->wh*4:0);
 draw_view(index);return 1;
}
/* ---- window <-> layout coordinates (scrolling docks) ------------------- */

#define DOCK_ARROW 16
static int win_main(struct DockView *v){return v->layout.orientation?v->wh:v->ww;}
static int main_at(struct DockView *v,int x,int y){return v->layout.orientation?y:x;}
static int view_start(struct DockView *v){return v->layout.start+(v->scroll>0?DOCK_ARROW:0);}
static int view_end(struct DockView *v){return win_main(v)-(v->scroll<v->scroll_max?DOCK_ARROW:0);}
/* -1 on the "back" arrow, 1 on the "more" arrow, 0 elsewhere. */
static int arrow_at(struct DockView *v,int x,int y) {
 if(!v->overflow)return 0;
 int m=main_at(v,x,y);
 if(v->scroll>0 && m>=v->layout.start && m<v->layout.start+DOCK_ARROW)return -1;
 if(v->scroll<v->scroll_max && m>=win_main(v)-DOCK_ARROW)return 1;
 return 0;
}
static void to_layout(struct DockView *v,int *x,int *y) {
 if(v->overflow && main_at(v,*x,*y)>=v->layout.start){if(v->layout.orientation)*y+=v->scroll;else *x+=v->scroll;}
}
static int slot_visible(struct DockView *v,int index) {
 const struct DockRect *r=&v->layout.slots[index].slot;
 int a=(v->layout.orientation?r->y:r->x)-v->scroll,b=a+(v->layout.orientation?r->h:r->w);
 return !v->overflow || (a>=view_start(v) && b<=view_end(v));
}
static int view_hit(struct DockView *v,int x,int y) {
 if(x<0 || y<0 || x>=v->ww || y>=v->wh || arrow_at(v,x,y))return -1;
 to_layout(v,&x,&y);
 int hit=dock_layout_hit(&v->layout,x,y);
 return hit>=0 && slot_visible(v,hit)?hit:-1;
}
/* Separators and spaces too: for editing and moving, never for starting. */
static int view_item(struct DockView *v,int x,int y) {
 if(x<0 || y<0 || x>=v->ww || y>=v->wh || arrow_at(v,x,y))return -1;
 to_layout(v,&x,&y);
 int item=dock_layout_item(&v->layout,x,y);
 return item>=0 && slot_visible(v,item)?item:-1;
}
static int view_before(struct DockView *v,int x,int y){to_layout(v,&x,&y);return dock_layout_before(&v->layout,x,y);}
static int view_center(struct DockView *v,int index,int x,int y){to_layout(v,&x,&y);return dock_layout_center(&v->layout,index,x,y);}
static int view_in_grip(struct DockView *v,int x,int y){return !v->overflow || main_at(v,x,y)<v->layout.start?dock_layout_in_grip(&v->layout,x,y):0;}
static void scroll_by(struct DockView *v,int direction) {
 int step=v->style.icon+10;
 v->scroll+=direction*step;
 if(v->scroll<0)v->scroll=0;
 if(v->scroll>v->scroll_max)v->scroll=v->scroll_max;
 v->scroll_at=milliseconds();v->hover=-1;v->dirty=1;
}
static void slot_on_screen(int view,int index,int *x,int *y,int *w,int *h) {
 struct DockView *v=&app.views[view];const struct DockRect *r=&v->layout.slots[index].slot;
 *x=v->window->LeftEdge+r->x-(v->layout.orientation?0:v->scroll);
 *y=v->window->TopEdge+r->y-(v->layout.orientation?v->scroll:0);
 *w=r->w;*h=r->h;
}
static int open_panel(int view,int container,int activate) {
 int index=-1,x,y,w,h;
 dock_applet_ui_close();close_tooltip();close_panel();
 for(int i=0;i<model_of(view)->count;i++)if(model_of(view)->items[i].kind==DOCK_GROUP && model_of(view)->items[i].group==container){index=i;break;}
 if(index<0 || !app.views[view].window)return 0;
 slot_on_screen(view,index,&x,&y,&w,&h);
 if(!lock_screen())return 0;
 int result=dock_panel_open(&app.panel,app.screen,model_of(view),container,app.views[view].style.icon,x,y,w,h,
  app.views[view].layout.orientation,activate,app.views[view].c);
 unlock_screen();
 if(result){app.panel_view=view;app.panel_dirty=0;}
 note("GROUP open=%d dock=%d container=%d",result,app.set.docks[view].id,container);return result;
}
/* What the open views show, for update_docks to find what changed. */
static struct DockSet shown_set;static int shown_valid;
static void close_all(void) {
 close_tooltip();close_panel();dock_applet_ui_close();dock_menu_close(&app.menu,app.screen);app.menu_view=-1;
 for(int i=0;i<DOCK_MAX_DOCKS;i++)close_view(i);
 shown_valid=0;
}
static int open_all(void) {
 dock_applets_layout_begin();
 int opened=0;
 for(int i=0;i<app.set.count;i++)prepare_view(i);
 for(int i=0;i<app.set.count;i++)opened+=show_view(i);
 shown_set=app.set;shown_valid=opened==app.set.count;
 return opened;
}
/* After an edit: only the docks that changed are made again, each while its
 * old window is still open, so it is gone for the time of one close and
 * open at most, not for the loading of every icon. A dock that keeps its
 * size and outline (a bar, not icons only) keeps its window and is only
 * redrawn. Loaded pictures move along, widgets keep their data. A change
 * to the set of docks or to shared settings rebuilds everything as before.
 * 1 done, 0 not possible here (the caller rebuilds everything). */
/* Resizing a bar in place: ChangeWindowBox, then the outline for the new
 * size. --reopen-on-resize turns it off (the old way: close and open). */
static int dock_resize_in_place=1;
/* Intuition changes the box asynchronously. The main loop goes on meanwhile;
 * the view is marked resizing and finish_resize installs the new outline and
 * draws once IDCMP_CHANGEWINDOW (or the new size) is seen. Events queued
 * before that were measured against the old box. */
#define RESIZE_TIMEOUT_MS 1000u
/* Test builds only: a resize counts as done no sooner than this, so clicks
 * during it and the fallback can be exercised. */
#ifndef DOCK_TEST_RESIZE_DELAY_MS
#define DOCK_TEST_RESIZE_DELAY_MS 0u
#endif
static int resize_settled(const struct DockView *v) {
#if DOCK_TEST_RESIZE_DELAY_MS
 return milliseconds()-v->resize_at>=DOCK_TEST_RESIZE_DELAY_MS;
#else
 (void)v;return 1;
#endif
}
static int resize_window(struct DockView *old,struct DockView *v) {
 struct Window *w=old->window;
 if(!w)return 0;
 v->from_x=w->LeftEdge;v->from_y=w->TopEdge;
 /* The window keeps the old outline until the new box is there. */
 v->old_shape=old->shape;old->shape=NULL;
 v->resizing=1;v->resize_seen=0;v->resize_at=milliseconds();v->dirty=1;
 ChangeWindowBox(w,v->x,v->y,v->ww,v->wh);
 return 1;
}
static struct DockView old_views[DOCK_MAX_DOCKS];
static int update_docks(void) {
 if(!shown_valid || !app.screen || shown_set.count!=app.set.count ||
    memcmp(&shown_set.defaults,&app.set.defaults,sizeof shown_set.defaults))return 0;
 int changed[DOCK_MAX_DOCKS]={0},any=0;
 for(int i=0;i<app.set.count;i++){
  if(shown_set.docks[i].id!=app.set.docks[i].id || !app.views[i].window)return 0;
  changed[i]=memcmp(&shown_set.docks[i],&app.set.docks[i],sizeof app.set.docks[i])!=0;any|=changed[i];
 }
 unsigned start=milliseconds();
 close_tooltip();close_panel();dock_applet_ui_close();dock_menu_close(&app.menu,app.screen);app.menu_view=-1;
 /* Widgets on docks that stay as they are remain active. */
 dock_applets_layout_begin();
 for(int i=0;i<app.set.count;i++)if(!changed[i])for(int k=0;k<app.set.docks[i].model.count;k++){
  const struct DockItem *it=&app.set.docks[i].model.items[k];
  if(it->kind==DOCK_APPLET)dock_applets_get(it->path,it->dir);
 }
 for(int i=0;i<app.set.count;i++)if(changed[i]){
  /* The old view's resources now belong to old_views only. */
  old_views[i]=app.views[i];memset(&app.views[i],0,sizeof app.views[i]);
  donor_view[i]=&old_views[i];donor_model[i]=&shown_set.docks[i].model;}
 int kept=0,reopened=0,failed=0;
 for(int i=0;i<app.set.count && any;i++){
  if(!changed[i])continue;
  struct DockView *old=&old_views[i];
  desk_donor=old;no_desk_read=1;
  if(!prepare_view(i)){failed=1;desk_donor=NULL;no_desk_read=0;break;}
  desk_donor=NULL;no_desk_read=0;
  struct DockView *v=&app.views[i];
  int same=old->x==v->x && old->y==v->y && old->ww==v->ww && old->wh==v->wh && old->corner==v->corner && !old->nobg && !v->nobg;
  /* A bar that only changes size: the same window, resized. */
  int resize=!same && !old->nobg && !v->nobg && old->corner==v->corner && dock_resize_in_place;
  unsigned gap=milliseconds();
  if(resize && resize_window(old,v)){
   v->window=old->window;v->drop=old->drop;old->window=NULL;memset(&old->drop,0,sizeof old->drop);
   kept++;
   note("UPDATE dock=%d resizing window to %dx%d",app.set.docks[i].id,v->ww,v->wh);
  } else if(same){
   /* The window's outline is the old region; it stays with the window. */
   v->window=old->window;v->drop=old->drop;old->window=NULL;memset(&old->drop,0,sizeof old->drop);
   DisposeRegion(v->shape);v->shape=old->shape;old->shape=NULL;
   draw_view(i);kept++;
   note("UPDATE dock=%d kept window",app.set.docks[i].id);
  } else {
   close_window_of(old);
   if(!show_view(i)){failed=1;break;}
   reopened++;
   note("UPDATE dock=%d reopened hidden=%u ms",app.set.docks[i].id,milliseconds()-gap);
  }
  /* A see-through dock whose picture could not be reused gets it now,
   * with whatever of ours is in the way closed for the moment. */
  if(view_transparent(i) && !v->snap.valid && !v->resizing){
   /* MOVED schedules a real picture (with our windows in the way closed for
    * it); OPENED would only watch for changes and leave the dock without one. */
   dock_snap_invalidate(&v->snap,DOCK_SNAP_MOVED,milliseconds());
   note("SNAP dock=%d picture scheduled after update",app.set.docks[i].id);
  }
 }
 for(int i=0;i<DOCK_MAX_DOCKS;i++)if(donor_view[i]){free_view(donor_view[i]);memset(donor_view[i],0,sizeof *donor_view[i]);donor_view[i]=NULL;donor_model[i]=NULL;}
 if(failed){note("UPDATE failed; rebuilding everything");return 0;}
 shown_set=app.set;
 note("UPDATE docks=%d kept=%d reopened=%d took=%u ms",app.set.count,kept,reopened,milliseconds()-start);
 return 1;
}

/* The new box is there: the new outline, the new picture. Without it within
 * RESIZE_TIMEOUT_MS the window is closed and opened at the new size instead. */
static void finish_resize(int index,int timed_out) {
 struct DockView *v=&app.views[index];struct Window *w=v->window;
 if(!v->resizing || !w)return;
 unsigned took=milliseconds()-v->resize_at;
 v->resizing=0;
 if(timed_out){
  note("UPDATE dock=%d resize not done after %u ms (%dx%d); reopening",app.set.docks[index].id,took,w->Width,w->Height);
  int x=v->x,y=v->y;close_window_of(v);v->x=x;v->y=y;
  if(v->old_shape){DisposeRegion(v->old_shape);v->old_shape=NULL;}
  if(!show_view(index))app.rebuild=1;
 } else if(v->compose && !dock_present_composed(w)){
  /* The surface could not grow (layers.library turns it into an ordinary
   * window, keeping its pixels): picture the dock from now on. */
  note("COMPOSE dock=%d surface lost on resize to %dx%d; using the desktop picture",app.set.docks[index].id,v->ww,v->wh);
  int x=v->x,y=v->y;close_window_of(v);v->x=x;v->y=y;
  if(v->old_shape){DisposeRegion(v->old_shape);v->old_shape=NULL;}
  uncompose(index);
  if(!show_view(index))app.rebuild=1;
 } else {
  struct Region *was=ChangeWindowShape(w,v->shape,NULL);
  /* The region given back is the old outline; it is ours to free. */
  if(was && was!=v->old_shape && was!=v->shape)DisposeRegion(was);
  if(v->old_shape){DisposeRegion(v->old_shape);v->old_shape=NULL;}
  draw_view(index);
  note("UPDATE dock=%d resized window to %dx%d in %u ms",app.set.docks[index].id,v->ww,v->wh,took);
 }
 if(v->window && view_transparent(index) && !v->snap.valid){
  dock_snap_invalidate(&v->snap,DOCK_SNAP_MOVED,milliseconds());
  note("SNAP dock=%d picture scheduled after update",app.set.docks[index].id);
 }
}
static int resizing_any(void) {
 for(int i=0;i<app.set.count;i++)if(app.views[i].resizing)return 1;
 return 0;
}
/* For a resize whose IDCMP_CHANGEWINDOW was not seen in dispatch. */
static void resize_poll(void) {
 for(int i=0;i<app.set.count;i++){
  struct DockView *v=&app.views[i];struct Window *w=v->window;
  if(!v->resizing || !w)continue;
  unsigned age=milliseconds()-v->resize_at;
  if(w->LeftEdge==v->x && w->TopEdge==v->y && w->Width==v->ww && w->Height==v->wh && resize_settled(v))finish_resize(i,0);
  else if(age>=RESIZE_TIMEOUT_MS)finish_resize(i,1);
 }
}

/* ---- see-through backgrounds ------------------------------------------ */

static int overlaps(struct Window *w,int x,int y,int width,int height) {
 return w && w->LeftEdge<x+width && x<w->LeftEdge+w->Width && w->TopEdge<y+height && y<w->TopEdge+w->Height;
}
/* Our own long-lived windows: their pixels are not desktop. */
static int ours_at(int x,int y) {
 for(int i=0;i<app.set.count;i++)if(inside(app.views[i].window,x,y))return 1;
 return inside(app.prefs.window,x,y) || inside(app.editor.window,x,y) || inside(app.about.window,x,y) || inside(dock_about_licenses_window(&app.about),x,y) || inside(app.items.window,x,y) || inside(dock_applet_ui_window(),x,y);
}
/* Short-lived ones come and go under the pointer; nothing is read while
 * they are open. */
static int transient_open(void){return app.tooltip || app.menu.window || app.panel.window || app.drawers.window || dock_applet_ui_window() || app.drag.own.kind;}
/* A fingerprint of the desktop just outside a dock: one line of pixels
 * three pixels off each side, without pixels of our windows. Only change
 * matters, so a hash is enough. */
static unsigned long desk_ring(int x,int y,int w,int h) {
 static unsigned char *line;static int line_size;
 int sw=app.screen->Width,sh=app.screen->Height,need=(sw>sh?sw:sh)*4;
 if(line_size<need){free(line);line=malloc((size_t)need);line_size=line?need:0;if(!line)return 0;}
 unsigned long hash=2166136261u;
 int x0=x-3,y0=y-3,x1=x+w+2,y1=y+h+2;
 for(int side=0;side<4;side++){
  int horizontal=side<2,fixed=side==0?y0:side==1?y1:side==2?x0:x1;
  if(fixed<0 || fixed>=(horizontal?sh:sw))continue;
  int from=horizontal?x0:y0,to=horizontal?x1:y1;
  if(from<0)from=0;
  if(to>=(horizontal?sw:sh))to=(horizontal?sw:sh)-1;
  if(to<from)continue;
  int n=to-from+1;
  ULONG got=horizontal?ReadPixelArray(line,0,0,(UWORD)(n*4),&app.screen->RastPort,from,fixed,(UWORD)n,1,RECTFMT_ARGB):
   ReadPixelArray(line,0,0,4,&app.screen->RastPort,fixed,from,1,(UWORD)n,RECTFMT_ARGB);
  if(got!=(ULONG)n){hash^=0x5a5a5a5au;continue;}
  for(int k=0;k<n;k++){
   int px=horizontal?from+k:fixed,py=horizontal?fixed:from+k;
   if(ours_at(px,py))continue;
   for(int c=1;c<4;c++){hash^=line[4*k+c];hash*=16777619u;}
  }
 }
 return hash&0xffffffffu;
}
/* Takes the desktop picture under an open dock. The dock window (and any
 * dock over it) is closed for the read and opened again; icons, pictures
 * and drop targets stay prepared, so nothing else is rebuilt.
 * ChangeWindowShape cannot do this on ABIv11 2026.09: the desktop is not
 * repainted under the emptied shape, and after the old shape is back the
 * window draws nothing (tests/mark_probe.c).
 * The desktop owner repaints the uncovered area through its layer backfill
 * during CloseWindow, or, while Wanderer is still loading its picture, a
 * little later from its own task. To tell, the dock first shows itself with
 * a marker pattern: blue changed by 8 of 255 levels on every other pixel,
 * hard to see for the few frames it lasts but not invisible (not measured
 * at other colour depths). The picture is taken once that pattern is gone.
 * That proves the area was repainted, not that the desktop picture is
 * complete; the watch after each event covers a picture that arrives
 * later. The windows stay
 * closed for at most HOLD_MS while the main loop goes on; no screen lock is
 * held meanwhile, so a screen change is never blocked by it. The same
 * pattern shows how much of the dock is visible at all. */
#define HOLD_MS 600u
/* Development builds only (scripts/build.sh dev/Dockbar-holdtest): after the
 * picture is read, keep the window closed until this long after closing, so
 * that a test can change the wallpaper between the read and the end of the
 * picture. 0 (compiled out) in every shipped build. */
#ifndef DOCK_TEST_MIN_HOLD_MS
#define DOCK_TEST_MIN_HOLD_MS 0
#endif
static struct {
 int index,closed[DOCK_MAX_DOCKS],x,y,ww,wh,marked,visible,taken;
 unsigned since;
 unsigned char *shown,*mark,*fresh;
} hold={.index=-1};
static void hold_free(void) {
 free(hold.shown);free(hold.mark);free(hold.fresh);
 hold.shown=hold.mark=hold.fresh=NULL;hold.index=-1;hold.taken=0;memset(hold.closed,0,sizeof hold.closed);
}
/* Kept marks among the visible ones: 0 when the desktop repainted all. */
static int hold_kept(void) {
 int kept=0;
 for(int j=0;j<hold.wh;j++){
  int inset=dock_row_inset(j,hold.ww,hold.wh,app.views[hold.index].corner);
  for(int i=inset+((j+inset)&1);i<hold.ww-inset;i+=2){
   if(app.views[hold.index].nobg && !app.views[hold.index].mask_shown[j*hold.ww+i])continue;
   unsigned char *a=hold.mark+4*(j*hold.ww+i),*b=hold.fresh+4*(j*hold.ww+i),*c=hold.shown+4*(j*hold.ww+i);
   if((a[1]!=c[1] || a[2]!=c[2] || a[3]!=c[3]) && a[1]==b[1] && a[2]==b[2] && a[3]==b[3])kept++;
  }
 }
 return kept;
}
/* Opens the closed windows again and ends the hold with this result. */
static int hold_end(int result) {
 int index=hold.index,lost=0;struct DockView *v=&app.views[index];
 if(result==DOCK_SNAP_OK){
  memcpy(v->desk,hold.fresh,(size_t)hold.ww*hold.wh*4);v->snap.valid=1;
  if(v->nobg)v->lum=dock_luminance(v->desk,v->mask_shown,hold.ww*hold.wh);
 }
 int locked=lock_screen();
 for(int i=0;i<app.set.count;i++)if(hold.closed[i]){
  app.views[i].hover=app.views[i].pressed=-1;
  if(locked && open_window(i)){if(i!=index)draw_view(i);}
  else{close_window_of(&app.views[i]);lost=1;note("SNAP dock=%d window could not reopen",app.set.docks[i].id);}
 }
 if(locked)unlock_screen();
 hold_free();
 compose_background(index);
 if(v->window)draw_view(index);
 /* A dock whose window would not come back is rebuilt with all the others. */
 if(lost){app.rebuild=1;app.reopen_view=-1;}
 return result;
}
/* Called each pass while windows are held closed: DOCK_SNAP_BUSY while
 * still waiting, else the final result. */
static int hold_poll(void) {
 int n=hold.ww*hold.wh;
#if DOCK_TEST_MIN_HOLD_MS
 if(hold.taken)return milliseconds()-hold.since<DOCK_TEST_MIN_HOLD_MS?DOCK_SNAP_BUSY:hold_end(DOCK_SNAP_OK);
#endif
 if(ReadPixelArray(hold.fresh,0,0,(UWORD)(hold.ww*4),&app.screen->RastPort,hold.x,hold.y,(UWORD)hold.ww,(UWORD)hold.wh,RECTFMT_ARGB)!=(ULONG)n)
  return hold_end(DOCK_SNAP_FAILED);
 int kept=hold_kept();
 /* A few may match by chance where the desktop is that exact colour. */
 if(kept*50<=hold.visible){
  if(milliseconds()-hold.since>0)note("SNAP dock=%d repainted after %u ms",app.set.docks[hold.index].id,milliseconds()-hold.since);
#if DOCK_TEST_MIN_HOLD_MS
  /* Picture taken; the window stays closed for the test. */
  hold.taken=1;return DOCK_SNAP_BUSY;
#endif
  return hold_end(DOCK_SNAP_OK);
 }
 if(milliseconds()-hold.since<HOLD_MS)return DOCK_SNAP_BUSY;
 note("SNAP dock=%d unpainted marked=%d visible=%d kept=%d",app.set.docks[hold.index].id,hold.marked,hold.visible,kept);
 return hold_end(DOCK_SNAP_UNPAINTED);
}
/* Starts a picture: DOCK_SNAP_BUSY with hold.index set means the windows
 * are closed and hold_poll goes on; any other value is final. */
static int capture_open(int index) {
 struct DockView *v=&app.views[index];struct Window *w=v->window;
 if(!w || !v->shape || transient_open() || hold.index>=0 || foreign_drag())return DOCK_SNAP_BUSY;
 int x=w->LeftEdge,y=w->TopEdge,ww=v->ww,wh=v->wh,n=ww*wh;
 if(overlaps(app.prefs.window,x,y,ww,wh) || overlaps(app.editor.window,x,y,ww,wh) || overlaps(app.about.window,x,y,ww,wh) || overlaps(dock_about_licenses_window(&app.about),x,y,ww,wh) || overlaps(app.items.window,x,y,ww,wh) || overlaps(dock_applet_ui_window(),x,y,ww,wh))return DOCK_SNAP_BUSY;
 hold.index=index;hold.x=x;hold.y=y;hold.ww=ww;hold.wh=wh;hold.marked=hold.visible=0;
 hold.shown=malloc((size_t)n*4);hold.mark=malloc((size_t)n*4);hold.fresh=malloc((size_t)n*4);
 if(!hold.shown || !hold.mark || !hold.fresh ||
    ReadPixelArray(hold.shown,0,0,(UWORD)(ww*4),&app.screen->RastPort,x,y,(UWORD)ww,(UWORD)wh,RECTFMT_ARGB)!=(ULONG)n){
  hold_free();note("SNAP dock=%d failed to read",app.set.docks[index].id);return DOCK_SNAP_FAILED;
 }
 memcpy(hold.mark,hold.shown,(size_t)n*4);
 for(int j=0;j<wh;j++)for(int i=(j&1);i<ww;i+=2)hold.mark[4*(j*ww+i)+3]^=0x08;
 WritePixelArray(hold.mark,0,0,(UWORD)(ww*4),w->RPort,0,0,(UWORD)ww,(UWORD)wh,RECTFMT_ARGB);
 /* Read back what the screen made of it: a 16-bit mode rounds colours, and
  * where another window covers the dock nothing changed. */
 int ok=ReadPixelArray(hold.mark,0,0,(UWORD)(ww*4),&app.screen->RastPort,x,y,(UWORD)ww,(UWORD)wh,RECTFMT_ARGB)==(ULONG)n;
 for(int j=0;ok && j<wh;j++){
  int inset=dock_row_inset(j,ww,wh,v->corner);
  for(int i=inset+((j+inset)&1);i<ww-inset;i+=2){
   if(v->nobg && !v->mask_shown[j*ww+i])continue;
   unsigned char *a=hold.mark+4*(j*ww+i),*b=hold.shown+4*(j*ww+i);
   hold.marked++;if(a[1]!=b[1] || a[2]!=b[2] || a[3]!=b[3])hold.visible++;
  }
 }
 if(!ok || hold.visible*2<hold.marked){
  int result=ok?DOCK_SNAP_COVERED:DOCK_SNAP_FAILED;
  note("SNAP dock=%d %s marked=%d visible=%d",app.set.docks[index].id,dock_snap_result(result),hold.marked,hold.visible);
  hold_free();draw_view(index);return result;
 }
 /* WA_PubScreen windows open only with the screen locked: check that it can
  * be had before closing anything, so that a dock never stays away. */
 if(!lock_screen()){hold_free();draw_view(index);return DOCK_SNAP_BUSY;}
 unlock_screen();
 for(int i=0;i<app.set.count;i++){
  struct DockView *o=&app.views[i];
  if(o->window && (i==index || overlaps(o->window,x,y,ww,wh))){close_window_of(o);hold.closed[i]=1;}
 }
 hold.since=milliseconds();
 int result=hold_poll();
 return result;
}
/* Every prepared see-through dock, also one whose window is closed for a
 * moment while its picture is taken: the request belongs to the dock, not
 * to its window. */
static void snapshot_invalidate(int reason) {
 unsigned now=milliseconds();int count=0;
 for(int i=0;i<app.set.count;i++)if(app.views[i].bitmap && view_transparent(i)){dock_snap_invalidate(&app.views[i].snap,reason,now);count++;}
 for(int i=0;i<app.set.count;i++)if(app.views[i].bitmap && app.views[i].compose && reason!=DOCK_SNAP_MOVED){app.views[i].lum_due=now+1500;app.views[i].lum_reads=2;}
 if(count)note("SNAP invalidate reason=%s gen=%u docks=%d",dock_snap_reason(reason),app.screen_gen,count);
}
/* At most one picture per pass of the main loop: each hides a dock for a
 * moment, and input stays responsive between them. */
static unsigned long hold_ring;
static unsigned hold_request;
static void snapshot_done(int index,unsigned request,int result,unsigned long ring) {
 struct DockView *v=&app.views[index];
 dock_snap_captured(&v->snap,app.screen_gen,request,result,ring,milliseconds());
 if(request!=v->snap.request && result!=DOCK_SNAP_BUSY)note("SNAP dock=%d newer request during the picture; another follows",app.set.docks[index].id);
 if(result!=DOCK_SNAP_BUSY)note("SNAP dock=%d gen=%u open result=%s tries=%d valid=%d",app.set.docks[index].id,app.screen_gen,dock_snap_result(result),v->snap.tries,v->snap.valid);
}
static void snapshot_work(void) {
 unsigned now=milliseconds();
 for(int i=0;i<app.set.count;i++){
  struct DockView *v=&app.views[i];
  if(!v->window || !v->compose || !v->lum_reads || (int)(now-v->lum_due)<0)continue;
  int was=v->light;
  compose_luminance(i);
  v->lum_reads--;v->lum_due=now+3500;
  if(dock_theme_light(v->look.theme,v->lum,v->light)!=was){compose_background(i);v->dirty=1;}
 }
 /* A picture in progress: nothing else until it ends. */
 if(hold.index>=0){
  int index=hold.index,result=hold_poll();
  if(result!=DOCK_SNAP_BUSY)snapshot_done(index,hold_request,result,hold_ring);
  return;
 }
 for(int i=0;i<app.set.count;i++){
  struct DockView *v=&app.views[i];
  if(!v->window || !view_transparent(i))continue;
  int watching=v->snap.watching;
  int action=dock_snap_next(&v->snap,app.screen_gen,now);
  /* After this line nothing more is read until the next event. */
  if(watching && !v->snap.watching){note("SNAP dock=%d watch ended samples=%d",app.set.docks[i].id,v->samples);v->samples=0;}
  if(action==DOCK_SNAP_CAPTURE && v->nobg && !v->extra){
   /* Nothing to hide: the desktop shows through everywhere the dock draws
    * nothing, and only its brightness is wanted. */
   unsigned long ring=desk_ring(v->window->LeftEdge,v->window->TopEdge,v->ww,v->wh);
   unsigned request=v->snap.request;
   int result=ReadPixelArray(v->desk,0,0,(UWORD)(v->ww*4),&app.screen->RastPort,v->window->LeftEdge,v->window->TopEdge,
    (UWORD)v->ww,(UWORD)v->wh,RECTFMT_ARGB)==(ULONG)(v->ww*v->wh)?DOCK_SNAP_OK:DOCK_SNAP_FAILED;
   if(result==DOCK_SNAP_OK){
    v->lum=dock_luminance(v->desk,v->mask_shown,v->ww*v->wh);
    int was=v->light;choose_colors(i);if(was!=v->light)v->dirty=1;
   }
   snapshot_done(i,request,result,ring);
   return;
  }
  if(action==DOCK_SNAP_CAPTURE){
   unsigned long ring=desk_ring(v->window->LeftEdge,v->window->TopEdge,v->ww,v->wh);
   unsigned request=v->snap.request;
   int result=capture_open(i);
   if(result==DOCK_SNAP_BUSY && hold.index==i){hold_ring=ring;hold_request=request;return;}
   snapshot_done(i,request,result,ring);
   return;
  }
  if(action==DOCK_SNAP_SAMPLE){
   unsigned long ring=transient_open()?v->snap.ring:desk_ring(v->window->LeftEdge,v->window->TopEdge,v->ww,v->wh);
   v->samples++;
   if(ring!=v->snap.ring)note("SNAP dock=%d desktop around changed",app.set.docks[i].id);
   dock_snap_sampled(&v->snap,app.screen_gen,ring,now);
  }
 }
}
/* The wallpaper is part of Wanderer's prefs. A notice means the file was
 * written, not that the new picture is on screen: it starts a watch. */
static void start_wallpaper_notify(void) {
 static char name[]="ENV:SYS/Wanderer/global.prefs";
 if(!(app.wall_port=dock_quiet_port()))return;
 memset(&app.wall_note,0,sizeof app.wall_note);
 app.wall_note.nr_Name=(STRPTR)name;app.wall_note.nr_Flags=NRF_SEND_MESSAGE;app.wall_note.nr_stuff.nr_Msg.nr_Port=app.wall_port;
 app.wall_active=StartNotify(&app.wall_note)!=0;
 note("WALLPAPER notify=%d",app.wall_active);
 if(!app.wall_active){dock_quiet_port_free(app.wall_port);app.wall_port=NULL;}
}
static void handle_wallpaper_notify(void) {
 struct Message *msg;int seen=0;
 while(app.wall_port && (msg=GetMsg(app.wall_port))){ReplyMsg(msg);seen++;}
 if(seen && app.screen)snapshot_invalidate(DOCK_SNAP_WALLPAPER);
}
static void open_prefs(int view);
/* A second start asks for Preferences (see main). Wanderer of ABIv11
 * 2026.09 does not show AppMenuItems, so the Tools menu is no way there. */
static void handle_second_start(void) {
 struct Message *msg;int asked=0;
 while(app.instance_port && (msg=GetMsg(app.instance_port))){ReplyMsg(msg);asked=1;}
 if(asked && app.screen){note("PREFS requested by a second start");open_prefs(0);}
}
/* Widget requests from other programs (include/dockbar/widgets.h): a
 * query, or a package to install. An install that adds a new package may
 * end with the offer to put it on a dock, asked after the reply. */
static struct { int pending; char id[DA_ID], name[80]; } install_offer;
static int widget_on_a_dock(const char *id) {
 for(int v=0;v<app.set.count;v++){const struct DockModel *m=&app.set.docks[v].model;
  for(int i=0;i<m->count;i++)if(m->items[i].kind==DOCK_APPLET && !strcmp(m->items[i].path,id))return 1;}
 return 0;
}
static void handle_widget_requests(void) {
 struct Message *m;
 while(app.widgets_port && (m=GetMsg(app.widgets_port))){
  struct DockbarWidgetsMsg *q=(struct DockbarWidgetsMsg *)m;
  /* Too short to hold the answer: nothing is written into it. */
  if(m->mn_Length<sizeof *q){note("WIDGETS request of %u bytes ignored",(unsigned)m->mn_Length);ReplyMsg(m);continue;}
  q->result=DOCKBAR_WIDGETS_BADREQUEST;q->text[0]=0;
  q->api_min=DA_API_MIN;q->api_max=DA_API_MAX;
  snprintf(q->dockbar_version,sizeof q->dockbar_version,"%s",DOCK_VERSION);
  snprintf(q->variant,sizeof q->variant,"%s",dock_applets_variant());
  snprintf(q->root,sizeof q->root,"%s",dock_applets_root());
  if(q->magic!=DOCKBAR_WIDGETS_MAGIC || q->version!=DOCKBAR_WIDGETS_VERSION){note("WIDGETS request with magic %08lx version %u refused",(unsigned long)q->magic,(unsigned)q->version);ReplyMsg(m);continue;}
  if(q->command==DOCKBAR_WIDGETS_QUERY){q->result=DOCKBAR_WIDGETS_OK;ReplyMsg(m);continue;}
  if(q->command!=DOCKBAR_WIDGETS_INSTALL || !q->path){ReplyMsg(m);continue;}
  struct DockAppletTarget target={"dockbar",dock_applets_variant(),DA_API_MAX};
  struct DockAppletPackage *pkg=calloc(1,sizeof *pkg);
  if(!pkg){q->result=DOCKBAR_WIDGETS_FAILED;snprintf(q->text,sizeof q->text,"Not enough memory");ReplyMsg(m);continue;}
  int r=dock_applet_install_check((const char *)q->path,&target,pkg,q->text,sizeof q->text);
  if(!r && dock_applets_package_busy(pkg->id)){r=DA_INSTALL_BUSY;snprintf(q->text,sizeof q->text,"%.80s is working right now; try again in a moment",pkg->name);}
  /* A fresh scan: a drawer added by hand since the last one must be seen,
   * or the install would take it for free space. */
  struct DockAppletCatalog *now=r?NULL:calloc(1,sizeof *now);
  if(!r && (!now || !dock_applet_scan(dock_applets_root(),&target,now))){r=DA_INSTALL_FAILED;snprintf(q->text,sizeof q->text,"The widget drawer could not be read");}
  if(!r)r=dock_applet_install_copy(pkg,dock_applets_root(),&target,now,q->text,sizeof q->text);
  free(now);
  note("WIDGETS install %s from %.200s: %d %s",pkg->id[0]?pkg->id:"?",(const char *)q->path,r,q->text);
  if(r==DA_INSTALL_OK || r==DA_INSTALL_UPDATED){
   dock_applets_rescan();
   if(r==DA_INSTALL_OK && (q->flags&DOCKBAR_WIDGETS_OFFER_ADD) && !widget_on_a_dock(pkg->id)){
    if(app.readonly || app.prefs.window || app.editor.window)snprintf(q->text,sizeof q->text,"%.80s: choose Add widget in the dock menu to put it on a dock",pkg->name);
    else{install_offer.pending=1;snprintf(install_offer.id,sizeof install_offer.id,"%s",pkg->id);snprintf(install_offer.name,sizeof install_offer.name,"%s",pkg->name);}
   }
  }
  q->result=r;free(pkg);ReplyMsg(m);
 }
 if(install_offer.pending && app.screen && !app.rebuild && lock_screen()){
  static char names[DOCK_MAX_DOCKS][DOCK_NAME+16];const char *list[DOCK_MAX_DOCKS];
  for(int i=0;i<app.set.count;i++){if(app.set.docks[i].name[0])snprintf(names[i],sizeof names[i],"%s",app.set.docks[i].name);else snprintf(names[i],sizeof names[i],"Dock %d",i+1);list[i]=names[i];}
  install_offer.pending=0;
  if(!dock_applet_ui_offer(app.screen,install_offer.id,install_offer.name,list,app.set.count))note("WIDGETS offer window could not open");
  unlock_screen();
 }
}
static void stop_wallpaper_notify(void) {
 /* EndNotify answers the notices still queued; ours are answered on receipt. */
 if(app.wall_active){EndNotify(&app.wall_note);app.wall_active=0;}
 if(app.wall_port){struct Message *msg;while((msg=GetMsg(app.wall_port)))ReplyMsg(msg);dock_quiet_port_free(app.wall_port);app.wall_port=NULL;}
}

/* ---- painting --------------------------------------------------------- */

static void grip_draw(struct DockView *v) {
 const struct DockRect *g=&v->layout.grip;
 if(!g->w)return;
 /* Without a bar the grip is a small handle of its own, so it is found and
  * grabbed on any desktop. */
 if(v->nobg){
  int along=v->layout.orientation?g->w-16:g->h-16,thick=12;
  if(along<16)along=16;
  if(v->layout.orientation)box(v,g->x+(g->w-along)/2,g->y+4,along,thick,5,v->c->fill_top);
  else box(v,g->x+4,g->y+(g->h-along)/2,thick,along,5,v->c->fill_top);
 }
 /* On a see-through bar the grey dots vanish into the desktop; a dark
  * under-dot keeps the handle findable on any picture. */
 int shadow=v->transparent || v->nobg;
 unsigned long dot=shadow && !v->light?0xd8dde0:v->c->grip;
 for(int a=0;a<3;a++)for(int b=0;b<2;b++){
  int x=v->layout.orientation?g->x+g->w/2-7+a*5:g->x+7+b*4;
  int y=v->layout.orientation?g->y+7+b*4:g->y+g->h/2-7+a*5;
  if(shadow)box(v,x+1,y+1,2,2,0,v->c->grip_shadow);
  box(v,x,y,2,2,0,dot);
 }
}
static int is_target(int index,int i) {
 return app.drag.own.armed && app.drag.own.target_container==code(index,0) && app.drag.own.target_index==i;
}
static int marks_of(int index) {
 const struct DockView *v=&app.views[index];
 int target=app.drag.own.armed && app.drag.own.target_container==code(index,0)?app.drag.own.target_index:-1;
 return ((v->hover+1)&0xff)|((v->pressed+1)&0xff)<<8|((target+1)&0xff)<<16|app.all_selected<<24;
}
/* Everything a dock shows, composed in v->pixels (and v->mask without a
 * bar); no window needed. Colours come only from v->c, highlight boxes only
 * from the layout's highlight rectangles. */
static void render_view(int index) {
 struct DockView *v=&app.views[index];struct DockModel *m=model_of(index);struct DockLayout *l=&v->layout;
 int dx=l->orientation?0:-v->scroll,dy=l->orientation?-v->scroll:0,th=app.font->tf_YSize;
 int partial=clip_x0>0 || clip_y0>0 || clip_x1<v->ww || clip_y1<v->wh;
 if(!partial)v->drawn_marks=marks_of(index);
 if(partial){
  for(int y=clip_y0;y<clip_y1;y++){
   size_t at=(size_t)y*v->ww+clip_x0,n=(size_t)(clip_x1-clip_x0);
   if(v->nobg){memset(v->pixels+4*at,0,4*n);memset(v->mask+at,0,n);}
   else memcpy(v->pixels+4*at,v->background+4*at,4*n);
  }
 }
 else if(v->nobg){memset(v->pixels,0,(size_t)v->ww*v->wh*4);memset(v->mask,0,(size_t)v->ww*v->wh);}
 else memcpy(v->pixels,v->background,(size_t)v->ww*v->wh*4);
 grip_draw(v);
 for(int i=0;i<m->count;i++){
  struct DockItem *item=&m->items[i];struct Visual *vis=&v->visual[i];
  if(!slot_visible(v,i)){dock_fade_clear(&vis->fade);continue;}
  struct DockSlot sh=l->slots[i],*s=&sh;
  dock_layout_shift(s,dx,dy);
  /* A partial redraw skips what lies outside its box (text is costly). */
  if(partial && (s->slot.x>=clip_x1 || s->slot.y>=clip_y1 || s->slot.x+s->slot.w<=clip_x0 || s->slot.y+s->slot.h<=clip_y0))continue;
  if(item->kind==DOCK_SEPARATOR){
   /* A quarter of the bar's thickness clear at each end: 14 px on an icon
    * bar as before, less on a slim bar of text buttons. */
   int in=(l->orientation?s->slot.w:s->slot.h)/4;if(in>14)in=14;
   if(l->orientation)box(v,s->slot.x+in,s->slot.y+5,s->slot.w-2*in,1,0,v->c->separator);
   else box(v,s->slot.x+5,s->slot.y+in,1,s->slot.h-2*in,0,v->c->separator);
   continue;
  }
  if(!s->hittable)continue;
  const struct DockRect *h=&s->highlight;
  /* The hover effect's frame: where the icon is drawn (lifted or grown)
   * and the highlight around it. Clicks still use the rest layout. */
  struct DockRect at=s->icon,fx_hl;int grow=0;
  if(v->fx.count==l->count && dock_hover_item(l,i)){
   grow=v->fx.grown[i];at=v->fx.icon[i];fx_hl=v->fx.highlight[i];
   at.x+=dx;at.y+=dy;fx_hl.x+=dx;fx_hl.y+=dy;h=&fx_hl;
  }
  /* Without a bar the window keeps one shape (see update_shape's absence
   * below): highlights would need a new one, so they are not drawn; the
   * name shows as a tooltip instead. */
  unsigned long hl=is_target(index,i)?v->c->target:i==v->pressed?v->c->pressed:i==v->hover?v->c->hover:0;
  if(s->button){
   /* A text button's area is part of the shape even without a bar (see
    * prepare: it is rendered once with every plate drawn), so hover and
    * pressed show on it. Plain buttons have a plate only then; where the
    * plate is absent a bar-less dock shows its desktop picture. */
   int plates=app.set.docks[index].look.buttons;
   unsigned long plate=hl?hl:plates?(v->nobg?v->c->fill_top:v->c->button):app.all_selected?v->c->fill_top:0;
   if(plate)box(v,h->x,h->y,h->w,h->h,DOCK_HIGHLIGHT_RADIUS,plate);
  } else if(!v->nobg && hl && h->w)box(v,h->x,h->y,h->w,h->h,DOCK_HIGHLIGHT_RADIUS,hl);
  /* A widget keeps a small plate of its own: its text changes, its shape
   * must not. */
  if(v->nobg && (item->kind==DOCK_CLOCK || item->kind==DOCK_APPLET) && h->w)box(v,h->x,h->y,h->w,h->h,DOCK_HIGHLIGHT_RADIUS,v->c->fill_top);
  if(item->kind==DOCK_APPLET){
   struct DockAppletRuntime *r=vis->applet;int y=s->content.y;
   char caption[160];fit(caption,sizeof caption,dock_applet_compact(r),s->content.w-APPLET_TEXT_X);
   /* An animation frame (partial) keeps the place found by the last whole
    * drawing: its caption is the one on screen. */
   if(!partial){int used=APPLET_TEXT_X+text_width(caption);vis->applet_dx=used<s->content.w?(s->content.w-used)/2:0;}
   int x=s->content.x+vis->applet_dx;
   if(vis->sprite.pixels){
    unsigned char *frame=vis->sprite.pixels+(size_t)vis->sprite.frame*32*32*4;
    for(int sy=0;sy<32;sy++)for(int sx=0;sx<32;sx++){
     unsigned char *p=frame+4*(sy*32+sx);
     if(p[0])put(v,x+sx,y+sy,((unsigned long)p[1]<<16)|((unsigned long)p[2]<<8)|p[3],p[0]);
    }
   }else if(vis->image.svg || vis->image.native.object)put_image(v,&vis->image,x,y,0);
   else {box(v,x+4,y+5,24,20,8,v->c->text_muted);put_text(v,x+11,y+6,"?",v->c->text);}
   /* An animation frame redraws the icon only; the text beside it stays. */
   if(!partial || x+APPLET_TEXT_X<clip_x1)put_text(v,x+APPLET_TEXT_X,y+(s->content.h-th)/2,caption,v->c->text);
   if(r && !r->package.manual && r->state.snapshot.status==DA_STALE)put_text(v,x+26,y,"*",v->c->text);
   if(vis->fade.from && vis->fade_pending){vis->fade.started=dock_applets_now();vis->fade_pending=0;}
   if(vis->fade.from && s->content.x>=0 && y>=0 && s->content.x+s->content.w<=v->ww && y+s->content.h<=v->wh)
    dock_fade_apply(&vis->fade,v->pixels+4*((size_t)y*v->ww+s->content.x),v->ww*4,dock_applets_now());
  }else if(item->kind==DOCK_CLOCK){
   int lines=vis->widget.lines?vis->widget.lines:1;
   for(int k=0;k<vis->widget.lines && k<lines;k++){
    const char *t=vis->widget.line[k];
    put_text(v,s->content.x+(s->content.w-text_width(t))/2,s->content.y+k*(th+2),t,k?v->c->text_muted:v->c->text);
   }
  } else if(item->kind==DOCK_GROUP && vis->use_minis && s->icon.w){
   /* The preview of a group lifts with Lift; it is not scaled for Magnify
    * and keeps its size in the middle of the grown box. */
   struct DockRect b=at;b.x+=grow/2;b.y+=grow/2;b.w-=grow;b.h-=grow;
   box(v,b.x,b.y,b.w,b.h,8,v->c->preview_border);
   box(v,b.x+1,b.y+1,b.w-2,b.h-2,7,v->c->preview_fill);
   int half=b.w/2;
   for(int j=0;j<vis->minis;j++)put_image(v,&vis->mini[j],b.x+3+(j%2)*(half-1),b.y+3+(j/2)*(half-1),0);
  } else if(s->icon.w){
   int sel=i==v->pressed || app.all_selected,size=0;
   unsigned char *a=grow?mag_picture(&vis->mag,s->icon.w,grow,sel,&size):NULL;
   if(a)put_argb(v,a,size,at.x+(at.w-size)/2,at.y+(at.h-size)/2);
   else put_image(v,&vis->image,at.x+(at.w-vis->image.size)/2,at.y+(at.h-vis->image.size)/2,sel);
  }
  if(s->text.w){
   char caption[DOCK_LABEL+4];item_caption(item,caption,sizeof caption);
   put_text(v,s->text.x,s->text.y,caption,v->c->text);
  }
  /* A group's mark: a small triangle pointing down, like a menu. */
  if(s->mark.w)for(int k=0;k<4 && 2*k<s->mark.w;k++)box(v,s->mark.x+k,s->mark.y+s->mark.h/2-2+k,s->mark.w-2*k,1,0,v->c->text_muted);
  if(s->label.w){
   char caption[DOCK_LABEL+4];fit(caption,sizeof caption,item->label,s->label.w-4);
   put_text(v,s->label.x+(s->label.w-text_width(caption))/2,s->label.y,caption,v->c->text);
  }
 }
 /* Overflow arrows: a small triangle at each end that has more items. */
 for(int side=-1;side<=1;side+=2){
  if(side<0?v->scroll<=0:v->scroll>=v->scroll_max)continue;
  int a=side<0?l->start:win_main(v)-DOCK_ARROW,c=(l->orientation?v->ww:v->wh)/2;
  for(int k=0;k<5;k++){
   int along=a+(side<0?5+k:10-k),half=k+1;
   if(l->orientation)box(v,c-half,along,2*half,1,0,v->c->arrow);else box(v,along,c-half,1,2*half,0,v->c->arrow);
  }
 }
 /* Insertion mark while an item is dragged over this dock. */
 if(!v->nobg && v->window && app.drag.own.kind==1 && app.drag.own.active && !app.drag.own.armed && !dock_locked(index) &&
    under_view==index && !inside(app.panel.window,app.screen->MouseX,app.screen->MouseY)){
  int before=view_before(v,app.screen->MouseX-v->window->LeftEdge,app.screen->MouseY-v->window->TopEdge);
  int at=dock_layout_mark(l,before)-v->scroll;
  if(at>=l->start-2 && at<=win_main(v)){
   if(l->orientation)box(v,5,at,v->ww-10,2,0,v->c->mark);else box(v,at,5,2,v->wh-10,0,v->c->mark);
  }
 }
 /* A dock without a bar that shows nothing would have an empty shape,
  * which AROS cannot take back (tests/mark_probe.c): keep a handle. */
 if(v->nobg){
  int any=0;for(int k=0;k<v->ww*v->wh && !any;k++)any=v->mask[k];
  if(!any && !partial)box(v,2,2,l->orientation?v->ww-4:12,l->orientation?12:v->wh-4,5,v->c->fill_top);
  /* Shape pixels the current state leaves empty show the desktop as it was
   * when the dock was last hidden for a picture. */
  if(v->extra && !v->compose)for(int y=partial?clip_y0:0;y<(partial?clip_y1:v->wh);y++)for(int x=partial?clip_x0:0;x<(partial?clip_x1:v->ww);x++){
   int k=y*v->ww+x;
   if(v->mask_shown[k] && !v->mask[k]){memcpy(v->pixels+4*k,v->desk+4*k,4);v->pixels[4*k]=255;}
  }
 }
}
/* The window shape of a dock without a bar: its drawn pixels, row runs. */
static struct Region *mask_region(struct DockView *v,const unsigned char *mask) {
 struct Region *r=NewRegion();
 if(!r)return NULL;
 for(int y=0;y<v->wh;y++){
  const unsigned char *row=mask+y*v->ww;
  for(int x=0;x<v->ww;){
   if(!row[x]){x++;continue;}
   int e=x;while(e<v->ww && row[e])e++;
   struct Rectangle rect={(WORD)x,(WORD)y,(WORD)(e-1),(WORD)y};
   if(!OrRectRegion(r,&rect)){DisposeRegion(r);return NULL;}
   x=e;
  }
 }
 return r;
}
/* A dock without a bar keeps the shape it opened with until it is rebuilt.
 * Changing a window's shape at run time is not safe over Wanderer's desktop
 * on ABIv11 2026.09: where the shape shrinks the desktop is not repainted
 * (old pixels stay), and with the shape changing every second while
 * Wanderer opened a drawer the whole system stopped responding once
 * (docs: v03 evidence). Opening and closing windows has neither problem. */
static void draw_view(int index) {
 struct DockView *v=&app.views[index];
 if(!v->window)return;
 /* The old box would clip the new picture; drawn when the resize ends. */
 if(v->resizing){v->dirty=1;return;}
 render_view(index);
 if(trace_present)note("PAINT dock=%d full %dx%d %s",app.set.docks[index].id,v->ww,v->wh,v->compose?"composed":"pictured");
 if(dump_frames)dump_buffer(index,v->pixels,v->ww,v->wh,0,0,v->ww,v->wh);
 /* A composed window takes the ARGB pixels as they are, alpha included;
  * a bitmap of the screen's format would drop the alpha. */
 uint64_t t0=dock_applets_now_us();
 if(v->compose)WritePixelArray(v->pixels,0,0,(UWORD)(v->ww*4),v->window->RPort,0,0,(UWORD)v->ww,(UWORD)v->wh,RECTFMT_ARGB);
 else {
  WritePixelArray(v->pixels,0,0,(UWORD)(v->ww*4),&v->rp,0,0,(UWORD)v->ww,(UWORD)v->wh,RECTFMT_ARGB);
  BltBitMapRastPort(v->bitmap,0,0,v->window->RPort,0,0,v->ww,v->wh,0xc0);
 }
 present_us+=dock_applets_now_us()-t0;
 v->dirty=0;v->part_dirty=0;
}
/* Only v->part: composed and copied to the window alone. */
static void draw_part(int index) {
 struct DockView *v=&app.views[index];struct DockRect r=v->part;
 v->part_dirty=0;
 if(!v->window || v->resizing){v->dirty=1;return;}
 if(r.x<0){r.w+=r.x;r.x=0;}
 if(r.y<0){r.h+=r.y;r.y=0;}
 if(r.x+r.w>v->ww)r.w=v->ww-r.x;
 if(r.y+r.h>v->wh)r.h=v->wh-r.y;
 if(r.w<=0 || r.h<=0)return;
 clip_x0=r.x;clip_y0=r.y;clip_x1=r.x+r.w;clip_y1=r.y+r.h;
 render_view(index);
 clip_x0=clip_y0=0;clip_x1=clip_y1=1<<30;
 if(v->hover_part){v->drawn_marks=marks_of(index);v->hover_part=0;}
 if(trace_present)note("PAINT dock=%d part %d,%d %dx%d %s",app.set.docks[index].id,r.x,r.y,r.w,r.h,v->compose?"composed":"pictured");
 if(dump_frames)dump_buffer(index,v->pixels,v->ww,v->wh,r.x,r.y,r.w,r.h);
 uint64_t t0=dock_applets_now_us();
 if(v->compose)WritePixelArray(v->pixels,r.x,r.y,(UWORD)(v->ww*4),v->window->RPort,r.x,r.y,(UWORD)r.w,(UWORD)r.h,RECTFMT_ARGB);
 else {
  WritePixelArray(v->pixels,r.x,r.y,(UWORD)(v->ww*4),&v->rp,r.x,r.y,(UWORD)r.w,(UWORD)r.h,RECTFMT_ARGB);
  BltBitMapRastPort(v->bitmap,r.x,r.y,v->window->RPort,r.x,r.y,r.w,r.h,0xc0);
 }
 present_us+=dock_applets_now_us()-t0;
}
/* A widget's icon area needs its next frame. --full-redraw: the whole dock,
 * as before (for comparing the cost). */
static int dock_partial_redraw=1;
/* Adds a box (window coordinates) to what the next pass draws of a dock. */
static void mark_rect(int index,struct DockRect r) {
 struct DockView *v=&app.views[index];
 if(r.w<=0 || r.h<=0 || v->dirty)return;
 /* A highlight that changed since the last whole drawing (a press ended
  * without a redraw, say) would be painted around the box only. */
 if(!dock_partial_redraw || (marks_of(index)!=v->drawn_marks && !v->hover_part)){v->dirty=1;return;}
 if(!v->part_dirty){v->part=r;v->part_dirty=1;return;}
 int x0=v->part.x<r.x?v->part.x:r.x,y0=v->part.y<r.y?v->part.y:r.y;
 int x1=v->part.x+v->part.w>r.x+r.w?v->part.x+v->part.w:r.x+r.w,y1=v->part.y+v->part.h>r.y+r.h?v->part.y+v->part.h:r.y+r.h;
 v->part.x=x0;v->part.y=y0;v->part.w=x1-x0;v->part.h=y1-y0;
}
/* Item i's hover box as drawn now (rest place, and the effect's frame),
 * in window coordinates; zero size for no item. */
static struct DockRect hover_box(struct DockView *v,int i) {
 struct DockRect u={0,0,0,0};
 if(i<0 || i>=v->layout.count)return u;
 const struct DockSlot *s=&v->layout.slots[i];
 struct DockRect r[3]={s->highlight,s->icon,s->highlight};int n=1;
 if(v->fx.count==v->layout.count){r[1]=v->fx.icon[i];r[2]=v->fx.highlight[i];n=3;}
 for(int k=0;k<n;k++){
  if(r[k].w<=0 || r[k].h<=0)continue;
  if(!u.w){u=r[k];continue;}
  int x0=u.x<r[k].x?u.x:r[k].x,y0=u.y<r[k].y?u.y:r[k].y;
  int x1=u.x+u.w>r[k].x+r[k].w?u.x+u.w:r[k].x+r[k].w,y1=u.y+u.h>r[k].y+r[k].h?u.y+u.h:r[k].y+r[k].h;
  u.x=x0;u.y=y0;u.w=x1-x0;u.h=y1-y0;
 }
 if(v->layout.orientation)u.y-=v->scroll;else u.x-=v->scroll;
 return u;
}
/* The pointer moved from item 'was' to 'now' (-1: none). When the window
 * shows exactly the marks it had (no press or drop target changed since),
 * only the two hover boxes are drawn again; otherwise the whole dock. */
static void hover_changed(int index,int now) {
 struct DockView *v=&app.views[index];int was=v->hover;
 int clean=!v->dirty && (!v->part_dirty || v->hover_part) && marks_of(index)==v->drawn_marks;
 v->hover=now;
 if(!dock_partial_redraw || !clean){v->dirty=1;return;}
 v->hover_part=1;
 struct DockRect a=hover_box(v,was),b=hover_box(v,now);
 if(a.w)mark_rect(index,a);
 if(b.w)mark_rect(index,b);
 if(!v->part_dirty)v->hover_part=0;   /* neither had a box: nothing to draw */
 if(!v->part_dirty)v->drawn_marks=marks_of(index);
}
static void mark_part(int index,int item) {
 struct DockView *v=&app.views[index];struct DockSlot s=v->layout.slots[item];
 dock_layout_shift(&s,v->layout.orientation?0:-v->scroll,v->layout.orientation?-v->scroll:0);
 struct DockRect r={s.content.x+v->visual[item].applet_dx,s.content.y,32,32};
 mark_rect(index,r);
}

/* ---- hover effects ---------------------------------------------------- */

/* Windows over the docks during which icons rest (as the hover highlight
 * does not follow the pointer either while they are open). */
static int effects_held(void) {
 return app.menu.window || app.panel.window || app.drawers.window || dock_applet_ui_window() ||
        app.editor.window || app.prefs.window;
}
/* One step of a dock's hover effect: progress towards where the pointer and
 * any drag say it should be, at the ramp's rate, and the boxes that changed
 * marked for the next pass, old place and new. Returns 1 while it moves. */
static int effects_step(int index,unsigned now) {
 struct DockView *v=&app.views[index];struct DockLayout *l=&v->layout;
 if(!v->window || v->resizing || !v->effect || v->fx.count!=l->count)return 0;
 unsigned elapsed=now-v->effect_at;v->effect_at=now;
 /* After a stall the effect still takes a few frames, not one jump. */
 if(elapsed>DOCK_HOVER_RAMP_MS/3)elapsed=DOCK_HOVER_RAMP_MS/3;
 int still=dock_drag_state_busy(&app.drag) || effects_held() || app.rebuild,moving=0;
 struct DockMagnifyFrame f;
 if(v->effect==DOCK_HOVER_LIFT){
  int eased[DOCK_MAX_ITEMS]={0};
  for(int i=0;i<l->count;i++){
   int target=!still && i==v->hover && dock_hover_item(l,i)?1000:0;
   v->progress[i]=dock_hover_ramp(v->progress[i],target,elapsed);
   moving|=v->progress[i]!=target;eased[i]=dock_hover_ease(v->progress[i]);
  }
  if(!dock_lift_frame(l,v->lift_sign,eased,&f))return 0;
 } else {
  int sx=app.screen->MouseX,sy=app.screen->MouseY,on=!still && under_view==index;
  /* Going out, icons shrink where they are rather than follow the pointer. */
  if(on){int x=sx-v->window->LeftEdge,y=sy-v->window->TopEdge;to_layout(v,&x,&y);v->along=l->orientation?y:x;}
  int target=on?1000:0;
  v->strength=dock_hover_ramp(v->strength,target,elapsed);moving=v->strength!=target;
  struct DockMagnifyParams p={DOCK_MAGNIFY_PERCENT,0};
  if(!dock_magnify_frame(l,&v->style,&p,v->along,dock_hover_ease(v->strength),&f))return 0;
 }
 int dx=l->orientation?0:-v->scroll,dy=l->orientation?-v->scroll:0;
 for(int i=0;i<l->count;i++){
  struct DockRect r=dock_hover_damage(&v->fx,&f,i);
  if(!r.w)continue;
  r.x+=dx;r.y+=dy;mark_rect(index,r);
 }
 v->fx=f;
 return moving;
}

/* ---- tooltip ---------------------------------------------------------- */

/* Name tooltips wait for the pointer to rest on an item, so that sweeping
 * across a dock does not open and close a window per icon. */
#define TIP_DELAY_MS 500u
#define TIP_SWITCH_MS 150u
static int tip_view=-1,tip_item=-1;
static unsigned tip_since;
static void update_tooltip(void) {
 const char *label=NULL;static char applet_tip[500];int view=-1,x=0,y=0,w=0,h=0;
 if(app.editor.window || app.prefs.window || app.menu.window || app.drawers.window || dock_applet_ui_window() || app.drag.own.kind==2){close_tooltip();tip_view=-1;return;}
 /* A button held for anything but our own drag, typically an icon dragged
  * from Wanderer: no new tooltips, and an open one stays until the release
  * (see foreign_drag). The hover mark on the bar shows the drop target. */
 if(foreign_drag()){tip_view=-1;return;}
 if(app.drag.own.active){label=app.drag.own.armed?"Release to drop here":"Drag between items to reorder";view=under_view;}
 else if(app.status[0] && (int)(app.status_until-milliseconds())>0){label=app.status;view=app.status_view;}
 else for(int i=0;i<app.set.count;i++){
  struct DockView *v=&app.views[i];
  if(v->window && v->hover>=0){
   /* A dock without a bar draws no highlight: its tooltip shows the name
    * unless the labels are always on. */
   int labels=app.set.docks[i].look.labels;
   const struct DockItem *it=&model_of(i)->items[v->hover];
   if(v->layout.slots[v->hover].button){
    /* A text button shows its name already: the tooltip only for a
     * shortened one, with the full name. */
    char caption[DOCK_LABEL+4];
    if(!item_caption(it,caption,sizeof caption))break;
   } else if(it->kind!=DOCK_APPLET && labels!=DOCK_LABELS_HOVER && !(v->nobg && labels==DOCK_LABELS_NONE))break;
   if(i!=tip_view || v->hover!=tip_item){tip_view=i;tip_item=v->hover;tip_since=milliseconds();}
   if(milliseconds()-tip_since<(app.tooltip?TIP_SWITCH_MS:TIP_DELAY_MS))return;
   label=model_of(i)->items[v->hover].label;
   if(it->kind==DOCK_APPLET){struct DockAppletRuntime *r=v->visual[v->hover].applet;
   /* An action widget: its name and what the last action said. */
   if(r && r->package.available && r->package.manual){snprintf(applet_tip,sizeof applet_tip,"%.60s%s%.200s",r->package.name,r->state.snapshot.message[0]?": ":"",r->state.snapshot.message);label=applet_tip;}
   else if(r && !r->state.has_data){snprintf(applet_tip,sizeof applet_tip,"%.60s: %.40s%s%.200s",r->package.name,dock_applet_primary(r),r->state.snapshot.message[0]?" | ":"",r->state.snapshot.message);label=applet_tip;}
   else if(r){double age=difftime(time(NULL),(time_t)r->state.snapshot.updated);snprintf(applet_tip,sizeof applet_tip,"%.100s | %.140s | %.70s | %.0f min%s",dock_applet_secondary(r),dock_applet_field(&r->state.snapshot,r->package.tooltip),r->state.snapshot.message,age>0&&r->state.has_data?age/60:0,r->state.snapshot.status==DA_STALE?" (stale)":"");label=applet_tip;}}
   view=i;slot_on_screen(i,v->hover,&x,&y,&w,&h);break;
  }
 }
 if(!label)tip_view=-1;
 if(!label || !label[0] || view<0 || !app.views[view].window || app.panel.window){close_tooltip();return;}
 struct Window *dw=app.views[view].window;
 if(!w){x=dw->LeftEdge;y=dw->TopEdge;w=dw->Width;h=dw->Height;}
 int width=text_width(label)+16,height=app.font->tf_YSize+10,left,top;
 if(app.views[view].layout.orientation){
  left=dw->LeftEdge+dw->Width+5;if(left+width>app.screen->Width)left=dw->LeftEdge-width-5;
  top=y+h/2-height/2;
 } else {
  left=x+w/2-width/2;top=dw->TopEdge-height-5;
  if(top<screen_top())top=dw->TopEdge+dw->Height+5;
 }
 if(left<0)left=0;
 if(left+width>app.screen->Width)left=app.screen->Width-width;
 if(top<screen_top())top=screen_top();
 if(top+height>app.screen->Height)top=app.screen->Height-height;
 if(app.tooltip && !strcmp(app.tooltip_text,label) && app.tooltip->LeftEdge==left && app.tooltip->TopEdge==top)return;
 close_tooltip();
 if(!lock_screen())return;
 
 app.tooltip=OpenWindowTags(NULL,WA_PubScreen,(IPTR)app.screen,WA_Left,left,WA_Top,top,WA_Width,width,WA_Height,height,
  WA_Borderless,TRUE,WA_Activate,FALSE,WA_RMBTrap,TRUE,WA_SmartRefresh,TRUE,TAG_DONE);
 unlock_screen();
 if(trace_present)note("WIN open tooltip %p %d,%d %dx%d",(void*)app.tooltip,left,top,width,height);
 if(!app.tooltip)return;
 snprintf(app.tooltip_text,sizeof app.tooltip_text,"%s",label);
 const struct DockColors *c=app.views[view].c?app.views[view].c:dock_colors(0);int light=app.views[view].light==1;
 struct RastPort *rp=app.tooltip->RPort;FillPixelArray(rp,0,0,width,height,c->popup_border);FillPixelArray(rp,1,1,width-2,height-2,c->popup_fill);
 if(app.smooth){
  /* Smooth text over the tooltip's fill, composed and written at once. */
  struct DockTextImage im;const unsigned char *cov=dock_text_render(&app.textr,label,&im);
  /* The image placed by its origin; only what falls inside the border. */
  int ox=8-im.left,oy=(height-dock_text_height(&app.textr))/2-im.top;
  int x0=ox<1?1:ox,y0=oy<1?1:oy,x1=ox+im.w>width-1?width-1:ox+im.w,y1=oy+im.h>height-1?height-1:oy+im.h;
  unsigned char *px=cov && x1>x0 && y1>y0?malloc((size_t)(x1-x0)*(y1-y0)*4):NULL;
  if(px){
   for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){
    unsigned long col=dock_mix(c->popup_fill,c->text,cov[(y-oy)*im.w+(x-ox)]/255.0f);
    unsigned char *q=px+4*((y-y0)*(x1-x0)+(x-x0));
    q[0]=255;q[1]=(unsigned char)(col>>16);q[2]=(unsigned char)(col>>8);q[3]=(unsigned char)col;
   }
   WritePixelArray(px,0,0,(UWORD)((x1-x0)*4),rp,(UWORD)x0,(UWORD)y0,(UWORD)(x1-x0),(UWORD)(y1-y0),RECTFMT_ARGB);
   free(px);
  }
 } else {SetFont(rp,app.font);text(rp,8,5+app.font->tf_Baseline,label,app.pen_text[light]);}
}

/* ---- committing changes ---------------------------------------------- */

static int commit(const char *action,int reopen_view,int reopen_container) {
 if(app.readonly){feedback(0,"Saved settings preserved; editing disabled");return 0;}
 /* Only growth is refused: a dock that no longer fits a smaller screen
  * scrolls, and must stay editable so the user can shorten it. */
 for(int i=0;i<next_set.count;i++){
  int was=dock_set_find(&app.set,next_set.docks[i].id);
  int grew=was<0 || next_set.docks[i].model.count>app.set.docks[was].model.count;
  if(grew && !fits(&next_set,i,DOCK_ICON_MIN)){feedback(i,"Dock is full");return 0;}
 }
 if(!dock_set_save(&next_set,app.config)){feedback(0,"Could not save settings");note("SAVE failed action=%s file=%s",action,app.config);return 0;}
 app.set=next_set;app.rebuild=1;app.reopen_view=reopen_view;app.reopen_container=reopen_container;
 feedback(reopen_view>=0?reopen_view:0,action);note("EDIT %s docks=%d",action,app.set.count);return 1;
}
/* Widgets in the item about to be removed (a group takes its members with
 * it); their data is deleted only after the configuration without them was
 * saved, see forget_removed. */
static struct{int n;char package[DOCK_MAX_ITEMS+1][DOCK_PATH],instance[DOCK_MAX_ITEMS+1][DOCK_DIR];}removed;
static void removed_add(const struct DockItem *it) {
 if(it->kind!=DOCK_APPLET || removed.n>DOCK_MAX_ITEMS)return;
 snprintf(removed.package[removed.n],DOCK_PATH,"%s",it->path);snprintf(removed.instance[removed.n],DOCK_DIR,"%s",it->dir);removed.n++;
}
static void note_removed(struct DockModel *m,int container,int index) {
 int n=0;struct DockItem *items=dock_items(m,container,&n);
 removed.n=0;
 if(!items || index<0 || index>=n)return;
 removed_add(&items[index]);
 if(items[index].kind==DOCK_GROUP){int k=0;struct DockItem *members=dock_items(m,items[index].group,&k);for(int i=0;members && i<k;i++)removed_add(&members[i]);}
}
static void forget_removed(void) {
 for(int i=0;i<removed.n;i++)if(dock_applets_forget(removed.package[i],removed.instance[i]))note("WIDGET %s/%s removed; data deleted once it is idle",removed.package[i],removed.instance[i]);
 removed.n=0;
}
/* Any item of the set, groups included, that uses this instance's data: the
 * instance itself, or a copy made from it that has not saved its own yet. */
static int set_uses(struct DockSet *set,const char *package,const char *instance) {
 size_t len=strlen(instance);
 for(int vi=0;vi<set->count;vi++)for(int c=0;c<=DOCK_MAX_GROUPS;c++){
  int n=0;struct DockItem *items=dock_items(&set->docks[vi].model,c,&n);
  for(int i=0;items && i<n;i++){
   const struct DockItem *it=&items[i];const char *dash;
   if(it->kind!=DOCK_APPLET || strcmp(it->path,package))continue;
   if(!strcmp(it->dir,instance))return 1;
   if(!strncmp(it->dir,"copy-",5) && (dash=strchr(it->dir+5,'-')) && !strncmp(dash+1,instance,len) && !dash[1+len])return 1;
  }
 }
 return 0;
}
static struct DockSet prefs_original;
static int widget_referenced(const char *package,const char *instance) {
 return set_uses(&app.set,package,instance) || (app.prefs.window && set_uses(&prefs_original,package,instance));
}
/* A widget may be changed (settings, removal) only while its dock could be
 * edited. Its card and Refresh stay available on a locked dock. */
static int widget_editable(struct DockAppletRuntime *r,const char **why) {
 for(int vi=0;vi<app.set.count;vi++)for(int ai=0;ai<app.set.docks[vi].model.count;ai++){
  const struct DockItem *it=&app.set.docks[vi].model.items[ai];
  if(it->kind!=DOCK_APPLET || strcmp(it->path,r->state.package) || strcmp(it->dir,r->state.instance))continue;
  if(app.readonly){*why="Editing unavailable";return 0;}
  if(app.prefs.window){*why="Close Preferences first";return 0;}
  if(dock_locked(vi)){*why="Dock is locked (see Preferences)";note("LOCKED dock=%d widget edit refused",app.set.docks[vi].id);return 0;}
  return 1;
 }
 return 1;
}
static int editing_blocked(int view) {
 if(app.readonly || app.prefs.window || app.editor.window){feedback(view,app.prefs.window?"Close Preferences first":"Editing unavailable");return 1;}
 if(dock_locked(view)){feedback(view,"Dock is locked (see Preferences)");note("LOCKED dock=%d edit refused",app.set.docks[view].id);return 1;}
 return 0;
}

/* ---- object identity --------------------------------------------------- */

/* The model compares path text, which misses aliases (SYS: and its volume
 * name, assigns). Before adding or moving into a container, ask DOS. The
 * same program may still be in other containers and other docks. */
static int already_in(int view,int container,const char *path,int skip) {
 int n=0;struct DockItem *items=dock_items(model_of(view),container,&n);
 for(int i=0;items && i<n;i++){
  if(i==skip || (items[i].kind!=DOCK_APP && items[i].kind!=DOCK_DRAWER))continue;
  int same=dock_same_object(items[i].path,path);
  note("IDENTITY %s vs %s same=%d",items[i].path,path,same);
  if(same)return 1;
 }
 return 0;
}

/* ---- Wanderer drops --------------------------------------------------- */

/* Adds one dropped object as a shortcut to next_set; *before moves on after
 * each one so that several files keep their order. 1 added. */
static int add_dropped(int view,int container,const char *path,int *before) {
 int drawer=dock_object_is_drawer(path)==1;
 /* Groups hold programs; a drawer or disk goes onto the bar itself. */
 if(drawer && container){feedback(view,"Drawers stay on the bar");return 0;}
 if(already_in(view,container,path,-1)){feedback(view,"Already in this place");note("DROP duplicate object path=%s",path);return 0;}
 struct DockModel *m=&next_set.docks[view].model;
 int n=0;dock_items(m,container,&n);if(*before<0 || *before>n)*before=n;
 int result=dock_add_kind(m,container,drawer?DOCK_DRAWER:DOCK_APP,path,*before);
 if(result==0){feedback(view,"Already in this place");note("DROP duplicate path=%s",path);return 0;}
 if(result<0){feedback(view,"Group or dock is full");return 0;}
 (*before)++;
 note("DROP path=%s dock=%d container=%d drawer=%d",path,app.set.docks[view].id,container,drawer);
 return 1;
}
static void start_command(int view,const struct DockItem *item,const char *const *files,int count);
/* Files dropped on the middle of a program or command that opens them. This
 * uses the dock, it does not change it, so a locked dock allows it. */
/* A Shell program with dropped files: started the way Workbench starts it
 * (asynchronous, an output window that closes when the program has none
 * to show), but with every name escaped. */
/* Its current directory is the program's own drawer, as when Wanderer
 * starts it (measured: Wanderer gives the program's drawer, while
 * workbench.library called from a dock gives the dock's own), so that
 * relative names mean the same. 1 started, 0 failed, -1 refused: the
 * Shell would misread the line (see dock_command_line_safe). */
static int start_shell_program(const char *program,long stack,const char *const *files,int count) {
 static char line[8*DOCK_PATH];
 if(!dock_command_program_files(program,files,count,line,sizeof line))return 0;
 if(!dock_command_line_safe(line)){note("SHELL refused misread line=%s",line);return -1;}
 BPTR lock=Lock((CONST_STRPTR)program,SHARED_LOCK),dir=lock?ParentDir(lock):BNULL;
 if(lock)UnLock(lock);
 if(!dir)return 0;
 /* No home drawer and no current directory while the console opens: a
  * CON: window keeps a copy of both after the program has ended (measured,
  * ABIv11 2026.09), and started from Workbench Dockbar's current directory
  * is its own drawer, which then stayed "in use" until a reboot. */
 struct Process *me=(struct Process *)FindTask(NULL);BPTR home=me->pr_HomeDir;me->pr_HomeDir=BNULL;
 BPTR cwd=CurrentDir(BNULL);
 BPTR in=Open((CONST_STRPTR)"CON:////Output Window/CLOSE/AUTO/WAIT",MODE_OLDFILE);
 CurrentDir(cwd);
 if(!in){me->pr_HomeDir=home;UnLock(dir);return 0;}
 me->pr_WindowPtr=app.requesters;
 LONG rc=SystemTags((CONST_STRPTR)line,SYS_Input,(IPTR)in,SYS_Output,(IPTR)NULL,SYS_Asynch,TRUE,
  NP_CurrentDir,(IPTR)dir,NP_StackSize,stack>65536?stack:65536,TAG_END);
 me->pr_WindowPtr=(APTR)-1;me->pr_HomeDir=home;
 note("SHELL start rc=%ld files=%d line=%s",(long)rc,count,line);
 /* On success the new process owns the console and the directory lock. */
 if(rc==-1){Close(in);UnLock(dir);return 0;}
 return 1;
}
static void open_dropped(int view,int index,const char *const *paths,int count) {
 struct DockItem *item=item_at(view,0,index);if(!item)return;
 app.views[view].pressed=index;app.views[view].dirty=1;
 app.flash_view=view;app.flash_index=index;app.flash_until=milliseconds()+400;
 if(item->kind==DOCK_COMMAND){start_command(view,item,paths,count);return;}
 long stack=0;
 int ok=dock_program_is_shell(item->path,&stack)?start_shell_program(item->path,stack,paths,count):dock_launch_with(item->path,paths,count);
 note("DROP open=%d files=%d program=%s first=%s",ok,count,item->path,paths[0]);
 char message[96];
 if(ok<0)snprintf(message,sizeof message,"Not opened: the Shell would misread a name with ; or |");
 else if(!ok)snprintf(message,sizeof message,"Could not open the %s",count>1?"files":"file");
 else if(count>1)snprintf(message,sizeof message,"Opening %d files in %s",count,item->label);
 else snprintf(message,sizeof message,"Opening in %s",item->label);
 feedback(view,message);
}
/* One drop from Wanderer, all its files together. The middle of an item
 * acts (opens the files, or adds them to a group); between items, or on a
 * program or command that does not open files, they become shortcuts as
 * before. Anything else is refused and nothing changes. */
static void dropped(const char *const *paths,int count,int missing,void *context) {
 int view=app.drop_view,container=app.drop_container,before=-1,added=0;(void)context;
 if(missing)note("DROP incomplete files=%d missing=%d",count,missing);
 struct DockView *v=&app.views[view];
 if(container==0){
  int x=v->drop.mouse_x,y=v->drop.mouse_y,hit=view_hit(v,x,y);
  if(hit>=0 && view_center(v,hit,x,y)){
   const struct DockItem *item=&model_of(view)->items[hit];
   if((item->kind==DOCK_APP || item->kind==DOCK_COMMAND) && item->on_drop==DOCK_DROP_OPEN){
    /* Never open or run with part of what was dropped. */
    if(missing){char m[80];snprintf(m,sizeof m,"Not opened: %d of %d files could not be read",missing,count+missing);feedback(view,m);return;}
    open_dropped(view,hit,paths,count);return;
   }
   if(item->kind==DOCK_GROUP)container=item->group;
   else if(item->kind!=DOCK_APP && item->kind!=DOCK_COMMAND){
    feedback(view,item->kind==DOCK_DRAWER?"Files are not copied into drawers":"Drop between icons to add a shortcut");
    note("DROP refused kind=%d files=%d",(int)item->kind,count);return;
   }
  }
  if(!container)before=view_before(v,x,y);
 } else before=dock_panel_before(&app.panel,app.panel.drop.mouse_x,app.panel.drop.mouse_y);
 if(editing_blocked(view))return;
 next_set=app.set;
 for(int i=0;i<count;i++)added+=add_dropped(view,container,paths[i],&before);
 if(added)commit(added>1?"Shortcuts added":"Pinned",view,app.panel.window?app.panel.container:0);
 /* After commit, which shows its own message: say what was left out. */
 if(missing){char m[80];snprintf(m,sizeof m,"%d of %d icons could not be added",missing,count+missing);feedback(view,m);}
}

/* ---- launching, menu, editor, preferences ---------------------------- */

/* A command item: started as its own process (asynchronous), never as a
 * Workbench start, from its working directory. With Run in Shell it gets a
 * console window that stays until closed, where errors show; without, its
 * input and output are NIL:. Problems found before starting are reported. */
/* Whether a script takes dropped files (see dock_command_script_takes_files):
 * its start, read from the working directory as Execute will. */
static int script_takes_files(BPTR dir,const char *path) {
 char text[1024];LONG n=0;
 BPTR old=CurrentDir(dir),file=Open((CONST_STRPTR)path,MODE_OLDFILE);CurrentDir(old);
 if(!file)return 0;
 n=Read(file,text,sizeof text-1);Close(file);
 if(n<0)n=0;
 text[n]=0;
 return dock_command_script_takes_files(text);
}
static void start_command(int view,const struct DockItem *item,const char *const *files,int count) {
 char line[4*DOCK_PATH];
 /* No "insert volume" requester for a missing disk or assign: it would
  * stop the dock until answered. */
 struct Process *me=(struct Process *)FindTask(NULL);
 BPTR dir=Lock((CONST_STRPTR)(item->dir[0]?item->dir:"SYS:"),SHARED_LOCK),script=BNULL;
 /* The script is looked up from the working directory, where Execute will
  * look for it too: a relative name means the same file in both. */
 if(dir && item->run&DOCK_RUN_SCRIPT){BPTR old=CurrentDir(dir);script=Lock((CONST_STRPTR)item->path,SHARED_LOCK);CurrentDir(old);}
 if(!dir){feedback(view,"Working directory not found");note("COMMAND no dir=%s",item->dir);return;}
 if(item->run&DOCK_RUN_SCRIPT){
  if(!script){UnLock(dir);feedback(view,"Script file not found");note("COMMAND no script=%s dir=%s",item->path,item->dir);return;}
  UnLock(script);
  if(count && !script_takes_files(dir,item->path)){
   UnLock(dir);feedback(view,"The script needs .KEY FILES/M to take files");note("COMMAND script without FILES/M %s",item->path);return;}
  if(!(count?dock_command_script_files(item->path,files,count,line,sizeof line):dock_command_script_line(item->path,line,sizeof line))){
   UnLock(dir);feedback(view,count?"Too many files for one command":"Script path too long");return;}
 } else if(count){
  /* Files go only where the command line asks for them. */
  int made=dock_command_with_files(item->path,files,count,line,sizeof line);
  if(made<=0){UnLock(dir);feedback(view,made==-1?"Add {files} to the command to pass files":made==-2?"Write {files} on its own, without quotes":"Too many files for one command");
   note("COMMAND files refused made=%d",made);return;}
 } else snprintf(line,sizeof line,"%s",item->path);
 if(count && !dock_command_line_safe(line)){
  UnLock(dir);feedback(view,"Not run: the Shell would misread a name with ; or |");note("COMMAND refused misread line=%s",line);return;}
 /* SystemTags ignores NP_HomeDir and gives the new process a copy of our
  * home drawer lock, and on ABIv11 that lock outlived the command: Dockbar's
  * drawer stayed "in use" until a reboot. A command has no use for our
  * PROGDIR:, so it gets none. The console is opened without it and without
  * a current directory too: a CON: window keeps copies of both (see
  * start_shell_program). */
 BPTR home=me->pr_HomeDir;me->pr_HomeDir=BNULL;
 BPTR in,cwd=CurrentDir(BNULL);
 if(item->run&DOCK_RUN_SHELL){
  char con[DOCK_LABEL+80];
  in=dock_command_console(item->label,con,sizeof con)?Open((CONST_STRPTR)con,MODE_OLDFILE):BNULL;
 } else in=Open((CONST_STRPTR)"NIL:",MODE_NEWFILE);
 CurrentDir(cwd);
 BPTR out=item->run&DOCK_RUN_SHELL?BNULL:Open((CONST_STRPTR)"NIL:",MODE_NEWFILE);
 if(!in || (!(item->run&DOCK_RUN_SHELL) && !out)){me->pr_HomeDir=home;if(in)Close(in);if(out)Close(out);UnLock(dir);feedback(view,"Could not start command");return;}
 /* The command inherits -1 otherwise and would never ask for a disk. */
 me->pr_WindowPtr=app.requesters;
 LONG rc=SystemTags((CONST_STRPTR)line,SYS_Input,(IPTR)in,SYS_Output,(IPTR)out,SYS_Asynch,TRUE,SYS_UserShell,TRUE,
  NP_CurrentDir,(IPTR)dir,NP_StackSize,65536,NP_Name,(IPTR)"Dockbar command",TAG_END);
 me->pr_WindowPtr=(APTR)-1;me->pr_HomeDir=home;
 note("COMMAND rc=%ld shell=%d script=%d dir=%s line=%s",(long)rc,item->run&DOCK_RUN_SHELL?1:0,item->run&DOCK_RUN_SCRIPT?1:0,item->dir,line);
 /* On success the new process owns the handles and the directory lock and
  * releases them when it ends; on failure they are still ours. */
 if(rc==-1){Close(in);if(out)Close(out);UnLock(dir);feedback(view,"Could not start command");}
}
/* The panel closes when it loses the activation, which a click on its own
 * drawer item causes first; that click must not open it again. */
static void toggle_drawers(int view,int index) {
 int same=app.drawers.window && app.drawers_view==view && app.drawers_index==index;
 if(same || (milliseconds()-app.drawers_closed_at<300 && app.drawers_view==view && app.drawers_index==index)){close_drawers();return;}
 struct DockItem *item=item_at(view,0,index);int x,y,w,h;
 if(!item || !app.views[view].window)return;
 close_panel();close_tooltip();dock_applet_ui_close();
 slot_on_screen(view,index,&x,&y,&w,&h);
 if(!lock_screen())return;
 int ok=dock_drawer_open(&app.drawers,app.screen,item->path,app.views[view].style.icon,x,y,w,h,
  app.views[view].layout.orientation,app.views[view].c);
 unlock_screen();
 app.drawers_view=view;app.drawers_index=index;app.drawers_moved_at=milliseconds();
 note("DRAWER panel open=%d path=%s",ok,item->path);
 if(!ok)feedback(view,"Could not show this drawer");
}
/* Events of the drawer panel's window. */
static void drawers_dispatch(void) {
 struct DockDrawerPanel *p=&app.drawers;struct IntuiMessage *m;
 while(p->window && (m=(struct IntuiMessage *)GetMsg(p->window->UserPort))){
  ULONG cls=m->Class;UWORD code=m->Code;int x=m->MouseX,y=m->MouseY;ReplyMsg((struct Message *)m);
  int index=-1,hit;char path[DOCK_PATH+120];
  switch(cls){
  case IDCMP_INACTIVEWINDOW:close_drawers();return;
  case IDCMP_REFRESHWINDOW:BeginRefresh(p->window);EndRefresh(p->window,TRUE);dock_drawer_draw(p);break;
  case IDCMP_RAWKEY:
   if(code==0x45){close_drawers();return;}
   if(code==0x7A || code==0x4C)dock_drawer_scroll(p,-1);
   if(code==0x7B || code==0x4D)dock_drawer_scroll(p,1);
   break;
  case IDCMP_MOUSEMOVE:{
   hit=dock_drawer_hit(p,x,y,&index);
   int hover=hit==DOCK_DRAWER_HIT_ENTRY?index:-1,hot=hit==DOCK_DRAWER_HIT_ENTRY?-1:hit;
   if(hover!=p->hover || hot!=p->hot){p->hover=hover;p->hot=hot;dock_drawer_draw(p);}
   break;}
  case IDCMP_MOUSEBUTTONS:
   hit=dock_drawer_hit(p,x,y,&index);
   if(code==SELECTDOWN){p->pressed=hit==DOCK_DRAWER_HIT_ENTRY?index:-1;dock_drawer_draw(p);break;}
   if(code!=SELECTUP)break;
   {int pressed=p->pressed;p->pressed=-1;
   /* The second click of a habitual double-click lands in the drawer just
    * entered; it must not open something there. */
   if(milliseconds()-app.drawers_moved_at<400){dock_drawer_draw(p);break;}
   if(hit==DOCK_DRAWER_HIT_ENTRY && index==pressed && dock_drawer_entry_path(p,index,path,sizeof path)){
    if(p->entries[index].is_dir){dock_drawer_show(p,path);app.drawers_moved_at=milliseconds();note("DRAWER enter %s",path);}
    else{long stack=0;int ok=dock_program_is_shell(path,&stack)?start_shell_program(path,stack,NULL,0)>0:dock_launch(path);note("DRAWER open=%d %s",ok,path);int view=app.drawers_view;close_drawers();if(!ok)feedback(view,"Could not open this file");return;}
   } else if(hit==DOCK_DRAWER_HIT_BACK && dock_dir_parent(p->path,path,sizeof path)){dock_drawer_show(p,path);app.drawers_moved_at=milliseconds();}
   else if(hit==DOCK_DRAWER_HIT_REFRESH)dock_drawer_refresh(p);
   else if(hit==DOCK_DRAWER_HIT_WANDERER){snprintf(path,sizeof path,"%s",p->path);int ok=dock_launch(path);note("DRAWER wanderer=%d %s",ok,path);close_drawers();return;}
   else if(hit==DOCK_DRAWER_HIT_PAGE_UP)dock_drawer_scroll(p,-p->rows);
   else if(hit==DOCK_DRAWER_HIT_PAGE_DOWN)dock_drawer_scroll(p,p->rows);
   else dock_drawer_draw(p);}
   break;
  }
 }
 unsigned long reads=p->reads;
 if(p->window && dock_drawer_step(p))dock_drawer_draw(p);
 /* With --log: how long the panel took to show the list and its icons. */
 if(p->window && p->timing.visible && !p->timing.reported){
  static const char *const why[]={"","open","refresh","notify","scroll"};
  const struct DockDrawerTiming *t=&p->timing;p->timing.reported=1;
  int r=t->reason>=1 && t->reason<=4?t->reason:0;
  const struct DockIconCache *c=dock_drawer_cache();
  note("DRAWER timing %s%s path=%s entries=%d list=%u listed=%u first_icon=%u visible=%u loads=%lu hits=%lu read=%lu scale=%lu draws=%lu draw=%lu reads=%lu cache=%lu/%luB evicted=%lu",
   why[r],r==1 && app.drawers_seen && !strcmp(app.drawers_last,p->path)?" (again)":"",p->path,p->count,t->list,t->listed,t->first_icon,t->visible,
   t->loads,t->hits,t->read_us/1000,t->scale_us/1000,t->draws,t->draw_us/1000,p->reads,c->count,c->bytes,c->evicted);
  if(r==1){snprintf(app.drawers_last,sizeof app.drawers_last,"%s",p->path);app.drawers_seen=1;}
 }
 /* With --log, each finished read lists what the panel shows, so that a
  * test can compare it with the disk. */
 if(app.log && p->window && p->reads!=reads){
  note("DRAWER read %lu path=%s count=%d error=%ld truncated=%d stale_reads=%lu%s",p->reads,p->path,p->read_count,p->error,p->truncated,p->stale_reads,
   p->scanning?" (changed meanwhile: reading again)":"");
  /* The entries themselves are listed once, when the panel closes: a
   * drawer that keeps changing is read many times, and 1500 lines a read
   * made the log the slowest part of the test. */
 }
}
static void launch_or_open(int view,int container,int index) {
 struct DockItem *item=item_at(view,container,index);if(!item)return;
 if(item->kind==DOCK_GROUP){if(app.panel.window && app.panel_view==view && app.panel.container==item->group)close_panel();else open_panel(view,item->group,1);}
 else if(item->kind==DOCK_APPLET){
  close_panel();close_tooltip();struct DockAppletRuntime *r=app.views[view].visual[index].applet;
  if(r && r->package.available && r->package.manual){if(lock_screen()){dock_applet_ui_actions(app.screen,r);unlock_screen();}return;}
  int x,y,w,h;slot_on_screen(view,index,&x,&y,&w,&h);
  if(lock_screen()){dock_applet_ui_open_at(app.screen,r,x,y,w,h,app.views[view].layout.orientation,app.views[view].c);unlock_screen();}
 }
 else if(item->kind==DOCK_COMMAND){close_panel();start_command(view,item,NULL,0);}
 else if(item->kind==DOCK_APP || item->kind==DOCK_DRAWER){
  /* Amiga users double-click by habit; each click would start the program
   * or open the drawer again, and Wanderer crashed once with several
   * windows of the same volume opened in quick succession. */
  if(item->kind==DOCK_DRAWER && item->open==DOCK_OPEN_PANEL && container==0){toggle_drawers(view,index);return;}
  static char last_path[DOCK_PATH];static unsigned int last_time;
  unsigned int now=milliseconds();
  if(!strcmp(last_path,item->path) && now-last_time<1500){note("LAUNCH repeat ignored path=%s",item->path);return;}
  snprintf(last_path,sizeof last_path,"%s",item->path);last_time=now;
  /* A Shell program is started the same way as with dropped files: from
   * its own drawer, with the stack of its icon. */
  long stack=0;
  int result=item->kind==DOCK_APP && dock_program_is_shell(item->path,&stack)?start_shell_program(item->path,stack,NULL,0)>0:dock_launch(item->path);
  note("LAUNCH result=%d path=%s",result,item->path);close_panel();
  if(!result)feedback(view,"Could not open item");
 }
}
/* Every drag of ours ends here (see DockDragState): dropped, cancelled, or
 * cut short by a screen change or a rebuild. restore: a dock moved by its
 * grip goes back to where it was (a cancel); a finished move stays. */
static void end_drag(int restore) {
 struct DockDrag was=dock_drag_state_end(&app.drag);
 if(restore && was.kind==2 && was.active && app.drag_view>=0 && app.drag_view<app.set.count && app.views[app.drag_view].window)
  ChangeWindowBox(app.views[app.drag_view].window,app.drag_left,app.drag_top,app.views[app.drag_view].ww,app.views[app.drag_view].wh);
 /* The pressed box, a drop target and the insertion mark were drawn; they
  * must go now, not with the next change. */
 for(int i=0;i<app.set.count;i++){
  if(app.views[i].pressed>=0){app.views[i].pressed=-1;app.views[i].dirty=1;}
  if(was.kind==1 && was.active)app.views[i].dirty=1;
 }
 if(app.panel.pressed>=0){app.panel.pressed=-1;app.panel_dirty=1;}
}
static void cancel_drag(void){end_drag(1);}
static void open_menu(int view) {
 if(app.menu.window){int same=app.menu_view==view;dock_menu_close(&app.menu,app.screen);app.menu_view=-1;if(same)return;}
 cancel_drag();close_tooltip();close_panel();
 struct Window *w=app.views[view].window;const struct DockRect *g=&app.views[view].layout.grip;
 
 int locked=lock_screen();
 
 int opened=locked && dock_menu_open(&app.menu,app.screen,app.font,w->LeftEdge+g->x,w->TopEdge+g->y,g->w,g->h,app.views[view].layout.orientation,app.views[view].c);
 
 if(locked)unlock_screen();
 
 if(opened)app.menu_view=view;else feedback(view,"Could not open menu");
}
/* The set as it was when Preferences opened; Apply shows the edited set on
 * the docks without saving, and Cancel, the close gadget or a screen change
 * put this one back. Content edits are refused while Preferences is open,
 * so the whole set is the transaction. */
static int prefs_applied;
static void open_prefs(int view) {
 if(app.prefs.window || app.editor.window)return;
 if(view<0 || view>=app.set.count)view=0;
 if(!lock_screen()){feedback(view,"Could not open preferences");note("PREFS open failed: screen not available");return;}
 /* Docks added in Preferences open composed where the screen composes. */
 {int all=1;for(int i=0;i<DOCK_MAX_DOCKS;i++){
   int c=i<app.set.count?(app.views[i].window?app.views[i].compose:app.compose_ok && !app.compose_refused[i]):app.compose_ok;
   dock_prefs_window_composed[i]=(unsigned char)c;if(i<app.set.count && !c)all=0;}
  dock_prefs_window_all_composed=all && app.compose_ok;}
 int opened=dock_prefs_window_open(&app.prefs,app.screen,&app.set,view,-1,-1);
 unlock_screen();
 if(opened){prefs_original=app.set;prefs_applied=0;}
 if(!opened){feedback(view,"Could not open preferences");note("PREFS open failed at prefs_window.c:%d signals=%d",dock_prefs_window_failed,free_signals());}
}
/* What Copy version info gives: name, version, target and the Weather
 * widget's version. Nothing about the place, settings or files. */
static void version_info(char *out,size_t size) {
 const struct DockAppletCatalog *c=dock_applets_catalog();
 const struct DockAppletPackage *w=c?dock_applet_find(c,"org.aros.weather"):NULL;
 const char *target=!strcmp(DOCK_APPLET_VARIANT,"x86_64-abiv11")?"x86_64, ABIv11":
  !strcmp(DOCK_APPLET_VARIANT,"x86_64-v1")?"x86_64, mainline (ABIv1)":DOCK_APPLET_VARIANT;
 int composed=0,shown=0;
 for(int i=0;i<app.set.count;i++)if(app.views[i].window){shown++;composed+=app.views[i].compose;}
 const char *present=!app.screen?"no screen":app.compose_ok?(composed==shown?"composed windows":"composed windows, some docks pictured"):
  app.no_compose?"desktop picture (--no-compose)":"desktop picture";
 snprintf(out,size,"Dockbar %s (%s)\nTarget: %s\nPresentation: %s\nWeather widget: %s\n",DOCK_VERSION,DOCK_DATE,target,present,
  !w?"not installed":w->version[0]?w->version:"version unknown");
}
static void open_about(int view) {
 if(app.about.window){dock_about_raise(&app.about);return;}
 char info[400],version[64];version_info(info,sizeof info);
 snprintf(version,sizeof version,"Version %s (%s)",DOCK_VERSION,DOCK_DATE);
 int locked=lock_screen();
 int opened=locked && dock_about_open(&app.about,app.screen,version,info,"PROGDIR:NOTICES");
 if(locked)unlock_screen();
 if(!opened)feedback(view,"Could not open About");
 note("ABOUT open=%d",opened);
}
static void poll_about(void) {
 if(app.about.window && dock_about_poll(&app.about)){dock_about_close(&app.about);note("ABOUT closed");}
}
/* ---- Edit items... ---------------------------------------------------- */

static const char *kind_name(enum DockKind k) {
 switch(k){case DOCK_APP:return "Program";case DOCK_GROUP:return "Group";case DOCK_DRAWER:return "Drawer";
 case DOCK_COMMAND:return "Command";case DOCK_CLOCK:return "Clock";case DOCK_APPLET:return "Widget";
 case DOCK_SEPARATOR:return "Separator";case DOCK_SPACER:return "Space";default:return "Item";}
}
/* The list shows the dock's own items as they are now; called after every
 * change and whenever the docks were rebuilt. */
static void fill_items(int active) {
 if(!app.items.window)return;
 int view=dock_set_find(&app.set,app.items.dock_id);
 if(view<0){dock_item_list_close(&app.items);return;}
 static char text[DOCK_MAX_ITEMS][DOCK_LABEL+48];static const char *lines[DOCK_MAX_ITEMS];
 struct DockModel *m=&app.set.docks[view].model;
 for(int i=0;i<m->count;i++){
  const struct DockItem *it=&m->items[i];char name[DOCK_LABEL+4]="";
  if(it->kind==DOCK_APPLET){struct DockAppletRuntime *r=dock_applets_get(it->path,it->dir);snprintf(name,sizeof name,"%.*s",(int)sizeof name-1,r?r->package.name:it->path);}
  else if(it->kind==DOCK_GROUP){int n=0;dock_items(m,it->group,&n);snprintf(name,sizeof name,"%s (%d)",it->label[0]?it->label:"Group",n);}
  else if(dock_kind_shown(it->kind))item_caption(it,name,sizeof name);
  snprintf(text[i],sizeof text[i],"%d. %s%s%s",i+1,kind_name(it->kind),name[0]?": ":"",name);
  lines[i]=text[i];
 }
 dock_item_list_fill(&app.items,lines,m->count,active,dock_locked(view) || app.readonly);
}
static void open_items(int view) {
 if(app.items.window && app.items.dock_id==app.set.docks[view].id){dock_item_list_raise(&app.items);return;}
 dock_item_list_close(&app.items);
 char title[DOCK_LABEL+32];snprintf(title,sizeof title,"Edit items: %s",app.set.docks[view].name);
 int locked=lock_screen();
 int opened=locked && dock_item_list_open(&app.items,app.screen,title,app.set.docks[view].id);
 if(locked)unlock_screen();
 if(!opened){feedback(view,"Could not open the item list");return;}
 fill_items(0);
}
static void start_editor(int view,int container,int index);
static int items_stale;
/* The item list closes while one of its items is edited: each Zune window
 * is an application of its own, and with both open a task with several
 * docks ran out of signals, so the editor could not open. It comes back,
 * on the same item, when the editor closes (back_dock: that dock's id). */
static int items_back_dock,items_back_index;
static void poll_items(void) {
 if(!app.items.window)return;
 int index,action=dock_item_list_poll(&app.items,&index),view=dock_set_find(&app.set,app.items.dock_id);
 if(view<0 || action==DOCK_LIST_CLOSE){dock_item_list_close(&app.items);return;}
 /* The docks were rebuilt (a change from anywhere): show the set as it is. */
 if(app.rebuild)items_stale=1;
 else if(items_stale){items_stale=0;fill_items(index>=0?index:0);}
 if(!action || index<0)return;
 const struct DockModel *m=&app.set.docks[view].model;
 if(index>=m->count)return;
 if(action==DOCK_LIST_EDIT){
  items_back_dock=app.items.dock_id;items_back_index=index;
  dock_item_list_close(&app.items);
  start_editor(view,0,index);
  return;
 }
 if(editing_blocked(view))return;
 next_set=app.set;int result=0,to=index;
 if(action==DOCK_LIST_REMOVE){note_removed(&app.set.docks[view].model,0,index);result=dock_remove(&next_set.docks[view].model,0,index);}
 else if(action==DOCK_LIST_UP && index>0){to=index-1;result=dock_set_move(&next_set,view,0,index,view,0,index-1);}
 else if(action==DOCK_LIST_DOWN && index+1<m->count){to=index+1;result=dock_set_move(&next_set,view,0,index,view,0,index+2);}
 if(result>0 && commit(action==DOCK_LIST_REMOVE?"Removed":"Moved",view,0)){
  if(action==DOCK_LIST_REMOVE)forget_removed();
  fill_items(action==DOCK_LIST_REMOVE?(index<next_set.docks[view].model.count?index:index-1):to);
 }
 removed.n=0;
}
static void add_root(int view,enum DockKind kind,const char *what) {
 if(editing_blocked(view))return;
 next_set=app.set;
 int result=dock_add_kind(&next_set.docks[view].model,0,kind,NULL,next_set.docks[view].model.count);
 if(result<=0){feedback(view,"Dock is full");return;}
 commit(what,view,0);
}
static void menu_choose(int choice) {
 int view=app.menu_view;
 dock_menu_close(&app.menu,app.screen);app.menu_view=-1;app.views[view].dirty=1;
 note("MENU choice=%d dock=%d",choice,app.set.docks[view].id);
 switch(choice){
 case DOCK_MENU_PREFS:open_prefs(view);break;
 case DOCK_MENU_SEPARATOR:add_root(view,DOCK_SEPARATOR,"Separator added");break;
 case DOCK_MENU_SPACER:add_root(view,DOCK_SPACER,"Space added");break;
 case DOCK_MENU_CLOCK:add_root(view,DOCK_CLOCK,"Clock added");break;
 case DOCK_MENU_APPLET:if(!editing_blocked(view)&&lock_screen()){app.edit_view=view;dock_applet_ui_picker(app.screen);unlock_screen();}break;
 case DOCK_MENU_COMMAND:add_command(view);break;
 case DOCK_MENU_REFRESH:app.rebuild=1;app.rebuild_asked=1;app.reopen_view=-1;feedback(view,"Icons reloaded");break;
 case DOCK_MENU_ITEMS:open_items(view);break;
 case DOCK_MENU_ABOUT:open_about(view);break;
 case DOCK_MENU_QUIT:app.quit=1;break;
 }
}
static void dispatch_menu(void) {
 struct IntuiMessage *msg;
 while(app.menu.window && (msg=(struct IntuiMessage*)GetMsg(app.menu.window->UserPort))){
  ULONG cls=msg->Class;UWORD code=msg->Code;int x=msg->MouseX,y=msg->MouseY;
  ReplyMsg((struct Message*)msg);
  int hit=dock_menu_hit(&app.menu,x,y);
  if(cls==IDCMP_REFRESHWINDOW){BeginRefresh(app.menu.window);dock_menu_draw(&app.menu);EndRefresh(app.menu.window,TRUE);}
  else if(cls==IDCMP_MOUSEMOVE){if(hit!=app.menu.hover){app.menu.hover=hit;dock_menu_draw(&app.menu);}}
  else if(cls==IDCMP_INACTIVEWINDOW || (cls==IDCMP_RAWKEY && code==0x45)){dock_menu_close(&app.menu,app.screen);app.menu_view=-1;return;}
  /* Release over an entry chooses it: press-drag-release with the right
   * button, or a separate left click. Releasing elsewhere keeps it open. */
  else if(cls==IDCMP_MOUSEBUTTONS && (code==SELECTUP || code==MENUUP) && hit>=0){menu_choose(hit);return;}
  else if(cls==IDCMP_MOUSEBUTTONS && code==SELECTDOWN && hit<0){dock_menu_close(&app.menu,app.screen);app.menu_view=-1;return;}
 }
}
/* "Add command...": the editor on a new item, inserted only when saved. */
static void add_command(int view) {
 if(editing_blocked(view))return;
 static struct DockItem fresh;
 memset(&fresh,0,sizeof fresh);fresh.kind=DOCK_COMMAND;strcpy(fresh.label,"Command");fresh.run=DOCK_RUN_SHELL;
 cancel_drag();close_tooltip();close_panel();
 app.edit_view=view;app.edit_container=0;app.edit_index=-1;app.edit_new=1;
 int locked=lock_screen();
 int opened=locked && dock_editor_open(&app.editor,app.screen,&fresh,app.screen->MouseX,app.screen->MouseY,1);
 if(locked)unlock_screen();
 if(!opened){app.edit_new=0;feedback(view,"Could not open editor");}
}
static void start_editor(int view,int container,int index) {
 struct DockItem *item=item_at(view,container,index);
 if(!item || app.readonly || app.prefs.window)return;
 if(dock_locked(view)){feedback(view,"Dock is locked (see Preferences)");return;}
 if(item->kind==DOCK_APPLET){struct DockAppletRuntime *r=app.views[view].visual[index].applet;if(lock_screen()){dock_applet_ui_open(app.screen,r,1);unlock_screen();}return;}
 cancel_drag();close_tooltip();close_panel();app.edit_view=view;app.edit_container=container;app.edit_index=index;
 int locked=lock_screen();
 int opened=locked && dock_editor_open(&app.editor,app.screen,item,app.screen->MouseX,app.screen->MouseY,container==0);
 if(locked)unlock_screen();
 if(!opened){feedback(view,"Could not open editor");note("EDITOR open failed at item_editor.c:%d signals=%d locked=%d",dock_editor_failed,free_signals(),locked);}
}
/* A command line that runs with dropped files must say where they go, in a
 * form that cannot be misread (see dock_command_files_token). NULL: fine. */
static const char *files_problem(const struct DockItem *item) {
 if(item->kind!=DOCK_COMMAND || item->on_drop!=DOCK_DROP_OPEN || item->run&DOCK_RUN_SCRIPT)return NULL;
 int t=dock_command_files_token(item->path);
 return t==1?NULL:t==0?"Add {files} where the dropped files should go (or On drop: Add a shortcut).":"Write {files} once, as a word of its own and not in quotes.";
}
static void poll_editor(void) {
 if(!app.editor.window)return;
 int action=dock_editor_poll(&app.editor);if(!action)return;
 int view=app.edit_view;
 next_set=app.set;int n=0;struct DockItem *items=dock_items(&next_set.docks[view].model,app.edit_container,&n);
 int changed=0;
 if(app.edit_new){
  app.edit_new=0;
  if(action==1){
   static struct DockItem fresh;
   memset(&fresh,0,sizeof fresh);fresh.kind=DOCK_COMMAND;
   snprintf(fresh.label,DOCK_LABEL,"%s",app.editor.label);snprintf(fresh.path,DOCK_PATH,"%s",app.editor.command);
   snprintf(fresh.dir,DOCK_DIR,"%s",app.editor.dir);snprintf(fresh.icon,DOCK_PATH,"%s",app.editor.icon);
   fresh.run=app.editor.run;fresh.present=app.editor.present;fresh.on_drop=app.editor.on_drop;
   /* Refused input is named at its field; the window keeps what was typed. */
   if(!fresh.label[0]){app.edit_new=1;dock_editor_refuse(&app.editor,0,"Give the command a name.");return;}
   if(!fresh.path[0]){app.edit_new=1;dock_editor_refuse(&app.editor,1,fresh.run&DOCK_RUN_SCRIPT?"Choose the script file.":"Enter the command line.");return;}
   {const char *why=files_problem(&fresh);if(why){app.edit_new=1;dock_editor_refuse(&app.editor,1,why);return;}}
   dock_editor_close(&app.editor);
   int r=dock_insert(&next_set.docks[view].model,0,&fresh,next_set.docks[view].model.count);
   if(r<=0){feedback(view,"Dock is full");return;}
   commit("Command added",view,0);
   return;
  }
  dock_editor_close(&app.editor);
  return;
 }
 if(items && app.edit_index>=0 && app.edit_index<n){
  struct DockItem *item=&items[app.edit_index];
  if(action==2){note_removed(&app.set.docks[view].model,app.edit_container,app.edit_index);changed=dock_remove(&next_set.docks[view].model,app.edit_container,app.edit_index)>0;}
  else if(action==1 && app.editor.label[0]){
   snprintf(item->label,DOCK_LABEL,"%s",app.editor.label);
   if(item->kind==DOCK_CLOCK){
    struct DockWidget w;
    if(dock_widget_start(&w,item->path)){w.options=app.editor.clock_options;w.cls->settings(&w,item->path,sizeof item->path);}
    dock_widget_stop(&w);
   } else {
    snprintf(item->icon,DOCK_PATH,"%s",app.editor.icon);if(app.editor.present_object)item->present=app.editor.present;
    if(app.editor.drop_object)item->on_drop=app.editor.on_drop;
    if(app.editor.open_object)item->open=app.editor.open;
    if(item->kind==DOCK_COMMAND){
     if(!app.editor.command[0]){dock_editor_refuse(&app.editor,1,app.editor.run&DOCK_RUN_SCRIPT?"Choose the script file.":"Enter the command line.");return;}
     snprintf(item->path,DOCK_PATH,"%s",app.editor.command);snprintf(item->dir,DOCK_DIR,"%s",app.editor.dir);item->run=app.editor.run;
     {const char *why=files_problem(item);if(why){dock_editor_refuse(&app.editor,1,why);return;}}
    }
   }
   changed=1;
  } else if(action==1){
   if(app.editor.kind==DOCK_COMMAND){dock_editor_refuse(&app.editor,0,"Give the command a name.");return;}
   feedback(view,"Name cannot be empty");
  }
 }
 dock_editor_close(&app.editor);
 if(changed && commit(action==2?"Removed":"Saved",view,app.edit_container) && action==2)forget_removed();
 removed.n=0;
}
static void prefs_restore(void) {
 if(!prefs_applied)return;
 app.set=prefs_original;app.rebuild=1;app.reopen_view=-1;prefs_applied=0;
 note("PREFS preview withdrawn");
}
/* PROGDIR: in MultiView's process would be MultiView's drawer, so the
 * ReadMe is named by its full path. */
static void show_readme(void) {
 char path[DOCK_PATH],line[2*DOCK_PATH+40];
 BPTR lock=Lock((CONST_STRPTR)"PROGDIR:ReadMe",SHARED_LOCK);
 int named=lock && NameFromLock(lock,(STRPTR)path,sizeof path);
 if(lock)UnLock(lock);
 if(!named || !dock_command_quoted("SYS:Utilities/MultiView",path,line,sizeof line)){dock_prefs_window_message(&app.prefs,PS(PS_README_MISSING));return;}
 BPTR in=Open((CONST_STRPTR)"NIL:",MODE_NEWFILE),out=Open((CONST_STRPTR)"NIL:",MODE_NEWFILE);
 /* As for commands: no copy of our home drawer lock for MultiView. */
 struct Process *me=(struct Process *)FindTask(NULL);BPTR home=me->pr_HomeDir;me->pr_HomeDir=BNULL;
 me->pr_WindowPtr=app.requesters;
 LONG rc=in && out?SystemTags((CONST_STRPTR)line,SYS_Input,(IPTR)in,SYS_Output,(IPTR)out,
  SYS_Asynch,TRUE,NP_Name,(IPTR)"Dockbar ReadMe",TAG_END):-1;
 me->pr_WindowPtr=(APTR)-1;me->pr_HomeDir=home;
 if(rc==-1){if(in)Close(in);if(out)Close(out);dock_prefs_window_message(&app.prefs,PS(PS_README_FAILED));}
 note("PREFS help rc=%ld line=%s",(long)rc,line);
}
static void poll_prefs(void) {
 if(!app.prefs.window)return;
 int result=dock_prefs_window_poll(&app.prefs);if(!result)return;
 if(result==DOCK_PREFS_SAVE){
  /* Sizes that do not fit this screen are allowed: the dock uses smaller
   * icons or scrolls for as long as the screen is too small. */
  next_set=*app.prefs.set;
  if(app.readonly || !dock_set_save(&next_set,app.config)){dock_prefs_window_message(&app.prefs,PS(app.readonly?PS_SAVE_READONLY:PS_SAVE_FAILED));note("PREFS save failed file=%s",app.config);dock_prefs_window_keep(&app.prefs);return;}
  dock_prefs_window_close(&app.prefs);prefs_applied=0;
  app.set=next_set;app.rebuild=1;app.reopen_view=-1;feedback(0,"Preferences saved");
  note("PREFS saved docks=%d",app.set.count);
  return;
 }
 if(result==DOCK_PREFS_APPLY){
  app.set=*app.prefs.set;app.rebuild=1;app.reopen_view=-1;prefs_applied=1;
  note("PREFS applied docks=%d text=%d",app.set.count,app.set.defaults.text);
  dock_prefs_window_keep(&app.prefs);return;
 }
 if(result==DOCK_PREFS_HELP){show_readme();dock_prefs_window_keep(&app.prefs);return;}
 dock_prefs_window_close(&app.prefs);
 prefs_restore();
}

/* ---- dragging --------------------------------------------------------- */

static void finish_grip(void) {
 
 int view=app.drag_view;struct DockView *v=&app.views[view];struct DockInstance *d=&app.set.docks[view];
 if(!app.drag.own.active){int menu=app.grip_click;end_drag(0);if(menu)open_menu(view);return;}
 if(d->place.locked || app.readonly){cancel_drag();feedback(view,d->place.locked?"Position locked in Preferences":"Editing unavailable");return;}
 end_drag(0);
 next_set=app.set;struct DockPlace *p=&next_set.docks[view].place;
 /* Moving by hand makes an edge dock free-standing at the new spot. */
 p->mode=DOCK_PLACE_FREE;p->valid=1;p->x=app.drag_x;p->y=app.drag_y;
 if(dock_set_save(&next_set,app.config)){
  app.set=next_set;feedback(view,"Position saved");note("POSITION dock=%d x=%d y=%d",d->id,p->x,p->y);
  /* A see-through background must be read again at the new place. */
  if(view_transparent(view))dock_snap_invalidate(&v->snap,DOCK_SNAP_MOVED,milliseconds());
  /* A composed dock on a new place: automatic colours measure there now. */
  if(v->compose){v->lum_due=milliseconds();v->lum_reads=1;}
 } else {
  ChangeWindowBox(v->window,app.drag_left,app.drag_top,v->ww,v->wh);feedback(view,"Could not save position");
 }
}
static void finish_drag(int sx,int sy) {
 if(!app.drag.own.kind)return;
 if(app.drag.own.kind==2){finish_grip();return;}
 int view=app.drag_view,from=app.drag.own.container,index=app.drag.own.index;
 if(!app.drag.own.active){
  int hit=-1;
  if(from==0 && app.views[view].window)hit=view_hit(&app.views[view],sx-app.views[view].window->LeftEdge,sy-app.views[view].window->TopEdge);
  else if(app.panel.window)hit=dock_panel_hit(&app.panel,sx-app.panel.window->LeftEdge,sy-app.panel.window->TopEdge);
  cancel_drag();
  if(hit==index)launch_or_open(view,from,index);
  return;
 }
 int to_view=-1,to=-1,before=0,pair=-1,reopen=0;
 if(inside(app.panel.window,sx,sy)){
  to_view=app.panel_view;to=app.panel.container;reopen=to;
  before=dock_panel_before(&app.panel,sx-app.panel.window->LeftEdge,sy-app.panel.window->TopEdge);
 } else if((to_view=view_at(sx,sy))>=0){
  struct DockView *v=&app.views[to_view];
  to=0;before=view_before(v,sx-v->window->LeftEdge,sy-v->window->TopEdge);
  if(app.drag.own.armed && app.drag.own.target_container==code(to_view,0)){
   struct DockItem *target=item_at(to_view,0,app.drag.own.target_index);
   if(target && target->kind==DOCK_GROUP){to=target->group;int n=0;dock_items(model_of(to_view),to,&n);before=n;reopen=to;}
   else if(target && target->kind==DOCK_APP && to_view==view)pair=app.drag.own.target_index;
  }
 }
 struct DockItem *source=item_at(view,from,index);
 cancel_drag();
 if(to_view<0 || !source){note("DRAG dropped outside");return;}
 if(editing_blocked(view) || (to_view!=view && editing_blocked(to_view)))return;
 if(pair>=0 && dock_same_object(source->path,model_of(view)->items[pair].path))return;
 if(pair<0 && (source->kind==DOCK_APP || source->kind==DOCK_DRAWER) &&
    already_in(to_view,to,source->path,to_view==view && to==from?index:-1)){
  feedback(to_view,"Already in this place");note("MOVE duplicate object path=%s",source->path);return;
 }
 next_set=app.set;int result=0;
 if(pair>=0)result=dock_group_pair(&next_set.docks[view].model,from,index,pair);
 else result=dock_set_move(&next_set,view,from,index,to_view,to,before);
 if(result>0){
  if(pair>=0){int root=pair-(from==0 && index<pair);reopen=next_set.docks[view].model.items[root].group;to_view=view;}
  commit(pair>=0?"Group created":to_view!=view?"Moved to another dock":"Moved",to_view,reopen);
 } else if(result==0 && (to_view!=view || to!=from))feedback(to_view,"Already in this place");
 else if(result<0)feedback(to_view,"Cannot move this item here");
}
static void pointer_update(int sx,int sy,int track) {
 int under=view_at(sx,sy);
 if(trace_present && under!=under_view)note("UNDER dock=%d",under>=0?app.set.docks[under].id:-1);
 under_view=under;
 /* During a drag Dockbar does not own (an icon from Wanderer), nothing of
  * the docks is repainted: Zune draws the drag image straight onto the
  * screen and later puts back the pixels it saved underneath
  * (muimaster/dragndrop.c), so a hover box drawn or erased under the image
  * would come back as fragments. Hover does not move to other items; the
  * drop goes where the icon is released, and a group may still spring open.
  * A dock no longer under the pointer (left, or covered by a window) loses
  * its hover at once, as state only: the box is erased by the redraw at the
  * drag's end, and an open tooltip closes then (update_tooltip), not now,
  * when closing a window over the desktop would leave its picture there. */
 int foreign=foreign_drag(),hold=hold_drawing();
 for(int i=0;i<app.set.count;i++){
  struct DockView *v=&app.views[i];if(!v->window || v->resizing)continue;
  if(hold){if(v->hover>=0 && i!=under){v->hover=-1;v->dirty=1;}continue;}
  int hover=i==under?view_hit(v,sx-v->window->LeftEdge,sy-v->window->TopEdge):-1;
  if(hover!=v->hover)hover_changed(i,hover);
 }
 int oldpanel=app.panel.hover,oldarmed=app.drag.own.armed;
 if(app.panel.window && !hold)app.panel.hover=dock_panel_hit(&app.panel,sx-app.panel.window->LeftEdge,sy-app.panel.window->TopEdge);
 if(oldpanel!=app.panel.hover)app.panel_dirty=1;
 /* An icon from Wanderer held on a group opens it, so it can be dropped at a
  * place inside. Only opened, never closed or swapped while the drag lasts. */
 struct DockDragState *ds=&app.drag;
 if(foreign && under>=0 && !app.panel.window && !dock_locked(under)){
  struct DockView *v=&app.views[under];
  int hit=view_hit(v,sx-v->window->LeftEdge,sy-v->window->TopEdge);
  struct DockItem *item=item_at(under,0,hit);
  if(under!=ds->spring_view || hit!=ds->spring_item){ds->spring_view=under;ds->spring_item=hit;ds->spring_since=milliseconds();}
  else if(item && item->kind==DOCK_GROUP && milliseconds()-ds->spring_since>=600u &&
          open_panel(under,item->group,0))note("SPRING dock=%d group=%d external",app.set.docks[under].id,item->group);
 } else ds->spring_view=-1;
 if(!app.drag.own.kind || !track)return;
 /* In a locked dock a press still launches; moving it does nothing. */
 if(app.drag.own.kind==1 && app.drag.own.active && dock_locked(app.drag_view)){
  note("LOCKED dock=%d drag refused",app.set.docks[app.drag_view].id);
  cancel_drag();feedback(app.drag_view,"Dock is locked (see Preferences)");return;
 }
 int target=-1,center=0,target_code=-1;
 struct DockItem *source=item_at(app.drag_view,app.drag.own.container,app.drag.own.index);
 /* Holding a dragged item on an overflow arrow scrolls towards it. */
 if(app.drag.own.kind==1 && app.drag.own.active && under>=0){
  struct DockView *v=&app.views[under];int a=arrow_at(v,sx-v->window->LeftEdge,sy-v->window->TopEdge);
  if(a && milliseconds()-v->scroll_at>400)scroll_by(v,a);
 }
 if(app.drag.own.kind==1 && source && under>=0 && !inside(app.panel.window,sx,sy)){
  struct DockView *v=&app.views[under];struct DockItem *item;
  target=v->hover;target_code=code(under,0);item=item_at(under,0,target);
  /* Dwell targets: a group (drop or spring-open) or, in the same dock, an
   * application to make a new group with. The dragged item itself is not. */
  int self=under==app.drag_view && app.drag.own.container==0 && target==app.drag.own.index;
  int eligible=!self && item && source->kind==DOCK_APP &&
   (item->kind==DOCK_GROUP || (item->kind==DOCK_APP && under==app.drag_view));
  center=eligible && view_center(v,target,sx-v->window->LeftEdge,sy-v->window->TopEdge);
 }
 dock_drag_update(&app.drag.own,sx,sy,target_code,target,center,milliseconds());
 if(app.drag.own.active || oldarmed!=app.drag.own.armed)for(int i=0;i<app.set.count;i++)app.views[i].dirty=1;
 if(app.drag.own.active){for(int i=0;i<app.set.count;i++)app.views[i].pressed=-1;app.panel.pressed=-1;}
 /* Spring-open: holding an item over a group opens it, so the drop can go
  * to a chosen place inside. The panel does not take the input focus. */
 if(app.drag.own.kind==1 && app.drag.own.armed && under>=0){
  struct DockItem *item=item_at(under,0,app.drag.own.target_index);
  if(item && item->kind==DOCK_GROUP && !(app.panel.window && app.panel_view==under && app.panel.container==item->group))
   if(open_panel(under,item->group,0))note("SPRING dock=%d group=%d",app.set.docks[under].id,item->group);
 }
 if(app.drag.own.kind==2 && app.drag.own.active){
  struct DockView *v=&app.views[app.drag_view];struct DockInstance *d=&app.set.docks[app.drag_view];
  if(d->place.locked || app.readonly)return;
  struct DockPlace p=d->place;p.mode=DOCK_PLACE_FREE;p.valid=1;
  p.x=app.drag_left+sx-app.drag.own.start_x;p.y=app.drag_top+sy-app.drag.own.start_y;
  int x,y;dock_place_resolve(&p,app.screen->Width,app.screen->Height,screen_top(),v->ww,v->wh,&x,&y);
  app.drag_x=x;app.drag_y=y;
  if(x!=v->window->LeftEdge || y!=v->window->TopEdge)ChangeWindowBox(v->window,x,y,v->ww,v->wh);
 }
}

/* ---- input ------------------------------------------------------------ */

static void handle_buttons(int view,int panel,UWORD code,UWORD qualifier,int x,int y,int sx,int sy) {
 struct DockView *v=&app.views[view];
 int container=panel?app.panel.container:0;
 int hit=panel?dock_panel_hit(&app.panel,x,y):view_hit(v,x,y);
 /* A separator or space is no target, but it can be edited and moved. */
 int item=panel?hit:view_item(v,x,y);
 
 if(code==MENUDOWN){
  if(!panel && item<0 && view_in_grip(v,x,y)){open_menu(view);return;}
  if(panel && inside(v->window,sx,sy)){container=0;item=view_item(v,sx-v->window->LeftEdge,sy-v->window->TopEdge);}
  start_editor(view,container,item);return;
 }
 if(code==SELECTDOWN){
  
  app.panel_pending=0;
  if(panel && x>=app.panel.width-64 && y<app.panel.grid_y-5){
   for(int i=0;i<model_of(view)->count;i++)if(model_of(view)->items[i].kind==DOCK_GROUP && model_of(view)->items[i].group==container){start_editor(view,0,i);return;}
  }
  if(!panel && arrow_at(v,x,y)){scroll_by(v,arrow_at(v,x,y));return;}
  
  if((qualifier&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)) && item>=0){start_editor(view,container,item);return;}
  /* Ctrl-click anywhere on a dock opens its menu: the way to Preferences
   * when the grip is hidden. */
  if(!panel && (qualifier&IEQUALIFIER_CONTROL)){open_menu(view);return;}
  
  if(!panel && item<0 && view_in_grip(v,x,y)){
   /* A click on the grip opens its menu; a drag moves the dock. The click
    * matters because an inactive window never sees the right button. */
   close_panel();close_tooltip();app.grip_click=1;app.drag_view=view;
   dock_drag_begin(&app.drag.own,2,0,-1,sx,sy);app.drag_left=app.drag_x=v->window->LeftEdge;app.drag_top=app.drag_y=v->window->TopEdge;
  } else if(item>=0){
   /* A press on a separator or space starts only a drag: its release in
    * place finds no target (view_hit), so nothing is launched. */
   app.drag_view=view;dock_drag_begin(&app.drag.own,1,container,item,sx,sy);
   if(panel)app.panel.pressed=hit;else v->pressed=hit;
   v->dirty=1;app.panel_dirty=1;
  }
 } else if(code==SELECTUP){if(app.drag.own.kind==1)pointer_update(sx,sy,1);finish_drag(sx,sy);}
}
static void drop_deferred(int view) {
 int kept=0;
 for(int k=0;k<deferred_count;k++)if(deferred[k].view!=view)deferred[kept++]=deferred[k];
 deferred_count=kept;
}
static void replay_deferred(int view) {
 struct DockView *v=&app.views[view];int kept=0;
 if(v->deferred_lost && !v->resizing){
  v->deferred_lost=0;drop_deferred(view);
  note("UPDATE dock=%d buttons from the resize dropped (too many); interaction cancelled",app.set.docks[view].id);
  if(app.drag.own.kind){cancel_drag();feedback(view,"Cancelled");}
  v->pressed=-1;v->dirty=1;
  return;
 }
 for(int k=0;k<deferred_count;k++){
  if(deferred[k].view!=view || v->resizing){deferred[kept++]=deferred[k];continue;}
  if(!v->window || app.rebuild || app.editor.window || app.prefs.window)continue;
  note("UPDATE dock=%d button %u from the resize handled",app.set.docks[view].id,deferred[k].code);
  handle_buttons(view,0,deferred[k].code,deferred[k].qualifier,deferred[k].sx-v->window->LeftEdge,deferred[k].sy-v->window->TopEdge,deferred[k].sx,deferred[k].sy);
 }
 deferred_count=kept;
}
static void dispatch(int view,int panel) {
 struct Window *window=panel?app.panel.window:app.views[view].window;
 struct IntuiMessage *msg;
 while(window && (msg=(struct IntuiMessage*)GetMsg(window->UserPort))){
  ULONG cls=msg->Class;UWORD code=msg->Code,qualifier=msg->Qualifier;int x=msg->MouseX,y=msg->MouseY;
  int sx=window->LeftEdge+x,sy=window->TopEdge+y;ReplyMsg((struct Message*)msg);
  if(!panel && app.views[view].resizing){
   struct DockView *v=&app.views[view];
   /* Messages come in input order: positions before the change report are
    * relative to the old box, those after it to the new one. */
   if(cls==IDCMP_CHANGEWINDOW && window->Width==v->ww && window->Height==v->wh){
    v->resize_seen=1;
    if(resize_settled(v)){
     finish_resize(view,0);replay_deferred(view);
     if(app.rebuild)return;
    }
   } else if(cls==IDCMP_MOUSEBUTTONS){
    int ox=v->resize_seen?v->x:v->from_x,oy=v->resize_seen?v->y:v->from_y;
    if(!v->deferred_lost && deferred_count<8){deferred[deferred_count].view=view;deferred[deferred_count].code=code;deferred[deferred_count].qualifier=qualifier;
     deferred[deferred_count].sx=ox+x;deferred[deferred_count].sy=oy+y;deferred_count++;}
    else if(!v->deferred_lost){v->deferred_lost=1;drop_deferred(view);}
   } else if(cls==IDCMP_REFRESHWINDOW){BeginRefresh(window);EndRefresh(window,TRUE);v->dirty=1;}
   else if(cls==IDCMP_RAWKEY && code==0x45 && !app.drag.own.kind && !app.panel.window)app.quit=1;
   /* Pointer motion: hover is taken from the screen position afterwards. */
   window=app.views[view].window;continue;
  }
  /* Pointer motion redraws only through pointer_update, when the item under
   * it changes; a full repaint per motion event made the dock flicker. */
  if(cls==IDCMP_REFRESHWINDOW){
   if(trace_present && !panel)note("REFRESH dock=%d",app.set.docks[view].id);
   BeginRefresh(window);if(panel)dock_panel_draw(&app.panel);else draw_view(view);EndRefresh(window,TRUE);}
  else if(cls==IDCMP_RAWKEY && code==0x45){
   if(app.drag.own.kind){cancel_drag();feedback(view,"Cancelled");}
   else if(app.panel.window){close_panel();return;}else app.quit=1;
  } else if(cls==IDCMP_INACTIVEWINDOW){
   if(panel && !app.drag.own.kind){app.panel_pending=1;app.pending_x=app.screen->MouseX;app.pending_y=app.screen->MouseY;app.pending_moved=0;}
  } else if(cls==IDCMP_MOUSEMOVE && app.drag.own.kind){
   if(app.drag.own.kind==1)pointer_update(sx,sy,(qualifier&IEQUALIFIER_LEFTBUTTON)!=0);
  } else if(cls==IDCMP_MOUSEBUTTONS && !app.editor.window && !app.prefs.window){
   handle_buttons(view,panel,code,qualifier,x,y,sx,sy);
   
   if(app.rebuild)return;
  }
  window=panel?app.panel.window:app.views[view].window;
 }
}

/* ---- screen lifecycle -------------------------------------------------- */

/* Everything that keeps the Workbench screen open: our windows, requesters,
 * Zune windows and pens. The screen is never held by a pubscreen lock
 * between calls; open windows keep it alive while they exist. */
static void detach_screen(const char *why) {
 if(!app.screen)return;
 if(app.drag.own.kind){note("DRAG cancelled by screen change kind=%d active=%d",app.drag.own.kind,app.drag.own.active);cancel_drag();}
 if(app.editor.window){dock_editor_close(&app.editor);note("EDITOR closed by screen change; unsaved changes discarded");}
 if(app.prefs.window){dock_prefs_window_close(&app.prefs);note("PREFS closed by screen change; unsaved changes discarded");prefs_restore();}
 if(app.about.window){dock_about_close(&app.about);app.about_again=1;}
 dock_item_list_close(&app.items);
 dock_applet_ui_close();
 close_tooltip();close_panel();dock_menu_close(&app.menu,app.screen);app.menu_view=-1;
 /* Pens before the windows: the screen is certainly alive while ours are open. */
 release_screen_things(app.screen);
 /* Drawer panel pictures were made for this screen. */
 dock_drawer_cache_flush();
 /* A picture in progress belongs to this screen: its closed windows simply
  * stay closed, as every other one is about to be. */
 if(hold.index>=0){note("SNAP dock=%d picture cancelled by screen change",app.set.docks[hold.index].id);hold_free();}
 for(int i=0;i<DOCK_MAX_DOCKS;i++)close_view(i);
 app.screen=NULL;app.font=NULL;app.detaches++;app.screen_gen++;
 note("SCREEN detach reason=%s count=%d",why,app.detaches);
}
#ifndef DOCK_TEST_NO_SCREEN_AT_START
#define DOCK_TEST_NO_SCREEN_AT_START 0
#endif
/* 1 attached, 0 the docks could not open, -1 no screen to open them on:
 * Intuition opens the Workbench screen for us when it can, so -1 means it
 * is not available yet (or is being reset) and its open notice is awaited. */
static int attach_screen(void) {
 if(app.screen)return 1;
#if DOCK_TEST_NO_SCREEN_AT_START
 /* Development build only (dev/Dockbar-noscreen): the first attach acts as
  * if there were no screen, so a test can show the wait for the open notice. */
 {static int once;if(!once){once=1;note("SCREEN test build: first attach treated as no screen");return -1;}}
#endif
 struct Screen *screen=lock_open_screen();
 if(!screen){note("SCREEN attach failed: no public screen");return -1;}
 app.screen=screen;app.font=screen->RastPort.Font;app.screen_gen++;
 /* Asked again for every screen: a new mode may compose or not. */
 app.compose_ok=!app.no_compose && dock_present_can_compose(screen);
 memset(app.compose_refused,0,sizeof app.compose_refused);
 note("COMPOSE screen depth=%d can_compose=%d (%s%s)",(int)GetBitMapAttr(screen->RastPort.BitMap,BMA_DEPTH),app.compose_ok,
  dock_present_build_name(),app.no_compose?", off by --no-compose":"");
 int opened=get_screen_things(screen)?open_all():0;
 app.attaches++;
 note("SCREEN attach width=%d height=%d docks=%d opened=%d count=%d gen=%u signals=%d",screen->Width,screen->Height,app.set.count,opened,app.attaches,app.screen_gen,free_signals());
 /* The screen is there; its desktop picture may still be on its way. */
 if(opened)snapshot_invalidate(DOCK_SNAP_SCREEN);
 /* About was open on the old screen: it comes back on the new one. */
 if(opened && app.about_again){app.about_again=0;open_about(0);}
 if(!opened){
  for(int i=0;i<DOCK_MAX_DOCKS;i++)close_view(i);
  release_screen_things(screen);
  app.screen=NULL;app.font=NULL;
 }
 UnlockPubScreen(NULL,screen);
 return opened>0;
}
/* Close requests are answered only after everything is released; open
 * notices are answered first, before anything that may need screen locks.
 * Repeated or out-of-order notices are harmless: attach and detach are
 * idempotent. Returns 0 when the docks could not come back. */
static int handle_screen_notify(void) {
 struct ScreenNotifyMessage *msg;int ok=1;
 while(app.notify_port && (msg=(struct ScreenNotifyMessage*)GetMsg(app.notify_port))){
  ULONG cls=msg->snm_Class;
  if(cls==SNOTIFY_BEFORE_CLOSEWB){app.suspended=1;detach_screen("workbench closing");ReplyMsg((struct Message*)msg);}
  else if(cls==SNOTIFY_AFTER_OPENWB){
   ReplyMsg((struct Message*)msg);
   app.suspended=0;
   if(!app.screen && attach_screen()<=0)ok=0;
  } else ReplyMsg((struct Message*)msg);
 }
 return ok;
}
static void stop_screen_notify(void) {
 if(app.notify){
  /* EndScreenNotify fails only while Intuition holds its notification list
   * to deliver a notice, waiting for every subscriber's reply, ours
   * included. Answer and retry until it succeeds: freeing the port while
   * still registered would leave Intuition posting to freed memory. */
  int tries=0;
  for(;;){
   struct Message *msg;while((msg=GetMsg(app.notify_port)))ReplyMsg(msg);
   if(EndScreenNotify(app.notify))break;
   if(++tries%250==0)note("SCREEN still unregistering after %d tries",tries);
   Delay(2);
  }
  app.notify=NULL;
 }
 if(app.notify_port){struct Message *msg;while((msg=GetMsg(app.notify_port)))ReplyMsg(msg);dock_quiet_port_free(app.notify_port);app.notify_port=NULL;}
}

/* ---- startup ---------------------------------------------------------- */

/* The contents of a new dock. Prefs sits beside Tools, both as drawers.
 * Only new configurations get this; existing ones are never extended. */
static void seed(struct DockModel *m) {
 dock_pin(m,"SYS:System/Shell");dock_pin(m,"SYS:Utilities/MultiView");
 dock_add_kind(m,0,DOCK_SEPARATOR,NULL,m->count);
 dock_add_kind(m,0,DOCK_DRAWER,"SYS:Prefs",m->count);
 dock_add_kind(m,0,DOCK_DRAWER,"SYS:Tools",m->count);
 dock_add_kind(m,0,DOCK_SEPARATOR,NULL,m->count);
 /* 0.1 stored its clock with an empty path; keep that for the migration. */
 struct DockItem clock={0};clock.kind=DOCK_CLOCK;strcpy(clock.label,"Clock");dock_insert(m,0,&clock,m->count);
}
/* docks.config, else 0.1's dock.config + dock.prefs (+ older pins). The 0.1
 * files are only read: they stay as the way back to 0.1. */
static void load_settings(void) {
 static struct DockModel legacy;struct DockPosition position={0};struct DockPrefs prefs;
 int loaded=dock_set_load(&app.set,app.config);
 note("CONFIG docks load=%d file=%s",loaded,app.config);
 if(loaded==1)return;
 if(loaded<0){app.readonly=1;feedback(0,"Saved settings need repair; file preserved");if(dock_set_valid(&app.set))return;}
 memset(&legacy,0,sizeof legacy);seed(&legacy);
 int v1=dock_config_load(&legacy,&position,app.legacy_config);
 int fresh_pins=0;
 if(!v1){int pins=dock_load_pins(&legacy,app.pins);if(pins<0)app.readonly=1;fresh_pins=pins>0;note("PINS import=%d file=%s",pins,app.pins);}
 if(v1<0)app.readonly=1;
 dock_prefs_default(&prefs);int p=dock_prefs_load(&prefs,app.legacy_prefs);
 note("CONFIG legacy load=%d prefs=%d file=%s",v1,p,app.legacy_config);
 dock_set_from_v1(&app.set,&legacy,&position,p==1?&prefs:NULL);
 /* Nothing of 0.1 found: a new configuration, with automatic colours. */
 if(!v1 && p!=1 && !fresh_pins)dock_appearance_default(&app.set.defaults);
 if(app.readonly)feedback(0,"Saved settings need repair; file preserved");
}
/* Strict: a typo must not silently become 0 or "no override". */
static int option_number(const char *text,int minimum,int maximum,int *value) {
 char *end;long n=strtol(text,&end,10);
 if(end==text || *end || n<minimum || n>maximum)return 0;
 *value=(int)n;return 1;
}
/* Without a requester (Dockbar runs with pr_WindowPtr -1 by then). */
static int object_exists(const char *path) {
 BPTR lock=Lock((CONST_STRPTR)path,SHARED_LOCK);
 if(lock)UnLock(lock);
 return lock!=BNULL;
}
static int usage(void) {
 puts("Dockbar " DOCK_VERSION " [--icon-size 32..72] [--corner 0..8] [--seconds N] [--log PATH] [--config PATH]"
      " [--legacy-config PATH] [--legacy-prefs PATH] [--pins PATH] [--svg-library PATH] [--widgets PATH] [--widget-state PATH] [--no-transitions] [--log-entries] [--reopen-on-resize] [--full-redraw] [--no-compose]");
 return 20;
}
/* A widget instance still shown or edited somewhere is not freed, whatever
 * its active flag says. One that is not is freed right after this answer,
 * so the windows forget it first. */
static int widget_in_use(const struct DockAppletRuntime *r) {
 if(r==configure_after_rebuild || dock_applet_ui_shows(r))return 1;
 for(int i=0;i<DOCK_MAX_DOCKS;i++)for(int k=0;k<DOCK_MAX_ITEMS;k++)if(app.views[i].visual[k].applet==r)return 1;
 dock_applet_ui_forget(r);
 return 0;
}
int main(int argc,char **argv) {
 int seconds=0,old_svg=0;const char *logpath=NULL,*library="PROGDIR:libs/svgicon.library",*applets="PROGDIR:Widgets",*applet_state="PROGDIR:WidgetState";int widgets_given=0,state_given=0;
 dock_applet_ui_editable=widget_editable;app.smooth_asked=-1;dock_drag_state_init(&app.drag);
 app.config="PROGDIR:docks.config";app.legacy_config="PROGDIR:dock.config";app.legacy_prefs="PROGDIR:dock.prefs";
 app.pins="PROGDIR:dock.pins";app.cli_corner=-1;
 app.panel_view=app.menu_view=-1;
 app.pen_text[0]=app.pen_text[1]=app.pen_muted[0]=app.pen_muted[1]=app.pen_white=-1;
 for(int i=1;i<argc;i++){
  if(!strcmp(argv[i],"--seconds") && i+1<argc){if(!option_number(argv[++i],0,1000000,&seconds))return usage();}
  else if(!strcmp(argv[i],"--icon-size") && i+1<argc){if(!option_number(argv[++i],DOCK_ICON_MIN,DOCK_ICON_MAX,&app.cli_size))return usage();}
  else if(!strcmp(argv[i],"--corner") && i+1<argc){if(!option_number(argv[++i],0,DOCK_CORNER_MAX,&app.cli_corner))return usage();}
  else if(!strcmp(argv[i],"--log") && i+1<argc)logpath=argv[++i];
  else if(!strcmp(argv[i],"--log-entries"))app.log_entries=1;
  else if(!strcmp(argv[i],"--reopen-on-resize"))dock_resize_in_place=0;
  else if(!strcmp(argv[i],"--full-redraw"))dock_partial_redraw=0;
  else if(!strcmp(argv[i],"--no-compose"))app.no_compose=1;
  else if(!strcmp(argv[i],"--trace-present"))trace_present=1;
  else if(!strcmp(argv[i],"--dump-frames") && i+1<argc)dump_frames=atoi(argv[++i]);
  else if(!strcmp(argv[i],"--config") && i+1<argc)app.config=argv[++i];
  else if(!strcmp(argv[i],"--legacy-config") && i+1<argc)app.legacy_config=argv[++i];
  else if(!strcmp(argv[i],"--legacy-prefs") && i+1<argc)app.legacy_prefs=argv[++i];
  else if(!strcmp(argv[i],"--pins") && i+1<argc)app.pins=argv[++i];
  else if(!strcmp(argv[i],"--no-transitions"))dock_transitions_enabled=0;
  else if((!strcmp(argv[i],"--widgets") || !strcmp(argv[i],"--applets")) && i+1<argc){applets=argv[++i];widgets_given=1;}
  else if((!strcmp(argv[i],"--widget-state") || !strcmp(argv[i],"--applet-state")) && i+1<argc){applet_state=argv[++i];state_given=1;}
  else if(!strcmp(argv[i],"--svg-library") && i+1<argc)library=argv[++i];
  /* Removed in 0.5-test13 (it only replaced the Tools drawer's own icon).
   * Start lines written for older versions still start the dock. */
  else if(!strcmp(argv[i],"--svg")){if(i+1<argc)i++;old_svg=1;
   puts("Dockbar: --svg is no longer used and is ignored; remove it from the start line.");
   /* Mainline before 2026-09-24 drops unflushed output to a redirected
    * file, and the dock runs on for a long time after this. */
   fflush(stdout);}
  else{printf("Dockbar: unknown option %s\n",argv[i]);return usage();}
 }
 /* Started from Wanderer: options come from the icon's tool types. */
 static char wb_log[DOCK_PATH];
 if(argc==0 && argv){
  struct WBStartup *wb=(struct WBStartup*)argv;
  if(wb->sm_NumArgs>0){
   BPTR old=CurrentDir(wb->sm_ArgList[0].wa_Lock);
   struct DiskObject *icon=GetDiskObject((CONST_STRPTR)wb->sm_ArgList[0].wa_Name);
   if(icon){
    static char log_key[]="LOG";
    STRPTR value=(STRPTR)FindToolType((CONST STRPTR *)icon->do_ToolTypes,(STRPTR)log_key);
    if(value){snprintf(wb_log,sizeof wb_log,"%s",(const char*)value);logpath=wb_log;}
    FreeDiskObject(icon);
   }
   CurrentDir(old);
  }
 }
 if(logpath){app.log=fopen(logpath,"w");if(!app.log)return 20;}
 /* A dock item, panel or drop on a disk that is not inserted (or an assign
  * that is gone) must never stop the dock behind an "insert volume"
  * requester: icon loading, identity checks and drawer reading all touch
  * such paths. Dockbar reports these itself. */
 {struct Process *me=(struct Process *)FindTask(NULL);app.requesters=me->pr_WindowPtr;me->pr_WindowPtr=(APTR)-1;}
 /* One process manages every dock; a second process would put duplicate
  * docks at the same saved positions. */
 static char instance_name[]="Dockbar";
 Forbid();
 struct MsgPort *running=FindPort((CONST_STRPTR)instance_name);
 if(running){
  /* Starting Dockbar again opens the running one's Preferences: the way
   * there that needs no dock, also with every grip hidden. */
  struct MsgPort *reply=CreateMsgPort();
  struct Message ask;
  if(reply){memset(&ask,0,sizeof ask);ask.mn_ReplyPort=reply;ask.mn_Length=sizeof ask;PutMsg(running,&ask);}
  Permit();
  if(reply){WaitPort(reply);GetMsg(reply);DeleteMsgPort(reply);}
  else{
   struct EasyStruct busy={sizeof(struct EasyStruct),0,(CONST_STRPTR)"Dockbar",(CONST_STRPTR)"Dockbar is already running.",(CONST_STRPTR)"OK"};
   EasyRequestArgs(NULL,&busy,NULL,NULL);
  }
  if(app.log)fclose(app.log);
  return 5;
 }
 app.instance_port=dock_quiet_port();
 if(app.instance_port){app.instance_port->mp_Node.ln_Name=instance_name;app.instance_port->mp_Node.ln_Pri=0;AddPort(app.instance_port);}
 /* Separate from "Dockbar", which older versions answer by opening
  * Preferences whatever the message holds: this name means widgets.h. */
 static char widgets_name[]=DOCKBAR_WIDGETS_PORT;
 app.widgets_port=FindPort((CONST_STRPTR)widgets_name)?NULL:dock_quiet_port();
 if(app.widgets_port){app.widgets_port->mp_Node.ln_Name=widgets_name;app.widgets_port->mp_Node.ln_Pri=0;AddPort(app.widgets_port);}
 Permit();
 CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);
 int ok=CyberGfxBase!=NULL;
 struct Screen *first_screen=NULL;
 if(ok){
  note("START version=%s icon.library=%u.%u intuition=%u.%u",DOCK_VERSION,IconBase->lib_Version,IconBase->lib_Revision,IntuitionBase->LibNode.lib_Version,IntuitionBase->LibNode.lib_Revision);
  if(old_svg)note("OPTION --svg ignored (no longer used)");
  app.input_port=dock_quiet_port();
  if(app.input_port)app.input_request=(struct IOStdReq*)CreateIORequest(app.input_port,sizeof(struct IOStdReq));
  if(app.input_request && !OpenDevice((CONST_STRPTR)"input.device",0,(struct IORequest*)app.input_request,0))InputBase=app.input_request->io_Device;
  note("SVG available=%d",dock_svg_open(library));
  if(!dock_elapsed_exact())note("TIME timer.device not available: elapsed time follows the calendar clock, steps over 10 s are not counted");
  /* Started from S:User-Startup the Workbench screen is not open yet; this
   * opens it, before our subscription could make that wait for ourselves
   * (see lock_open_screen). Held until the docks are open on it. */
  first_screen=LockPubScreen(NULL);
  note("SCREEN at start=%d",first_screen!=NULL);
  /* Without these notices the docks would keep the Workbench screen from
   * closing, which blocks every screen mode change. */
  app.notify_port=dock_quiet_port();
  if(app.notify_port)app.notify=StartScreenNotifyTags(SNA_Notify,SNOTIFY_WAIT_REPLY|SNOTIFY_BEFORE_CLOSEWB|SNOTIFY_AFTER_OPENWB,
   SNA_MsgPort,(IPTR)app.notify_port,SNA_Priority,0,TAG_END);
  note("SCREEN notify=%d",app.notify!=NULL);
  ok=InputBase!=NULL && app.notify!=NULL;
  if(!InputBase)note("FAIL input device");
  if(!app.notify){
   /* Without the notices our windows would block every screen mode change. */
   note("FAIL screen notification unavailable");
   struct EasyStruct failed={sizeof(struct EasyStruct),0,(CONST_STRPTR)"Dockbar",
    (CONST_STRPTR)"Dockbar cannot watch for screen changes\nand will not start, so that it cannot block\nscreen mode changes.",(CONST_STRPTR)"OK"};
   EasyRequestArgs(NULL,&failed,NULL,NULL);
  }
 } else note("FAIL libraries");
 /* 0.5.1 had a second apkg package, dockbar-info-only; dockbar replaces it
  * since 0.5.2, and apkg keeps the user's files in the old drawer. Take its
  * settings over once, when this drawer has none yet. */
 if(!state_given && !strcmp(app.config,"PROGDIR:docks.config") && !strcmp(app.pins,"PROGDIR:dock.pins") &&
    !strcmp(app.legacy_config,"PROGDIR:dock.config"))
  dock_migrate_from_sibling("dockbar","dockbar-info-only",note);
 /* Widgets were called applets before 0.5-test3: a drawer from an earlier
  * build is still used when the new one is not there. */
 if(!widgets_given && !object_exists("PROGDIR:Widgets") && object_exists("PROGDIR:Applets"))applets="PROGDIR:Applets";
 if(!state_given && !object_exists("PROGDIR:WidgetState") && object_exists("PROGDIR:AppletState"))applet_state="PROGDIR:AppletState";
 if(ok){note("TRANSITIONS enabled=%d duration=%d",dock_transitions_enabled,DOCK_FADE_MS);dock_applets_note=note;note("WIDGETS init=%d dir=%s state=%s",dock_applets_init(applets,applet_state),applets,applet_state);load_settings();start_wallpaper_notify();
  /* No screen even so (it could not be opened, or is being reset): wait
   * for its open notice instead of ending. */
  int attached=attach_screen();
  if(attached<0)note("SCREEN not available at start; waiting for the Workbench screen");
  ok=attached!=0;}
 if(first_screen){UnlockPubScreen(NULL,first_screen);first_screen=NULL;}
 uint64_t started_ms=dock_elapsed_ms();time_t last=0;
 unsigned long draws=0,part_draws=0;uint64_t draw_us=0,part_us=0,run_us=dock_applets_now_us();
 while(ok && !app.quit){
  if(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C)break;
  time_t now=time(NULL);if(seconds && dock_elapsed_ms()-started_ms>=(uint64_t)seconds*1000u)break;
  
  /* Screen notices first: nothing below may touch a closing screen. */
  if(!handle_screen_notify()){note("SCREEN reattach failed; waiting for the next open");}
  handle_wallpaper_notify();
  handle_second_start();
  handle_widget_requests();
  if(!app.screen){Delay(5);continue;}
  
  if(now!=last){
   dock_applets_poll(dock_applets_now());
   if(!app.rebuild){int freed=dock_applets_collect(widget_in_use);if(freed)note("WIDGET %d removed instance(s) freed, %d left",freed,dock_applets_count());
    /* Preferences can still put back a set that has them. */
    if(!app.prefs.window)dock_applets_purge(widget_referenced);}
   for(int vi=0;vi<app.set.count;vi++)for(int ai=0;ai<model_of(vi)->count;ai++){
    struct Visual *vis=&app.views[vi].visual[ai];struct DockAppletRuntime *r=vis->applet;
    /* A package that came, went or changed (install, Rescan): its size
     * and picture may differ, so the docks are built again from scratch
     * (an update would keep the item's old picture and size). */
    if(r && r->package_changed){r->package_changed=0;if(!app.rebuild_asked){app.rebuild=app.rebuild_asked=1;note("WIDGET %s/%s package changed; docks rebuilt",r->state.package,r->state.instance);}}
    if(r && r->changed){
     struct DockView *v=&app.views[vi];v->dirty=1;
     uint64_t signature=dock_applet_compact_signature(&r->state.snapshot,r->package.primary);
     if(strcmp(vis->applet_icon,r->state.snapshot.icon))applet_image_load(vis);
     if(signature!=vis->signature && v->pixels && slot_visible(v,ai)){
      struct DockRect rect=v->layout.slots[ai].content;
      if(v->layout.orientation)rect.y-=v->scroll;else rect.x-=v->scroll;
      if(rect.x>=0&&rect.y>=0&&rect.x+rect.w<=v->ww&&rect.y+rect.h<=v->wh){
       vis->fade_pending=dock_fade_begin(&vis->fade,v->pixels+4*((size_t)rect.y*v->ww+rect.x),v->ww*4,rect.w,rect.h,dock_applets_now());
       if(!vis->fade_pending)dock_fade_clear(&vis->fade);
      }
     }
     vis->signature=signature;
    }
   }
   for(int vi=0;vi<app.set.count;vi++)for(int ai=0;ai<model_of(vi)->count;ai++)if(app.views[vi].visual[ai].applet)app.views[vi].visual[ai].applet->changed=0;
   for(int i=0;i<app.set.count;i++)for(int j=0;j<model_of(i)->count;j++)
    if(app.views[i].window && dock_widget_update(&app.views[i].visual[j].widget,now))app.views[i].dirty=1;
   if(app.status[0] && (int)(milliseconds()-app.status_until)<=1000)app.views[app.status_view].dirty=1;
   last=now;
  }
  /* Advance by monotonic time, skipping missed frames without catch-up work. */
  uint64_t animation_now=dock_applets_now();int animating=0;unsigned frame_in=40;
  for(int vi=0;vi<app.set.count;vi++)if(app.views[vi].window)
   for(int ai=0;ai<model_of(vi)->count;ai++){
    struct Visual *vis=&app.views[vi].visual[ai];
    if(vis->fade.from){animating=1;app.views[vi].dirty=1;}
    struct DockSprite *sprite=&vis->sprite;
    if(sprite->pixels && sprite->fps>0){uint64_t ms=animation_now-sprite->started;int frame=dock_sprite_index(sprite,ms);
     /* A new frame redraws the icon only; the loop sleeps until the next one. */
     if(frame!=sprite->frame){sprite->frame=frame;if(slot_visible(&app.views[vi],ai))mark_part(vi,ai);}
     unsigned m=(unsigned)(ms%1000),k=m*(unsigned)sprite->fps/1000,next=((k+1)*1000+(unsigned)sprite->fps-1)/(unsigned)sprite->fps-m;
     if(next<frame_in)frame_in=next;}
   }
  poll_editor();poll_prefs();poll_about();poll_items();
  /* After a save the docks are rebuilt first; the list shows the result. */
  if(items_back_dock && !app.editor.window && app.screen && !app.rebuild){
   int view=dock_set_find(&app.set,items_back_dock),at=items_back_index;
   items_back_dock=0;
   if(view>=0){open_items(view);fill_items(at<app.set.docks[view].model.count?at:0);}
  }
  char selected_applet[80];int applet_action=dock_applet_ui_poll(dock_applets_now(),selected_applet,sizeof selected_applet);
  {int offered=dock_applet_ui_offered_dock();if(applet_action==1 && offered>=0 && offered<app.set.count)app.edit_view=offered;}
  if(applet_action==1 && !editing_blocked(app.edit_view)){
   next_set=app.set;struct DockItem fresh={0};fresh.kind=DOCK_APPLET;snprintf(fresh.path,sizeof fresh.path,"%s",selected_applet);dock_applets_new_id(fresh.dir,sizeof fresh.dir);
   const struct DockAppletPackage *pkg=dock_applet_find(dock_applets_catalog(),selected_applet);snprintf(fresh.label,sizeof fresh.label,"%.63s",pkg?pkg->name:"Applet");
   if(dock_insert(&next_set.docks[app.edit_view].model,0,&fresh,next_set.docks[app.edit_view].model.count)>0 && commit("Widget added",app.edit_view,0)){
    configure_after_rebuild=dock_applets_get(fresh.path,fresh.dir);
    /* A widget without settings is ready as it is. */
    if(configure_after_rebuild && !dock_applet_has_settings(configure_after_rebuild))configure_after_rebuild=NULL;
   }
  }else if(applet_action==2){struct DockAppletRuntime *r=dock_applet_ui_instance();if(r){next_set=app.set;for(int vi=0;vi<next_set.count;vi++)for(int ai=0;ai<next_set.docks[vi].model.count;ai++){struct DockItem *it=&next_set.docks[vi].model.items[ai];if(it->kind==DOCK_APPLET&&!strcmp(it->path,r->state.package)&&!strcmp(it->dir,r->state.instance)){
   /* The settings window checks too; this is the place that changes the dock. */
   if(!editing_blocked(vi)){note_removed(&app.set.docks[vi].model,0,ai);dock_remove(&next_set.docks[vi].model,0,ai);if(commit("Widget removed",vi,0))forget_removed();removed.n=0;}
   goto applet_removed;}}}}applet_removed:;
  for(int i=0;i<app.set.count && !app.rebuild;i++)if(app.views[i].window && !app.views[i].resizing){app.drop_view=i;app.drop_container=0;dock_drop_poll_batch(&app.views[i].drop,dropped,NULL);}
  if(!app.rebuild && app.panel.window){app.drop_view=app.panel_view;app.drop_container=app.panel.container;dock_drop_poll_batch(&app.panel.drop,dropped,NULL);}
  if(!app.rebuild)dispatch_menu();
  for(int i=0;i<app.set.count && !app.rebuild;i++)dispatch(i,0);
  if(!app.rebuild && app.panel.window)dispatch(app.panel_view,1);
  if(!app.rebuild && app.drawers.window)drawers_dispatch();
  if(resizing_any()){resize_poll();for(int i=0;i<app.set.count;i++)replay_deferred(i);}
  /* Another edit waits for a resize in progress (at most RESIZE_TIMEOUT_MS). */
  int resize_wait=resizing_any();
  
  if(app.rebuild && !resize_wait && !app.rebuild_asked && hold.index<0 && app.set.defaults.text==app.smooth_asked){
   int view=app.reopen_view,container=app.reopen_container;
   cancel_drag();
   if(update_docks()){
    app.rebuild=0;app.reopen_view=-1;app.reopen_container=0;
    if(view>=0 && view<app.set.count && container)open_panel(view,container,1);
   }
  }
  if(app.rebuild && !resize_wait){
   dock_applet_ui_close();
   int view=app.reopen_view,container=app.reopen_container;
   int asked=app.rebuild_asked;
   if(hold.index>=0)hold_free();
   cancel_drag();close_all();app.rebuild=0;app.rebuild_asked=0;app.reopen_view=-1;app.reopen_container=0;
   /* The text setting may have changed in Preferences; measured sizes
    * depend on it, so it is set up before the docks are laid out. */
   if(app.screen && app.set.defaults.text!=app.smooth_asked)smooth_open(app.screen);
   /* Hold the screen while none of our windows is open. */
   struct Screen *held=lock_open_screen();
   int opened=held==app.screen?open_all():0;
   if(held)UnlockPubScreen(NULL,held);
   if(!opened){detach_screen("rebuild failed");if(attach_screen()<=0){ok=0;break;}}
   else snapshot_invalidate(asked?DOCK_SNAP_ASKED:DOCK_SNAP_OPENED);
   if(view>=0 && view<app.set.count && container)open_panel(view,container,1);
  }
  
  if(configure_after_rebuild && lock_screen()){dock_applet_ui_open(app.screen,configure_after_rebuild,1);configure_after_rebuild=NULL;unlock_screen();}
  if(app.flash_until && (int)(milliseconds()-app.flash_until)>=0){
   if(app.flash_view<app.set.count && app.views[app.flash_view].pressed==app.flash_index){app.views[app.flash_view].pressed=-1;app.views[app.flash_view].dirty=1;}
   app.flash_until=0;
  }
  int left_down=InputBase && (PeekQualifier()&IEQUALIFIER_LEFTBUTTON);
  /* A release outside every dock window may never reach us as SELECTUP. */
  if(app.drag.own.kind && !left_down)finish_drag(app.screen->MouseX,app.screen->MouseY);
  /* Wanderer draws its drag image straight onto the screen and puts back
   * what it saved underneath, which can be older than a dock or a group
   * panel opened meanwhile; no refresh event follows. Redraw our windows
   * once when such a drag ends. */
  if(dock_drag_state_poll(&app.drag,left_down)){for(int i=0;i<app.set.count;i++)app.views[i].dirty=1;app.panel_dirty=1;}
  if(app.panel_pending && app.panel.window){
   int sx=app.screen->MouseX,sy=app.screen->MouseY;
   if(abs(sx-app.pending_x)>=6 || abs(sy-app.pending_y)>=6)app.pending_moved=1;
   if(!left_down){
    int close=!app.pending_moved || (!inside(app.panel.window,sx,sy) && view_at(sx,sy)<0);
    app.panel_pending=0;if(close)close_panel();
   }
  }
  
  if(!app.editor.window && !app.prefs.window && !app.menu.window && !dock_applet_ui_window())pointer_update(app.screen->MouseX,app.screen->MouseY,left_down);
  else under_view=-1;
  /* Hover effects hold still during a foreign drag, as all drawing does
   * (see pointer_update); afterwards they go on from where they were. */
  if(!hold_drawing()){unsigned now_ms=milliseconds();for(int i=0;i<app.set.count;i++)if(effects_step(i,now_ms))animating=1;}
  
  /* A picture closes windows; not while one is changing its box. */
  if(!resizing_any())snapshot_work();
  /* Not while a foreign drag lasts (see pointer_update): its end redraws all. */
  if(!hold_drawing())for(int i=0;i<app.set.count;i++){
   if(app.views[i].dirty){uint64_t t=dock_applets_now_us();draw_view(i);draws++;draw_us+=dock_applets_now_us()-t;}
   else if(app.views[i].part_dirty){uint64_t t=dock_applets_now_us();draw_part(i);part_draws++;part_us+=dock_applets_now_us()-t;}
  }
  /* --trace-present: the counts every 10 s, for idle and hover costs */
  {static uint64_t last;uint64_t now_us=dock_applets_now_us();
   if(trace_present && now_us-last>=10000000u){last=now_us;
    note("DRAWS total=%lu partial=%lu full_ms=%lu partial_ms=%lu present_ms=%lu run_ms=%lu",draws,part_draws,(unsigned long)(draw_us/1000),(unsigned long)(part_us/1000),(unsigned long)(present_us/1000),(unsigned long)((now_us-run_us)/1000));}
  }
#if DOCK_DRAGDIAG
  {/* Screen inside each dock against the buffer last drawn: pixels that
    * differ (more than a little, for colour rounding) and their box. */
   static unsigned last;static int after;int f=foreign_drag();
   if(f)after=1500;
   if((f || after>0) && milliseconds()-last>=500){
    last=milliseconds();if(!f)after-=500;
    for(int i=0;i<app.set.count;i++){
     struct DockView *v=&app.views[i];if(!v->window || v->dirty)continue;
     unsigned char *scr=malloc((size_t)v->ww*v->wh*4);if(!scr)continue;
     if(ReadPixelArray(scr,0,0,(UWORD)(v->ww*4),&app.screen->RastPort,v->window->LeftEdge,v->window->TopEdge,(UWORD)v->ww,(UWORD)v->wh,RECTFMT_ARGB)==(ULONG)(v->ww*v->wh)){
      int n=0,x0=v->ww,y0=v->wh,x1=-1,y1=-1;
      for(int y=0;y<v->wh;y++)for(int x=0;x<v->ww;x++){
       if(v->nobg && !v->mask_shown[y*v->ww+x])continue;
       unsigned char *a=scr+4*(y*v->ww+x),*b=v->pixels+4*(y*v->ww+x);
       if(abs(a[1]-b[1])+abs(a[2]-b[2])+abs(a[3]-b[3])>24){n++;if(x<x0)x0=x;if(y<y0)y0=y;if(x>x1)x1=x;if(y>y1)y1=y;}
      }
      note("DIAG %s dock=%d differ=%d box=%d,%d-%d,%d hover=%d",f?"during":"after",app.set.docks[i].id,n,x0,y0,x1,y1,v->hover);
     }
     free(scr);
    }
   }}
#endif
  if(app.panel.window && app.panel_dirty && !hold_drawing()){dock_panel_draw(&app.panel);app.panel_dirty=0;}
  update_tooltip();
  /* Pictures kept for the drawer panel go back two minutes after it closed. */
  if(!app.drawers.window && dock_drawer_cache()->count && milliseconds()-app.drawers_closed_at>120000u){
   note("DRAWER cache released icons=%lu bytes=%lu",dock_drawer_cache()->count,dock_drawer_cache()->bytes);dock_drawer_cache_flush();}
  /* A panel still reading or loading icons gets the next pass at once. */
  if(!(app.drawers.window && dock_drawer_busy(&app.drawers)))Delay(animating || frame_in<30?1:2);
 }
 note("DRAWS total=%lu partial=%lu full_ms=%lu partial_ms=%lu present_ms=%lu run_ms=%lu",draws,part_draws,(unsigned long)(draw_us/1000),(unsigned long)(part_us/1000),(unsigned long)(present_us/1000),(unsigned long)((dock_applets_now_us()-run_us)/1000));
 dock_drawer_cache_flush();
 note("END success=%d",ok);
 if(app.drag.own.kind)cancel_drag();
 dock_editor_close(&app.editor);dock_prefs_window_close(&app.prefs);
 dock_about_close(&app.about);app.about_again=0;dock_item_list_close(&app.items);
 detach_screen("quit");
 stop_wallpaper_notify();
 stop_screen_notify();
 if(InputBase){CloseDevice((struct IORequest*)app.input_request);InputBase=NULL;}
 if(app.input_request)DeleteIORequest((struct IORequest*)app.input_request);
 if(app.input_port)dock_quiet_port_free(app.input_port);
 dock_applet_ui_close();dock_applets_close();dock_svg_close();
 if(CyberGfxBase)CloseLibrary(CyberGfxBase);
 /* A second start that asked just now is waiting for its answer. */
 if(app.instance_port){struct Message *m;RemPort(app.instance_port);while((m=GetMsg(app.instance_port)))ReplyMsg(m);dock_quiet_port_free(app.instance_port);}
 if(app.widgets_port){struct Message *m;RemPort(app.widgets_port);
  while((m=GetMsg(app.widgets_port))){if(m->mn_Length>=sizeof(struct DockbarWidgetsMsg)){((struct DockbarWidgetsMsg *)m)->result=DOCKBAR_WIDGETS_FAILED;snprintf(((struct DockbarWidgetsMsg *)m)->text,sizeof ((struct DockbarWidgetsMsg *)m)->text,"Dockbar is quitting");}ReplyMsg(m);}
  dock_quiet_port_free(app.widgets_port);}
 if(app.log)fclose(app.log);
 ((struct Process *)FindTask(NULL))->pr_WindowPtr=app.requesters;
 return ok?0:20;
}
