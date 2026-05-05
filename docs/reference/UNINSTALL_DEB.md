# Uninstall DEB Package

Use this page for DEB packages installed system-wide.

Repository script:

```bash
scripts/uninstall-deb-system.sh
```

The script removes the system-installed Debian package files for Reva Player. It does not remove user playback history, settings, bookmarks, resume data, or cache.

Manual package-manager removal is also expected to work when the package name is `revaplayer`:

```bash
sudo apt remove revaplayer
```

Needs verification: exact package name if a future package builder changes metadata.

For full local data cleanup after uninstall:

```bash
scripts/purge-local-data.sh
```
