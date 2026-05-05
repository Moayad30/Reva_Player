# Uninstall Local AppImage Installation

This page documents the local AppImage install created by:

```bash
scripts/install-appimage-local.sh /path/to/Reva-Player-<version>-<arch>.AppImage
```

To remove that local installation:

```bash
scripts/uninstall-appimage-local.sh
```

The uninstall script removes the local launcher/symlink files that it manages. It does not remove playback history, settings, bookmarks, resume data, or thumbnail cache.

For full local data cleanup after uninstall:

```bash
scripts/purge-local-data.sh
```

Do not run cleanup while Reva Player is running.
