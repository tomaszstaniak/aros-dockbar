#ifndef DOCK_DOCKS_H
#define DOCK_DOCKS_H
#include "model.h"
#include "config.h"
#include "prefs.h"
#include "theme.h"

/* Several independent docks in one process. Each has a stable id, a name,
 * its own items and groups, placement and look. The set is saved as one
 * file, replaced as a whole, so a failed save leaves every dock as it was. */

#define DOCK_MAX_DOCKS 8
#define DOCK_NAME 32

enum DockPlacement { DOCK_PLACE_FREE, DOCK_PLACE_TOP, DOCK_PLACE_BOTTOM, DOCK_PLACE_LEFT, DOCK_PLACE_RIGHT };
enum DockAlign { DOCK_ALIGN_START, DOCK_ALIGN_CENTER, DOCK_ALIGN_END };

/* Appearance that all docks share unless a dock overrides it. */
enum DockBackground { DOCK_BG_BAR = 0, DOCK_BG_NONE = 1 };
/* What an icon does while the pointer is on it (see hover.h). */
enum DockHoverEffect { DOCK_HOVER_NONE = 0, DOCK_HOVER_LIFT = 1, DOCK_HOVER_MAGNIFY = 2 };
#define DOCK_INHERIT (-1)

struct DockAppearance {
    int theme;         /* DockThemeChoice */
    int background;    /* DockBackground: bar, or icons and text only */
    int transparency;  /* 0 opaque .. 100 fully see-through bar */
    int glass;         /* 1 blurs the desktop picture behind the bar */
    int grip;          /* 1 shown, 0 hidden */
    int text;          /* 0 the system's own text drawing, 1 smooth (FreeType on
                        * the system's font files, see textrender.h) */
    int hover;         /* DockHoverEffect: Lift in new configurations, None in
                        * files written before 0.5-test11 */
    int running;       /* 1: a dot under each program that is running (see
                        * running.h); off unless chosen */
};

struct DockLook {
    int icon, corner, spacing, labels, orientation;
    /* Each is DOCK_INHERIT (use the set's defaults) or an own value. */
    int transparency, glass, theme, background, grip, hover, running;
    /* Own values only (no defaults): how items show unless an item says
     * otherwise (DockPresentation - 1: 0 icon, 1 text, 2 icon and text),
     * and whether text buttons always have a plate (1) or only when
     * hovered or pressed (0). */
    int present, buttons;
    /* Icon and text: 0 caption under the icon (new docks), 1 beside it
     * (how 0.4-test1 drew it; docks from that version keep it). */
    int beside;
};

struct DockPlace {
    int mode, align, offset;  /* edge placement; offset from the edge in pixels */
    int valid, x, y;          /* free position (valid 0: not yet placed) */
    int locked;
};

struct DockInstance {
    int id;
    char name[DOCK_NAME];
    struct DockLook look;
    struct DockPlace place;
    struct DockModel model;
};

struct DockSet {
    int count, next_id;
    struct DockAppearance defaults;
    struct DockInstance docks[DOCK_MAX_DOCKS];
};

/* New docks inherit every shared setting. */
void dock_look_default(struct DockLook *look);
/* Defaults of a new configuration (automatic colours). Settings read from
 * 0.1 or 0.2 files get the dark colours those versions drew instead. */
void dock_appearance_default(struct DockAppearance *appearance);
void dock_appearance_legacy(struct DockAppearance *appearance);
/* The values dock 'index' is drawn with: its own, else the defaults. */
void dock_appearance_of(const struct DockSet *set, int index, struct DockAppearance *out);
void dock_place_default(struct DockPlace *place);
int dock_set_valid(const struct DockSet *set);
/* Index of the dock with this id, or -1. */
int dock_set_find(const struct DockSet *set, int id);

/* Version 10 adds running indicators, shared and per dock (older files
 * read as off with every dock inheriting). Version 9 since 0.5-test11 (hover effect, shared and per dock; older
 * files read as None with every dock inheriting). Version 5 since 0.4-test2 (caption under or beside the icon); version 4
 * files of 0.4-test1 keep it beside. Version 4 since 0.4 (presentation of
 * items: icon, text, both); version 3
 * files of 0.3 are read as icons. Version 3 since 0.3 (shared defaults,
 * inheritance); version 2 files of
 * 0.2 are read and keep their look. 0.2 does not read version 3: it leaves
 * such a file untouched.
 * 1 loaded, 0 missing, -1 invalid/unreadable/future (file left untouched;
 * a valid .old recovery also returns -1 so the caller stays read-only). */
int dock_set_load(struct DockSet *set, const char *path);
/* 1 after complete replacement; 0 leaves the previous file in place. Refuses
 * to replace an unreadable or future-version file. */
int dock_set_save(const struct DockSet *set, const char *path);

/* One dock from 0.1's composition, position and preferences. The 0.1 files
 * are not modified by this or by any later save. */
void dock_set_from_v1(struct DockSet *set, const struct DockModel *model,
                      const struct DockPosition *position, const struct DockPrefs *prefs);

/* New empty dock (index returned) or -1 when full; name may be NULL. */
int dock_set_add(struct DockSet *set, const char *name);
/* Copy of dock 'index' with a new id and a name ending in " copy"; -1 when full. */
int dock_set_duplicate(struct DockSet *set, int index);
/* Removes a dock and its shortcuts; the last dock cannot be removed. */
int dock_set_remove(struct DockSet *set, int index);

/* Moves an item between containers of the same or different docks. A group
 * keeps its members. 1 moved, 0 no-op/duplicate, -1 invalid/full; failure
 * leaves the set unchanged. */
int dock_set_move(struct DockSet *set, int from_dock, int from_container, int index,
                  int to_dock, int to_container, int before);

/* Window position for a placement on a screen of sw x sh whose title bar
 * ends at 'top'. Always inside the screen; the free position is clamped. */
void dock_place_resolve(const struct DockPlace *place, int sw, int sh, int top,
                        int width, int height, int *x, int *y);

#endif
