#ifndef DOCK_RUNNING_H
#define DOCK_RUNNING_H
/* Running indicators: which program items have a process running them.
 *
 * A process carries no path of its program. What ties it to a file is its
 * home drawer (pr_HomeDir) together with the file name: the task name of a
 * Workbench start, the command name of a Shell command. A scan copies those
 * fields of every process; nothing here keeps a pointer into another
 * process, and no DOS call ever touches another process's lock (the lock
 * can be freed at any time, and a packet would end the scan's Forbid).
 * Findings: docs/evidence/running-indicators-20261029/report.md. */

#define DOCK_RUN_NAME 108

/* What a scan copied of one process. name: the program's file name (last
 * path part); empty when the process runs nothing we can name (an idle
 * Shell). volume, key, port: fl_Volume, fl_Key and fl_Task of its home
 * drawer lock, as values only. */
struct DockRunProcess {
    char name[DOCK_RUN_NAME];
    int has_home;
    unsigned long volume, key, port;
};

/* An item's program: its file name and the same fields of a lock of our own
 * on its drawer, taken when the dock opened and released at once (a lock
 * kept would make the drawer impossible to delete). usable 0: no dot can be
 * certain, because the drawer could not be locked or its file system gives
 * locks keys that do not tell drawers apart (FAT gives every lock 0). */
struct DockRunTarget {
    char file[DOCK_RUN_NAME];
    int usable;
    unsigned long volume, key, port;
};

/* 1 when some process has exactly this program: the same file name (case
 * does not count, as in AmigaDOS) and a home drawer lock on the same
 * volume, handler and key. Never for an unusable target. */
int dock_running_match(const struct DockRunTarget *target, const struct DockRunProcess *seen, int count);

/* Whether a drawer's lock key can tell it from other drawers of its volume:
 * not 0, and not the key of its parent (has_parent 0 for a volume's root). */
int dock_running_keys_usable(unsigned long key, int has_parent, unsigned long parent_key);

/* The dot of one item: on at the first scan that finds the program, off
 * after two scans in a row without it, so a program that restarts does not
 * blink. Returns 1 when shown changed. */
struct DockRunState {
    unsigned char shown, missed;
};
int dock_running_step(struct DockRunState *state, int found);

/* AROS only (running_aros.c). */
/* Copies every process into seen (at most max); returns the count. */
int dock_running_scan(struct DockRunProcess *seen, int max);
/* The target for a program path, with its drawer locked and unlocked again. */
void dock_running_target(struct DockRunTarget *target, const char *path);

#endif
