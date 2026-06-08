<p align="center">
  <img src="resources/icons/revaplayer.svg" alt="Reva Player logo" width="112">
</p>

# 🎬 Reva Player

Reva Player is an open-source Linux desktop media player for people who spend
real time with local media: courses, lectures, recordings, playlists,
subtitle-heavy videos, long series, and folders they return to again and again.

It is built with Qt Widgets, libmpv, OpenGL rendering, and SQLite. The goal is a
native Linux player that feels fast and practical, while keeping a useful local
workspace around playback: playlists, saved folders, resume history, bookmarks,
subtitles, screenshots, thumbnails, and per-user settings.

Reva Player is free software licensed under the GNU General Public License,
version 3 or later (`GPL-3.0-or-later`). See [LICENSE](LICENSE).

Repository: [github.com/Moayad30/Reva_Player](https://github.com/Moayad30/Reva_Player)

⭐ If Reva Player is useful to you, please consider giving the repository a star
on GitHub. Stars help more Linux users discover the project and show that there
is real interest in continued development. 🌱🌱

## 🖼️ Preview


<table align="center">
  <tr>
    <td align="center" width="50%">
      <img src="docs/screenshots/main-window.png" alt="Reva Player main playback window interface" width="100%">
      <br><sub>Main Playback Window</sub>
    </td>
    <td align="center" width="50%">
      <img src="docs/screenshots/playlist-workflow.png" alt="Reva Player playlist and folder workflow view" width="100%">
      <br><sub>Playlist Workflow</sub>
    </td>
  </tr>
  
  <tr>
    <td align="center" width="50%">
      <img src="docs/screenshots/bookmarks-history.png" alt="Reva Player bookmarks and history workflow interface" width="100%">
      <br><sub>Bookmarks & History Workflow</sub>
    </td>
    <td align="center" width="50%">
      <img src="docs/screenshots/playlist-workflow-folders.png" alt="Reva Player playlist folders manager view" width="100%">
      <br><sub>Playlist Workflow</sub>
    </td>
  </tr>

  <tr>
    <td align="center" width="50%">
      <img src="docs/screenshots/playlist-workflow-graphite.png" alt="Reva Player UI in Gray Theme with Graphite accent color" width="100%">
      <br><sub>Gray Theme (Graphite Accent)</sub>
    </td>
    <td align="center" width="50%">
      <img src="docs/screenshots/playlist-workflow-orange.png" alt="Reva Player UI in Gray Theme with Orange accent color" width="100%">
      <br><sub>Gray Theme (Orange Accent)</sub>
    </td>
  </tr>

  <tr>
    <td align="center" width="50%">
      <img src="docs/screenshots/settings.png" alt="Reva Player settings window showcasing Dark Theme" width="100%">
      <br><sub>Gray Theme (Settings View)</sub>
    </td>
    <td align="center" width="50%">
      <img src="docs/screenshots/Setting-Dayt-theme.png" alt="Reva Player settings window showcasing Day Light Theme" width="100%">
      <br><sub>Day Theme / Light Mode</sub>
    </td>
  </tr>
</table>

## 📦 Download

Ready-to-use Linux packages are published from
[GitHub Releases](https://github.com/Moayad30/Reva_Player/releases) when
available.

Typical release assets may include:

- 📦 AppImage for portable Linux testing.
- 🧩 DEB for Debian/Ubuntu-family distributions.
- 🧰 RPM for RPM-family distributions.

Download the package that fits your system, open your media, and keep your
playlists, progress, bookmarks, and subtitles organized in one local workspace.

## ⚡ Quick Start

### AppImage

```bash
chmod +x RevaPlayer-v*.AppImage
./RevaPlayer-v*.AppImage
```

### DEB

```bash
sudo apt install ./reva-player_*.deb
RevaPlayer
```

### RPM

```bash
sudo dnf install ./reva-player-*.rpm
RevaPlayer
```

For package-specific notes, see [docs/packaging/README.md](docs/packaging/README.md).

## 💡 Why Reva Player

Most media players are excellent at opening a file and getting out of the way.
Reva Player is for the sessions that need more structure: a lecture series,
course folder, playlist, subtitle-heavy archive, long recording, or any local
media collection where remembering context matters.

- 🧭 **A playback workspace, not just a video window.** Keep playlists, saved
  folders, history, bookmarks, thumbnails, screenshots, and settings together in
  one local desktop app.
- 🔁 **Built around repeated viewing.** Continue where you left off, move through
  folders naturally, and keep useful viewing context across sessions.
- 📊 **Track progress across a playlist or series.** See completion across the
  current list, including watched time, full-playlist progress percentage, and
  automatically calculated total playlist duration when media metadata is
  available.
- 💬 **Serious subtitle controls.** Adjust visibility, timing, scale, position,
  language preferences, font behavior, and styling options from the app.
- 🔖 **Bookmarks for real moments.** Save timestamps with titles, categories, and
  notes so important scenes, lessons, or references are easy to return to.
- 🎛️ **Power-user playback controls.** Use libmpv-backed playback with speed,
  volume, mute, repeat, seeking, track selection, aspect ratio, zoom, and pan.
- 🔒 **Local-first by design.** No account, no cloud library, no media server
  requirement. Your playback state is stored locally in SQLite.
- 🖥️ **Native Linux distribution path.** The repository includes Linux desktop
  metadata and packaging paths for AppImage, DEB, and RPM-family systems.

## 🎯 Designed For

Reva Player is especially useful when playback is part of a larger workflow,
not just a one-off file open.

- 🎓 Course and lecture playlists where progress across the whole series matters.
- ⏱️ Long recordings that need reliable resume positions and repeat viewing.
- 🗂️ Local video folders that should stay organized between sessions.
- 💬 Subtitle-heavy media where timing, size, position, and language preferences
  need quick adjustment.
- 🔖 Review sessions where bookmarks, notes, screenshots, and thumbnails help you
  return to important moments.
- 🔒 Privacy-conscious local media use where an account, cloud library, or media
  server would be unnecessary.

## ✨ Key Features

- 📊 **Playlist intelligence.** Reva Player can calculate the total duration of
  the current playlist automatically when metadata is available, and it shows
  progress across the playlist as a percentage.
- 🔁 **Resume and continuity.** Playback history, watched duration, resume
  positions, window state, and preferences are kept locally so repeated sessions
  can continue with less setup.
- 🗂️ **Folder-oriented workflow.** Open files or folders, keep saved folders, and
  return to media collections without rebuilding the same context every time.
- 🔖 **Bookmarks with context.** Save meaningful timestamps with titles,
  categories, and notes for lessons, scenes, references, or review points.
- 💬 **Subtitle control.** Load external subtitles, switch tracks, adjust timing,
  scale, position, language defaults, and styling options from the app.
- 🖼️ **Viewing tools.** Use screenshots, thumbnails, zoom, pan, aspect ratio,
  speed, repeat, volume, mute, seeking, and track selection in one native Linux
  interface.
- 💾 **Local persistence.** Settings and playback state are stored in SQLite using
  standard Qt/XDG application paths.

## 🧭 What Makes It Different

Reva Player uses libmpv for playback, but it adds a local workspace around that
backend. The app is designed to remember what matters around the video: the
playlist, where you stopped, how much of the list is done, which folders you
return to, which subtitle behavior you prefer, and which moments you saved.

That makes it a practical fit for long-form local media, learning material,
archives, and repeated review workflows where the surrounding context is as
important as the playback engine.

## 📋 Feature Highlights

| Area | Purpose |
| --- | --- |
| 🎬 Playback | Local video/audio playback through libmpv, plus media URLs supported by mpv. |
| 🗂️ Library workflow | File and folder playlists, saved folders, current-list context, progress percentage across a playlist or series, and automatic total playlist duration calculation. |
| 🔁 Continuity | Local history, resume positions, watched duration, total known duration, window state, and playback preferences across sessions. |
| 🔖 Bookmarks | Timestamped notes with title and category fields for returning to meaningful moments. |
| 💬 Subtitles | External subtitle loading, track selection, timing, style, scale, position, and language controls. |
| 🖼️ Viewing tools | Zoom, pan, aspect ratio, speed, repeat, screenshots, thumbnails, and configurable capture path. |
| 🖥️ Desktop integration | Linux desktop entry, AppStream metadata, scalable icon, and package-friendly install layout. |

## 🖥️ Built For Linux

Reva Player is focused exclusively on Linux desktop systems.

| Platform/package | Status |
| --- | --- |
| Linux source code | ✅ Supported |
| AppImage | ✅ Available |
| DEB | ✅ Available |
| RPM | ✅ Available |
| Flatpak | 🚧 Not available yet |

Compatibility still depends on the target distribution, desktop environment,
GPU/OpenGL stack, audio stack, codecs, Qt plugins, and bundled or system libmpv
runtime. See [docs/COMPATIBILITY_MATRIX.md](docs/COMPATIBILITY_MATRIX.md) for
current packaging notes.

## 🛠️ Build From Source

Developer and packager build instructions are documented in
[docs/BUILDING.md](docs/BUILDING.md). The project uses CMake and C++20, with Qt
6 as the default Qt target and Qt 5 available through the documented CMake
option.

Basic development build:

```bash
cmake -S . -B build
cmake --build build --parallel
./build/RevaPlayer
```

Main build requirements include CMake, a C++20 compiler, Qt Widgets, Qt Sql,
Qt DBus, Qt OpenGL modules, libmpv development files, and the Qt SQLite driver.

## 🔒 Local Data

Reva Player is local-first. It does not require an online account, cloud
library, or remote media server, and it does not need to upload your local media
library to be useful.

Stored local data can include:

- ⚙️ Settings.
- 🕘 Playback history.
- 🔁 Resume positions.
- 🔖 Bookmarks.
- 🪟 Window/layout state.
- 🗂️ Saved folders and playlist-related preferences.
- 🖼️ Thumbnail/cache data.

Package uninstall and local-data cleanup are separate operations. Removing the
application should not remove playback history, settings, bookmarks, resume
positions, or cache unless cleanup is explicitly requested.

For storage paths and cleanup notes, see
[docs/reference/STORAGE_PATHS.md](docs/reference/STORAGE_PATHS.md) and
[docs/reference/PURGE_LOCAL_DATA.md](docs/reference/PURGE_LOCAL_DATA.md).

## 🚧 Project Status

Reva Player is focused on Linux desktop use and currently provides source code
and release packaging paths for AppImage, DEB, and RPM-family packages.
Flatpak support is not available yet.

The project is still shaped by real Linux packaging and runtime testing. Codec
support, subtitle behavior, GPU/OpenGL rendering, and desktop integration can
depend on the bundled or system Qt, libmpv, FFmpeg, platform plugins, and target
distribution.

## 📝 Release Notes

- 🧱 [Release build notes](docs/packaging/RELEASE_BUILDS.md)
- 📦 [Packaging overview](docs/packaging/README.md)

## 📚 Documentation

- 📖 [User Guide](docs/USER_GUIDE.md)
- 📦 [Packaging](docs/packaging/README.md)
- 🧱 [Architecture](docs/ARCHITECTURE.md)
- 📄 [Third-party notices](THIRD_PARTY_NOTICES.md)

## 🤝 Contributing

Reva Player is an open-source Linux desktop project. Contributions, feedback,
testing, and practical improvements are welcome.

## 📄 License

Reva Player is licensed under `GPL-3.0-or-later`.

Binary release packages may include or depend on third-party runtime components
such as Qt, libmpv, FFmpeg, SQLite, D-Bus, and desktop integration libraries.
See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
