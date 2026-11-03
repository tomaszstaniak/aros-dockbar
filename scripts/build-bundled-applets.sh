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
WEATHER_CC="$AROS_CC" WEATHER_CFLAGS="--sysroot=$AROS_SDK -ffile-prefix-map=$repo=dockbar -ffile-prefix-map=$AROS_SDK=sdk" \
 WEATHER_LIBS="${WEATHER_TARGET_LIBS:-$dock_weather_libs}" \
 WEATHER_BUILD_DIR="$output/weather-provider" sh "$repo/scripts/build-weather.sh"
# collect-aros can leave an init/exit set entry of a library outside
# __INIT_LIST__/__EXIT_LIST__ (AROS issue 2071); such an entry is unplaced,
# at address 0, and its function never runs. Refuse that provider.
# The static libraries' debug sections hold their build paths.
strip=${AROS_CC%gcc}strip; [ -x "$strip" ] || strip=${AROS_CC%cc}strip
if [ -x "$strip" ]; then "$strip" --strip-debug "$output/weather-provider/bin/WeatherProvider"
else command -v strip >/dev/null || { echo "No $strip for the Weather provider" >&2; exit 1; }; fi
nm=${AROS_CC%gcc}nm
[ -x "$nm" ] || nm=${AROS_CC%cc}nm
# The host tests use a stand-in compiler without a target nm.
[ -x "$nm" ] || nm=$(command -v nm) || { echo "No nm to check the Weather provider" >&2; exit 1; }
orphans=$("$nm" "$output/weather-provider/bin/WeatherProvider" | awk '$3 ~ /^__aros_set_/ && $1 ~ /^0+$/ {print $3}')
[ -z "$orphans" ] || { echo "Weather provider has init/exit set entries outside the lists (name the library on the link line):" $orphans >&2; exit 1; }
python3 "$repo/scripts/stage-weather.py" --target "$dock_target" "$output/weather-provider/bin/WeatherProvider" "$output/package/Widgets"
