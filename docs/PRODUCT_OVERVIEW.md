# Product Overview

Reva Player is a desktop video player focused on local playback workflows on Linux. It is built with Qt Widgets for the interface, libmpv for media playback, OpenGL-backed rendering, and SQLite for persistent local state.

This is not a generic media library manager, streaming service client, or cloud-sync application. It is primarily a local video player with dense controls, playlist management, subtitles, bookmarks, history/resume, saved folders, screenshots, and configurable interface behavior.

## Target Users

- Linux desktop users who want a native media player with rich controls.
- Users who organize local video folders, courses, playlists, and repeated viewing sessions.
- Users who need subtitle controls, seek/zoom controls, bookmarks, and resume history.
- Maintainers and packagers who need a conventional Qt/libmpv desktop application that can be packaged for Linux.

## Verified Core Features

| Area | Current behavior |
| --- | --- |
| Video playback | Uses libmpv through `PlaybackController`, `MpvCore`, and `MpvRenderHost`. |
| Audio playback | Uses libmpv audio output and exposes volume, mute, delay, and track-selection controls. |
| Playlist | Internal playlist controller plus mpv playlist synchronization, folder loading behavior, progress tracking, and aggregate watched/total duration display. |
| Saved folders | Persisted through custom SQLite settings keys from the main window logic. |
| Settings | Seeded and read through `SettingsController` and SQLite. |
| Local storage | SQLite database at Qt `AppDataLocation`, unless `REVAPLAYER_DB_PATH` is set. |
| Cache | Thumbnail cache under Qt `CacheLocation` in a `thumbnails` subdirectory. |
| Subtitles | libmpv subtitle loading plus application-level subtitle defaults and style controls. |
| Zoom | Video zoom, pan, reset, wheel behavior, and per-file/session policy settings exist in `SettingsController`. |
| Folder browser | Main window code includes folder browsing, saved folders, and natural sorting options. |
| Window state | Geometry/state/maximized/fullscreen persisted in SQLite table `window_state`. |
| Screenshots | Screenshot path generation uses timestamp plus sanitized media label; directory is configurable. |

## What Makes It Different

Reva Player is closer to a power-user local playback workspace than a minimal video player. The repository shows emphasis on:

- Persistent local state for resume, history, bookmarks, settings, and shortcuts.
- Dense UI panels for playlist, details, bookmarks, scene browsing, saved folders, and settings.
- Playlist and series progress context, including completed item counts and watched/total known duration.
- libmpv as the media backend instead of a custom decoder.
- Linux desktop integration through `.desktop`, AppStream metadata, icons, CPack DEB, AppImage scripts, bundled DEB/RPM scripts, and cleanup scripts.

## Non-Goals

- No repository-verified cloud sync.
- No repository-verified media server mode.
- No repository-verified streaming-service account integration.
- No promise of codec support beyond what the linked/bundled libmpv and FFmpeg stack provides.

## Design Philosophy

The application delegates playback, demuxing, decoding, subtitle rendering support, and media properties to libmpv. Reva Player adds user workflow features around that backend: interface state, playlist experience, folder-oriented navigation, saved settings, bookmarks, screenshots, and packaging.

This keeps playback behavior aligned with mpv while allowing a more structured desktop UI than raw mpv.

## Current Limits

- Linux is the only platform with repository packaging assets.
- Runtime behavior depends on installed or bundled Qt, libmpv, OpenGL, audio, and platform plugins.
- AppImage, bundled DEB, and bundled RPM scripts require external tools and a compatible build host.
- Bundled RPM still needs Fedora/openSUSE/RHEL-family target-system testing.
- GitHub Actions CI exists for Ubuntu Qt 6 and Debian Bookworm builds/tests.

## Components

- `app/`: application entry point and bootstrap.
- `ui/`: Qt Widgets windows, dialogs, viewport, controls, and file-dialog utilities.
- `application/`: controllers for playback, playlist, settings, history, bookmarks, snapshots, theme, and language.
- `infrastructure/mpv/`: libmpv handle, events, property cache, and render host.
- `infrastructure/storage/`: SQLite storage and schema migration.
- `services/media/`: thumbnail and metadata services.
- `platform/`: desktop integration helpers.
- `dist/linux/`: desktop entry and AppStream metadata.
- `scripts/`: packaging, install, uninstall, cleanup, and branding-audit helpers.

## In Development / Needs Verification

- Exact distro compatibility matrix needs real installation tests per release.
- RPM support needs clean Fedora/openSUSE/RHEL-family install, upgrade, uninstall, and runtime testing.
- Automatic update behavior is not implemented for packages in repository files.
