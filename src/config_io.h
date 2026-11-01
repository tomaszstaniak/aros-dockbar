#ifndef DOCK_CONFIG_IO_H
#define DOCK_CONFIG_IO_H
/* Record-level helpers shared by the version 1 reader (config.c) and the
 * multi-dock format (docks.c). Not part of the public configuration API. */
#include "model.h"
#include <stdio.h>
#define CONFIG_LINE (2*DOCK_PATH+DOCK_LABEL+DOCK_DIR+64)
int dock_cfg_text_valid(const char *text,size_t capacity);
int dock_cfg_number(const char *text,int minimum,int maximum,int *result);
/* Tab-separated record ending in a newline; returns the field count, 0 on error/EOF. */
int dock_cfg_record(FILE *file,char *line,char **fields,int capacity);
int dock_cfg_read_item(FILE *file,struct DockItem *item,int version);
int dock_cfg_write_item(FILE *file,const struct DockItem *item);
/* Version 4 records add the item's presentation (DockPresentation). */
int dock_cfg_write_item_v4(FILE *file,const struct DockItem *item);
int dock_cfg_item_valid(const struct DockItem *item,int child,int version);
int dock_cfg_model_valid(const struct DockModel *model,int version);
#endif
