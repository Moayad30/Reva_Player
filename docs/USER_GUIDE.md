# Reva Player User Guide

This guide describes user-facing behavior verified from repository structure and controller/UI code. Some detailed UI labels may change; verify against the running build before publishing screenshots or tutorials.

## Quick Start

- Open media files from the UI or pass file paths to `RevaPlayer`.
- Open media URLs with the `--url` command-line option or the UI action when available.
- Use playback controls for play/pause, seek, volume, mute, speed, repeat, and playlist navigation.
- Open Preferences/Settings to configure playback, subtitles, playlist behavior, mouse behavior, appearance, and shortcuts.

## Main Areas

| Area | Purpose |
| --- | --- |
| Video viewport | Displays libmpv-rendered video and playback overlays. |
| Control bar | Provides common playback controls and timeline interaction. |
| Playlist panel | Shows current media list and folder-loaded items. |
| Details/side panels | Host supporting views such as bookmarks, tracks, history, scenes, and media information where enabled by the UI. |
| Settings dialog | Stores preferences in the local SQLite database. |

## Playback

Reva Player uses libmpv as its playback backend. Supported codecs, containers, streams, tracks, and subtitles depend on the libmpv/FFmpeg stack available to the build or package.

Common actions:

- Open one or more local files.
- Open a URL supported by mpv.
- Seek by configured short/long seek steps.
- Change speed.
- Change volume or mute.
- Select playlist items, chapters, audio tracks, and subtitle tracks when available.

## Playlist And Folders

The application supports playlists, folder-loaded media, natural sorting options, auto-follow behavior, and saved folder/list workflows. Saved folder and view state is persisted through SQLite settings/custom values.

Playlist progress is shown from stored resume/history data and can update while the current item is playing. Progress can be reset for a selected playlist item from its context menu, or for the current playlist/list from the playlist settings menu.

Needs verification before user-facing release notes: exact menu labels and all supported playlist view presets.

## History, Resume, And Bookmarks

When enabled, Reva Player stores:

- Resume state per media source.
- Playback history.
- Bookmarks with title, category, note, and timestamp.
- Window state.

Explicit seeks near the beginning of a media file are preserved when playback progress is force-saved on pause/stop/exit. A forced save at position `0` clears stored progress for that media source.

These values live in the local SQLite database. See [reference/STORAGE_PATHS.md](reference/STORAGE_PATHS.md).

## Subtitles

Subtitle behavior combines libmpv support with application settings. Verified settings include:

- Subtitle visibility.
- Scale and position.
- Font family and size.
- Preferred languages.
- External subtitle preference.
- Local matching subtitle auto-load.
- Sync small/large step settings.
- ASS override and style-related settings.

Codec/rendering behavior still depends on libmpv/libass support in the runtime package.

## Screenshots

Screenshot file names are generated from a timestamp plus a sanitized media label. The screenshot directory is configurable. If no directory is configured, verify the actual fallback behavior in the current UI before publishing user instructions.

## Settings And Storage

Settings are stored in SQLite, not in a plain text config file. To test without touching your normal profile:

```bash
REVAPLAYER_DB_PATH=/tmp/revaplayer-test.sqlite RevaPlayer
```

For cleanup behavior, see [reference/PURGE_LOCAL_DATA.md](reference/PURGE_LOCAL_DATA.md).

## Troubleshooting

| Problem | Action |
| --- | --- |
| App fails to start | Run from terminal and check Qt/libmpv/plugin errors. |
| Video does not render | Try X11 vs Wayland with `QT_QPA_PLATFORM=xcb` or `QT_QPA_PLATFORM=wayland`. |
| Settings/history do not persist | Check database path and write permissions. |
| Subtitles do not load | Verify file path, subtitle format, and libmpv support. |

## Permissions And Data Access

- Reva Player does not require `sudo` and does not require full-disk-access permission.
- The app reads only media files and folders explicitly selected by the user through open dialogs or drag/drop.
- Reva Player writes local app data only: settings, history, bookmarks, resume state, and related local UI state.
- Local data is stored in the app-local storage paths documented in `docs/reference/STORAGE_PATHS.md`.
- Reva Player does not read `~/.config/mpv` by default (isolated mpv behavior is used unless changed in source code).
| File dialog does not look native | See [FILE_DIALOGS.md](FILE_DIALOGS.md). |
