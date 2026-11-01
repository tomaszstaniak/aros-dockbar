#ifndef DOCK_CONFIG_H
#define DOCK_CONFIG_H
#include "model.h"
struct DockPosition { int valid,x,y; };
/* Save returns 1 only after complete replacement; 0 leaves the primary intact
 * or recoverable as .old. Existing unreadable, invalid or future-version files are never renamed. */
int dock_config_save(const struct DockModel *model,const struct DockPosition *position,const char *file);
/* 1 valid, 0 missing, -1 invalid/unreadable. Invalid input leaves model and
 * position untouched. A valid .old recovery is installed but returns -1 so
 * callers keep the recovered configuration read-only. */
int dock_config_load(struct DockModel *model,struct DockPosition *position,const char *file);
/* Clamp x to 0..max(0,screen-width), y to 20..max(20,screen-height).
 * Preserves valid; oversized docks anchor at (0,20). */
void dock_clamp_position(struct DockPosition *position,int screen_width,int screen_height,int width,int height);
#endif
