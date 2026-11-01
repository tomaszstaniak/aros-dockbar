#ifndef DOCK_APPLET_CARDS_AROS_H
#define DOCK_APPLET_CARDS_AROS_H
#include <stdint.h>
struct Screen;struct Window;struct DockColors;struct DockAppletRuntime;
int dock_applet_cards_open(struct Screen *,struct DockAppletRuntime *,int,int,int,int,int,const struct DockColors *);
/* 6 opens settings for instance(); 0 otherwise. Owns its Intuition queue. */
int dock_applet_cards_poll(uint64_t);
struct Window *dock_applet_cards_window(void);
struct DockAppletRuntime *dock_applet_cards_instance(void);
void dock_applet_cards_forget(const struct DockAppletRuntime *);
void dock_applet_cards_close(void);
#endif
