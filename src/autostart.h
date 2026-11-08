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
   and is replaced only on request.

   Nothing else is searched: only SYS:WBStartup, and S:User-Startup is read
   to point out a second start line. */

enum DockAutostartState {
    DOCK_AUTOSTART_OFF,       /* nothing in SYS:WBStartup starts Dockbar */
    DOCK_AUTOSTART_ON,        /* a launcher starts this Dockbar */
    DOCK_AUTOSTART_OTHER,     /* a launcher starts another Dockbar (found) */
    DOCK_AUTOSTART_COPY,      /* a copy of the program is there (found) */
    DOCK_AUTOSTART_SELF_COPY, /* this Dockbar is such a copy itself */
    DOCK_AUTOSTART_BLOCKED,   /* SYS:WBStartup/Dockbar is something else */
    DOCK_AUTOSTART_UNKNOWN    /* SYS:WBStartup or this program not found */
};

struct DockAutostart {
    int state;
    char found[512];   /* the other launcher's program or the copy */
    char version[32];  /* of `found`, from its version string */
    int copy_settings; /* the copy's drawer has docks.config */
    int user_startup;  /* line number in S:User-Startup naming Dockbar, or 0 */
};

/* The program's own file name (FilePart), from the start: argv[0] or the
   Workbench argument. "Dockbar" when not set. */
void dock_autostart_set_program(const char *name);

int dock_autostart_check(struct DockAutostart *a);

/* Makes SYS:WBStartup start this Dockbar: writes the launcher, replacing an
   existing launcher (keeping its tool types) or a copy. With `migrate`, a
   copy's docks.config, dock.pins and WidgetState are copied here first when
   this drawer has none of them; otherwise both sets stay and `message` says
   how to choose. `no_compose` adds the NOCOMPOSE tool type. 1 done (message
   may still hold a notice), 0 nothing changed (message says why). */
int dock_autostart_enable(int migrate, int no_compose, char *message, size_t size,
                          void (*note)(const char *format, ...));

/* Removes a launcher that starts a Dockbar. A copy is left alone and
   reported. 1 removed or nothing there, 0 not removed (message says why). */
int dock_autostart_disable(char *message, size_t size, void (*note)(const char *format, ...));

#endif
