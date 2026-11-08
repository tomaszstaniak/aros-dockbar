#ifndef DOCK_AUTOSTART_PLAN_H
#define DOCK_AUTOSTART_PLAN_H

/* What turning Start with Wanderer on or off may change in SYS:WBStartup
   (autostart.h), decided from what was found there. No file access: the
   caller scans and carries the plan out; kept apart so that the rules can
   be tested on any system.

   The rules:
   - On manages this Dockbar's own start icon only. When something else in
     SYS:WBStartup starts a Dockbar (a copy of the program, or a start icon
     of another installation), On changes nothing and says so: replacing
     that is a separate request, Replace.
   - Off removes this Dockbar's own start icons only.
   - Nothing that could not be read is taken to be free or harmless: an icon
     that could not be read, an entry of the program's name that is not a
     Dockbar start icon, or a drawer that could not be read to the end (or
     had more Dockbar entries than fit here) stops On and Replace. Off still
     removes this Dockbar's own start icons, which were recognised. */

#define AS_MAX_ITEMS 32
#define AS_NAME 108

enum AsKind {
    AS_UNREADABLE = 1, /* an icon that could not be read */
    AS_FOREIGN,        /* an icon of the program's name that is not Dockbar's */
    AS_OURS,           /* a start icon for this Dockbar */
    AS_OTHER,          /* a start icon for another Dockbar */
    AS_MISSING,        /* a Dockbar start icon whose program was not found */
    AS_COPY,           /* the program itself, with its tool icon */
    AS_COPY_FILE       /* the program itself, without an icon */
};

struct AsItem {
    char base[AS_NAME]; /* the name in SYS:WBStartup, without .info */
    int kind;
};

struct AsScan {
    struct AsItem item[AS_MAX_ITEMS];
    int count;
    int complete;  /* read to the end, every relevant entry kept */
    int self_copy; /* this Dockbar is itself in SYS:WBStartup */
};

enum AsAction { AS_ON, AS_REPLACE, AS_OFF };

enum AsVerdict {
    AS_GO,
    AS_REFUSE_SELF_COPY,
    AS_REFUSE_INCOMPLETE,
    AS_REFUSE_UNREADABLE,  /* culprit: the icon */
    AS_REFUSE_BLOCKED,     /* culprit: the entry of the program's name */
    AS_REFUSE_NEEDS_REPLACE /* culprit: the first copy or other start icon */
};

/* What to remove of one item. */
enum { AS_RM_ICON = 1, AS_RM_FILE = 2 };

struct AsPlan {
    int verdict;
    int culprit;            /* item index for a refusal, or -1 */
    int write;              /* write the program's start icon */
    int types_from;         /* item whose tool types are kept, or -1 */
    int remove[AS_MAX_ITEMS];
};

int as_plan_make(const struct AsScan *scan, int action, const char *program, struct AsPlan *plan);

/* After the plan was carried out: `failed[i]` holds the AS_RM_* parts of
   item i that could not be removed. 1 when something that failed still
   starts with Wanderer: its icon is still there and is not the one that
   was written over. */
int as_still_starts(const struct AsScan *scan, const struct AsPlan *plan, const int *failed, int i,
                    const char *program);

#endif
