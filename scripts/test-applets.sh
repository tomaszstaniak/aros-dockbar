#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run=$(mktemp -d "${TMPDIR:-/tmp}/dockbar-applets.XXXXXX")
trap 'rm -rf "$run"' EXIT HUP INT TERM
compiler=${CC:-cc}
flags='-std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -g'
for test in json snapshot package instance settings cache view; do
 sources="$repo/src/applets/json.c $repo/src/applets/snapshot.c"
 case "$test" in view) sources="$sources $repo/src/applets/view.c";; cache) sources="$sources $repo/src/applets/cache.c";; package) sources="$sources $repo/src/applets/package.c $repo/src/applets/view.c";; instance) sources="$sources $repo/src/applets/instance.c";; settings) sources="$sources $repo/src/applets/settings.c";; esac
 "$compiler" $flags -I"$repo/src" "$repo/tests/test_applet_$test.c" $sources -lm -o "$run/$test"
 case "$test" in package) mkdir "$run/packages"; "$run/$test" "$run/packages";; settings) "$run/$test" "$repo/applets/org.aros.weather/settings.json" "$run";; *) "$run/$test";; esac
done
"$compiler" $flags -I"$repo/src" -I"$repo/applets/org.aros.weather/provider" \
 "$repo/tests/test_weather.c" "$repo/applets/org.aros.weather/provider/weather.c" \
 "$repo/src/applets/json.c" "$repo/src/applets/snapshot.c" -lm -o "$run/weather"
"$run/weather" "$repo/tests/fixtures/weather/forecast.json"
"$compiler" $flags -I"$repo/src" "$repo/tests/test_applet_config.c" "$repo/src/docks.c" \
 "$repo/src/model.c" "$repo/src/config.c" "$repo/src/layout.c" "$repo/src/theme.c" -lm -o "$run/config"
"$run/config" "$run/config-data"
"$compiler" $flags -I"$repo/src" "$repo/tests/test_applet_purge.c" "$repo/src/applets/purge.c" -o "$run/purge"
mkdir "$run/purge-data"; "$run/purge" "$run/purge-data"
if command -v curl-config >/dev/null 2>&1; then
 "$compiler" $flags -I"$repo/applets/org.aros.weather/provider" $(curl-config --cflags) \
  "$repo/tests/test_weather_http.c" "$repo/applets/org.aros.weather/provider/http.c" \
  $(curl-config --libs) -o "$run/http"
 "$run/http"
else echo 'SKIP weather HTTP bounds: curl development files not installed'; fi
