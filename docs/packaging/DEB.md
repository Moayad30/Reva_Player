# DEB Packaging

DEB is the native package format for Debian and Ubuntu-family systems. Reva Player has two repository-supported DEB paths:

- Release bundled DEB through `scripts/build-deb.sh` / `scripts/build-bundled-deb.sh`.
- Small system-library DEB through CPack in `CMakeLists.txt`, available as an advanced option.

Verified against repository files and a local bundled DEB build on 2026-04-26.

## Target Systems

- Debian
- Ubuntu
- Linux Mint
- Pop!_OS
- elementary OS

Needs verification: exact release versions, dependency names, and Wayland/X11 behavior on each distribution.

## Package Goals

- Install `RevaPlayer` into the system command path.
- Register desktop launcher and icon.
- Register AppStream metadata.
- Depend on or bundle Qt/libmpv runtime components.
- Preserve user data across upgrades.
- Remove installed files on uninstall without deleting user data by default.

## System DEB Layout

CMake install rules currently install:

| File | Destination |
| --- | --- |
| `RevaPlayer` | `${CMAKE_INSTALL_BINDIR}` usually `/usr/bin` |
| Desktop file | `${CMAKE_INSTALL_DATAROOTDIR}/applications` |
| AppStream metadata | `${CMAKE_INSTALL_DATAROOTDIR}/metainfo` |
| SVG icon | `${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps` |
| Root `README.md`, `LICENSE`, `THIRD_PARTY_NOTICES.md` | `${CMAKE_INSTALL_DATAROOTDIR}/doc/revaplayer` |

There are no repository-verified default config files installed outside the SQLite defaults seeded at runtime.

## Dependencies

The release DEB is bundled. It installs the application under `/opt/revaplayer`,
provides `/usr/bin/RevaPlayer`, and includes the Qt/libmpv/FFmpeg runtime copied
from the AppImage AppDir.

Qt DBus is a build-time Qt component and a runtime library requirement for the
desktop sleep-inhibition path. Bundled DEB builds copy Qt DBus/libdbus when they
are available in the source runtime.

The 2026-04-26 bundled package dependencies on amd64 are:

```text
bash, libc6, libstdc++6, libgcc-s1, zlib1g, libgl1, libglx0, libglvnd0, libopengl0, libegl1, libx11-6, libx11-xcb1, libxcb1, libxcb-dri3-0, libdrm2, libgbm1, libfontconfig1, libfreetype6, libharfbuzz0b, libasound2t64 | libasound2, libpipewire-0.3-0t64 | libpipewire-0.3-0, libglib2.0-0t64 | libglib2.0-0, libdbus-1-3
```

These are base OS, graphics, and audio libraries that should come from the target
system. They are intentionally not all bundled because GL/GPU and desktop stack
libraries are safer when matched to the host.

The small system-library CPack DEB currently sets:

- Qt 6: `libqt6sql6-sqlite, qt6-wayland`
- Qt 5: `libqt5sql5-sqlite, qtwayland5`
- `CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON`

The 2026-04-26 local system-library package generated these dependencies on amd64:

```text
libqt6sql6-sqlite, qt6-wayland, libc6 (>= 2.34), libgcc-s1 (>= 3.0), libmpv2 (>= 0.29), libqt6core6t64 (>= 6.9.1), libqt6gui6 (>= 6.9.1), libqt6openglwidgets6 (>= 6.1.2), libqt6sql6 (>= 6.1.2), libqt6widgets6 (>= 6.3.0), libstdc++6 (>= 14)
```

Needs verification: exact dependency names and minimum versions on each target
Debian/Ubuntu release.

## Build Steps

Preferred release command:

```bash
scripts/build-deb.sh
```

Output:

```text
dist/deb/revaplayer_<version>_<arch>.deb
```

Manual equivalent:

```bash
scripts/build-bundled-deb.sh
```

Small system-library DEB for development/packaging comparison:

```bash
scripts/build-deb.sh --system
```

Inspect:

```bash
dpkg -I path/to/package.deb
dpkg -c path/to/package.deb
```

Install test:

```bash
sudo apt install ./path/to/package.deb
RevaPlayer --version
```

## Bundled DEB

Script:

```bash
scripts/build-bundled-deb.sh
```

The bundled DEB installs under `/opt/revaplayer` and writes a `/usr/bin/RevaPlayer` wrapper. It expects bundled runtime content from `dist/AppDir/usr` unless `--bundle-source` is passed.

The bundled DEB script verifies bundled ELF dependencies with `ldd` when the
tool is available and stops the build if any dependency is unresolved.

Use bundled DEB when offline-friendly installation is more important than minimal package size. It is the current release DEB.

## mpv Handling

System DEB should depend on system libmpv. Bundled DEB is designed to include runtime libraries/plugins from the AppImage payload. Both require testing with common codecs and subtitle formats.

## Uninstall And Upgrade

- Package uninstall should remove installed files only.
- User data remains under Qt/XDG data/cache/config paths.
- For data cleanup, use [../reference/PURGE_LOCAL_DATA.md](../reference/PURGE_LOCAL_DATA.md).
- Upgrade should preserve `revaplayer.sqlite`.

## Common Issues

- Missing Qt SQL SQLite plugin.
- Missing libmpv runtime or incompatible libmpv ABI.
- Missing Qt platform plugin for xcb/Wayland.
- AppStream validation failures.
- Bundled DEB missing runtime files if `dist/AppDir/usr` was not prepared.

## Evaluation

| Factor | Rating | Notes |
| --- | --- | --- |
| Performance | High | Native system package; no sandbox overhead. |
| Installation ease | High | Best for Debian/Ubuntu users. |
| Update ease | Medium | Depends on repository or manual DEB distribution. |
| Maintenance | Medium | Dependency correctness must be maintained. |
| Compatibility | High on deb-based | Not useful for RPM distributions. |
| Build difficulty | Medium | CPack easy; clean dependency validation still required. |
| Distribution reach | Medium | Strong on Debian/Ubuntu-family only. |

Ratings are estimates, not benchmarks.

## When DEB Is Best

- Target users are on Debian/Ubuntu-family systems.
- You want native desktop integration.
- You can test install, upgrade, uninstall, and dependencies on clean VMs.

## When DEB Is Not Best

- You need one artifact for many distributions.
- You cannot validate dependencies on target OS releases.
- You need sandboxed app-store style distribution.
