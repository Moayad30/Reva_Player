# RPM Packaging

RPM is the native package family for Fedora, openSUSE, RHEL, AlmaLinux, and Rocky Linux. Reva Player includes a bundled RPM packaging script for offline-friendly release artifacts.

Status: Implemented through `scripts/build-bundled-rpm.sh`; still needs target-system verification on clean RPM-family installations.

## Target Systems

- Fedora
- openSUSE
- RHEL
- AlmaLinux
- Rocky Linux

## What Still Needs Verification

- Clean install/upgrade/uninstall tests on Fedora, openSUSE, and RHEL-family systems.
- Desktop integration behavior across GNOME, KDE Plasma, and Wayland/X11 sessions.
- GPU/OpenGL, audio, and codec behavior on real target hardware.
- CI or VM validation.

## Differences From DEB

- Dependency package names differ.
- RPM macros and filesystem conventions differ.
- Fedora/openSUSE/RHEL-family packaging policies are not identical.
- RHEL-family systems may have older libraries than Fedora.
- AppStream and desktop metadata still apply but validation tooling and package policies may differ.

## Bundled Package Layout

The bundled RPM layout is:

- `/opt/revaplayer/bin/RevaPlayer`
- `/opt/revaplayer/bin/RevaPlayer.bin`
- `/opt/revaplayer/lib`
- `/opt/revaplayer/plugins`
- `/opt/revaplayer/translations`
- `/usr/bin/RevaPlayer`
- `/usr/bin/revaplayer`
- `/usr/share/applications/io.github.moayad30.revaplayer.desktop`
- `/usr/share/metainfo/io.github.moayad30.revaplayer.metainfo.xml`
- `/usr/share/icons/hicolor/scalable/apps/revaplayer.svg`
- `/usr/share/doc/revaplayer/README.md`
- `/usr/share/doc/revaplayer/LICENSE`
- `/usr/share/doc/revaplayer/THIRD_PARTY_NOTICES.md`

## mpv And Runtime Dependencies

The release RPM bundles Qt, Qt DBus when available, libmpv, FFmpeg/media runtime libraries, Qt platform plugins, Qt SQLite plugin, Qt translations, and desktop metadata under `/opt/revaplayer`. It still relies on host kernel, glibc, GPU/GL stack, audio stack, and desktop services.

The bundled RPM script verifies bundled ELF dependencies with `ldd` when the
tool is available and stops the build if any dependency is unresolved.

Needs verification: package availability and names for each target distribution.

## Build Command

```bash
scripts/build-bundled-rpm.sh
```

## Uninstall And Upgrade

Expected behavior:

- RPM uninstall removes package files only.
- User data under XDG/Qt paths remains.
- Upgrade preserves SQLite database.

## Difficulty And Feasibility

| Factor | Rating | Notes |
| --- | --- | --- |
| Performance | High | Native package. |
| User convenience | Medium | Good on RPM systems, but repo/signing matters. |
| Maintenance burden | Medium-High | Multiple RPM families differ. |
| Build difficulty | Medium | Bundled script exists; clean target validation is still required. |
| Current feasibility | Medium | Source is portable CMake/Qt and bundled RPM output is scripted. |

## Recommendation

Use bundled RPM as an advanced Linux artifact after AppImage and DEB checks pass. Do not advertise broad RPM-family support until clean install, upgrade, uninstall, desktop integration, and playback checks pass on Fedora, openSUSE, and the intended RHEL-family baseline.
