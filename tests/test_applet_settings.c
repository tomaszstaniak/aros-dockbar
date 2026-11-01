#include "applets/settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
  assert(argc == 3);
  struct DockAppletForm f;
  char error[160];
  assert(dock_applet_settings_schema(argv[1], &f, error, sizeof error));
  assert(f.count == 6);
  strcpy(f.fields[0].value, "Berlin");
  strcpy(f.fields[1].value, "52.52");
  strcpy(f.fields[2].value, "13.4");
  char json[4096];
  assert(
      dock_applet_settings_encode(&f, json, sizeof json, error, sizeof error));
  strcpy(f.fields[1].value, "91");
  assert(
      !dock_applet_settings_encode(&f, json, sizeof json, error, sizeof error));
  /* A refused form leaves nothing behind: no half object for requests. */
  assert(json[0] == 0);
  strcpy(f.fields[0].value, "");
  assert(!dock_applet_settings_encode(&f, json, sizeof json, error, sizeof error) &&
         json[0] == 0 && strstr(error, "Enter"));
  strcpy(f.fields[0].value, "Berlin");
  strcpy(f.fields[1].value, "52.52");
  assert(dock_applet_settings_encode(&f, json, sizeof json, error, sizeof error) &&
         json[strlen(json) - 1] == '}');
  char path[1024], backup[1100];
  snprintf(path, sizeof path, "%s/settings.json", argv[2]);
  snprintf(backup, sizeof backup, "%s.old", path);
  FILE *fp = fopen(backup, "w");
  assert(fp);
  fputs("{\"schema\":1,\"location\":\"Berlin\",\"latitude\":52.52,"
        "\"longitude\":13.4}",
        fp);
  fclose(fp);
  int recovered = 0;
  assert(dock_applet_settings_recover(path, &f, &recovered) && recovered);
  assert(!strcmp(f.fields[0].value, "Berlin"));
  fp = fopen(path, "w");
  assert(fp);
  fputs("broken", fp);
  fclose(fp);
  assert(!dock_applet_settings_recover(path, &f, &recovered));
  fp = fopen(path, "w");
  assert(fp);
  fputs("{\"schema\":2,\"location\":\"Future "
        "City\",\"latitude\":52,\"longitude\":13}",
        fp);
  fclose(fp);
  assert(!dock_applet_settings_recover(path, &f, &recovered));
  puts("applet settings: passed");
  return 0;
}
