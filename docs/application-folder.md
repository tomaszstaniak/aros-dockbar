# Application folder

Each release archive includes `Dockbar/application.json` (arospkg schema 1).
It lets `apkg run "Work:My Apps/Dockbar"` start a manually unpacked drawer,
without a catalogue or installed-package registry entry.

The source is `packaging/application-<target>.toml`. The packaging script
runs `scripts/application-manifest.py` with the release version. Keep the
revision, target, system requirements and launch fields consistent with the
arospkg catalogue manifest; its archive review checks the generated file.
Workbench launch uses `Dockbar.info` for stack and tool types.

The 0.5.2 archives were replaced with author approval on 5 November 2026 to
add this file. The catalogue revision is 2. All previously shipped files,
including executable binaries, are byte-identical. The original v0.5.2 tag
still identifies the binary source; the manifest packaging change is recorded
in later commits on main. Each new version starts again at revision 1.
