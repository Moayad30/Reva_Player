# Storage Paths

Reva Player uses Qt `QStandardPaths`, so exact resolved paths depend on OS, desktop environment, and XDG environment variables.

Verified against repository files on 2026-04-25.

## Database

Default database:

```text
QStandardPaths::AppDataLocation/revaplayer.sqlite
```

The database is created by `infrastructure/storage/SqliteStore.cpp`.

Override:

```bash
REVAPLAYER_DB_PATH=/custom/path/revaplayer.sqlite RevaPlayer
```

Legacy compatibility: the application still contains an older database-path environment fallback. Use `REVAPLAYER_DB_PATH` for all new scripts and documentation.

## SQLite Contents

| Table | Data |
| --- | --- |
| `settings` | UI, playback, subtitle, playlist, input, shortcuts, saved custom values. |
| `window_state` | Main window geometry, dock/widget state, maximized/fullscreen flags. |
| `resume_state` | Per-source resume position. |
| `playback_history` | Recent media records and last positions. |
| `bookmarks` | Per-source bookmarks with category and note. |
| `custom_commands` | Custom mpv command scripts. |

## Cache

Thumbnail cache:

```text
QStandardPaths::CacheLocation/thumbnails
```

The path is created by `services/media/ThumbnailService.cpp`.

## Scripts And mpv Helper Files

`MpvCore` can copy or locate `thumbfast.lua` from:

- Embedded Qt resource `:/mpv/thumbfast.lua`.
- Application-adjacent resource paths.
- `QStandardPaths::AppDataLocation/scripts/thumbfast.lua`.
- `~/.config/mpv/scripts/thumbfast.lua`.

Exact selected path is runtime-dependent.

## Linux XDG Expectations

Typical Linux paths may resolve under:

- `~/.local/share/...` for application data.
- `~/.cache/...` for cache.
- `~/.config/...` for config-related Qt paths.

Do not hard-code these in package scripts unless you intentionally respect `XDG_DATA_HOME`, `XDG_CACHE_HOME`, and related variables.

## Logs

No repository-verified persistent log file path was found. Runtime warnings currently use Qt/debug output paths rather than a documented log file.

## Cleanup

See [PURGE_LOCAL_DATA.md](PURGE_LOCAL_DATA.md). Package uninstall and local-data cleanup are separate operations.
