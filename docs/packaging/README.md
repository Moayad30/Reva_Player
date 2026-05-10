# Packaging Overview

Reva Player has Linux packaging support for AppImage, bundled DEB, and bundled RPM. Flatpak is not available yet because the repository does not include a Flatpak manifest.

Verified against repository files on 2026-05-10.

## Formats

| Format | Meaning | Current repository status | Use when |
| --- | --- | --- | --- |
| DEB | Native Debian package | Supported through bundled release script; CPack system mode is available for comparison | Debian, Ubuntu, Linux Mint, Pop!_OS, elementary OS. |
| RPM | Native RPM package | Supported through bundled `rpmbuild` script | Fedora/openSUSE/RHEL-family testing when AppImage is not preferred. |
| AppImage | Portable Linux application image | Supported through scripts | Fast cross-distro testing and distribution. |
| Flatpak | Sandboxed Linux package | Not available yet | Future Linux packaging path after a manifest and portal/runtime testing exist. |

## Runtime Components To Verify

Every package must provide or depend on:

- `RevaPlayer` executable.
- Qt Widgets runtime and platform plugins.
- Qt SQL SQLite plugin.
- Qt DBus runtime for Linux desktop inhibition integration.
- libmpv runtime and its media stack.
- OpenGL-capable runtime environment.
- Desktop entry and icon on Linux.
- Writable application data and cache locations.

## Detailed Pages

- [DEB.md](DEB.md)
- [RPM.md](RPM.md)
- [APPIMAGE.md](APPIMAGE.md)
- [COMPARISON.md](COMPARISON.md)
- [RECOMMENDATION.md](RECOMMENDATION.md)
- [CHECKLIST.md](CHECKLIST.md)

## Current Recommendation

Use AppImage first for broad Linux testing, DEB for Debian/Ubuntu-family users, and RPM for RPM-family users after clean target-system checks.

The v1.0.0 native package scripts bundle Qt/libmpv/media runtime files that are
stable enough to ship with the application, while graphics, audio, desktop
session, X11/Wayland, DBus, and other host-sensitive libraries are left to the
target package manager through package dependencies.

Flatpak should not be advertised as a release artifact until the project has a
manifest, runtime strategy, portal permissions, and target-system tests.
