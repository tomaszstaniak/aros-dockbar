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
WEATHER_CC="$AROS_CC" WEATHER_CFLAGS="--sysroot=$AROS_SDK -ffile-prefix-map=$repo=dockbar -ffile-prefix-map=$AROS_SDK=sdk $dock_weather_cflags" \
 WEATHER_LIBS="${WEATHER_TARGET_LIBS:-$dock_weather_libs}" \
 WEATHER_BUILD_DIR="$output/weather-provider" sh "$repo/scripts/build-weather.sh"
# The static libraries' debug sections hold their build paths: strip them
# with the target's strip, and check the init/exit sets with its nm. Only the
# host tests, which use a stand-in compiler, may skip that (DOCK_HOST_TEST=1);
# a package build never does.
provider="$output/weather-provider/bin/WeatherProvider"
if [ "${DOCK_HOST_TEST:-0}" = 1 ]; then
  echo "Host test: provider not stripped or checked (stand-in compiler)"
  : > "$output/HOST-TEST-BUILD"   # scripts/package.sh refuses this build
else
  rm -f "$output/HOST-TEST-BUILD"
  strip=${AROS_CC%gcc}strip; [ -x "$strip" ] || strip=${AROS_CC%cc}strip
  [ -x "$strip" ] || { echo "No target strip next to $AROS_CC" >&2; exit 1; }
  "$strip" --strip-debug "$provider" || { echo "$strip failed on $provider" >&2; exit 1; }
  # collect-aros can leave an init/exit set entry of a library outside
  # __INIT_LIST__/__EXIT_LIST__ (AROS issue 2071); such an entry is unplaced,
  # at address 0, and its function never runs. Refuse that provider.
  nm=${AROS_CC%gcc}nm; [ -x "$nm" ] || nm=${AROS_CC%cc}nm
  [ -x "$nm" ] || { echo "No target nm next to $AROS_CC" >&2; exit 1; }
  syms=$("$nm" "$provider") || { echo "$nm cannot read $provider" >&2; exit 1; }
  case "$syms" in *" __INIT_LIST__"*) ;; *) echo "$nm output of $provider has no __INIT_LIST__" >&2; exit 1 ;; esac
  orphans=$(printf '%s\n' "$syms" | awk '$3 ~ /^__aros_set_/ && $1 ~ /^0+$/ {print $3}')
  [ -z "$orphans" ] || { echo "Weather provider has init/exit set entries outside the lists (name the library on the link line):" $orphans >&2; exit 1; }
  # libpthread calls __stdc_set_taskexit(), which older stdc.library 0.x
  # systems do not have under the same version number. The provider's own
  # definition (stdc_compat.c) must replace the SDK's weak stub.
  if printf '%s\n' "$syms" | grep -q ' ____stdc_set_taskexit_StdCBase_wrapper$'; then
    printf '%s\n' "$syms" | grep -q ' T __stdc_set_taskexit$' || {
      echo "Weather provider uses the SDK stub of __stdc_set_taskexit; build stdc_compat.c with -DWEATHER_STDC_COMPAT" >&2; exit 1; }
  fi
fi
python3 "$repo/scripts/stage-weather.py" --target "$dock_target" "$output/weather-provider/bin/WeatherProvider" "$output/package/Widgets"
