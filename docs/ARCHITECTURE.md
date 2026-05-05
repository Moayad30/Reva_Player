# Architecture

Reva Player is a single-process Qt Widgets desktop application. The codebase is layered around UI widgets, application controllers, libmpv integration, and SQLite persistence.

Verified against repository files on 2026-04-30.

## Runtime Stack

| Layer | Repository paths | Responsibility |
| --- | --- | --- |
| Entry/bootstrap | `app/main.cpp`, `app/AppBootstrap.cpp` | QApplication setup, command-line parsing, controller construction, theme/language bootstrap. |
| UI | `ui/` | Main window, video viewport, dialogs, control bar, file dialogs, overlays. |
| Application controllers | `application/` | Playback, playlist, settings, history, bookmarks, screenshots, theme, language. |
| mpv integration | `infrastructure/mpv/` | libmpv handle, commands, properties, events, OpenGL render host. |
| Storage | `infrastructure/storage/` | SQLite schema, settings, window state, resume, history, bookmarks, custom commands. |
| Media services | `services/media/` | Metadata and thumbnail/preview support. |
| Desktop integration | `platform/`, `dist/linux/` | Linux desktop entry, AppStream metadata, icons, platform helpers. |

## Why C++

Reva Player is written in C++ because the application sits close to native
desktop APIs, Qt Widgets, OpenGL rendering, libmpv, and SQLite. C++ keeps those
integration points direct, avoids an additional runtime layer around the video
path, and gives the project predictable control over packaging, memory
ownership, and event-driven UI performance on Linux desktops.

Qt provides the desktop UI, model/view widgets, dialogs, settings paths, SQL
plugin integration, translations, and platform plugins. libmpv handles playback,
demuxing, decoding, subtitle rendering support, and media properties, so the
application can focus on workflow features instead of maintaining a custom media
engine.

## Application Startup

1. `main.cpp` configures Qt OpenGL defaults and handles `--version` / `-v`.
2. `QApplication` is created with application name `Reva Player`, organization `RevaPlayer`, version `1.0.0`, and desktop file name `io.github.moayad30.revaplayer`.
3. `AppBootstrap` parses `--url` and positional media arguments.
4. SQLite stores are created using `REVAPLAYER_DB_PATH` when provided, otherwise Qt `AppDataLocation`.
5. Controllers are initialized, default settings are seeded, Qt translations are loaded for the selected UI language, and the selected theme is applied.
6. `MainWindow` is shown and startup media is opened once the render host is ready.

## Playback Architecture

- `PlaybackController` is the application-facing playback API.
- `MpvCore` owns the libmpv handle, sends commands, observes properties, and exposes Qt signals.
- `MpvRenderHost` integrates the mpv render context with Qt/OpenGL.
- `MpvEventBridge` converts mpv events into Qt signal flow.
- `MpvPropertyCache` keeps observed mpv state available to the UI.

The backend is libmpv only. There is no alternate VLC/GStreamer backend in repository files.

## Storage Architecture

SQLite is the authoritative persistent store. `SqliteStore` creates and migrates schema version `3`.

Tables verified in `SqliteStore.cpp`:

| Table | Purpose |
| --- | --- |
| `settings` | Key/value settings, including UI, playback, subtitle, playlist, input, and custom JSON values. |
| `window_state` | Main window geometry/state/maximized/fullscreen values. |
| `resume_state` | Per-source resume position and duration. |
| `playback_history` | Recent playback records. |
| `bookmarks` | Per-source bookmarks with title, category, note, position, and timestamps. |
| `custom_commands` | User-defined mpv command scripts. |

Database path:

- Default: `QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)/revaplayer.sqlite`.
- Override: `REVAPLAYER_DB_PATH`.
- A legacy database override fallback is still accepted for migration compatibility; new scripts should use `REVAPLAYER_DB_PATH`.

## Cache Architecture

`ThumbnailService` uses `QStandardPaths::CacheLocation/thumbnails` for disk thumbnail cache. Thumbnail preview uses the bundled resource `resources/mpv/thumbfast.lua` or located script copies when available.

Needs verification: exact cache path differs by Qt platform integration and environment variables such as `XDG_CACHE_HOME`.

## Command-Line Interface

Verified options:

```bash
RevaPlayer --version
RevaPlayer --help
RevaPlayer --url <url>
RevaPlayer <media-file> [more-media-files]
```

## Desktop Integration

Linux integration files are installed by CMake:

- Binary: `${CMAKE_INSTALL_BINDIR}/RevaPlayer`, usually `/usr/bin/RevaPlayer`.
- Desktop file: `${CMAKE_INSTALL_DATAROOTDIR}/applications/io.github.moayad30.revaplayer.desktop`.
- AppStream metadata: `${CMAKE_INSTALL_DATAROOTDIR}/metainfo/io.github.moayad30.revaplayer.metainfo.xml`.
- Icon: `${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps/revaplayer.svg`.
- README, license, and third-party notices:
  `${CMAKE_INSTALL_DATAROOTDIR}/doc/revaplayer/`.

## Build System

- CMake minimum version: `3.24`.
- Language: C++20.
- Default Qt major: 6.
- Optional Qt major: 5 via `-DREVAPLAYER_QT_MAJOR=5`.
- Testing is controlled by `BUILD_TESTING`.
- CPack generator currently defaults to `DEB`.

## Packaging Architecture

- System-library DEB: CPack from `CMakeLists.txt`.
- Bundled/offline DEB: `scripts/build-bundled-deb.sh`.
- AppImage: `scripts/build-appimage.sh`.
- AppImage fallback repack: `scripts/repack-appimage-fallback.sh`.
- Local AppImage install/uninstall: `scripts/install-appimage-local.sh`, `scripts/uninstall-appimage-local.sh`.
- Bundled/offline RPM: `scripts/build-bundled-rpm.sh`.

## Open Questions

- Needs verification: Wayland/X11 behavior across KDE, GNOME, Xfce, Cinnamon, and other desktops.
- Needs verification: AppImage portability across glibc baselines and non-Ubuntu distributions.
