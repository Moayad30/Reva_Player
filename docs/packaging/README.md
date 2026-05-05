# Packaging Overview

Reva Player currently has real Linux packaging support for AppImage, bundled DEB, and bundled RPM. Flatpak is not available yet because the repository does not include a Flatpak manifest.

Verified against repository files on 2026-04-30.

## Formats

| Format | Meaning | Current repository status | Use when |
| --- | --- | --- | --- |
| DEB | Native Debian package | Supported through CPack; bundled script also exists | Debian, Ubuntu, Linux Mint, Pop!_OS, elementary OS. |
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

Flatpak should not be advertised as a release artifact until the project has a
manifest, runtime strategy, portal permissions, and target-system tests.
