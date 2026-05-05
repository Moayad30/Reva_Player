#!/usr/bin/env bash
set -euo pipefail

readonly APP_BINARY_NAME="RevaPlayer"
readonly APP_DISPLAY_NAME="Reva Player"
readonly APP_ID="io.github.moayad30.revaplayer"
readonly DESKTOP_ID="${APP_ID}.desktop"
readonly ICON_NAME="revaplayer"

readonly XDG_DATA_HOME_DIR="${XDG_DATA_HOME:-${HOME}/.local/share}"
readonly BIN_DIR="${HOME}/.local/bin"
readonly APPIMAGE_DIR="${HOME}/.local/lib/revaplayer"
readonly DESKTOP_DIR="${XDG_DATA_HOME_DIR}/applications"
readonly ICON_DIR="${XDG_DATA_HOME_DIR}/icons/hicolor/scalable/apps"
readonly METAINFO_DIR="${XDG_DATA_HOME_DIR}/metainfo"
readonly CURRENT_APPIMAGE_LINK="${APPIMAGE_DIR}/current.AppImage"
readonly BIN_LINK="${BIN_DIR}/${APP_BINARY_NAME}"
readonly LOWERCASE_BIN_LINK="${BIN_DIR}/revaplayer"
readonly DESKTOP_PATH="${DESKTOP_DIR}/${DESKTOP_ID}"
readonly ICON_PATH="${ICON_DIR}/${ICON_NAME}.svg"
readonly METAINFO_PATH="${METAINFO_DIR}/${APP_ID}.metainfo.xml"

resolve_desktop_shortcut_dir() {
    local configured_dir=""

    if command -v xdg-user-dir >/dev/null 2>&1; then
        configured_dir="$(xdg-user-dir DESKTOP 2>/dev/null || true)"
        if [ -n "${configured_dir}" ] && [ "${configured_dir}" != "${HOME}" ]; then
            printf '%s\n' "${configured_dir}"
            return 0
        fi
    fi

    printf '%s\n' "${HOME}/Desktop"
}

readonly DESKTOP_SHORTCUT_DIR="$(resolve_desktop_shortcut_dir)"
readonly DESKTOP_SHORTCUT_PATH="${DESKTOP_SHORTCUT_DIR}/${APP_DISPLAY_NAME}.desktop"

usage() {
    cat <<'EOF'
Usage: scripts/uninstall-appimage-local.sh

Removes the Reva Player AppImage local installation that was created by
scripts/install-appimage-local.sh.

What this removes:
  - ~/.local/lib/revaplayer/
  - ~/.local/bin/RevaPlayer
  - ~/.local/bin/revaplayer
  - ~/.local/share/applications/io.github.moayad30.revaplayer.desktop
  - ~/.local/share/icons/hicolor/scalable/apps/revaplayer.svg
  - ~/.local/share/metainfo/io.github.moayad30.revaplayer.metainfo.xml
  - desktop shortcut created by the installer

What this does NOT remove:
  - playback history / settings / cache / thumbnails

Use scripts/purge-local-data.sh separately if you also want to remove user data.
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
        die "close ${APP_DISPLAY_NAME} before removing its AppImage installation"
    fi
}

refresh_desktop_metadata() {
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "${DESKTOP_DIR}" >/dev/null 2>&1 || true
    fi

    if command -v xdg-desktop-menu >/dev/null 2>&1; then
        xdg-desktop-menu forceupdate >/dev/null 2>&1 || true
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

remove_if_exists "${DESKTOP_SHORTCUT_PATH}"
remove_if_exists "${DESKTOP_PATH}"
remove_if_exists "${ICON_PATH}"
remove_if_exists "${METAINFO_PATH}"
remove_if_exists "${BIN_LINK}"
remove_if_exists "${LOWERCASE_BIN_LINK}"
remove_if_exists "${CURRENT_APPIMAGE_LINK}"
remove_if_exists "${APPIMAGE_DIR}"

remove_directory_if_empty "${BIN_DIR}"
remove_directory_if_empty "${DESKTOP_DIR}"
remove_directory_if_empty "${ICON_DIR}"
remove_directory_if_empty "${METAINFO_DIR}"

refresh_desktop_metadata

printf 'Reva Player AppImage local installation removed.\n'
