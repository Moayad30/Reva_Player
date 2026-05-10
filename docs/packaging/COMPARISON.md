# Packaging Comparison

Ratings are practical estimates for this repository, not performance benchmarks.

| Format | Platform | Target users | Install ease | Update story | Performance | Portability | System integration | Sandboxing | Build difficulty | Maintenance burden | Best use case | Current feasibility | Priority |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| DEB | Debian/Ubuntu family | Normal Linux users | High | Medium | High | Low-Medium | High | None | Medium | Medium | Native package for deb-based systems | High | High |
| Bundled RPM | Fedora/openSUSE/RHEL family | Normal Linux users | High if repo exists | Medium | High | Low-Medium | High | None | Medium | Medium-High | Native package for RPM systems | Medium, scripted | Medium |
| AppImage | Most Linux desktops | Testers, broad Linux users | High | Low-Medium | High | High | Medium | None | Medium | Medium | Fast beta/public artifact | High | Highest |
| Flatpak | Most Linux desktops | Store-style Linux users | High | High | Medium-High | High | Medium-High | Yes | High | Medium-High | Future sandboxed distribution | Not available yet | Future |

## Best Starting Option

AppImage is the best first public/testing artifact because the repository already has build, repack, local install, and uninstall scripts. It reaches users outside Debian/Ubuntu without requiring native package work for every distribution.

## Best For Linux Users

DEB is best for Debian/Ubuntu-family users because it integrates naturally with system package management. Bundled RPM is the native option for RPM-family users after clean target-system checks. AppImage remains the best cross-distro fallback.

## Best For Experimental Builds

AppImage is best for beta builds and one-off test artifacts. It avoids per-distro package work and can bundle runtime dependencies.

## Future Linux Packaging

Flatpak could become useful for sandboxing, update infrastructure, and possible
Flathub delivery. It is not a current release format because no manifest,
permission model, runtime strategy, or portal/file-access testing exists in the
public packaging path yet.

## CI/CD Needs

- Build matrix for Qt 6 and optional Qt 5.
- `ctest` on Linux.
- AppImage build artifact.
- Bundled DEB artifact.
- Package install smoke tests in clean containers/VMs where possible.
- Bundled RPM build artifact.
- Later: Flatpak build after a manifest exists.

## Signing Needs

- DEB repository publishing should use repository signing.
- RPM repository publishing should use package/repository signing.
- AppImage signing is optional but useful for release integrity.
- Flatpak/Flathub has repository signing through Flatpak infrastructure if that path is added later.

## Needs Deeper Testing

- Wayland/X11 behavior.
- Qt platform plugins inside bundled packages.
- libmpv codec/subtitle behavior.
- SQLite migration and upgrade.
- Non-ASCII paths.
- File dialogs on KDE/GNOME/Xfce/Cinnamon.
- Uninstall/purge behavior.
