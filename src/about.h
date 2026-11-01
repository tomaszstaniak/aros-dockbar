#ifndef DOCK_ABOUT_H
#define DOCK_ABOUT_H
/* About Dockbar: a nonblocking Zune window (see docs/backlog/about-dialog.md
 * in the workspace). The docks, widgets and screen changes go on while it
 * is open; main polls it. Zero-initialize. */
struct Screen;
struct Window;
struct Library;

struct DockAbout {
    struct Window *window;
    struct Library *mui_base;
    void *application, *mui_window, *licenses, *licenses_text, *copy, *link, *root;
    char *notices;          /* the text shown by Licenses, read when asked */
    char info[400];         /* what Copy version info puts on the clipboard */
    char notices_path[256];
    unsigned int signals;   /* see zune_input.h */
    int again;
};

/* version: the program's version line; info: the plain text to copy (no
 * paths, settings or places). 1 open. */
int dock_about_open(struct DockAbout *a, struct Screen *screen, const char *version,
                    const char *info, const char *notices_path);
void dock_about_raise(struct DockAbout *a);
/* The Licenses window while it is open, else NULL: it is ours too, so it must
 * not end up in a see-through dock's desktop picture. */
struct Window *dock_about_licenses_window(const struct DockAbout *a);
/* 1 when the user closed it (main then calls dock_about_close). */
int dock_about_poll(struct DockAbout *a);
void dock_about_close(struct DockAbout *a);

#endif
