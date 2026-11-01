/* Minimal original one-shot applet provider: no GUI or networking dependency.
 */
#include "applets/json.h"
#include "applets/snapshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
int main(int argc, char **argv) {
  if (argc != 2)
    return 5;
  size_t len;
  char *input = dock_applet_read(argv[1], 65536, &len);
  struct DockJson *j = malloc(sizeof *j);
  struct DockAppletSnapshot *s = calloc(1, sizeof *s);
  int result = 5, g;
  if (!input || !j || !s || !dock_json_parse(j, input, len))
    goto done;
  if (!dock_json_string(j, dock_json_get(j, 0, "package"), s->package,
                        sizeof s->package) ||
      strcmp(s->package, "org.aros.sample") ||
      !dock_json_string(j, dock_json_get(j, 0, "instance"), s->instance,
                        sizeof s->instance) ||
      !dock_json_string(j, dock_json_get(j, 0, "session"), s->session,
                        sizeof s->session) ||
      !dock_json_int(j, dock_json_get(j, 0, "generation"), &g) || g < 1)
    goto done;
  s->generation = (unsigned long)g;
  s->status = DA_READY;
  s->updated = s->source_time = (double)time(NULL);
  strcpy(s->title, "Sample applet");
  strcpy(s->fingerprint, "sample-v1");
  s->field_count = 2;
  strcpy(s->fields[0].key, "value");
  strcpy(s->fields[0].text, "Hello AROS");
  strcpy(s->fields[1].key, "caption");
  strcpy(s->fields[1].text, "External package");
  s->row_count = 1;
  strcpy(s->rows[0].label, "Provider");
  strcpy(s->rows[0].value, "Separate process, shared package API");
  char path[1024], tmp[1024];
  if (snprintf(path, sizeof path, "%s.done", argv[1]) >= (int)sizeof path ||
      snprintf(tmp, sizeof tmp, "%s.tmp", argv[1]) >= (int)sizeof tmp)
    goto done;
  FILE *f = fopen(tmp, "wb");
  if (!f)
    goto done;
  int ok = dock_applet_snapshot_write(f, s);
  if (fclose(f))
    ok = 0;
  if (ok && !rename(tmp, path))
    result = 0;
done:
  free(input);
  free(j);
  free(s);
  return result;
}
