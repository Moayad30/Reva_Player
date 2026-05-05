# Reva Player Documentation

Reva Player is a Linux desktop media player built from this repository with C++20, Qt Widgets, libmpv, OpenGL rendering, and SQLite-backed local state. The application is a video player first, with playlist, subtitles, bookmarks, history, saved folders, screenshots, configurable UI behavior, and local packaging support.

Status: verified against repository files on 2026-04-30. Items marked "Needs verification" are not proven by current repository files or require testing on target systems.

## Documentation Map

| File | Audience | Purpose |
| --- | --- | --- |
| [PRODUCT_OVERVIEW.md](PRODUCT_OVERVIEW.md) | Users, maintainers | Product scope, capabilities, limits, and current behavior. |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Developers | Code layout, runtime components, storage, and integration points. |
| [BUILDING.md](BUILDING.md) | Developers, packagers | Verified build, install, test, and packaging commands. |
| [RUNNING_BINARY.md](RUNNING_BINARY.md) | Users, testers | Running the local build and diagnosing runtime dependency failures. |
| [COMPATIBILITY_MATRIX.md](COMPATIBILITY_MATRIX.md) | Maintainers, packagers | Current platform and package feasibility. |
| [QA_CHECKLIST.md](QA_CHECKLIST.md) | Testers, maintainers | Practical pre-release and smoke-test checklist. |
| [USER_GUIDE.md](USER_GUIDE.md) | Users | Existing user-facing guide. Needs periodic verification as UI changes. |
| [FILE_DIALOGS.md](FILE_DIALOGS.md) | Developers, packagers | Host/native file-dialog behavior. |
| [packaging/README.md](packaging/README.md) | Packagers | Entry point for DEB, RPM, AppImage, and Flatpak status notes. |
| [reference/README.md](reference/README.md) | Users, developers | Reference index for storage paths, uninstall, and cleanup. |

## Packaging Documentation

The active packaging documentation is under [packaging/](packaging/README.md):

- [DEB](packaging/DEB.md)
- [RPM](packaging/RPM.md)
- [AppImage](packaging/APPIMAGE.md)
- [Comparison](packaging/COMPARISON.md)
- [Recommendation](packaging/RECOMMENDATION.md)
- [Checklist](packaging/CHECKLIST.md)

Flatpak is mentioned in the public packaging status, but it is not documented as
a release path until a manifest exists.

## Repository-Verified Facts

- App display name: `Reva Player`.
- Executable target: `RevaPlayer`.
- App ID / desktop ID stem: `io.github.moayad30.revaplayer`.
- Current project version in CMake: `1.0.0`.
- UI framework: Qt Widgets, with Qt 6 default and Qt 5 selectable through `REVAPLAYER_QT_MAJOR`.
- Playback backend: libmpv through `infrastructure/mpv/`.
- Persistent store: SQLite through `infrastructure/storage/SqliteStore`.
- Runtime override for the database path: `REVAPLAYER_DB_PATH`.
- A legacy database environment fallback still exists in code for migration compatibility, but new docs and scripts should use only `REVAPLAYER_DB_PATH`.

## Documentation Notes

- Build commands are documented only when they exist in this repository.
- AppImage, bundled/offline DEB, and bundled/offline RPM build paths are supported by repository scripts. Flatpak is not implemented yet.
- The project contains both system-library DEB support through CPack and bundled/offline DEB support through `scripts/build-bundled-deb.sh`.
- AppImage support is implemented through `scripts/build-appimage.sh`, with fallback repacking through `scripts/repack-appimage-fallback.sh`.
- Bundled RPM support is implemented through `scripts/build-bundled-rpm.sh`.
