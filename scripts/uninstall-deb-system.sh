#!/usr/bin/env bash
set -euo pipefail

readonly PACKAGE_NAME="revaplayer"
readonly APP_BINARY_NAME="RevaPlayer"
readonly APP_DISPLAY_NAME="Reva Player"

usage() {
    cat <<'EOF'
Usage: scripts/uninstall-deb-system.sh

Removes the system-installed Reva Player Debian package.

This removes:
  - the installed revaplayer package
  - files placed by the package under /opt/revaplayer and /usr/share

This does NOT remove:
  - user playback history
  - user settings
  - user cache / thumbnails

Use scripts/purge-local-data.sh separately if you also want to remove local user data.
EOF
}

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

run_as_root() {
    if [ "$(id -u)" -eq 0 ]; then
        "$@"
        return 0
    fi

    if command -v sudo >/dev/null 2>&1; then
        sudo "$@"
        return 0
    fi

    die "this operation needs root privileges; run as root or install sudo"
}

ensure_not_running() {
    if pgrep -x "${APP_BINARY_NAME}" >/dev/null 2>&1; then
        die "close ${APP_DISPLAY_NAME} before removing the Debian package"
    fi
}

package_installed() {
    dpkg-query -W -f='${Status}' "${PACKAGE_NAME}" 2>/dev/null | grep -Fq "install ok installed"
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

if ! package_installed; then
    printf 'Package %s is not currently installed.\n' "${PACKAGE_NAME}"
    exit 0
fi

if command -v apt-get >/dev/null 2>&1; then
    run_as_root env DEBIAN_FRONTEND=noninteractive apt-get purge -y "${PACKAGE_NAME}"
else
    run_as_root dpkg --purge "${PACKAGE_NAME}"
fi

printf 'Reva Player Debian package removed from the system.\n'
