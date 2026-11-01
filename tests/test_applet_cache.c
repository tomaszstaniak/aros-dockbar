#include "applets/cache.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
  struct DockAppletSnapshot *s = calloc(1, sizeof *s),
                            *out = calloc(1, sizeof *out);
  strcpy(s->package, "org.test");
  strcpy(s->instance, "one");
  strcpy(s->session, "old");
  strcpy(s->title, "City A");
  strcpy(s->fingerprint, "a");
  s->generation = 1;
  s->status = DA_READY;
  s->updated = s->source_time = 100;
  const char *cfg = "{\"schema\":1,\"city\":\"A\"}";
  FILE *f = tmpfile();
  assert(f && dock_applet_cache_write(f, cfg, s));
  assert(!fseek(f, 0, SEEK_END));
  long n = ftell(f);
  rewind(f);
  char *data = malloc((size_t)n + 1);
  assert(data && fread(data, 1, (size_t)n, f) == (size_t)n);
  data[n] = 0;
  assert(dock_applet_cache_parse(data, (size_t)n, cfg, out));
  assert(!strcmp(out->title, "City A"));
  assert(!dock_applet_cache_parse(data, (size_t)n,
                                  "{\"schema\":1,\"city\":\"B\"}", out));
  assert(!strcmp(out->title, "City A"));
  assert(!dock_applet_cache_parse(data, (size_t)n - 3, cfg, out));
  fclose(f);
  free(data);
  free(s);
  free(out);
  puts("applet cache: passed");
  return 0;
}
