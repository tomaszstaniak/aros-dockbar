# Sourced by the build and packaging scripts: what each Dockbar target is.
# A target is one ABI with its own compiler, SDK, build directory, widget
# provider name and package name; nothing is shared between two of them.
#
#   x86_64-abiv11   AROS x86_64 ABIv11 (AROS One, ABIv11 2026.09)
#   x86_64-v1       AROS x86_64 mainline (ABIv1), compositor-era SDK
#   aarch64-raspi-v1  native Raspberry Pi build (SVG icons with an aarch64
#                     svgicon.library, see dock_check_svgicon)
#
# dock_archive is the archive name's target part (<system>-<arch>-<abi>-
# <platform>, the shared naming of 2026-10-26); dock_pack names the
# packaging files and nothing inside the archive changes with it.
#
# Callers set DOCK_TARGET and source this file after $repo is known.

dock_target=${DOCK_TARGET:?Set DOCK_TARGET to x86_64-abiv11, x86_64-v1 or aarch64-raspi-v1}
case "$dock_target" in
  x86_64-abiv11) dock_abi=v11; dock_weather=yes; dock_pack=x86_64-abiv11; dock_archive=aros-x86_64-abiv11-pc
    dock_weather_libs="-lcurl -lssl -lcrypto -lzstd -lz.static -lm" ;;
  x86_64-v1) dock_abi=v1; dock_weather=yes; dock_pack=x86_64-v1; dock_archive=aros-x86_64-abiv1-pc
    # contrib curl 8.21 over contrib OpenSSL 4.0.1; curl reaches sockets
    # through miami.library and net.lib. -lpthread is named for the reason
    # given under aarch64-raspi-v1: here only libpthread's exit set was lost
    # (AROS issue 2071), so __pthread_Exit_Func never ran.
    dock_weather_libs="-lcurl -lssl -lcrypto -lzstd-static -lbrotlidec -lbrotlicommon -lz.static -lmiami -lnet -latomic -lpthread -lm" ;;
  aarch64-raspi-v1) dock_abi=v1; dock_weather=yes; dock_pack=aarch64-raspi-v1; dock_archive=aros-aarch64-abiv1-raspi
    # The same contrib curl and OpenSSL as x86_64-v1, from the Pi contrib SDK.
    # -lpthread is named although curl pulls it in: collect-aros only lists
    # the init/exit sets of archive members it sees on the command line, and
    # without it libpthread's constructor never runs (thread_sem and tls_sem
    # stay uninitialised: hangs and "Semaphore in an illegal state").
    dock_weather_libs="-lcurl -lssl -lcrypto -lzstd-static -lbrotlidec -lbrotlicommon -lz.static -lmiami -lnet -latomic -lpthread -lm" ;;
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

# dock_check_svgicon LIBRARY [BUILDINFO]: refuses an svgicon.library that was not built
# for this target. The ABI is read from the binary itself: a mainline build
# opens posixc/stdc, an ABIv11 build crt.library; the CPU from its ELF header.
# A BUILDINFO written next to it by aros-svg-icons' build script must name the
# same target and checksum.
# Mainline libraries must not open the per-task posixc/stdcio bases
# themselves nor carry libpthread. x86_64 mainline libraries must carry
# libgcc's register size table, or the first C++ exception inside them aborts
# the program (GCC 10.5 before 2026-09-13); the Raspberry Pi library unwinds
# with LLVM's libunwind instead, which must be linked into it.
dock_check_svgicon() {
  lib=$1
  [ -s "$lib" ] || { echo "svgicon.library missing or empty: $lib" >&2; exit 1; }
  nm=${AROS_CC%gcc}nm
  [ -x "$nm" ] || nm=${AROS_CC%cc}nm
  [ -x "$nm" ] || { echo "No $nm to check svgicon.library" >&2; exit 1; }
  syms=$("$nm" "$lib" 2>/dev/null) || { echo "Not an AROS ELF library: $lib" >&2; exit 1; }
  case "$syms" in
    *__aros_libset_PosixCBase*|*__aros_libset_StdCBase*) lib_abi=v1 ;;
    *__aros_libset_CrtBase*) lib_abi=v11 ;;
    *) echo "Cannot tell which ABI $lib was built for" >&2; exit 1 ;;
  esac
  [ "$lib_abi" = "$dock_abi" ] || {
    echo "svgicon.library is ABI$lib_abi, $dock_target needs ABI$dock_abi: $lib" >&2; exit 1; }
  # Both mainline targets are ABIv1: the CPU tells them apart.
  case "$(od -An -tu1 -j18 -N2 "$lib" | tr -s ' \n' '  ' | sed 's/^ *//; s/ *$//')" in
    "62 0") lib_cpu=x86_64 ;;
    "183 0") lib_cpu=aarch64 ;;
    *) lib_cpu=unknown ;;
  esac
  [ "$lib_cpu" = "${dock_target%%-*}" ] || {
    echo "svgicon.library is for $lib_cpu, $dock_target needs ${dock_target%%-*}: $lib" >&2; exit 1; }
  if [ "$lib_abi" = v1 ]; then
    # posixc and stdcio have one base per task: a library that opens them
    # itself keeps its first opener's base, and later programs in the same
    # Shell process are handed it (aros-svg-icons 3b132dc and later avoid it).
    case "$syms" in *__aros_libset_PosixCBase*|*__aros_libset_StdCIOBase*)
      echo "svgicon.library opens posixc.library or stdcio.library itself; rebuild it (aros-svg-icons 3b132dc or later): $lib" >&2; exit 1 ;;
    esac
    # libpthread linked into a library tracks its callers as threads.
    case "$syms" in *" __pthread_Init_Func"*)
      echo "svgicon.library contains libpthread; rebuild it (aros-svg-icons 3b132dc or later): $lib" >&2; exit 1 ;;
    esac
    if [ "$lib_cpu" = aarch64 ]; then
      case "$syms" in *" T __unw_init_local"*) ;; *)
        echo "svgicon.library has no libunwind; C++ exceptions in it would abort: $lib" >&2; exit 1 ;;
      esac
    else
      case "$syms" in *" _Unwind_RaiseException"*)
        case "$syms" in *" dwarf_reg_size_table"*) ;; *)
          echo "svgicon.library has no libgcc register size table; C++ exceptions in it would abort: $lib" >&2; exit 1 ;;
        esac ;;
      esac
    fi
  fi
  info=${2:-$(dirname "$lib")/BUILDINFO}
  if [ -f "$info" ]; then
    info_target=$(sed -n 's/^target=//p' "$info")
    case "$info_target" in abiv11) info_dock=x86_64-abiv11 ;; v1) info_dock=x86_64-v1 ;; raspi) info_dock=aarch64-raspi-v1 ;; *) info_dock=unknown ;; esac
    [ "$info_dock" = "$dock_target" ] || { echo "$info says target $info_target, not one for $dock_target" >&2; exit 1; }
    info_sum=$(sed -n 's/^sha256=//p' "$info")
    [ "$info_sum" = "$(shasum -a 256 "$lib" | cut -d' ' -f1)" ] ||
      { echo "$lib does not match the checksum in $info" >&2; exit 1; }
  elif [ "$dock_target" = aarch64-raspi-v1 ]; then
    echo "No BUILDINFO ($info) for $lib: the Raspberry Pi library must come from aros-svg-icons' build script" >&2; exit 1
  fi
}
