#ifndef DOCK_PAK_H
#define DOCK_PAK_H
#include <stddef.h>
/* One entry of a Dockbar resource pack (scripts/dockpak.py has the format):
 * licensed artwork that ships inside the program but not as loose files.
 * Returns malloc'd bytes and *size, or NULL when the pack or entry is missing
 * or malformed. Entries larger than max are refused. */
unsigned char *dock_pak_read(const char *pak, const char *name, size_t max, size_t *size);
/* "DIR/x.pak#entry" -> pak "DIR/x.pak", entry "entry"; 0 when path has no
 * entry part or a part does not fit. */
int dock_pak_split(const char *path, char *pak, size_t pak_cap, char *entry, size_t entry_cap);
#endif
