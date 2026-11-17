#!/bin/sh
# Builds the Currency widget's provider. It shares the Weather provider's
# HTTPS code and Dockbar's JSON and snapshot code, and takes the same
# WEATHER_CC, WEATHER_CFLAGS and WEATHER_LIBS words as scripts/build-weather.sh.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output=${CURRENCY_BUILD_DIR:-"$repo/build/currency-host"}
compiler=${WEATHER_CC:-${CC:-cc}}
mkdir -p "$output/bin"
"$compiler" -std=gnu99 -O2 -Wall -Wextra -Werror ${WEATHER_CFLAGS:-} \
 -I"$repo/src" -I"$repo/applets/org.aros.currency/provider" -I"$repo/applets/org.aros.weather/provider" \
 "$repo/applets/org.aros.currency/provider/main.c" "$repo/applets/org.aros.currency/provider/rates.c" \
 "$repo/applets/org.aros.weather/provider/http.c" "$repo/applets/org.aros.weather/provider/stdc_compat.c" "$repo/src/applets/json.c" "$repo/src/applets/snapshot.c" \
 -o "$output/bin/CurrencyProvider" ${WEATHER_LIBS:--lcurl -lm}
printf 'Provider: %s\n' "$output/bin/CurrencyProvider"
