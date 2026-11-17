#ifndef DOCK_APPLET_VIEW_H
#define DOCK_APPLET_VIEW_H
#include <stddef.h>
#include <stdint.h>
struct DockAppletSnapshot;
/* A card described by the widget itself (views.json, "expanded" as an
 * array): blocks stacked from top to bottom, drawn by the host in its own
 * colours and fonts inside its frame, header controls and footer.
 *   {"text":"<field>"} or {"literal":"..."}, "style":"body|muted|big", "align":"right"
 *   {"trend":"<numeric field>","text":"<field>"}   number above zero rose, below fell
 *   {"icon":"<field>" or "icon","size":N}          artwork key from the data
 *   {"separator":true}   {"spacer":N}
 *   {"row":[blocks]}                               side by side
 *   {"list":"<section>","max":N,"row":[blocks]}    one row per data row; inside
 *        it text names label, value, detail or caption, trend the row's plot
 * A parent's blocks follow it directly in the array. */
enum { DV_TEXT, DV_TREND, DV_ICON, DV_SEPARATOR, DV_SPACER, DV_ROW, DV_LIST };
enum { DV_BODY, DV_MUTED, DV_BIG };
#define DV_NODES 48
#define DV_LIST_MAX 10
struct DockViewNode {
 unsigned char type, style, right, count; /* count: blocks inside a row or list */
 short size;                              /* icon or spacer size, list rows */
 char source[40], number[40], literal[64];
};
struct DockAppletView {
 char title[40],value[40],subtitle[40],range[40];
 char metric[2][40],metric_label[2][40];
 char strip[40],list[40],strip_title[64],list_title[64];
 int layout_count;struct DockViewNode layout[DV_NODES];
};
uint64_t dock_applet_view_signature(const struct DockAppletSnapshot *,const struct DockAppletView *,int);
uint64_t dock_applet_compact_signature(const struct DockAppletSnapshot *,const char *);
int dock_applet_view_parse(const char *,size_t,struct DockAppletView *);
void dock_applet_anchor(int sw,int sh,int bar,int w,int h,int ax,int ay,int aw,int ah,int vertical,int *x,int *y);
#endif
