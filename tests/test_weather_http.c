#include "http.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
  struct WeatherHttpBuffer b = {0};
  b.limit = 8;
  assert(weather_http_write("1234", 1, 4, &b) == 4);
  assert(weather_http_write("5678", 2, 2, &b) == 4);
  assert(!weather_http_write("9", 1, 1, &b));
  assert(!weather_http_write("x", SIZE_MAX, 2, &b));
  assert(b.size == 8 && !strcmp(b.data, "12345678"));
  free(b.data);
  puts("weather HTTP bounds: passed");
  /* The bundled file first; CURL_CA_BUNDLE no longer replaces the others. */
  const char *o[3];
  assert(weather_http_ca_order("P", "E", "S", o) == 3);
  assert(!strcmp(o[0], "P") && !strcmp(o[1], "E") && !strcmp(o[2], "S"));
  assert(weather_http_ca_order("", "E", "S", o) == 2);
  assert(!strcmp(o[0], "E") && !strcmp(o[1], "S"));
  assert(weather_http_ca_order("P", NULL, "S", o) == 2);
  assert(!strcmp(o[0], "P") && !strcmp(o[1], "S"));
  assert(weather_http_ca_order("S", "S", "S", o) == 1);
  assert(weather_http_ca_order(NULL, "", NULL, o) == 0);
  puts("weather CA order: passed");
  /* A failure that is not about certificates is still named first. */
  char e[96] = "Network: Couldn't resolve host name";
  weather_http_bundle_note(e, sizeof e, "");
  assert(!strncmp(e, "Network: Couldn't resolve host name (Also: bundled", 50));
  char k[96] = "Network: x";
  weather_http_bundle_note(k, sizeof k, "SYS:W/certs/ca-bundle.crt");
  assert(!strcmp(k, "Network: x"));
  char s[12] = "Network: x";
  weather_http_bundle_note(s, sizeof s, "");
  assert(strlen(s) < sizeof s);
  puts("weather CA note: passed");
  return 0;
}
