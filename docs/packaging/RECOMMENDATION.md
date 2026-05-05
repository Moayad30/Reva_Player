# Packaging Recommendation

Recommendation is based on repository state on 2026-04-30.

## Phase Plan

| Option | Priority | Reason | Missing requirements | Risk | Recommendation |
| --- | --- | --- | --- | --- | --- |
| AppImage | Phase 1 | Existing scripts and broad Linux reach | Clean multi-distro testing, runtime baseline validation | Medium | Use as first public/beta artifact. |
| DEB | Phase 1 | CPack and bundled DEB paths already exist | Clean install/upgrade/uninstall tests | Medium | Use for Debian/Ubuntu-family users. |
| Bundled DEB | Phase 1.5 | Offline-friendly and scripted | Reliable AppImage/AppDir runtime payload | Medium-High | Keep as advanced artifact after AppImage validation. |
| Bundled RPM | Phase 1.5 | RPM-family users and scripted output | Clean Fedora/openSUSE/RHEL-family install, upgrade, uninstall, and runtime tests | Medium-High | Publish only after target-system checks. |
| Flatpak | Future | Sandboxed Linux distribution and possible Flathub delivery | Manifest, permissions, mpv runtime plan, portal tests | High | Mention as unavailable; do not publish yet. |

## Why This Order

AppImage, DEB, and bundled RPM are represented by repository files. Flatpak is only a future Linux packaging path today. Starting with implemented Linux paths reduces release risk and produces useful user feedback before expanding maintenance scope.

## Fastest Publishable Format

AppImage is the fastest format to publish because `scripts/build-appimage.sh` exists and produces a single artifact. It still needs target-distro testing before being called stable.

## Best User Format Today

DEB is best for Debian/Ubuntu-family users. Bundled RPM is the native option for RPM-family users after clean install testing. AppImage is the safest fallback for other Linux desktops.

## Lowest Risk

Source build and CPack DEB on the same distribution used for development are lowest risk. For public distribution, AppImage plus DEB is the best risk/reach balance; bundled RPM can follow after clean RPM-family verification.

## Before Official Release

- Run `ctest`.
- Build AppImage, DEB, and RPM artifacts when publishing all Linux formats.
- Test clean install on at least Debian and Ubuntu.
- Test AppImage on at least one DEB and one RPM distribution.
- Test bundled RPM on at least Fedora and one additional RPM-family target.
- Verify desktop file, icon, AppStream metadata, file dialogs, playback, subtitles, cache, and uninstall.
- Confirm no unwanted old branding in docs, metadata, scripts, and package names.
