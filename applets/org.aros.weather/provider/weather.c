#include "weather.h"
#include "applets/json.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static int str(const struct DockJson *j, int o, const char *key, char *out,
               size_t cap) {
  return dock_json_string(j, dock_json_get(j, o, key), out, cap);
}
int weather_settings(const char *text, size_t len,
                     struct WeatherSettings *out) {
  struct DockJson *j = malloc(sizeof *j);
  struct WeatherSettings s;
  memset(&s, 0, sizeof s);
  int ok = 0, v;
  if (!j)
    return 0;
  if (!dock_json_parse(j, text, len) ||
      !dock_json_int(j, dock_json_get(j, 0, "schema"), &v) || v != 1 ||
      !str(j, 0, "location", s.location, sizeof s.location) || !s.location[0] ||
      !dock_json_number(j, dock_json_get(j, 0, "latitude"), &s.latitude) ||
      !dock_json_number(j, dock_json_get(j, 0, "longitude"), &s.longitude) ||
      fabs(s.latitude) > 90 || fabs(s.longitude) > 180)
    goto done;
  strcpy(s.temperature_unit, "celsius");
  strcpy(s.wind_unit, "kmh");
  s.refresh_minutes = 15;
  if (dock_json_get(j, 0, "temperatureUnit") >= 0 &&
      !str(j, 0, "temperatureUnit", s.temperature_unit,
           sizeof s.temperature_unit))
    goto done;
  if (dock_json_get(j, 0, "windUnit") >= 0 &&
      !str(j, 0, "windUnit", s.wind_unit, sizeof s.wind_unit))
    goto done;
  if (strcmp(s.temperature_unit, "celsius") &&
      strcmp(s.temperature_unit, "fahrenheit"))
    goto done;
  if (strcmp(s.wind_unit, "kmh") && strcmp(s.wind_unit, "ms") &&
      strcmp(s.wind_unit, "mph"))
    goto done;
  if (dock_json_get(j, 0, "refreshMinutes") >= 0 &&
      (!dock_json_int(j, dock_json_get(j, 0, "refreshMinutes"),
                      &s.refresh_minutes) ||
       s.refresh_minutes < 15 || s.refresh_minutes > 60))
    goto done;
  for (const char *p = s.location; *p; p++)
    if ((unsigned char)*p < 32)
      goto done;
  *out = s;
  ok = 1;
done:
  free(j);
  return ok;
}
const char *weather_condition_key(int code, int day) {
  switch (code) {
  case 0:
    return day ? "clear-day" : "clear-night";
  case 1:
    return day ? "partly-cloudy-day" : "partly-cloudy-night";
  case 2:
    return day ? "scattered-clouds-day" : "scattered-clouds-night";
  case 3:
    return day ? "cloudy-day" : "cloudy-night";
  case 45:
  case 48:
    return day ? "fog-day" : "fog-night";
  case 51:
  case 53:
  case 55:
    return day ? "drizzle-day" : "drizzle-night";
  case 56:
  case 57:
  case 66:
  case 67:
    return day ? "freezing-rain-day" : "freezing-rain-night";
  case 61:
  case 63:
  case 65:
    return day ? "rain-day" : "rain-night";
  case 71:
  case 73:
  case 75:
  case 77:
    return day ? "snow-day" : "snow-night";
  case 80:
  case 81:
  case 82:
    return day ? "showers-day" : "showers-night";
  case 85:
  case 86:
    return day ? "snow-showers-day" : "snow-showers-night";
  case 95:
  case 97:
    return day ? "thunderstorm-day" : "thunderstorm-night";
  case 96:
  case 99:
    return day ? "thunderstorm-hail-day" : "thunderstorm-hail-night";
  default:
    return "unknown";
  }
}
int weather_url(const struct WeatherSettings *s, char *out, size_t cap) {
  int n = snprintf(
      out, cap,
      "https://api.open-meteo.com/v1/"
      "forecast?latitude=%.8f&longitude=%.8f&temperature_unit=%s&wind_speed_"
      "unit=%s&timezone=auto&forecast_days=7&current=temperature_2m,apparent_"
      "temperature,relative_humidity_2m,precipitation,weather_code,is_day,wind_"
      "speed_10m&hourly=temperature_2m,precipitation_probability,weather_code,"
      "is_day&daily=temperature_2m_min,temperature_2m_max,weather_code,"
      "precipitation_probability_max,sunrise,sunset",
      s->latitude, s->longitude, s->temperature_unit, s->wind_unit);
  return n > 0 && (size_t)n < cap;
}
static const char *description(const char *key){
 static const char *keys[]={"clear-day","clear-night","partly-cloudy-day","partly-cloudy-night","scattered-clouds-day","scattered-clouds-night","cloudy","fog","drizzle","rain","freezing-rain","snow","showers-day","showers-night","snow-showers","thunderstorm","thunderstorm-hail"};
 static const char *names[]={"Clear sky","Clear sky","Mainly clear","Mainly clear","Partly cloudy","Partly cloudy","Cloudy","Fog","Light drizzle","Rain","Freezing rain","Snow","Rain showers","Rain showers","Snow showers","Thunderstorms","Thunderstorms with hail"};
 for(size_t i=0;i<sizeof keys/sizeof *keys;i++)if(!strcmp(key,keys[i]) || (!strncmp(key,keys[i],strlen(keys[i])) && (!strcmp(key+strlen(keys[i]),"-day") || !strcmp(key+strlen(keys[i]),"-night"))))return names[i];
 return "Weather unavailable";
}
static const char *weekday(const char *date){
 int y=0,m=0,d=0;static const int shift[]={0,3,2,5,0,3,5,1,4,6,2,4};
 static const char *names[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
 if(sscanf(date,"%d-%d-%d",&y,&m,&d)!=3||m<1||m>12)return "";
 if(m<3)y--;
 return names[(y+y/4-y/100+y/400+shift[m-1]+d)%7];
}
static struct DockAppletField *field(struct DockAppletSnapshot *s,
                                     const char *key) {
  if (s->field_count == DA_FIELDS)
    return NULL;
  struct DockAppletField *f = &s->fields[s->field_count++];
  snprintf(f->key, sizeof f->key, "%s", key);
  return f;
}
static void text_field(struct DockAppletSnapshot *s, const char *key,
                       const char *value) {
  struct DockAppletField *f = field(s, key);
  if (f)
    snprintf(f->text, sizeof f->text, "%s", value);
}
static int measure(const struct DockJson *j, int obj, const char *key,
                   double *n, int required) {
  int t = dock_json_get(j, obj, key);
  if (t < 0 || j->token[t].type == DJ_NULL) {
    *n = NAN;
    return !required;
  }
  return dock_json_number(j, t, n) && fabs(*n) < 1e7;
}
static void number_field(struct DockAppletSnapshot *s, const char *key,
                         double n, const char *unit) {
  struct DockAppletField *f = field(s, key);
  if (!f)
    return;
  f->numeric = isfinite(n);
  f->number = f->numeric ? n : 0;
  snprintf(f->unit, sizeof f->unit, "%s", unit);
  if (f->numeric)
    snprintf(f->text, sizeof f->text, "%.0f %s", round(n), unit);
  else
    strcpy(f->text, "--");
}
static struct DockAppletRow *row(struct DockAppletSnapshot *s,
                                 const char *label, const char *value) {
  if (s->row_count == DA_ROWS)
    return NULL;
  struct DockAppletRow *r = &s->rows[s->row_count++];
  snprintf(r->label, sizeof r->label, "%s", label);
  snprintf(r->value, sizeof r->value, "%s", value);
  return r;
}
static int array(const struct DockJson *j, int obj, const char *key,
                 int count) {
  int t = dock_json_get(j, obj, key);
  return t >= 0 && j->token[t].type == DJ_ARRAY &&
                 dock_json_count(j, t) == count
             ? t
             : -1;
}
static int optional_time(const struct DockJson *j, int obj, const char *key,
                         int index, char *out, size_t cap) {
  int t = dock_json_at(j, dock_json_get(j, obj, key), index);
  if (t < 0)
    return 0;
  if (j->token[t].type == DJ_NULL) {
    out[0] = 0;
    return 1;
  }
  return dock_json_string(j, t, out, cap);
}
static int anum(const struct DockJson *j, int obj, const char *key, int index,
                double *n) {
  int t = dock_json_at(j, dock_json_get(j, obj, key), index);
  if (t < 0)
    return 0;
  if (j->token[t].type == DJ_NULL) {
    *n = NAN;
    return 1;
  }
  return dock_json_number(j, t, n) && fabs(*n) < 1e7;
}
/* ISO civil labels come directly from the API, preserving forecast-local clock
 * labels through DST without depending on a host timezone database. */
static int iso(const char *s, int date_only) {
  int y, m, d, h = 0, mi = 0;
  char tail;
  int n = date_only ? sscanf(s, "%d-%d-%d%c", &y, &m, &d, &tail)
                    : sscanf(s, "%d-%d-%dT%d:%d%c", &y, &m, &d, &h, &mi, &tail);
  return n == (date_only ? 3 : 5) &&
         strlen(s) == (size_t)(date_only ? 10 : 16) && y >= 1970 && y < 2200 &&
         m >= 1 && m <= 12 && d >= 1 && d <= 31 && h >= 0 && h < 24 &&
         mi >= 0 && mi < 60;
}
static double unix_time(const char *s, int offset) {
  int y, m, d, h, mi;
  if (sscanf(s, "%d-%d-%dT%d:%d", &y, &m, &d, &h, &mi) != 5)
    return 0;
  y -= m <= 2;
  int era = y / 400, yoe = y - era * 400, mp = m + (m > 2 ? -3 : 9);
  int days = era * 146097 +
             (yoe * 365 + yoe / 4 - yoe / 100 + (153 * mp + 2) / 5 + d - 1) -
             719468;
  return (double)days * 86400 + h * 3600 + mi * 60 - offset;
}
int weather_decode(const char *text, size_t len,
                   const struct WeatherSettings *settings,
                   struct WeatherForecast *out) {
  int ok = 0;
  struct DockJson *j = malloc(sizeof *j);
  struct WeatherForecast *f = calloc(1, sizeof *f);
  if (!j || !f)
    goto done;
  struct DockAppletSnapshot *s = &f->snapshot;
  if (!dock_json_parse(j, text, len) || dock_json_get(j, 0, "error") >= 0)
    goto done;
  int cur = dock_json_get(j, 0, "current"),
      hourly = dock_json_get(j, 0, "hourly"),
      daily = dock_json_get(j, 0, "daily"), offset, code, day;
  char current[40];
  if (!str(j, cur, "time", current, sizeof current) || !iso(current, 0) ||
      !dock_json_int(j, dock_json_get(j, 0, "utc_offset_seconds"), &offset) ||
      offset < -50400 || offset > 50400)
    goto done;
  double temp, feels, humidity, wind, rain;
  if (!measure(j, cur, "temperature_2m", &temp, 1) || temp < -150 ||
      temp > 150 || !measure(j, cur, "apparent_temperature", &feels, 0) ||
      !measure(j, cur, "relative_humidity_2m", &humidity, 0) ||
      !measure(j, cur, "wind_speed_10m", &wind, 0) ||
      !measure(j, cur, "precipitation", &rain, 0) ||
      !dock_json_int(j, dock_json_get(j, cur, "weather_code"), &code) ||
      !dock_json_int(j, dock_json_get(j, cur, "is_day"), &day) ||
      (day != 0 && day != 1))
    goto done;
  snprintf(s->title, sizeof s->title, "%s", settings->location);
  snprintf(s->icon, sizeof s->icon, "%s", weather_condition_key(code, day));
  s->source_time = unix_time(current, offset);
  s->updated = (double)time(NULL);
  s->status = DA_READY;
  snprintf(s->fingerprint, sizeof s->fingerprint, "%.8f,%.8f,%s,%s",
           settings->latitude, settings->longitude, settings->temperature_unit,
           settings->wind_unit);
  strcpy(s->attribution, "Weather data by Open-Meteo (CC BY 4.0)");
  strcpy(s->url, "https://open-meteo.com/");
  const char *unit =
      !strcmp(settings->temperature_unit, "fahrenheit") ? "F" : "C";
  text_field(s, "location", settings->location);
  number_field(s, "temperature", temp, unit);
  number_field(s, "feelsLike", feels, unit);
  number_field(s, "humidity", humidity, "%");
  number_field(s, "wind", wind, settings->wind_unit);
  number_field(s, "precipitation", rain, "mm");
  text_field(s, "condition", s->icon);
  text_field(s,"description",description(s->icon));
  char summary[160];
  snprintf(summary, sizeof summary, "%.45s; feels %.35s", description(s->icon),
           dock_applet_field(s, "feelsLike"));
  text_field(s, "summary", summary);
  row(s, "Condition", s->icon);
  row(s, "Feels like", dock_applet_field(s, "feelsLike"));
  row(s, "Humidity", dock_applet_field(s, "humidity"));
  row(s, "Wind", dock_applet_field(s, "wind"));
  row(s, "Precipitation", dock_applet_field(s, "precipitation"));
  row(s, "Local forecast time", current);
  row(s, "Next 24 hours", "Temperature / rain probability");
  int count = dock_json_count(j, dock_json_get(j, hourly, "time"));
  if (count < 24 || count > 400)
    goto done;
  const char *hk[] = {"time", "temperature_2m", "precipitation_probability",
                      "weather_code", "is_day"};
  for (size_t k = 0; k < sizeof hk / sizeof *hk; k++)
    if (array(j, hourly, hk[k], count) < 0)
      goto done;
  int used = 0;
  for (int i = 0; i < count; i++) {
    char date[40];
    double t, prob, c, d;
    if (!dock_json_string(j,
                          dock_json_at(j, dock_json_get(j, hourly, "time"), i),
                          date, sizeof date) ||
        !iso(date, 0) || !anum(j, hourly, "temperature_2m", i, &t) ||
        !anum(j, hourly, "precipitation_probability", i, &prob) ||
        !anum(j, hourly, "weather_code", i, &c) ||
        !anum(j, hourly, "is_day", i, &d))
      goto done;
    if (strcmp(date, current) < 0 || used == 24)
      continue;
    char val[100], tv[32], pv[32];
    if (isfinite(t))
      snprintf(tv, sizeof tv, "%.0f %s", round(t), unit);
    else
      strcpy(tv, "--");
    if (isfinite(prob))
      snprintf(pv, sizeof pv, "%.0f%%", prob);
    else
      strcpy(pv, "--");
    snprintf(val, sizeof val, "%s / %s", tv, pv);
    struct DockAppletRow *r = row(s, date, val);
    if (!r)
      goto done;
    strcpy(r->section,"hourly");
    snprintf(r->caption,sizeof r->caption,"%.5s",date+11);
    snprintf(r->value,sizeof r->value,"%s",tv);
    snprintf(r->detail,sizeof r->detail,"%s",pv);
    r->has_plot = isfinite(t);
    r->plot = isfinite(t) ? t : 0;
    snprintf(r->icon, sizeof r->icon, "%s",
             weather_condition_key(isfinite(c) ? (int)c : -1,
                                   isfinite(d) && d == 1));
    used++;
  }
  if (used < 24)
    goto done;
  row(s, "Next 7 days", "Low / high / rain probability");
  count = dock_json_count(j, dock_json_get(j, daily, "time"));
  if (count != 7)
    goto done;
  const char *dk[] = {"time",
                      "temperature_2m_min",
                      "temperature_2m_max",
                      "weather_code",
                      "precipitation_probability_max",
                      "sunrise",
                      "sunset"};
  for (size_t k = 0; k < sizeof dk / sizeof *dk; k++)
    if (array(j, daily, dk[k], count) < 0)
      goto done;
  for (int i = 0; i < count; i++) {
    char date[32], sunrise[40], sunset[40], lo[32], hi[32], pr[32], val[140];
    double a, b, c, prob;
    if (!dock_json_string(j,
                          dock_json_at(j, dock_json_get(j, daily, "time"), i),
                          date, sizeof date) ||
        !iso(date, 1) || !anum(j, daily, "temperature_2m_min", i, &a) ||
        !anum(j, daily, "temperature_2m_max", i, &b) ||
        !anum(j, daily, "weather_code", i, &c) ||
        !anum(j, daily, "precipitation_probability_max", i, &prob) ||
        !optional_time(j, daily, "sunrise", i, sunrise, sizeof sunrise) ||
        !optional_time(j, daily, "sunset", i, sunset, sizeof sunset))
      goto done;
    if (isfinite(a))
      snprintf(lo, sizeof lo, "%.0f", round(a));
    else
      strcpy(lo, "--");
    if (isfinite(b))
      snprintf(hi, sizeof hi, "%.0f", round(b));
    else
      strcpy(hi, "--");
    if (isfinite(prob))
      snprintf(pr, sizeof pr, "%.0f%%", prob);
    else
      strcpy(pr, "--");
    snprintf(val, sizeof val, "%s / %s %s / %s", lo, hi, unit, pr);
    struct DockAppletRow *r = row(s, date, val);
    if (!r)
      goto done;
    strcpy(r->section,"daily");
    snprintf(r->caption,sizeof r->caption,"%s",i?weekday(date):"Today");
    snprintf(r->value,sizeof r->value,"%s / %s %s",lo,hi,unit);
    snprintf(r->detail,sizeof r->detail,"%s",pr);
    if(!i){char range[96];snprintf(range,sizeof range,"Low %s   High %s %s",lo,hi,unit);text_field(s,"range",range);}
    snprintf(r->icon, sizeof r->icon, "%s",
             weather_condition_key(isfinite(c) ? (int)c : -1, 1));
  }
  *out = *f;
  ok = 1;
done:
  free(j);
  free(f);
  return ok;
}
int weather_geocode_url(const char *q, char *out, size_t cap) {
  char escaped[800];
  size_t n = 0;
  if (!q || !*q || strlen(q) > 120)
    return 0;
  for (; *q; q++) {
    unsigned char c = (unsigned char)*q;
    if (n + 4 >= sizeof escaped)
      return 0;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_')
      escaped[n++] = (char)c;
    else {
      snprintf(escaped + n, 4, "%%%02X", c);
      n += 3;
    }
  }
  escaped[n] = 0;
  int w = snprintf(out, cap,
                   "https://geocoding-api.open-meteo.com/v1/"
                   "search?name=%s&count=10&language=en&format=json",
                   escaped);
  return w > 0 && (size_t)w < cap;
}
int weather_geocode(const char *text, size_t len,
                    struct DockAppletSnapshot *out) {
  struct DockJson *j = malloc(sizeof *j);
  struct DockAppletSnapshot *s = calloc(1, sizeof *s);
  int ok = 0;
  if (!j || !s)
    goto done;
  if (!dock_json_parse(j, text, len) || dock_json_get(j, 0, "error") >= 0)
    goto done;
  int results = dock_json_get(j, 0, "results"), n = dock_json_count(j, results);
  if (n > 10)
    goto done;
  for (int i = 0; i < n; i++) {
    int r = dock_json_at(j, results, i);
    char name[100], country[80] = "", tz[80] = "";
    double lat, lon;
    if (!str(j, r, "name", name, sizeof name) ||
        !measure(j, r, "latitude", &lat, 1) ||
        !measure(j, r, "longitude", &lon, 1) || fabs(lat) > 90 ||
        fabs(lon) > 180)
      goto done;
    str(j, r, "country", country, sizeof country);
    str(j, r, "timezone", tz, sizeof tz);
    char label[160], v[160];
    snprintf(label, sizeof label, "%.90s, %.60s", name, country);
    snprintf(v, sizeof v, "%.8f,%.8f|%s", lat, lon, tz);
    row(s, label, v);
  }
  s->status = DA_READY;
  strcpy(s->title, "City search");
  s->updated = s->source_time = (double)time(NULL);
  *out = *s;
  ok = 1;
done:
  free(j);
  free(s);
  return ok;
}
