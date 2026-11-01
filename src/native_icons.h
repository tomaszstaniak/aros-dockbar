#ifndef DOCK_NATIVE_ICONS_H
#define DOCK_NATIVE_ICONS_H

struct DiskObject;
struct RastPort;
struct Screen;
struct Window;
struct MsgPort;
struct AppWindow;

enum DockNativeMode {
    DOCK_NATIVE_PLANAR = 0,
    DOCK_NATIVE_ARGB = 1,
    DOCK_NATIVE_PALETTE = 2
};

/* Where the picture came from, for diagnostics and for the rule that a
 * dropped program shows the same artwork as in Wanderer. */
enum DockIconOrigin {
    DOCK_ICON_NONE = 0,       /* load failed */
    DOCK_ICON_OWN = 1,        /* the object's own .info */
    DOCK_ICON_IDENTIFIED = 2, /* system default chosen by object type, as Wanderer does */
    DOCK_ICON_GENERIC = 3     /* nothing identified: generic Tool picture */
};

typedef struct DockNativeIcon {
    struct DiskObject *object;
    int origin;
    int width;
    int height;
    int source_width;
    int source_height;
    int mode;
    /* Private owned straight A,R,G,B caches; drawing never resamples. */
    unsigned char *pixels[2];
    int pixel_width[2];
    int pixel_height[2];
} DockNativeIcon;

typedef struct DockDropTarget {
    struct MsgPort *port;
    int quiet;  /* port made by dock_quiet_port */
    struct AppWindow *window;
    /* Last delivered AppMessage coordinates, relative to the target window. */
    int mouse_x, mouse_y;
} DockDropTarget;

typedef void (*DockDropCallback)(const char *absolute_path, void *context);

/* Zero-initialize objects before first use. Free before reloading an icon.
 * Keep the screen and icon.library open until all icons have been freed.
 * CyberGfxBase must be open for drawing cached pixels. The caller supplies it.
 * Loading returns 1 on success and sets origin. Without an own .info the
 * picture is identified from the object like Wanderer's icon list does;
 * the generic Tool picture is only the last resort. Sources
 * are bounded to 1024px per axis; outputs to 512px, with no enlargement.
 * ARGB/indexed sources use independent filtered caches for normal/selected
 * states. Pure planar sources retain native rendering and native downsampling. */
int dock_native_load(DockNativeIcon *icon, const char *path,
                     struct Screen *screen, int size);
void dock_native_draw(DockNativeIcon *icon, struct RastPort *rp,
                      int x, int y, int selected);
void dock_native_free(DockNativeIcon *icon);
/* Time spent by dock_native_load since the program started, in
 * microseconds: reading and decoding (icon.library) and scaling (our
 * resampler), for the drawer panel's timing log. */
struct DockNativeTiming { unsigned long loads, read_us, scale_us; };
extern struct DockNativeTiming dock_native_timing;
const char *dock_icon_origin_name(int origin);
/* 1 drawer or volume, 0 other existing object, -1 cannot be examined. */
int dock_object_is_drawer(const char *path);

/* Close the target before closing its Window. The caller may Wait() on
 * 1UL << target.port->mp_SigBit. All calls belong to the UI task. */
/* A message port that never signals: for a program that polls its ports
 * instead of waiting on them. Every port made by CreateMsgPort takes one of
 * the task's 16 free signal bits, and each dock window takes one more for its
 * IDCMP port; with several docks nothing was left for Zune (the Preferences
 * window failed to open). Free only with dock_quiet_port_free. */
struct MsgPort *dock_quiet_port(void);
void dock_quiet_port_free(struct MsgPort *port);
/* The drop port is a quiet port: poll it. */
int dock_drop_open(DockDropTarget *target, struct Window *window);
/* Returns the number of paths delivered. Paths are valid during the callback
 * only; copy them into the model. Invalid/overlong paths (4096 bytes including
 * NUL) are skipped. Messages are replied before callbacks, even on failure.
 * target.mouse_x/mouse_y are set from the copied message before each callback.
 * Callbacks must not close/reopen the target or recursively poll it. */
int dock_drop_poll(DockDropTarget *target, DockDropCallback callback, void *context);
/* One drop, all its files at once: what was dropped together is opened
 * together. paths[] and the strings are valid during the callback only.
 * Paths are exact (a dropped .info stays .info). missing counts objects of
 * the same drop that could not be copied (no path, out of memory); count
 * may then be 0. */
typedef void (*DockDropBatchCallback)(const char *const *paths, int count, int missing, void *context);
int dock_drop_poll_batch(DockDropTarget *target, DockDropBatchCallback callback, void *context);
void dock_drop_close(DockDropTarget *target);
/* Launches through Workbench, preserving paths containing spaces. */
int dock_launch(const char *path);
/* Starts a program through Workbench with files as its Workbench arguments
 * (WBOPENA_ArgLock/ArgName), as Wanderer does when files are dropped on a
 * program's icon: the program's own icon (stack, tool types) is used, and a
 * Shell program gets them as quoted arguments. Workbench copies the locks
 * before this returns. At most 64 files. */
int dock_launch_with(const char *program, const char *const *files, int count);
/* 1 when Workbench would start this program as a Shell command: an
 * executable file with no icon of its own, or with a tool icon carrying the
 * CLI tool type; then *stack is the icon's stack (0 none).
 * Workbench builds that command line without escaping * and " in names
 * (workbench.library CLI_BuildCommandLine), so a caller with files starts
 * such a program itself. */
int dock_program_is_shell(const char *program, long *stack);
/* Compare .info-normalized, case-insensitive names, then resolve existing DOS
 * objects through Lock/SameLock. No locks are retained. Unresolved differing
 * names compare unequal; the original configured paths remain unchanged. */
int dock_same_object(const char *first, const char *second);

#endif
