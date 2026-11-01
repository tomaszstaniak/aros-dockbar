#include "docks.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  assert(argc == 2);
  struct DockSet *a = calloc(1, sizeof *a), *b = calloc(1, sizeof *b);
  struct DockModel model = {0};
  struct DockPosition pos = {0};
  struct DockPrefs prefs = {40, 5, 0};
  dock_set_from_v1(a, &model, &pos, &prefs);
  struct DockItem item = {0};
  item.kind = DOCK_APPLET;
  strcpy(item.path, "org.aros.weather");
  strcpy(item.dir, "one");
  strcpy(item.label, "Weather");
  assert(dock_insert(&a->docks[0].model, 0, &item, 0) == 1);
  assert(dock_set_save(a, argv[1]));
  assert(dock_set_load(b, argv[1]) == 1);
  assert(b->docks[0].model.items[0].kind == DOCK_APPLET &&
         !strcmp(b->docks[0].model.items[0].dir, "one"));
  int copy = dock_set_duplicate(b, 0);
  assert(copy == 1);
  assert(strcmp(b->docks[0].model.items[0].dir,
                b->docks[copy].model.items[0].dir));
  assert(dock_insert(&a->docks[0].model,0,&item,0)==0);
  puts("applet config: passed");
  free(a);
  free(b);
  return 0;
}
