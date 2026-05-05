# Third-Party Notices

This file summarizes the main third-party components used by Reva Player. It is a practical notice file, not a replacement for the full license texts of those projects.

Binary release packages may include additional system/runtime libraries depending on the build host and packaging format. Review the produced AppImage, DEB, or RPM contents before publishing a release.

## Qt

- Purpose: UI framework, widgets, SQL integration, DBus desktop integration, OpenGL integration, platform plugins, and translations.
- License: Qt is available under open-source licenses including LGPL/GPL terms, or under a commercial Qt license.
- Project: https://www.qt.io/
- Notes: When distributing binaries built with open-source Qt, comply with the applicable Qt open-source license obligations for the exact Qt build used.

## mpv / libmpv

- Purpose: media playback backend and rendering integration.
- License: GPL-2.0-or-later by default; LGPL builds are possible only with suitable mpv build configuration and dependency choices.
- Project: https://mpv.io/
- Notes: Reva Player release binaries that link to or bundle GPL libmpv are distributed under GPL-compatible terms.

## FFmpeg

- Purpose: codec, demuxing, filtering, and media runtime functionality used through mpv/libmpv.
- License: LGPL-2.1-or-later or GPL-2.0-or-later depending on FFmpeg build options and enabled external libraries.
- Project: https://ffmpeg.org/
- Notes: Do not publish release binaries that use FFmpeg configured with nonfree components. Verify the FFmpeg configuration of any bundled runtime before release.

## SQLite

- Purpose: local settings, history, resume state, bookmarks, window state, and custom command storage.
- License: Public Domain.
- Project: https://www.sqlite.org/

## D-Bus Runtime Libraries

- Purpose: Linux desktop session communication used for display sleep inhibition while video playback is active.
- License: Common Linux distributions provide D-Bus components under permissive/GPL-compatible open-source licenses; verify the exact bundled files in each binary artifact.
- Project: https://www.freedesktop.org/wiki/Software/dbus/

## thumbfast.lua

- Purpose: mpv thumbnail helper script used for preview/thumbnail integration.
- Path: [resources/mpv/thumbfast.lua](resources/mpv/thumbfast.lua)
- License: Mozilla Public License 2.0 (`MPL-2.0`).
- Project: https://github.com/po5/thumbfast
- Notes: The vendored file contains its own MPL-2.0 notice and was not modified as part of the GPL license switch.
