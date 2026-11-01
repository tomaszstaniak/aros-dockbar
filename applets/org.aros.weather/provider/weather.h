#ifndef AROS_WEATHER_H
#define AROS_WEATHER_H
#include "applets/snapshot.h"
struct WeatherSettings {
  char location[120], temperature_unit[16], wind_unit[8];
  double latitude, longitude;
  int refresh_minutes;
};
struct WeatherForecast {
  struct DockAppletSnapshot snapshot;
};
int weather_settings(const char *, size_t, struct WeatherSettings *);
int weather_url(const struct WeatherSettings *, char *, size_t);
const char *weather_condition_key(int code, int is_day);
int weather_decode(const char *, size_t, const struct WeatherSettings *,
                   struct WeatherForecast *);
int weather_geocode_url(const char *, char *, size_t);
int weather_geocode(const char *, size_t, struct DockAppletSnapshot *);
#endif
