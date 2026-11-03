# SVG adapter and development dependency

The dock uses `svgicon.library` as a separately loaded application dependency.
It does not install or replace the system `icon.library`. The adapter opens the
explicit runtime path supplied by the application, normally
`PROGDIR:libs/svgicon.library`, and calls the generated library interface.
No renderer implementation is copied into this repository.

## Build inputs

Supply the generated include root through the build's SVG include setting. It
must contain `proto/svgicon.h`, `inline/svgicon.h`, `defines/svgicon.h` and
`clib/svgicon_protos.h`. Add that root to the compiler include path. Use GNU C
mode (for example `-std=gnu99`): AROS SDK inline headers use GNU `asm` syntax.
No SVG link archive is needed. The C adapter uses `OpenLibrary` and the generated
inline vectors, then `CloseLibrary` on shutdown.

The development dependency consists of two separately supplied inputs:

- Generated headers from the dependency's `build/svgiconlib/gen/include/` tree.
- Its matching x86_64 ABIv11 `build/svgiconlib/svgicon.library` binary.

A reusable development kit may place them under `include/` and
`libs/svgicon.library`. Preserve the include subdirectories; a single copied
`svgicon.h` is not sufficient. Keep the kit outside versioned application source
and configure its paths locally. Stage only the binary into the application's
`libs/` drawer. Do not copy the dependency's source workspace, private documents,
patched `icon.library`, or build tools into the application package.

The interface inspected for this prototype is `aros-svg-icons` revision
`d08d2f8b7d56a469586e325b26e4211527f32254`, library interface 0.1. The inspected
local development binary has SHA-256:

```text
4c37ce0783171f07ab6e347f452cae78ecafc34e028be2a9498156cc93ad5772
```

Record the actual dependency hash with each local build. This hash identifies
the inspected binary; it is not a claim of a reproducible rebuild.

## Ownership and limits

`dock_svg_open` and `dock_svg_render` return 1 for success and 0 for failure.
Failed rendering always leaves the output pointer NULL. Missing or invalid SVG
must fall back to a visible icon at the application layer.

The adapter is used from one application task. It limits each source to 1 MiB
and square output to 1–512 pixels. It releases source bytes and parsed documents
before returning. Successful output is an application-owned buffer of tightly
packed, straight-alpha A,R,G,B bytes; release it with `dock_svg_free`. Buffers
remain valid after closing the library. The caller caches by source and size,
then composites these pixels into its background outside the adapter.

`dock_svg_render_count` counts successful calls through this adapter since its
last open attempt. It does not count other programs using the library and does
not query global library statistics. Repeated painting of a cached icon must
leave this count unchanged.

Input-size and output-size limits do not establish safety for arbitrary hostile
SVG content. The present prototype uses its own small runtime assets; the
renderer retains its own documented implementation limitations.

## Provenance and packaging scope

The dependency pins LunaSVG, including bundled PlutoVG, at
`cf3594d5232e075fb344365191f0ecadd1519e1e`. Both checked dependency LICENSE files
state MIT, copyright 2020–2026 Samuel Ugochukwu. Keep their full notices when
preparing a redistributable dependency package. Generated headers carry AROS
Development Team notices. The inspected `aros-svg-icons` wrapper has no
repository-wide license file; a public redistribution package therefore still
needs an explicit wrapper license and a review of its bundled runtime notices.
This prototype defines local development staging, not a public dependency SDK.

`assets/runtime/folder.svg`, `terminal.svg`, and `compass.svg` are original vector
art created for this project. They contain no copied Docky, system, or third-party
icon artwork, embedded raster images, fonts, scripts, or external resources.

Verification on 2026-10-13: the adapter compiled and linked with x86_64-aros-gcc
and the ABIv11 2026.09 SDK, without an SVG link library. Host checks exercised
input limits, missing files, parse/render failures, cleanup and counters with
sanitizers. The existing host renderer rendered all three assets at 24, 48, 72
and 128 pixels with transparent corners and partially transparent edge pixels.
Separate native application checks confirmed the folder icon at48/64px, constant
render count during ordinary redraws, and native fallback for missing/invalid SVG
or library. See [development verification](development.md#checks-and-current-scope).
No performance benchmark is claimed.

Mainline (x86_64-v1): aros-svg-icons builds a separate `svgicon.library` for
mainline (`scripts/build-svgiconlib.sh v1`), with the mainline compiler, SDK and
genmodule. `scripts/build.sh` and `scripts/package.sh` refuse a library or
generated headers built for the other ABI (the library's C runtime bases and the
`BUILDINFO` written next to it tell them apart), a mainline library without
libgcc's register size table, and a library whose `BUILDINFO` revision differs
from `SVGICON_REVISION`. Packaging also checks the renderer's provenance against
`packaging/svg-provenance.txt` (see development.md, "Tester archives"). That the
mainline library's C++ exceptions are caught at run time is checked by a
developer probe in aros-svg-icons (`tests/exctest/`), not by these scripts.
