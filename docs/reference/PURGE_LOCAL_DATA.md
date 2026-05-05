# Purge Local Data

Reva Player separates package uninstall from user-data cleanup.

Verified against `scripts/purge-local-data.sh` on 2026-04-25.

## Clear Cache

Clear cache means removing generated cache content, primarily thumbnail-related cache. The current purge script removes broader data than cache only, so use it only when you want a full local reset.

Needs verification: whether the UI exposes a cache-only action and exactly which files it removes.

## Factory Reset

Factory reset means removing local settings/history/resume/bookmarks/custom commands/window state so the next launch recreates default settings.

Repository script:

```bash
scripts/purge-local-data.sh
```

The script refuses to run while `RevaPlayer` is running.

## What The Purge Script Removes

The script targets current and historical local paths under:

- `${XDG_DATA_HOME:-$HOME/.local/share}`
- `${XDG_CACHE_HOME:-$HOME/.cache}`
- `${XDG_CONFIG_HOME:-$HOME/.config}`
- `${XDG_STATE_HOME:-$HOME/.local/state}`

It removes paths for:

- `RevaPlayer/Reva Player`
- `RevaPlayer`
- `Reva Player`
- `revaplayer`
- `io.github.moayad30.revaplayer`
- historical legacy paths from older builds

## What It Does Not Remove

- System-installed DEB package files.
- Local AppImage installation files.
- Custom database path supplied through `REVAPLAYER_DB_PATH`.
- User media files.

## Uninstall Cleanup

Package uninstall removes application files. It should not delete user data by default. If a user wants a complete cleanup, run package uninstall first, then run:

```bash
scripts/purge-local-data.sh
```

If `REVAPLAYER_DB_PATH` is set, manually inspect that custom database path because the purge script intentionally does not remove it.
