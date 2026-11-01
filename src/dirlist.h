#ifndef DOCK_DIRLIST_H
#define DOCK_DIRLIST_H
/* Pure rules of the drawer panel's listing (no AROS calls). */

/* 0 for names the panel never lists as entries: icon files (.info, any
 * case), "." and ".." and empty names. An object's .info is its picture,
 * not a separate item. */
int dock_dir_listed(const char *name);
/* qsort order: drawers before files, then names without regard to case
 * (ASCII and ISO-8859-1/2 letters compare by byte after folding A-Z), then
 * by exact bytes so that the order is total. */
int dock_dir_compare(int a_is_dir, const char *a, int b_is_dir, const char *b);
/* The parent of a drawer path, or 0 when path is a volume or assign root.
 * "Work:a/b" -> "Work:a", "Work:a" -> "Work:", "Work:" -> 0. */
int dock_dir_parent(const char *path, char *out, unsigned long capacity);
/* path + "/" + name (no "/" after a colon); 0 when it does not fit. */
int dock_dir_join(const char *path, const char *name, char *out, unsigned long capacity);
/* For "Name.info" (any case): the object's name, "Name". 0 for other names. */
int dock_dir_info_base(const char *name, char *out, unsigned long capacity);
/* Names compared as the file system does, without regard to case (ASCII
 * folding as above); for matching an object with its .info. */
int dock_dir_samename(const char *a, const char *b);
#endif
