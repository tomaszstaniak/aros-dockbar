#ifndef DOCK_APPLET_WORKER_AROS_H
#define DOCK_APPLET_WORKER_AROS_H
#include "instance.h"
#include "package.h"
#include "settings.h"
#include "../sprite.h"
struct Screen;
struct DockAppletRuntime {
  struct DockAppletPackage package;
  struct DockAppletInstance state;
  char directory[DA_PATH], settings_path[DA_PATH], settings[4096], icon_key[48],
      caption[160];
  int configured, active, changed, searching, recovered, reconcile_pending;
  struct DockAppletSnapshot *search_result;
  struct DockAppletRuntime *next;
};
int dock_applets_init(const char *, const char *);
void dock_applets_rescan(void);
const struct DockAppletCatalog *dock_applets_catalog(void);
struct DockAppletRuntime *dock_applets_get(const char *, const char *);
void dock_applets_layout_begin(void);
uint64_t dock_applets_now(void);
uint64_t dock_applets_now_us(void);
void dock_applets_poll(uint64_t);
void dock_applets_close(void);
int dock_applets_collect(int (*)(const struct DockAppletRuntime *));
int dock_applets_count(void);
/* After a removal the user confirmed and the configuration saved without
 * the instance: delete its settings and cache once its runtime is gone and
 * referenced() says no item uses it (then it stays). */
int dock_applets_forget(const char *package, const char *instance);
int dock_applets_purge(int (*referenced)(const char *package, const char *instance));
int dock_applets_ending(void);
extern void (*dock_applets_note)(const char *, ...);
int dock_applets_new_id(char *, size_t);
int dock_applet_save_settings(struct DockAppletRuntime *,
                              const struct DockAppletForm *, char *, size_t);
int dock_applet_load_form(struct DockAppletRuntime *, struct DockAppletForm *,
                          char *, size_t);
int dock_applet_search(struct DockAppletRuntime *, const char *, uint64_t);
int dock_applet_asset_path(const struct DockAppletRuntime *, const char *, char *, size_t, struct DockSpriteSpec *);
int dock_applet_icon_path(const struct DockAppletRuntime *, char *, size_t);
int dock_applet_sprite_path(const struct DockAppletRuntime *, char *, size_t, struct DockSpriteSpec *);
const char *dock_applet_primary(const struct DockAppletRuntime *);
const char *dock_applet_secondary(const struct DockAppletRuntime *);
/* The value on the dock: the primary field once there is data, otherwise a
 * mark short enough for the fixed room ("--", "..."); the tooltip and the
 * card name the state in words (dock_applet_primary). */
const char *dock_applet_compact(const struct DockAppletRuntime *);
#endif
