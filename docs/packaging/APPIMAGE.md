# AppImage Packaging

AppImage provides a portable Linux application image that can run without system package installation. Reva Player has repository scripts for building, repacking, local installing, and uninstalling AppImage artifacts.

Verified against repository files and the v1.0.0 packaging path on 2026-05-10.

## Scripts

```bash
scripts/build-appimage.sh
scripts/repack-appimage-fallback.sh
scripts/install-appimage-local.sh
scripts/uninstall-appimage-local.sh
```

## Why AppImage Fits This Project

- Fastest current way to share Reva Player across non-DEB Linux distributions.
- Can bundle Qt, libmpv, plugins, and runtime libraries.
- Bundles Qt DBus/libdbus when available so active video playback can request desktop sleep inhibition.
- Does not require root installation.
- Good for beta builds and compatibility testing.

It is not always the best final update channel or desktop-integration story.

## Build Requirements

`scripts/build-appimage.sh` requires:

- CMake toolchain.
- Qt build tools and `qmake`.
- `linuxdeploy`.
- `linuxdeploy-plugin-qt`.
- `appimagetool`.
- Optional `APPIMAGE_RUNTIME` / `--runtime-file`.

Example:

```bash
scripts/build-appimage.sh \
  --linuxdeploy /path/to/linuxdeploy \
  --linuxdeploy-plugin-qt /path/to/linuxdeploy-plugin-qt \
  --appimagetool /path/to/appimagetool
```

Output name format:

```text
dist/appimage/RevaPlayer-v<version>-<arch>.AppImage
```

Fallback repack:

```bash
scripts/repack-appimage-fallback.sh --source-appimage /path/to/existing.AppImage
```

Use the fallback only when `linuxdeploy`, `linuxdeploy-plugin-qt`, or
`appimagetool` are not available. It reuses the runtime from an existing
AppImage, replaces the application binary and metadata from the current source
tree, and repacks a new AppImage.

## Runtime Handling

The AppImage wrapper configures:

- `PATH`
- `LD_LIBRARY_PATH`
- `QT_PLUGIN_PATH`
- `XDG_DATA_DIRS`
- `QT_QPA_PLATFORM` preference for Wayland when detected
- `QT_QPA_PLATFORMTHEME` for KDE/GTK where matching bundled plugin exists

The build script moves the real binary to `RevaPlayer.bin` and installs a wrapper as `RevaPlayer`.

The fallback repack script also copies optional Qt platform plugins for
`minimal`, `offscreen`, and Wayland when they are available on the build host.
This keeps smoke tests and Wayland fallback behavior from depending only on the
old source AppImage payload.

Before writing the final image, the AppImage scripts check bundled ELF files with
`ldd` when available and fail the build if a dependency reports `not found`.
Low-level host-sensitive graphics, audio, desktop session, X11/Wayland, DBus,
Samba, and kernel-adjacent libraries are pruned from the bundle where possible
so they come from the target system.

## Desktop Integration

The AppDir includes:

- Desktop file.
- AppStream metadata and `.appdata.xml` copy.
- SVG icon.
- `.DirIcon`.
- `AppRun` symlink.

Local integration is handled by:

```bash
scripts/install-appimage-local.sh /path/to/RevaPlayer-v<version>-<arch>.AppImage
```

Uninstall local integration:

```bash
scripts/uninstall-appimage-local.sh
```

## Sandboxing

AppImage is not a sandbox. It runs with the user's normal permissions. File access should behave like a normal desktop application unless desktop portals or environment settings interfere.

## Limitations

- No built-in repository update channel is implemented.
- AppImage portability depends on build host baseline and bundled libraries.
- The AppImage still relies on host kernel, glibc/loader, GPU drivers, FUSE
  for direct mounting, and low-level graphics libraries such as libGL.
- Deep desktop integration can vary by desktop environment.
- File dialogs may differ between KDE, GNOME, and other desktops.
- GPU/OpenGL behavior still depends on host drivers.

## Storage Paths

AppImage does not change the application storage model:

- SQLite: Qt `AppDataLocation/revaplayer.sqlite`, unless `REVAPLAYER_DB_PATH` is set.
- Thumbnails: Qt `CacheLocation/thumbnails`.
- Local AppImage installation files are separate from application data.

## Smoke Tests

```bash
APPIMAGE_EXTRACT_AND_RUN=1 ./dist/appimage/RevaPlayer-v<version>-<arch>.AppImage --version
QT_QPA_PLATFORM=minimal APPIMAGE_EXTRACT_AND_RUN=1 timeout 8s ./dist/appimage/RevaPlayer-v<version>-<arch>.AppImage
```

Needs verification: real playback on target desktops cannot be replaced by `minimal` smoke tests.

## Evaluation

| Factor | Rating | Notes |
| --- | --- | --- |
| Performance | High | Near-native; startup may include AppImage mount/extract overhead. |
| Portability | High | Best current cross-distro artifact. |
| Difficulty | Medium | External tools and runtime bundling complexity. |
| User convenience | High | Download, chmod/run, optional local install. |
| Maintenance burden | Medium | Must test runtime bundle and desktop behavior. |

Ratings are estimates, not benchmarks.
