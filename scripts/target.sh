# Sourced by the build and packaging scripts: what each Dockbar target is.
# A target is one ABI with its own compiler, SDK, build directory, widget
# provider name and package name; nothing is shared between two of them.
#
#   x86_64-abiv11   AROS x86_64 ABIv11 (AROS One, ABIv11 2026.09)
#   x86_64-v1       AROS x86_64 mainline (ABIv1), compositor-era SDK
#   aarch64-raspi-v1  native Raspberry Pi build, not run (no Weather)
#
# Callers set DOCK_TARGET and source this file after $repo is known.

dock_target=${DOCK_TARGET:?Set DOCK_TARGET to x86_64-abiv11 or x86_64-v1}
case "$dock_target" in
  x86_64-abiv11) dock_abi=v11; dock_weather=yes; dock_pack=x86_64-abiv11
    dock_weather_libs="-lcurl -lssl -lcrypto -lzstd -lz.static -lm" ;;
  x86_64-v1) dock_abi=v1; dock_weather=yes; dock_pack=x86_64-v1
    # contrib curl 8.21 over contrib OpenSSL 4.0.1; curl reaches sockets
    # through miami.library and net.lib.
    dock_weather_libs="-lcurl -lssl -lcrypto -lzstd-static -lbrotlidec -lbrotlicommon -lz.static -lmiami -lnet -latomic -lm" ;;
  aarch64-raspi-v1) dock_abi=v1; dock_weather=no; dock_pack=aarch64-raspi-v1-untested
    dock_weather_libs= ;;
  *) echo "Unknown DOCK_TARGET '$dock_target' (x86_64-abiv11, x86_64-v1, aarch64-raspi-v1)" >&2; exit 1 ;;
esac
dock_build=${DOCK_BUILD_DIR:-"$repo/build/$dock_target"}

# The two x86_64 ABIs use the same compiler name and the same predefined
# macros, and their binaries do not run on each other's system, so a wrong
# AROS_CC/AROS_SDK pair builds without complaint. Mainline's driver knows
# -noposixc and its SDK has libposixc; ABIv11 has neither.
dock_check_abi() {
  if echo 'int dock_probe;' | "$AROS_CC" --sysroot="$AROS_SDK" -noposixc -x c -c -o /dev/null - 2>/dev/null; then cc_abi=v1; else cc_abi=v11; fi
  if [ -e "$AROS_SDK/lib/libposixc.a" ]; then sdk_abi=v1; else sdk_abi=v11; fi
  if [ "$cc_abi" != "$dock_abi" ] || [ "$sdk_abi" != "$dock_abi" ]; then
    echo "DOCK_TARGET $dock_target needs an ABI$dock_abi compiler and SDK; AROS_CC is ABI$cc_abi, AROS_SDK is ABI$sdk_abi" >&2
    exit 1
  fi
}
