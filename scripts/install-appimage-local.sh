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
Usage: scripts/install-appimage-local.sh /path/to/RevaPlayer-v<version>.AppImage

Installs the AppImage for the current user under ~/.local, refreshes the
desktop launcher, desktop shortcut, and icon, and removes previously installed
versions from the same local AppImage directory.
EOF
}

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

make_absolute_path() {
    local input_path="$1"

    if [ -d "${input_path}" ]; then
        (cd -- "${input_path}" && pwd)
        return 0
    fi

    local parent_dir
    parent_dir="$(cd -- "$(dirname -- "${input_path}")" && pwd)"
    printf '%s/%s\n' "${parent_dir}" "$(basename -- "${input_path}")"
}

run_appimage() {
    local appimage_path="$1"
    shift
    APPIMAGE_EXTRACT_AND_RUN=1 "${appimage_path}" "$@"
}

extract_appimage_metadata() {
    local appimage_path="$1"
    local temp_dir="$2"

    (
        cd -- "${temp_dir}"
        run_appimage "${appimage_path}" --appimage-extract >/dev/null
    )
}

find_metadata_file() {
    local extract_root="$1"
    shift

    local candidate
    for candidate in "$@"; do
        if [ -f "${extract_root}/${candidate}" ]; then
            printf '%s\n' "${extract_root}/${candidate}"
            return 0
        fi
    done

    return 1
}

rewrite_desktop_entry() {
    local source_path="$1"
    local destination_path="$2"
    local exec_line="$3"
    local icon_line="$4"
    local version_line="$5"
    local appimage_line="$6"

    awk \
        -v exec_line="${exec_line}" \
        -v icon_line="${icon_line}" \
        -v version_line="${version_line}" \
        -v appimage_line="${appimage_line}" \
        '
        BEGIN {
            seen_exec = 0
            seen_icon = 0
            seen_version = 0
            seen_appimage = 0
        }
        /^Exec=/ {
            print exec_line
            seen_exec = 1
            next
        }
        /^TryExec=/ {
            next
        }
        /^Icon=/ {
            print icon_line
            seen_icon = 1
            next
        }
        /^X-AppImage-Version=/ {
            print version_line
            seen_version = 1
            next
        }
        /^X-AppImage-Path=/ {
            print appimage_line
            seen_appimage = 1
            next
        }
        {
            print
        }
        END {
            if (!seen_exec) {
                print exec_line
            }
            if (!seen_icon) {
                print icon_line
            }
            if (!seen_version) {
                print version_line
            }
            if (!seen_appimage) {
                print appimage_line
            }
        }
        ' "${source_path}" >"${destination_path}"
}

prune_previous_appimages() {
    local keep_path="$1"
    local candidate

    shopt -s nullglob
    for candidate in "${APPIMAGE_DIR}"/*.AppImage; do
        if [ "${candidate}" != "${keep_path}" ] && [ "${candidate}" != "${CURRENT_APPIMAGE_LINK}" ]; then
            rm -f -- "${candidate}"
        fi
    done
    shopt -u nullglob
}

if [ "$#" -ne 1 ]; then
    usage >&2
    exit 1
fi

case "$1" in
    --help|-h)
        usage
        exit 0
        ;;
esac

source_appimage="$(make_absolute_path "$1")"
[ -f "${source_appimage}" ] || die "AppImage not found: ${source_appimage}"

mkdir -p "${BIN_DIR}" "${APPIMAGE_DIR}" "${DESKTOP_DIR}" "${ICON_DIR}" "${METAINFO_DIR}"
mkdir -p "${DESKTOP_SHORTCUT_DIR}"

installed_name="$(basename -- "${source_appimage}")"
case "${installed_name}" in
    *.AppImage)
        ;;
    *)
        installed_name="${APP_BINARY_NAME}.AppImage"
        ;;
esac

installed_appimage="${APPIMAGE_DIR}/${installed_name}"

if [ "${source_appimage}" != "${installed_appimage}" ]; then
    install -m 0755 "${source_appimage}" "${installed_appimage}"
else
    chmod 0755 "${installed_appimage}"
fi

temp_dir="$(mktemp -d)"
cleanup() {
    rm -rf "${temp_dir}"
}
trap cleanup EXIT

extract_appimage_metadata "${installed_appimage}" "${temp_dir}"
extract_root="${temp_dir}/squashfs-root"

desktop_source="$(find_metadata_file "${extract_root}" \
    "usr/share/applications/${DESKTOP_ID}" \
    "${DESKTOP_ID}")" || die "desktop file missing inside AppImage"
icon_source="$(find_metadata_file "${extract_root}" \
    "usr/share/icons/hicolor/scalable/apps/${ICON_NAME}.svg" \
    "${ICON_NAME}.svg" \
    ".DirIcon")" || die "icon missing inside AppImage"
metainfo_source="$(find_metadata_file "${extract_root}" \
    "usr/share/metainfo/${APP_ID}.metainfo.xml")" || die "metainfo file missing inside AppImage"

install -m 0644 "${icon_source}" "${ICON_PATH}"
install -m 0644 "${metainfo_source}" "${METAINFO_PATH}"

appimage_version="$(sed -n 's/^X-AppImage-Version=//p' "${desktop_source}" | head -n 1)"
if [ -z "${appimage_version}" ]; then
    appimage_version="$(printf '%s\n' "${installed_name}" | sed -nE 's/.*-([0-9]+(\.[0-9]+)+).*\.AppImage$/\1/p' | head -n 1)"
fi
if [ -z "${appimage_version}" ]; then
    appimage_version="unknown"
fi

exec_line="Exec=\"${BIN_LINK}\" %U"
icon_line="Icon=${ICON_PATH}"
version_line="X-AppImage-Version=${appimage_version}"
appimage_line="X-AppImage-Path=${installed_appimage}"

rewrite_desktop_entry "${desktop_source}" "${DESKTOP_PATH}" \
    "${exec_line}" "${icon_line}" "${version_line}" "${appimage_line}"
install -m 0755 "${DESKTOP_PATH}" "${DESKTOP_SHORTCUT_PATH}"

ln -sfn "${installed_name}" "${CURRENT_APPIMAGE_LINK}"
ln -sfn "${CURRENT_APPIMAGE_LINK}" "${BIN_LINK}"
ln -sfn "${CURRENT_APPIMAGE_LINK}" "${LOWERCASE_BIN_LINK}"

prune_previous_appimages "${installed_appimage}"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${DESKTOP_DIR}" >/dev/null 2>&1 || true
fi

if command -v xdg-desktop-menu >/dev/null 2>&1; then
    xdg-desktop-menu forceupdate >/dev/null 2>&1 || true
fi

printf 'Installed AppImage: %s\n' "${installed_appimage}"
printf 'Launcher: %s\n' "${DESKTOP_PATH}"
printf 'Desktop shortcut: %s\n' "${DESKTOP_SHORTCUT_PATH}"
printf 'Command: %s\n' "${BIN_LINK}"
