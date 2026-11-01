#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output=${WEATHER_BUILD_DIR:-"$repo/build/weather-host"}
compiler=${WEATHER_CC:-${CC:-cc}}
mkdir -p "$output/bin"
# WEATHER_CFLAGS/LIBS are compiler words, deliberately supplied by the caller.
# A target build sets WEATHER_CC, --sysroot in WEATHER_CFLAGS and static libs.
"$compiler" -std=gnu99 -O2 -Wall -Wextra -Werror ${WEATHER_CFLAGS:-} \
 -I"$repo/src" -I"$repo/applets/org.aros.weather/provider" \
 "$repo/applets/org.aros.weather/provider/main.c" "$repo/applets/org.aros.weather/provider/weather.c" \
 "$repo/applets/org.aros.weather/provider/http.c" "$repo/src/applets/json.c" "$repo/src/applets/snapshot.c" \
 -o "$output/bin/WeatherProvider" ${WEATHER_LIBS:--lcurl -lm}
printf 'Provider: %s\n' "$output/bin/WeatherProvider"
