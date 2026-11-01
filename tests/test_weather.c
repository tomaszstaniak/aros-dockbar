#include "applets/json.h"
#include "weather.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  assert(argc == 2);
  const int codes[] = {0,  1,  2,  3,  45, 48, 51, 53, 55, 56, 57, 61, 63, 65,
                       66, 67, 71, 73, 75, 77, 80, 81, 82, 85, 86, 95, 96, 97, 99};
  for (size_t i = 0; i < sizeof codes / sizeof *codes; i++)
    { const char *day = weather_condition_key(codes[i], 1);
      const char *night = weather_condition_key(codes[i], 0);
      assert(strcmp(day, "unknown"));
      assert(strstr(day, "-day") && strstr(night, "-night"));
      assert(strcmp(day, night));
    }
  assert(!strcmp(weather_condition_key(0, 0), "clear-night"));
  assert(!strcmp(weather_condition_key(1, 1), "partly-cloudy-day"));
  assert(!strcmp(weather_condition_key(2, 1), "scattered-clouds-day"));
  assert(!strcmp(weather_condition_key(2, 0), "scattered-clouds-night"));
  assert(!strcmp(weather_condition_key(97, 0), "thunderstorm-night"));
  assert(!strcmp(weather_condition_key(999, 1), "unknown"));
  struct WeatherSettings settings;
  const char *cfg = "{\"schema\":1,\"location\":\"Berlin\",\"latitude\":52.52,"
                    "\"longitude\":13.405,\"temperatureUnit\":\"celsius\","
                    "\"windUnit\":\"kmh\",\"refreshMinutes\":15}";
  assert(weather_settings(cfg, strlen(cfg), &settings));
  assert(settings.refresh_minutes == 15);
  const char *invalid =
      "{\"schema\":1,\"location\":\"X\",\"latitude\":100,\"longitude\":0}";
  assert(!weather_settings(invalid, strlen(invalid), &settings));
  assert(weather_settings(cfg, strlen(cfg), &settings));
  size_t n;
  char *text = dock_applet_read(argv[1], 512 * 1024, &n);
  assert(text);
  struct WeatherForecast *f = calloc(1, sizeof *f),
                         *old = calloc(1, sizeof *old);
  assert(f && old);
  assert(weather_decode(text, n, &settings, f));
  assert(!strcmp(f->snapshot.icon, "clear-night"));
  assert(!strcmp(dock_applet_field(&f->snapshot, "temperature"), "-3 C"));
  assert(f->snapshot.row_count >= 31);
  assert(strstr(f->snapshot.rows[7].label, "2026"));
  assert(!strcmp(f->snapshot.rows[7].section,"hourly"));
  assert(strlen(f->snapshot.rows[7].caption)==5);
  assert(!strcmp(f->snapshot.rows[32].section,"daily"));
  assert(!strcmp(f->snapshot.rows[32].caption,"Today"));
  assert(!strcmp(dock_applet_field(&f->snapshot,"description"),"Clear sky"));
  assert(dock_applet_field(&f->snapshot,"range")[0]);
  /* Exercise the complete response path and descriptions for every code/phase. */
  const char *code_at = strstr(text, "\"weather_code\": 0");
  assert(code_at);
  code_at += strlen("\"weather_code\": ");
  char *variant = malloc(n + 16);
  assert(variant);
  for (size_t i = 0; i < sizeof codes / sizeof *codes; i++) {
    for (int day = 0; day <= 1; day++) {
      int length = snprintf(variant, n + 16, "%.*s%d%s",
                            (int)(code_at-text), text, codes[i], code_at+1);
      char *phase = strstr(variant, "\"is_day\": 0");
      assert(phase); phase[strlen("\"is_day\": ")] = (char)('0'+day);
      assert(length > 0 && weather_decode(variant, (size_t)length, &settings, f));
      assert(!strcmp(f->snapshot.icon, weather_condition_key(codes[i],day)));
      assert(strcmp(dock_applet_field(&f->snapshot,"description"),"Weather unavailable"));
    }
  }
  free(variant);
  assert(weather_decode(text,n,&settings,f));
  *old = *f;
  assert(!weather_decode("{\"error\":true}", 14, &settings, f));
  assert(!memcmp(f, old, sizeof *f));
  char url[2048];
  assert(weather_url(&settings, url, sizeof url));
  assert(strstr(url, "https://api.open-meteo.com/") &&
         strstr(url, "temperature_unit=celsius"));
  char fixture[1024];
  const char *invalids[] = {"null-current.json", "mismatched-hourly.json",
                            "missing-timezone.json"};
  for (size_t i = 0; i < sizeof invalids / sizeof *invalids; i++) {
    snprintf(fixture, sizeof fixture, "%.*s%s",
             (int)(strrchr(argv[1], '/') - argv[1] + 1), argv[1], invalids[i]);
    size_t size;
    char *bad = dock_applet_read(fixture, 512 * 1024, &size);
    assert(bad);
    assert(!weather_decode(bad, size, &settings, f));
    assert(!memcmp(f, old, sizeof *f));
    free(bad);
  }
  snprintf(fixture, sizeof fixture, "%.*spolar.json",
           (int)(strrchr(argv[1], '/') - argv[1] + 1), argv[1]);
  size_t polar_size;
  char *polar = dock_applet_read(fixture, 512 * 1024, &polar_size);
  assert(polar);
  assert(weather_decode(polar, polar_size, &settings, f));
  free(polar);
  puts("weather: passed");
  free(text);
  free(f);
  free(old);
  return 0;
}
