# Packaging Checklist

Use this checklist for every release artifact.

## General Release Checklist

- [ ] Version updated in `CMakeLists.txt`.
- [ ] `RevaPlayer --version` output updated if version changed.
- [ ] AppStream release entry checked.
- [ ] Changelog/release notes updated if used.
- [ ] App name verified: `Reva Player`.
- [ ] Binary name verified: `RevaPlayer`.
- [ ] App ID verified: `io.github.moayad30.revaplayer`.
- [ ] Branding verified.
- [ ] No unwanted old names in user-facing files.
- [ ] Icons correct.
- [ ] Settings migration checked.
- [ ] Storage paths checked.
- [ ] Cache/reset behavior checked.
- [ ] Subtitles checked.
- [ ] Playlist checked.
- [ ] Saved folders checked.
- [ ] Folder browser checked.
- [ ] Video playback checked.
- [ ] Audio playback checked.
- [ ] Zoom checked.
- [ ] File dialogs checked.
- [ ] Window state checked.

## Local Build Notes

- [x] `desktop-file-validate` passed.
- [x] `appstreamcli validate --no-net` passed.
- [x] Release build and CTest passed locally.
- [x] AppImage fallback build produced a versioned `Reva-Player-<version>-x86_64.AppImage`.
- [x] Bundled DEB build produced a versioned `revaplayer_<version>_amd64.deb`.
- [x] Bundled RPM build path exists through `scripts/build-bundled-rpm.sh`.
- [x] AppImage `--version` passed with `APPIMAGE_EXTRACT_AND_RUN=1`.
- [x] Bundled DEB `--version` passed from extracted package contents.
- [x] AppImage and build-tree binary launched with temporary HOME under `QT_QPA_PLATFORM=minimal` until timeout.
- [ ] Direct AppImage FUSE mount was not available on this machine.
- [ ] Real video/audio/subtitle playback still needs a graphical desktop/manual media test.
- [ ] Clean Debian/Ubuntu VM install, upgrade, and uninstall still need target-system verification.

## Linux Checklist

- [ ] Desktop file installed.
- [ ] Icon installed.
- [ ] AppStream metadata installed.
- [ ] Executable permissions correct.
- [ ] Runtime dependencies available.
- [ ] libmpv/backend available.
- [ ] Qt SQL SQLite plugin available.
- [ ] Qt platform plugins available.
- [ ] MIME types do not break validation.
- [ ] Uninstall behavior tested.
- [ ] Upgrade behavior tested.
- [ ] User data preserved on package uninstall.

## DEB Checklist

- [ ] Bundled release DEB builds.
- [ ] `dpkg -I` metadata reviewed.
- [ ] `dpkg -c` contents reviewed.
- [ ] Installed on clean Debian/Ubuntu-family VM.
- [ ] `/usr/bin/RevaPlayer` works.
- [ ] Desktop launcher works.
- [ ] `apt remove` removes package files.
- [ ] Upgrade preserves `revaplayer.sqlite`.
- [ ] System-library DEB tested separately only if published.

## RPM Checklist

- [ ] Bundled release RPM builds with `scripts/build-bundled-rpm.sh`.
- [ ] Generated package metadata reviewed with `rpm -qip`.
- [ ] Generated package contents reviewed with `rpm -qlp`.
- [ ] Fedora build tested.
- [ ] openSUSE build tested if targeted.
- [ ] RHEL-family build tested if targeted.
- [ ] Install/upgrade/uninstall tested.
- [ ] Package signed if published through repository.

## AppImage Checklist

- [ ] `scripts/build-appimage.sh` completes.
- [ ] AppImage filename/version/arch correct.
- [ ] `--version` works.
- [ ] AppImage launches on build host.
- [ ] AppImage launches with `APPIMAGE_EXTRACT_AND_RUN=1`.
- [ ] Local install script works.
- [ ] Local uninstall script works.
- [ ] KDE test completed.
- [ ] GNOME test completed.
- [ ] RPM-distro test completed if claiming cross-distro support.
- [ ] File dialogs and screenshots work.
