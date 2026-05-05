# QA Checklist

Use this checklist before publishing source builds, AppImage builds, DEB packages, or RPM packages.

Verified against repository files on 2026-04-30. Items that require target machines are marked "Needs verification".

## Build Verification

- [ ] `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` succeeds.
- [ ] `cmake --build build --parallel` succeeds.
- [ ] `ctest --test-dir build --output-on-failure` succeeds.
- [ ] `./build/RevaPlayer --version` prints the expected version.
- [ ] Smoke launch succeeds with `REVAPLAYER_DB_PATH=/tmp/revaplayer-smoke.sqlite`.

## General Release Checklist

- [ ] Version updated in `CMakeLists.txt`.
- [ ] Version output checked in `app/main.cpp`.
- [ ] AppStream release version/date checked in `dist/linux/io.github.moayad30.revaplayer.metainfo.xml`.
- [ ] Changelog or release notes updated if used by the release process.
- [ ] App name is `Reva Player`.
- [ ] Binary name is `RevaPlayer`.
- [ ] Desktop ID is `io.github.moayad30.revaplayer`.
- [ ] No unwanted old branding in user-facing docs or metadata.
- [ ] Icons are present in `resources/icons/`.
- [ ] Desktop file validates with `desktop-file-validate` when available.
- [ ] AppStream metadata validates with `appstreamcli validate --no-net` when available.

## Functional Smoke Tests

- [ ] Open a local video file.
- [ ] Confirm playback starts and the video viewport renders non-blank frames.
- [ ] Pause and resume playback.
- [ ] Seek with the timeline and keyboard shortcuts.
- [ ] Change volume, mute, and unmute.
- [ ] Enter and exit fullscreen.
- [ ] Open a local audio file and confirm playback works without music-library metadata clutter in the playlist.
- [ ] Open multiple files and verify playlist population.
- [ ] Open a media URL supported by mpv.
- [ ] Play/pause, seek, volume, mute, speed.
- [ ] Next/previous playlist item.
- [ ] Subtitle visibility, external subtitle load, style, delay, scale, position, and reset.
- [ ] Audio track and subtitle track selection.
- [ ] Video zoom in/out/reset and panning.
- [ ] Confirm panning does not move the video when zoom is at `1.0x`.
- [ ] Screenshot capture and output path.
- [ ] Bookmark create/delete.
- [ ] Resume playback after app restart.
- [ ] Seek near the beginning of a media file, close/reopen, and confirm the early resume position is preserved.
- [ ] History list updates.
- [ ] Playlist add/remove/keep-selected/clear behavior.
- [ ] Reset progress for one playlist item from the item context menu.
- [ ] Reset progress for the current playlist/list from the playlist settings menu.
- [ ] Playlist Ctrl/Cmd-click and Shift-click multi-select.
- [ ] Playlist drag reorder for one item and multiple selected items.
- [ ] Confirm playlist reorder does not accidentally start playback.
- [ ] Confirm current item tracking is preserved after playlist reorder.
- [ ] Saved folders add/edit/remove.
- [ ] Saved folders move up/down and drag reorder.
- [ ] Confirm saved folder order appears immediately and persists after restart.
- [ ] Folder browser opens a folder, shows playable files, shows subfolders, enters subfolders, goes back, and handles empty/unsupported folders.
- [ ] Saved playlist views/presets if used.
- [ ] Main window state restored when enabled.
- [ ] Preferences dialog opens and settings persist.
- [ ] Show Status Bar disabled, app restarted, file opened, fullscreen toggled, and status bar remains hidden in windowed mode.
- [ ] Clear cache removes generated cache only and keeps settings/history/bookmarks/saved lists.
- [ ] Factory reset is available from Settings only, asks for confirmation, resets local state, and does not delete media files.

## File Dialogs

- [ ] `Ctrl+O` opens file dialog.
- [ ] Open folder dialog works.
- [ ] Subtitle file dialog works.
- [ ] Screenshot directory dialog works.
- [ ] AppImage host file-dialog behavior checked.
- [ ] Flatpak portal behavior checked only if a Flatpak manifest exists.

## Linux Checklist

- [ ] `/usr/bin/RevaPlayer` exists after package install.
- [ ] Desktop entry exists and launches the application.
- [ ] Icon appears in desktop menu.
- [ ] MIME types in desktop file do not break validation.
- [ ] Qt platform plugins are available.
- [ ] Qt SQL SQLite plugin is available.
- [ ] libmpv runtime is available.
- [ ] Wayland launch tested.
- [ ] X11/xcb launch tested.
- [ ] Display sleep inhibition starts during active video playback and is released on pause/stop/error/exit.
- [ ] Uninstall removes package files.
- [ ] Uninstall does not remove user data unless cleanup script is explicitly run.
- [ ] Upgrade preserves SQLite database.

## Package Checklist

- [ ] CPack DEB built and inspected with `dpkg -I` and `dpkg -c`.
- [ ] Bundled DEB built only after `dist/AppDir/usr` exists or a valid `--bundle-source` is supplied.
- [ ] Bundled package build fails if any bundled ELF dependency reports `not found` through `ldd`.
- [ ] AppImage built with `scripts/build-appimage.sh`.
- [ ] AppImage `--version` works.
- [ ] AppImage runs with `APPIMAGE_EXTRACT_AND_RUN=1`.
- [ ] Local AppImage install script works for the current user.
- [ ] Local AppImage uninstall script removes launcher/symlink files.
- [ ] Bundled RPM built with `scripts/build-bundled-rpm.sh`.
- [ ] Bundled RPM inspected and tested on a clean RPM-family target.
- [ ] Flatpak not advertised as a release artifact unless a manifest exists and package tests pass.

## First Release Focus Checklist

- [ ] Smoke test with a disposable database: `REVAPLAYER_DB_PATH=/tmp/revaplayer-smoke.sqlite`.
- [ ] Playback test: video, audio, pause/resume, seek, volume, mute, fullscreen.
- [ ] Playlist test: add, remove, keep selected, clear, multi-select, drag reorder, current item preservation.
- [ ] Folder browser test: root folder, nested folder, back navigation, empty folder, unsupported-only folder, permission error.
- [ ] Subtitles test: external load, visibility, language selection, style, delay, position, reset, persistence.
- [ ] Zoom test: in/out/reset, panning only while zoomed, min/max limits, reset on new file when enabled.
- [ ] Settings persistence test: restart after changing UI, playback, playlist, subtitle, zoom, and folder settings.
- [ ] Status bar test: disabled setting remains authoritative after restart, file open, window changes, and fullscreen toggle.
- [ ] Saved folders reorder test: drag reorder and move up/down persist after restart.
- [ ] Cache clear test: cache cleared, preferences/history/bookmarks/saved lists retained.
- [ ] Factory reset test: settings/history/bookmarks/favorites/saved lists/layouts cleared, defaults reseeded, cache cleared.
- [ ] Packaging sanity test: desktop file, icon, AppStream metadata, binary path, package dependencies.
- [ ] Uninstall cleanup test: package uninstall leaves user data; cleanup script removes local data only when explicitly run.
- [ ] Docs links test: English docs links resolve.

## Post-Build Diagnostics

Use these when a build runs locally but fails on another machine:

```bash
ldd build/RevaPlayer
./build/RevaPlayer --version
QT_QPA_PLATFORM=xcb ./build/RevaPlayer
QT_QPA_PLATFORM=wayland ./build/RevaPlayer
REVAPLAYER_DB_PATH=/tmp/revaplayer-smoke.sqlite ./build/RevaPlayer
```

For package outputs, inspect content before publishing:

```bash
dpkg -I path/to/package.deb
dpkg -c path/to/package.deb
```

## Release Blockers

- [ ] Build failure on the intended release host.
- [ ] Test failure in `ctest`.
- [ ] App cannot initialize libmpv.
- [ ] App cannot create SQLite database.
- [ ] App cannot open a local file through the expected file dialog path.
- [ ] Desktop file or AppStream metadata validation fails.
- [ ] Package uninstall leaves broken launchers.
- [ ] Published docs claim Flatpak is available before that path exists.
- [ ] Published docs claim broad RPM support before clean RPM-family package tests pass.
