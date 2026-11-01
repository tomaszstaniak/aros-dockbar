/* Finite native protocol peer: completes five seconds after the host exits.
 * This fixture deliberately has no reference to host-owned memory. */
#include "applets/json.h"
#include "applets/snapshot.h"
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
int main(int argc, char **argv) {
  if (argc != 2)
    return 5;
  size_t n;
  char *input = dock_applet_read(argv[1], 65536, &n);
  struct DockJson *j = malloc(sizeof *j);
  struct DockAppletSnapshot *s = calloc(1, sizeof *s);
  int generation, result = 5;
  if (!input || !j || !s || !dock_json_parse(j, input, n))
    goto done;
  if (!dock_json_string(j, dock_json_get(j, 0, "package"), s->package,
                        sizeof s->package) ||
      !dock_json_string(j, dock_json_get(j, 0, "instance"), s->instance,
                        sizeof s->instance) ||
      !dock_json_string(j, dock_json_get(j, 0, "session"), s->session,
                        sizeof s->session) ||
      !dock_json_int(j, dock_json_get(j, 0, "generation"), &generation))
    goto done;
  s->generation = (unsigned long)generation;
  free(input);
  input = NULL;
  free(j);
  j = NULL;
  Delay(250);
  s->status = DA_READY;
  s->updated = s->source_time = (double)time(NULL);
  strcpy(s->title, "Delayed fixture");
  strcpy(s->fingerprint, "late");
  s->field_count = 2;
  strcpy(s->fields[0].key, "value");
  strcpy(s->fields[0].text, "Late reply");
  strcpy(s->fields[1].key, "caption");
  strcpy(s->fields[1].text, "Parent-independent");
  char tmp[1024], completed[1024];
  if (snprintf(tmp, sizeof tmp, "%s.tmp", argv[1]) >= (int)sizeof tmp ||
      snprintf(completed, sizeof completed, "%s.done", argv[1]) >=
          (int)sizeof completed)
    goto done;
  FILE *f = fopen(tmp, "wb");
  if (!f)
    goto done;
  int ok = dock_applet_snapshot_write(f, s);
  if (fclose(f))
    ok = 0;
  if (ok && !rename(tmp, completed))
    result = 0;
done:
  free(input);
  free(j);
  free(s);
  return result;
}
