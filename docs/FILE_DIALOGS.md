# Linux File Dialogs

Reva Player uses Qt file-dialog flows. Packaging can affect whether users see native desktop dialogs, Qt fallback dialogs, or host helper dialogs.

Verified repository files:

- `ui/FileDialogUtils.cpp`
- `scripts/build-appimage.sh`
- `scripts/repack-appimage-fallback.sh`
- `scripts/build-bundled-deb.sh`

## Policy

- Prefer native or desktop-integrated file dialogs when available.
- Do not force Qt's non-native dialog unless a specific bug requires it.
- Test file opening on KDE, GNOME, and at least one non-primary desktop before publishing package claims.

## AppImage Notes

The AppImage wrapper configures Qt plugin search paths and may set platform/theme variables based on the host desktop and bundled plugins.

Important risks:

- Mixing host Qt plugins with bundled Qt libraries can break dialogs.
- KDE, GNOME, and other desktops may behave differently.
- Missing platform theme plugins can fall back to less integrated dialogs.

## Bundled DEB Notes

The bundled DEB wrapper configures runtime library/plugin paths similarly to the AppImage wrapper. File dialogs should be tested independently from the system-library DEB.

## QA Cases

- Open media file.
- Open multiple media files.
- Open folder.
- Open external subtitle.
- Choose screenshot directory.
- Export bookmark/quiz files if that feature is enabled in the current UI.

## Success Criteria

- Dialog opens reliably.
- Selected file/folder paths are readable by the app.
- Non-ASCII paths work.
- AppImage/bundled packages do not crash due to Qt plugin conflicts.
- Fallback dialog is acceptable only when native integration is unavailable.
