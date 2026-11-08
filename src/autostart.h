#ifndef DOCK_AUTOSTART_H
#define DOCK_AUTOSTART_H
#include <stddef.h>

/* Starting Dockbar with Wanderer, through SYS:WBStartup.

   Wanderer's ExecuteStartup opens every icon in SYS:WBStartup, tool and
   project icons alike. A project icon without a file of its own is enough:
   workbench.library starts its default tool from the tool's own drawer.
   So the launcher is a project icon, SYS:WBStartup/Dockbar.info, whose
   default tool is the full path of this Dockbar: the program in its own
   drawer starts, finds its widgets and settings there (PROGDIR:), and an
   update of that drawer is what starts next time.

   A copy of the program in SYS:WBStartup instead has SYS:WBStartup as its
   PROGDIR: it finds no widgets, keeps its settings there, and is not
   updated with the drawer it came from. Such a copy is recognised only by
   its name (beginning with Dockbar) and the program's own version string,
   and is replaced only on request, as is a start icon of another
   installation. The rules are in autostart_plan.h.

   Nothing else is searched: only SYS:WBStartup, and S:User-Startup is read
   to point out a second start line. */

enum DockAutostartState {
    DOCK_AUTOSTART_OFF,        /* nothing in SYS:WBStartup starts Dockbar */
    DOCK_AUTOSTART_ON,         /* a start icon starts this Dockbar */
    DOCK_AUTOSTART_OTHER,      /* a start icon starts another Dockbar (found) */
    DOCK_AUTOSTART_COPY,       /* a copy of the program is there (found) */
    DOCK_AUTOSTART_SELF_COPY,  /* this Dockbar is such a copy itself */
    DOCK_AUTOSTART_BLOCKED,    /* an entry that could not be read, or of the
                                  program's name and not Dockbar's (problem) */
    DOCK_AUTOSTART_UNKNOWN,    /* SYS:WBStartup or this program not found */
    DOCK_AUTOSTART_INCOMPLETE  /* SYS:WBStartup could not be read to the end */
};

/* The settings of a copy, against those of this drawer (docks.config,
   dock.pins and the WidgetState drawer). */
enum DockAutostartSettings {
    DOCK_SETTINGS_NONE,   /* the copy has none */
    DOCK_SETTINGS_TAKE,   /* this drawer has none: the copy's are copied here */
    DOCK_SETTINGS_SAME,   /* both have the same */
    DOCK_SETTINGS_DIFFER, /* this drawer has its own: both sets stay */
    DOCK_SETTINGS_LEFT    /* the copy has some but no docks.config: they stay */
};

/* Results of enable and disable. */
enum {
    DOCK_AUTOSTART_UNCHANGED = 0, /* nothing changed; message says why */
    DOCK_AUTOSTART_DONE = 1,      /* done; message may hold a notice */
    DOCK_AUTOSTART_PARTIAL = 2    /* done in part; message says what is left */
};

struct DockAutostart {
    int state;
    int ours;          /* a start icon for this Dockbar is there */
    char found[512];   /* the other start icon's program or the copy */
    char version[32];  /* of `found`, from its version string */
    char problem[512]; /* for BLOCKED: the entry */
    int copy_settings; /* the copy's drawer has settings */
    int settings;      /* DockAutostartSettings, for a copy */
    int user_startup;  /* line number in S:User-Startup naming Dockbar, or 0 */
};

/* The program's own file name (FilePart), from the start: argv[0] or the
   Workbench argument. "Dockbar" when not set. */
void dock_autostart_set_program(const char *name);

int dock_autostart_check(struct DockAutostart *a);

/* Makes SYS:WBStartup start this Dockbar: writes the start icon, keeping
   the tool types of the one there. Without `replace` only this Dockbar's
   own start icon is written: with a copy or another installation's start
   icon there nothing changes. With `replace` those are removed after the
   start icon was written; with `migrate`, a copy's docks.config, dock.pins
   and WidgetState are copied here first when this drawer has none of them,
   otherwise both sets stay and `message` says how to choose. `no_compose`
   adds the NOCOMPOSE tool type. Returns DOCK_AUTOSTART_UNCHANGED, _DONE or
   _PARTIAL (something to be removed was not: message names what still
   starts and what was left behind). */
int dock_autostart_enable(int replace, int migrate, int no_compose, char *message, size_t size,
                          void (*note)(const char *format, ...));

/* Removes this Dockbar's own start icons. Start icons of other
   installations and copies are left alone and reported. */
int dock_autostart_disable(char *message, size_t size, void (*note)(const char *format, ...));

#endif
