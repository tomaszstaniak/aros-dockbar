#ifndef DOCK_APPLET_VIEW_H
#define DOCK_APPLET_VIEW_H
#include <stddef.h>
#include <stdint.h>
struct DockAppletSnapshot;
struct DockAppletView {
 char title[40],value[40],subtitle[40],range[40];
 char metric[2][40],metric_label[2][40];
 char strip[40],list[40],strip_title[64],list_title[64];
};
uint64_t dock_applet_view_signature(const struct DockAppletSnapshot *,const struct DockAppletView *,int);
uint64_t dock_applet_compact_signature(const struct DockAppletSnapshot *,const char *);
int dock_applet_view_parse(const char *,size_t,struct DockAppletView *);
void dock_applet_anchor(int sw,int sh,int bar,int w,int h,int ax,int ay,int aw,int ah,int vertical,int *x,int *y);
#endif
