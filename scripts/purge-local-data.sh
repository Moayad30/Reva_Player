#!/usr/bin/env bash
set -euo pipefail

readonly APP_BINARY_NAME="RevaPlayer"
readonly APP_DISPLAY_NAME="Reva Player"
readonly APP_ID="io.github.moayad30.revaplayer"

readonly XDG_DATA_HOME_DIR="${XDG_DATA_HOME:-${HOME}/.local/share}"
readonly XDG_CACHE_HOME_DIR="${XDG_CACHE_HOME:-${HOME}/.cache}"
readonly XDG_CONFIG_HOME_DIR="${XDG_CONFIG_HOME:-${HOME}/.config}"
readonly XDG_STATE_HOME_DIR="${XDG_STATE_HOME:-${HOME}/.local/state}"

usage() {
    cat <<'EOF'
Usage: scripts/purge-local-data.sh

Removes Reva Player local data, cache, thumbnails, and config for the current user.

This targets the standard local paths used by the application, including:
  - SQLite database and settings under XDG data/config roots
  - playlist thumbnail cache
  - generated thumbfast/script helper files
  - historical legacy paths from older builds

This does NOT remove:
  - system-installed Debian package files
  - local AppImage installation files
  - custom database paths supplied manually through REVAPLAYER_DB_PATH
EOF
}

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

remove_if_exists() {
    local path="$1"
    if [ -e "${path}" ] || [ -L "${path}" ]; then
        rm -rf -- "${path}"
        printf 'removed: %s\n' "${path}"
    fi
}

remove_directory_if_empty() {
    local path="$1"
    if [ -d "${path}" ]; then
        rmdir --ignore-fail-on-non-empty "${path}" 2>/dev/null || true
    fi
}

ensure_not_running() {
    if pgrep -x "${APP_BINARY_NAME}" >/dev/null 2>&1; then
        die "close ${APP_DISPLAY_NAME} before removing local data"
    fi
}

if [ "$#" -gt 1 ]; then
    usage >&2
    exit 1
fi

case "${1:-}" in
    "" )
        ;;
    --help|-h)
        usage
        exit 0
        ;;
    *)
        usage >&2
        exit 1
        ;;
esac

ensure_not_running

legacy_prefix="new"
legacy_stem="potplayer"
legacy_home_dir="${HOME}/.${legacy_prefix}${legacy_stem}"
legacy_database_file="${XDG_DATA_HOME_DIR}/RevaPlayer/Reva Player/${legacy_prefix}${legacy_stem}.sqlite"

data_paths=(
    "${XDG_DATA_HOME_DIR}/RevaPlayer/Reva Player"
    "${XDG_DATA_HOME_DIR}/RevaPlayer"
    "${XDG_DATA_HOME_DIR}/Reva Player"
    "${XDG_DATA_HOME_DIR}/revaplayer"
    "${XDG_DATA_HOME_DIR}/${APP_ID}"
    "${legacy_home_dir}"
)

cache_paths=(
    "${XDG_CACHE_HOME_DIR}/RevaPlayer/Reva Player"
    "${XDG_CACHE_HOME_DIR}/RevaPlayer"
    "${XDG_CACHE_HOME_DIR}/Reva Player"
    "${XDG_CACHE_HOME_DIR}/revaplayer"
    "${XDG_CACHE_HOME_DIR}/${APP_ID}"
)

config_paths=(
    "${XDG_CONFIG_HOME_DIR}/RevaPlayer/Reva Player"
    "${XDG_CONFIG_HOME_DIR}/RevaPlayer"
    "${XDG_CONFIG_HOME_DIR}/Reva Player"
    "${XDG_CONFIG_HOME_DIR}/revaplayer"
    "${XDG_CONFIG_HOME_DIR}/${APP_ID}"
)

state_paths=(
    "${XDG_STATE_HOME_DIR}/RevaPlayer/Reva Player"
    "${XDG_STATE_HOME_DIR}/RevaPlayer"
    "${XDG_STATE_HOME_DIR}/Reva Player"
    "${XDG_STATE_HOME_DIR}/revaplayer"
    "${XDG_STATE_HOME_DIR}/${APP_ID}"
)

legacy_paths=(
    "${legacy_database_file}"
    "${XDG_DATA_HOME_DIR}/RevaPlayer/Reva Player/scripts"
    "${XDG_DATA_HOME_DIR}/RevaPlayer/Reva Player/scripts-panel"
    "${XDG_CACHE_HOME_DIR}/RevaPlayer/Reva Player/playlist-thumbnails"
)

for path in "${legacy_paths[@]}" "${data_paths[@]}" "${cache_paths[@]}" "${config_paths[@]}" "${state_paths[@]}"; do
    remove_if_exists "${path}"
done

remove_directory_if_empty "${XDG_DATA_HOME_DIR}/RevaPlayer"
remove_directory_if_empty "${XDG_CACHE_HOME_DIR}/RevaPlayer"
remove_directory_if_empty "${XDG_CONFIG_HOME_DIR}/RevaPlayer"
remove_directory_if_empty "${XDG_STATE_HOME_DIR}/RevaPlayer"

printf 'Reva Player local data and cache were removed for user %s.\n' "${USER}"
if [ -n "${REVAPLAYER_DB_PATH:-}" ]; then
    printf 'note: REVAPLAYER_DB_PATH is set in your shell; any custom database at that path was not touched.\n'
fi
