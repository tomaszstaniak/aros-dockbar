#include "applets/json.h"
#include "http.h"
#include "weather.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static int str(const struct DockJson *j, int o, const char *k, char *v,
               size_t cap) {
  return dock_json_string(j, dock_json_get(j, o, k), v, cap);
}
#ifdef __AROS__
#include <dos/dos.h>
#include <proto/dos.h>
#endif
/* The bundled certificates are <package>/certs/ca-bundle.crt. Dockbar
 * names the package directory in the request ("packageRoot", an absolute
 * path); the provider's own drawer (bin/<variant>, two levels below) is only
 * the fallback for a host that does not. A file that cannot be read is
 * logged: otherwise only the error of the last certificate file tried (the
 * system's) is seen. */
static int try_package_ca(const char *root, const char *from) {
  char path[512];
  if (!root || !*root)
    return 0;
#ifdef __AROS__
  snprintf(path, sizeof path, "%s", root);
  if (!AddPart((STRPTR)path, (CONST_STRPTR) "certs/ca-bundle.crt", sizeof path)) {
    weather_http_log("CA bundled: path too long under %s (%s)", root, from);
    return 0;
  }
#else
  if (snprintf(path, sizeof path, "%s/certs/ca-bundle.crt", root) >=
      (int)sizeof path) {
    weather_http_log("CA bundled: path too long under %s (%s)", root, from);
    return 0;
  }
#endif
  FILE *f = fopen(path, "rb");
  int byte = f ? fgetc(f) : EOF;
  if (f)
    fclose(f);
  if (byte == EOF) {
    weather_http_log("CA bundled: cannot read %s (%s)", path, from);
    return 0;
  }
  snprintf(weather_http_package_ca, sizeof weather_http_package_ca, "%s", path);
  weather_http_log("CA bundled: %s (%s)", path, from);
  return 1;
}
#ifdef __AROS__
static int program_package_root(char *out, size_t cap) {
  BPTR bin = GetProgramDir(), variant = bin ? ParentDir(bin) : BNULL,
       root = variant ? ParentDir(variant) : BNULL;
  int ok = root && NameFromLock(root, (STRPTR)out, (LONG)cap);
  if (root)
    UnLock(root);
  if (variant)
    UnLock(variant);
  return ok;
}
#else
static int program_package_root(char *out, size_t cap) {
  const char *dir = getenv("WEATHER_PACKAGE_DIR");
  return dir && snprintf(out, cap, "%s", dir) < (int)cap;
}
#endif
static void find_package_ca(const struct DockJson *j) {
  char root[512];
  weather_http_package_ca[0] = 0;
  if (str(j, 0, "packageRoot", root, sizeof root) && try_package_ca(root, "from Dockbar"))
    return;
  if (program_package_root(root, sizeof root) &&
      try_package_ca(root, "from the provider's drawer"))
    return;
  weather_http_log("CA bundled: none readable; trying CURL_CA_BUNDLE and the system's");
}
/* <state>/.s-session/r-N.json -> <state>/weather.log */
static void find_log(const char *request) {
  char path[512];
  snprintf(path, sizeof path, "%s", request);
  for (int up = 0; up < 2; up++) {
    char *cut = strrchr(path, '/');
    if (!cut)
      cut = strrchr(path, ':') ? strrchr(path, ':') + 1 : NULL;
    if (!cut)
      return;
    *cut = 0;
  }
  size_t n = strlen(path);
  if (n + 13 > sizeof weather_http_log_path)
    return;
  memcpy(weather_http_log_path, path, n);
  if (n && path[n - 1] != ':')
    weather_http_log_path[n++] = '/';
  memcpy(weather_http_log_path + n, "weather.log", 12);
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 5;
  find_log(argv[1]);
  size_t length;
  char *input = dock_applet_read(argv[1], 65536, &length);
  struct DockJson *j = malloc(sizeof *j);
  struct WeatherForecast *forecast = calloc(1, sizeof *forecast);
  struct DockAppletSnapshot *reply = calloc(1, sizeof *reply);
  int result = 5, g, api;
  /* A request that cannot be answered gets no reply file; say why on the
   * error stream, so that a provider started by hand explains itself. */
  const char *why = NULL;
  if (!input) why = "cannot read the request file";
  else if (!j || !forecast || !reply) why = "out of memory";
  else if (!dock_json_parse(j, input, length)) why = "request is not valid JSON";
  else if (!dock_json_int(j, dock_json_get(j, 0, "api"), &api) || api != 1) why = "unsupported api";
  else if (!str(j, 0, "package", reply->package, sizeof reply->package) ||
           strcmp(reply->package, "org.aros.weather")) why = "wrong package";
  else if (!str(j, 0, "instance", reply->instance, sizeof reply->instance) ||
           !dock_applet_id_valid(reply->instance)) why = "bad instance";
  else if (!str(j, 0, "session", reply->session, sizeof reply->session) ||
           !dock_applet_id_valid(reply->session)) why = "bad session";
  else if (!dock_json_int(j, dock_json_get(j, 0, "generation"), &g) || g < 1) why = "bad generation";
  if (why) {
    fprintf(stderr, "WeatherProvider: %s\n", why);
    goto done;
  }
  find_package_ca(j);
  reply->generation = (unsigned long)g;
  reply->status = DA_ERROR;
  reply->updated = reply->source_time = (double)time(NULL);
  strcpy(reply->title, "Weather");
  char operation[32], url[2048];
  char *body = NULL;
  size_t size = 0;
  int success = 0;
  if (!str(j, 0, "operation", operation, sizeof operation)) {
    strcpy(reply->message, "Missing operation");
    goto publish;
  }
  if (!strcmp(operation, "forecast")) {
    int token = dock_json_get(j, 0, "settings");
    struct WeatherSettings settings;
    if (token < 0 ||
        !weather_settings(input + j->token[token].start,
                          (size_t)(j->token[token].end - j->token[token].start),
                          &settings)) {
      strcpy(reply->message, "Choose a location in Settings");
      goto publish;
    }
    if (weather_url(&settings, url, sizeof url) &&
        weather_http_get(url, 512u * 1024u, &body, &size, reply->message,
                         sizeof reply->message))
      success = weather_decode(body, size, &settings, forecast);
  } else if (!strcmp(operation, "geocode")) {
    char query[128];
    if (!str(j, 0, "query", query, sizeof query) ||
        !weather_geocode_url(query, url, sizeof url)) {
      strcpy(reply->message, "Enter a city name");
      goto publish;
    }
    if (weather_http_get(url, 65536, &body, &size, reply->message,
                         sizeof reply->message))
      success = weather_geocode(body, size, &forecast->snapshot);
  } else {
    strcpy(reply->message, "Unsupported operation");
    goto publish;
  }
  free(body);
  if (success) {
    struct DockAppletSnapshot *s = &forecast->snapshot;
    strcpy(s->package, reply->package);
    strcpy(s->instance, reply->instance);
    strcpy(s->session, reply->session);
    s->generation = reply->generation;
    *reply = *s;
  } else if (!reply->message[0])
    strcpy(reply->message,
           "Weather service returned invalid or incomplete data");
publish:;
  char temporary[DA_PATH], completed[DA_PATH];
  int a = snprintf(temporary, sizeof temporary, "%s.tmp", argv[1]),
      b = snprintf(completed, sizeof completed, "%s.done", argv[1]);
  if (a < 0 || b < 0 || (size_t)a >= sizeof temporary ||
      (size_t)b >= sizeof completed) {
    fprintf(stderr, "WeatherProvider: request path too long\n");
    goto done;
  }
  /* The host gives each request a unique absent destination. Publication never
   * replaces an existing file, so DOS need not implement POSIX replacement. */
  FILE *check = fopen(completed, "rb");
  if (check) {
    fclose(check);
    goto done;
  }
  FILE *f = fopen(temporary, "wb");
  if (!f) {
    fprintf(stderr, "WeatherProvider: cannot write the reply\n");
    goto done;
  }
  int ok = dock_applet_snapshot_write(f, reply);
  if (fclose(f))
    ok = 0;
  if (ok && !rename(temporary, completed))
    result = 0;
  else {
    fprintf(stderr, "WeatherProvider: cannot publish the reply\n");
    remove(temporary);
  }
done:
  free(input);
  free(j);
  free(forecast);
  free(reply);
  return result;
}
