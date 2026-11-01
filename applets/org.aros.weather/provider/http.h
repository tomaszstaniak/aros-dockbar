#ifndef WEATHER_HTTP_H
#define WEATHER_HTTP_H
#include <stddef.h>
struct WeatherHttpBuffer {
  char *data;
  size_t size, limit;
};
size_t weather_http_write(char *, size_t, size_t, void *);
int weather_http_get(const char *, size_t, char **, size_t *, char *, size_t);
/* Set by main before requests: the CA bundle shipped with the package
 * (empty: none was readable) and the log file (empty: no log). */
extern char weather_http_package_ca[512];
extern char weather_http_log_path[512];
/* Certificate files to try, in order: the bundled one, CURL_CA_BUNDLE, the
 * system's; empty and repeated entries are skipped. Returns the count. */
int weather_http_ca_order(const char *package, const char *env,
                          const char *system, const char *out[3]);
/* Adds to a failure message that the bundled certificates were not found,
 * after the failure itself, which stays the main message (a DNS error is
 * not a certificate problem). Nothing when the bundled file was found. */
void weather_http_bundle_note(char *error, size_t cap, const char *package_ca);
void weather_http_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
#endif
