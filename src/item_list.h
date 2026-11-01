#ifndef DOCK_ITEM_LIST_H
#define DOCK_ITEM_LIST_H
/* "Edit items...": every item of one dock as a list, the invisible ones
 * (spaces, and everything on a dock without a bar) included, with Edit,
 * Remove and moving up and down. Nonblocking Zune window; main owns the
 * model and fills the list again after each change. Zero-initialize. */
struct Screen;
struct Window;
struct Library;

enum { DOCK_LIST_EDIT = 1, DOCK_LIST_REMOVE, DOCK_LIST_UP, DOCK_LIST_DOWN, DOCK_LIST_CLOSE };

struct DockItemList {
    struct Window *window;
    struct Library *mui_base;
    void *application, *mui_window, *list, *note, *edit, *remove, *up, *down;
    unsigned int signals;   /* see zune_input.h */
    int again;
    int dock_id;            /* which dock it lists; views are rebuilt by id */
    int count;
    char title[128];        /* Zune keeps the pointer, not a copy */
};

int dock_item_list_open(struct DockItemList *l, struct Screen *screen, const char *title, int dock_id);
/* Replaces the entries; 'active' is selected (clamped), 'locked' disables
 * every change and shows why. */
void dock_item_list_fill(struct DockItemList *l, const char *const *lines, int count, int active, int locked);
/* 0 nothing yet, else DOCK_LIST_*; *index is the selected entry or -1. */
int dock_item_list_poll(struct DockItemList *l, int *index);
void dock_item_list_raise(struct DockItemList *l);
void dock_item_list_close(struct DockItemList *l);

#endif
