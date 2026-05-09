# Compatibility Matrix

This matrix separates repository-verified support from expected or planned feasibility. It is not a benchmark.

Verified against repository files on 2026-04-30.

## Runtime Components

| Component | Status | Notes |
| --- | --- | --- |
| Linux desktop | Verified target | Desktop file, AppStream metadata, icons, DEB/AppImage/RPM scripts exist. |
| Qt 6 | Verified build target | Default CMake path. |
| Qt 5 | Build path exists | Needs runtime/feature verification before release. |
| libmpv | Required | Only playback backend in repository files. |
| SQLite | Required | Used for settings/history/resume/bookmarks/window state. |
| OpenGL | Required | Qt and libmpv render path use OpenGL. |
| Wayland | Partially supported | Qt Wayland dependencies/scripts exist; needs per-desktop testing. |
| X11/xcb | Expected | Qt xcb is a common fallback; needs package/runtime testing. |

## Linux Distributions

| Platform | Current feasibility | Recommended package path |
| --- | --- | --- |
| Debian | High | DEB or AppImage. Needs clean-VM test per release. |
| Ubuntu | High | DEB or AppImage. |
| Linux Mint | High | DEB or AppImage. Needs verification. |
| Pop!_OS | Medium-High | DEB or AppImage. Needs verification. |
| elementary OS | Medium | DEB or AppImage. Portal/theme behavior needs verification. |
| Fedora | Medium | AppImage or bundled RPM after clean target-system testing. |
| openSUSE | Medium | AppImage or bundled RPM after clean target-system testing. |
| RHEL/AlmaLinux/Rocky | Medium-Low | AppImage may be possible; bundled RPM needs older-runtime testing. |
| Arch/Manjaro | Medium | AppImage or source build. Native package not present. |

## Package Formats

| Format | Repository support | Feasibility | Notes |
| --- | --- | --- | --- |
| CPack DEB | Implemented | High | Uses system libraries and CPack. |
| Bundled DEB | Implemented script | Medium-High | Depends on prepared runtime bundle from AppImage payload. |
| AppImage | Implemented script | High | Best current cross-distro test/release path. |
| Bundled RPM | Implemented script | Medium | Needs clean install/upgrade/uninstall tests on RPM-family systems. |
| Flatpak | Not available yet | Future | Needs manifest, portals, permissions, mpv/Qt runtime plan. |

## Desktop Environments

| Environment | Expected status | Notes |
| --- | --- | --- |
| KDE Plasma | High priority | AppImage scripts try KDE platform theme when bundled plugin exists. |
| GNOME | High priority | AppImage scripts try GTK platform theme when bundled plugin exists. |
| Xfce/MATE/Cinnamon | Needs verification | File dialogs and platform themes need testing. |
| Wayland sessions | Needs verification | AppImage wrapper prefers `wayland;xcb` when Wayland is detected. |
| X11 sessions | Expected fallback | Verify xcb plugin availability. |

## Unsupported / Not Confirmed

- Mobile platforms: not targeted.
- Sandboxed Linux package: Flatpak is not implemented yet.
- Automatic updates: not implemented by repository files.

## Practical Recommendation

Start compatibility testing with:

1. Ubuntu LTS or Debian stable for building.
2. AppImage smoke tests on Debian/Ubuntu/Fedora/openSUSE.
3. DEB install/upgrade/uninstall tests on Debian/Ubuntu-family systems.
4. Bundled RPM install/upgrade/uninstall tests on Fedora/openSUSE/RHEL-family systems before advertising it broadly.
5. Flatpak tests after a manifest exists.
