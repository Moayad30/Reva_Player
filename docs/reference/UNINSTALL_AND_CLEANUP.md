# Uninstall And Cleanup

Uninstalling Reva Player and deleting user data are separate operations.

## Package Uninstall

- DEB uninstall: see [UNINSTALL_DEB.md](UNINSTALL_DEB.md).
- AppImage local uninstall: see [UNINSTALL_APPIMAGE.md](UNINSTALL_APPIMAGE.md).

Package uninstall should remove application files and launchers. It should not remove playback history, settings, bookmarks, cache, or custom user data by default.

## Local Data Cleanup

To remove settings, history, resume state, bookmarks, window state, and cache paths targeted by the repository cleanup script:

```bash
scripts/purge-local-data.sh
```

See [PURGE_LOCAL_DATA.md](PURGE_LOCAL_DATA.md) for details and exclusions.

## Custom Database Path

If `REVAPLAYER_DB_PATH` was used, manually inspect that path. The purge script intentionally does not remove custom database paths.
