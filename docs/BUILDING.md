# Building Reva Player

This document lists build commands and packaging commands that are present in the repository. Commands marked "Needs verification" require target-system testing before release use.

Verified against repository files on 2026-05-10.

## Requirements

Repository-verified build requirements:

- CMake `3.24` or newer.
- C++20 compiler.
- Qt Widgets, Qt Sql, Qt DBus, and OpenGL module.
- libmpv development files discoverable by `cmake/Findmpv.cmake`.
- SQLite driver for Qt SQL.
- Optional test dependency: Qt Test.

Qt selection:

- Default: Qt 6 using `Qt6::Widgets`, `Qt6::Sql`, `Qt6::DBus`, `Qt6::OpenGLWidgets`, `Qt6::Test`.
- Optional: Qt 5 using `-DREVAPLAYER_QT_MAJOR=5`, with `Qt5::Widgets`, `Qt5::Sql`, `Qt5::DBus`, `Qt5::OpenGL`, `Qt5::Test`.

Linux package names vary by distribution. The following Debian/Ubuntu names are expected but should be checked per release:

- `cmake`
- `g++` or `clang++`
- `qt6-base-dev`
- `qt6-base-dev-tools`
- `qt6-base-dev` normally provides Qt DBus development files on Debian/Ubuntu-family systems.
- `qt6-wayland`
- `libqt6sql6-sqlite`
- `libmpv-dev`
- `libgl1-mesa-dev`
- `ninja-build` if using Ninja

Needs verification: exact package names for Debian, Ubuntu, Fedora, openSUSE, RHEL-family, and Arch derivatives.

## Development Build

```bash
cmake -S . -B build
cmake --build build --parallel
```

Run:

```bash
./build/RevaPlayer
./build/RevaPlayer --version
./build/RevaPlayer --help
```

Use a disposable database for smoke tests:

```bash
REVAPLAYER_DB_PATH=/tmp/revaplayer-smoke.sqlite ./build/RevaPlayer
```

## Release Build

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
```

Install into a staging directory:

```bash
DESTDIR=/tmp/revaplayer-install-check cmake --install build-release
```

The install rules place files under `/usr` by default because `CMAKE_INSTALL_PREFIX` defaults to `/usr` in `CMakeLists.txt`.

## Qt 5 Build

```bash
cmake -S . -B build-qt5 -DREVAPLAYER_QT_MAJOR=5
cmake --build build-qt5 --parallel
```

Needs verification: feature parity and packaging behavior with Qt 5 should be tested before release.

## Tests

Build with tests enabled, which is the default:

```bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Tests declared by CMake:

- `RevaPlayer_tests`

The test environment sets `QT_QPA_PLATFORM=offscreen` and `XDG_DATA_HOME=/tmp/revaplayer-tests`.

## System DEB With CPack

`CMakeLists.txt` configures CPack with `CPACK_GENERATOR "DEB"`.

```bash
cmake -S . -B build-deb -DCMAKE_BUILD_TYPE=Release
cmake --build build-deb --parallel
cpack --config build-deb/CPackConfig.cmake
```

This creates a DEB that uses system runtime libraries. CPack enables `CPACK_DEBIAN_PACKAGE_SHLIBDEPS`, and adds Qt SQL SQLite / Wayland package dependencies based on the selected Qt major.

Needs verification: run `dpkg -I`, `dpkg -c`, install on a clean VM, and verify runtime dependencies.

## Bundled Offline-Friendly DEB

Repository script:

```bash
scripts/build-bundled-deb.sh
```

Purpose:

- Builds/stages the application.
- Installs under `/opt/revaplayer`.
- Wraps `/usr/bin/RevaPlayer`.
- Reuses bundled runtime libraries/plugins from `dist/AppDir/usr` by default.

Important: this script expects a bundle source, normally produced by the AppImage flow. If `dist/AppDir/usr` does not exist, prepare AppImage assets first or pass `--bundle-source`.

## AppImage

Repository script:

```bash
scripts/build-appimage.sh
```

Required external tools:

- `linuxdeploy`
- `linuxdeploy-plugin-qt`
- `appimagetool`
- `qmake` for the selected Qt major

Example with explicit tool paths:

```bash
scripts/build-appimage.sh \
  --linuxdeploy /path/to/linuxdeploy \
  --linuxdeploy-plugin-qt /path/to/linuxdeploy-plugin-qt \
  --appimagetool /path/to/appimagetool
```

Fallback repack path:

```bash
scripts/repack-appimage-fallback.sh --source-appimage /path/to/old.AppImage
```

## Common Failures

| Symptom | Likely cause | Diagnostic |
| --- | --- | --- |
| `find_package(mpv REQUIRED)` fails | libmpv development files missing | Check `libmpv-dev` or distro equivalent. |
| Qt package not found | Qt dev package missing or wrong Qt major | Use `-DREVAPLAYER_QT_MAJOR=5` or install Qt 6 dev packages. |
| App starts but video area fails | OpenGL/Qt platform/plugin issue | Try `QT_QPA_PLATFORM=xcb ./build/RevaPlayer` or `QT_QPA_PLATFORM=wayland ./build/RevaPlayer`. |
| SQLite driver missing | Qt SQL SQLite plugin not installed | Install Qt SQLite package for selected Qt major. |
| AppImage build fails immediately | linuxdeploy/appimagetool missing | Provide tool paths or environment overrides. |

## Pre-Publish Build Checklist

- Confirm `project(RevaPlayer VERSION ...)` is correct.
- Run `ctest --test-dir <build-dir> --output-on-failure`.
- Run `./<build-dir>/RevaPlayer --version`.
- Smoke-launch with a temporary database path.
- Inspect desktop file and AppStream metadata.
- For package builds, inspect package contents before install.
- Install on a clean target VM before publishing.
