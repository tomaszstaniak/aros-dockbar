#ifndef DOCK_MODEL_H
#define DOCK_MODEL_H
#include <stddef.h>
#define DOCK_MAX_ITEMS 24
#define DOCK_MAX_GROUPS 8
#define DOCK_PATH 512
#define DOCK_LABEL 64
/* Values are stored in configuration files; append only. */
enum DockKind { DOCK_APP, DOCK_GROUP, DOCK_SEPARATOR, DOCK_CLOCK, DOCK_DRAWER, DOCK_SPACER, DOCK_COMMAND, DOCK_APPLET };
#define DOCK_DIR 256
/* DOCK_COMMAND: path holds an AmigaDOS command line, or a script file run
 * with Execute; dir is the working directory (empty: SYS:). */
enum { DOCK_RUN_SHELL = 1, DOCK_RUN_SCRIPT = 2 };
/* What files dropped on the middle of a program or command do: add a shortcut
 * (as before 0.5, and the default) or open them with it. Never switched on
 * by itself: a command does not run because something was dropped on it. */
enum { DOCK_DROP_ADD = 0, DOCK_DROP_OPEN = 1 };
/* A click on a drawer: a Wanderer window (as before) or the dock's own panel
 * with the drawer's current contents. */
enum { DOCK_OPEN_WANDERER = 0, DOCK_OPEN_PANEL = 1 };
/* How an item is shown; stored per item, 0 follows its dock (DockLook.present
 * holds DockPresentation - 1 there: 0 icon, 1 text, 2 icon and text). */
enum DockPresentation { DOCK_PRESENT_DOCK = 0, DOCK_PRESENT_ICON = 1, DOCK_PRESENT_TEXT = 2, DOCK_PRESENT_ICON_TEXT = 3 };
struct DockItem {
 enum DockKind kind;
 char path[DOCK_PATH];
 char label[DOCK_LABEL];
 char icon[DOCK_PATH];
 int group;
 int present;  /* DockPresentation */
 char dir[DOCK_DIR];
 int run;      /* DOCK_RUN_* flags of a command */
 int on_drop;  /* DOCK_DROP_*: programs and commands */
 int open;     /* DOCK_OPEN_*: drawers */
};
/* Items shown with an icon and/or a name, and started by a click. */
static inline int dock_kind_shown(int kind) { return kind == DOCK_APP || kind == DOCK_GROUP || kind == DOCK_DRAWER || kind == DOCK_COMMAND; }
struct DockGroup { int used,count; struct DockItem items[DOCK_MAX_ITEMS]; };
struct DockModel {
 struct DockItem items[DOCK_MAX_ITEMS];
 int count;
 struct DockGroup groups[DOCK_MAX_GROUPS];
};
int dock_normalize_path(const char *path,char *out,size_t capacity);
/* Container 0 is the root; 1..8 are used groups. Returns NULL for invalid
 * containers/counts. The returned array belongs to model. */
struct DockItem *dock_items(struct DockModel *model,int container,int *count);
/* Mutations: 1 changed, 0 duplicate/no-op, -1 invalid/full. Failure leaves the
 * model unchanged. Group children are APP only; groups cannot nest. 'before'
 * is in 0..destination count, using original indices for a same-container move. */
int dock_move(struct DockModel *model,int from,int index,int to,int before);
int dock_group_pair(struct DockModel *model,int from,int index,int targetRootIndex);
int dock_remove(struct DockModel *model,int container,int index);
int dock_add(struct DockModel *model,int container,const char *path,int before);
/* Drawers, separators, spacers and clocks are root items; a clock's path holds
 * its settings ("clock:" tokens). dock_insert copies an existing item. */
int dock_add_kind(struct DockModel *model,int container,enum DockKind kind,const char *path,int before);
int dock_insert(struct DockModel *model,int container,const struct DockItem *item,int before);
/* Legacy root APP import/export, retained for migration compatibility. */
int dock_pin(struct DockModel *model,const char *path);
int dock_save_pins(const struct DockModel *model,const char *file);
/* 1 complete, 0 missing, -1 invalid/unreadable (preserve existing file). */
int dock_load_pins(struct DockModel *model,const char *file);
int dock_item_width(const struct DockItem *item,int size);
int dock_width(const struct DockModel *model,int size);
int dock_item_x(const struct DockModel *model,int index,int size);
int dock_hit(const struct DockModel *model,int x,int y,int top,int size);
#endif
