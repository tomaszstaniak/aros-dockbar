#!/bin/sh
# Builds the bundled Weather widget for DOCK_TARGET (an x86_64 target) and
# stages it into the target's package.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
: "${AROS_CC:?Set AROS_CC to the target compiler}"
: "${AROS_SDK:?Set AROS_SDK to the matching SDK sysroot}"
. "$repo/scripts/target.sh"
[ "$dock_weather" = yes ] || { echo "Bundled Weather has no provider for $dock_target" >&2; exit 1; }
output=$dock_build
# Weather links libcurl statically; without it there is no Weather, and a
# Dockbar package without Weather is not built for these targets.
for lib in libcurl.a libssl.a libcrypto.a; do
  [ -f "$AROS_SDK/lib/$lib" ] || {
    echo "Weather for $dock_target needs $lib in $AROS_SDK/lib (mainline: build contrib-curl and contrib-openssl)" >&2
    exit 1
  }
done
[ -f "$AROS_SDK/include/curl/curl.h" ] || { echo "Weather for $dock_target needs curl/curl.h in $AROS_SDK/include" >&2; exit 1; }
WEATHER_CC="$AROS_CC" WEATHER_CFLAGS="--sysroot=$AROS_SDK" \
 WEATHER_LIBS="${WEATHER_TARGET_LIBS:-$dock_weather_libs}" \
 WEATHER_BUILD_DIR="$output/weather-provider" sh "$repo/scripts/build-weather.sh"
python3 "$repo/scripts/stage-weather.py" --target "$dock_target" "$output/weather-provider/bin/WeatherProvider" "$output/package/Widgets"
