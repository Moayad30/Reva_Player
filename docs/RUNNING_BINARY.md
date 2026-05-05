# Running the Local Binary

`build/RevaPlayer` is a locally linked binary. It is not a portable application by itself unless packaged with its runtime dependencies.

Verified against repository files on 2026-04-25.

## Basic Commands

```bash
./build/RevaPlayer
./build/RevaPlayer --version
./build/RevaPlayer --help
./build/RevaPlayer --url <media-url>
./build/RevaPlayer /path/to/video.mp4
```

Version output is currently:

```text
Reva Player 1.0.0
```

## Runtime Requirements

The binary must find:

- Qt runtime libraries.
- Qt platform plugin, such as xcb or Wayland.
- Qt SQL SQLite plugin.
- libmpv runtime library.
- OpenGL stack usable by Qt and libmpv.
- Audio output stack supported by mpv on the host system.

## Database Path

Default database location is computed by Qt:

```text
QStandardPaths::AppDataLocation/revaplayer.sqlite
```

Use a temporary database for testing:

```bash
REVAPLAYER_DB_PATH=/tmp/revaplayer-smoke.sqlite ./build/RevaPlayer
```

Legacy compatibility:

- A legacy fallback variable is still read if `REVAPLAYER_DB_PATH` is not set.
- Do not use the legacy fallback in new scripts.

## Platform Selection

Useful diagnostics:

```bash
QT_QPA_PLATFORM=xcb ./build/RevaPlayer
QT_QPA_PLATFORM=wayland ./build/RevaPlayer
QT_QPA_PLATFORM=minimal REVAPLAYER_DB_PATH=/tmp/revaplayer-minimal.sqlite timeout 8s ./build/RevaPlayer
```

The `minimal` launch is useful only as a smoke test. It does not verify real playback, file dialogs, GPU rendering, or desktop integration.

## Troubleshooting

| Error | Meaning | Action |
| --- | --- | --- |
| `libmpv.so` not found | Runtime libmpv missing | Install distro libmpv runtime package or use bundled package. |
| `Could not load the Qt platform plugin "xcb"` | Qt platform plugin/runtime missing | Install Qt platform plugin packages or use packaged AppImage/bundled DEB. |
| SQLite settings fail | Qt SQLite plugin missing or database unwritable | Check Qt SQL SQLite package and writable XDG data path. |
| Video area is black | OpenGL/rendering/platform issue | Test xcb vs Wayland and inspect terminal output. |
| File dialogs look non-native | Missing portal/platform theme integration | See [FILE_DIALOGS.md](FILE_DIALOGS.md). |

## When Not To Copy the Binary Alone

Do not distribute only `build/RevaPlayer` to users unless they have matching Qt, libmpv, platform plugins, and SQL plugins installed. Prefer:

- AppImage for quick cross-distro testing.
- DEB for Debian/Ubuntu-family systems.
- Bundled DEB when offline-friendly runtime bundling is required.

See [packaging/README.md](packaging/README.md).
