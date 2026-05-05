# Release Builds

Use this page for reproducible release artifact builds. Local verification should be repeated for every release version.

## Commands

Run tests before publishing:

```bash
cmake -S . -B /tmp/reva-player-build-check -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/reva-player-build-check -j4
ctest --test-dir /tmp/reva-player-build-check --output-on-failure
```

Build the official DEB:

```bash
scripts/build-deb.sh
```

This builds the bundled runtime DEB by default. Use `scripts/build-deb.sh --system`
only when you intentionally want the small system-library DEB for comparison.

Build the primary AppImage when AppImage tooling is installed:

```bash
scripts/build-appimage.sh
```

Fallback AppImage build when AppImage tooling is unavailable:

```bash
scripts/repack-appimage-fallback.sh \
  --build-dir /tmp/reva-player-build-check \
  --source-appimage /path/to/existing.AppImage
```

The fallback path writes a fresh runtime payload to
`dist/appimage-repack/AppDir/usr`. Use that same payload for bundled DEB and RPM
artifacts so the three release formats carry the same Qt/libmpv runtime.

Build the official DEB from the prepared AppDir payload:

```bash
scripts/build-deb.sh --bundle-source dist/appimage-repack/AppDir/usr
```

Build the bundled RPM from the prepared AppDir payload:

```bash
scripts/build-bundled-rpm.sh --bundle-source dist/appimage-repack/AppDir/usr
```

## Local Artifacts

| Format | Path | Notes |
| --- | --- | --- |
| AppImage | `dist/appimage/final/Reva-Player-<version>-x86_64.AppImage` | Built by AppImage tooling or fallback repack from an existing AppImage runtime. |
| DEB | `dist/deb/revaplayer_<version>_amd64.deb` | Bundled runtime DEB installed under `/opt/revaplayer` with `/usr/bin/RevaPlayer`. |
| RPM | `dist/rpm/x86_64/revaplayer-<version>-1.x86_64.rpm` | Bundled runtime RPM installed under `/opt/revaplayer` with `/usr/bin/RevaPlayer`. |

## Verification Checklist

- Run `desktop-file-validate` when available.
- Run `appstreamcli validate --no-net` when available.
- Run the legacy-name audit.
- Run CTest.
- Confirm AppImage, bundled DEB, and bundled RPM builds fail if any bundled ELF
  file reports an unresolved `ldd` dependency.
- Check AppImage `--version` with `APPIMAGE_EXTRACT_AND_RUN=1`.
- Check bundled DEB `--version` from extracted package contents.
- Check bundled RPM `--version` from extracted package contents.
- Inspect bundled RPM with `rpm -qip` and `rpm -qlp` when rpm tooling is available.
- Smoke-launch the build-tree binary and packages with a temporary HOME.
- Complete real GUI playback tests on a graphical desktop.

Manual playback QA is still required on a real graphical desktop with sample
video, audio, subtitles, playlist reorder, saved folders, folder browser, zoom,
settings persistence, clear cache, factory reset from settings, and status bar
setting.
