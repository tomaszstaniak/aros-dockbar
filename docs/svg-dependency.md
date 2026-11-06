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
- Its matching target-specific `svgicon.library` binary (PC v1, PC ABIv11 or Pi aarch64).

A reusable development kit may place them under `include/` and
`libs/svgicon.library`. Preserve the include subdirectories; a single copied
`svgicon.h` is not sufficient. Keep the kit outside versioned application source
and configure its paths locally. Stage only the binary into the application's
`libs/` drawer. Do not copy the dependency's source workspace, private documents,
patched `icon.library`, or build tools into the application package.

Keep the library's `BUILDINFO` alongside it. Packaging checks its target, revision and checksum against the configured inputs and [provenance pins](../packaging/svg-provenance.txt). Do not reuse a binary from another ABI or architecture.

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
SVG content. The renderer retains its own documented implementation limitations.

## Provenance and packaging

The aros-svg-icons wrapper is MIT-licensed. LunaSVG and its bundled PlutoVG are also MIT-licensed; their pinned revision is recorded in `packaging/svg-provenance.txt`. Preserve all notices, including the AROS interface notices and the GCC or LLVM runtime terms for the selected target. The package includes these dependency notices separately from Dockbar's licence.

The build and packaging scripts check ABI compatibility, BUILDINFO, renderer revision and licence provenance. They do not replace runtime checks of C++ exception handling; those probes belong to aros-svg-icons. See [packaging requirements](development.md#packaging).

Original Dockbar artwork is described in the [asset guide](../assets/README.md). The optional renderer is a general SVG library; it does not require the purchased Weather artwork.
