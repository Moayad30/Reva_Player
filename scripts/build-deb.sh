#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly PROJECT_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
readonly PACKAGE_NAME="revaplayer"

build_dir="/tmp/reva-player-deb-build"
output_dir=""
qt_major="6"
generator=""
bundle_source=""
package_mode="bundled"

usage() {
    cat <<'EOF'
Usage: scripts/build-deb.sh [options]

Build the release Debian/Ubuntu package for Reva Player.

By default this builds the bundled runtime DEB intended for release. It installs
Reva Player under /opt/revaplayer, provides /usr/bin/RevaPlayer, and includes
the Qt/libmpv runtime copied from the AppImage AppDir.

Options:
  --build-dir DIR     CMake build directory
  --output-dir DIR    Final .deb output directory
  --qt-major 5|6      Qt major version to build against (default: 6)
  --generator NAME    Force a CMake generator
  --bundle-source DIR Runtime bundle source directory
  --bundled           Build bundled runtime DEB (default)
  --system            Build small system-library CPack DEB
  --help              Show this help
EOF
}

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

detect_generator() {
    if [ -n "${generator}" ]; then
        printf '%s\n' "${generator}"
        return 0
    fi

    if command -v ninja >/dev/null 2>&1; then
        printf 'Ninja\n'
        return 0
    fi

    printf 'Unix Makefiles\n'
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --build-dir)
            build_dir="$2"
            shift 2
            ;;
        --output-dir)
            output_dir="$2"
            shift 2
            ;;
        --qt-major)
            qt_major="$2"
            shift 2
            ;;
        --generator)
            generator="$2"
            shift 2
            ;;
        --bundle-source)
            bundle_source="$2"
            shift 2
            ;;
        --bundled)
            package_mode="bundled"
            shift
            ;;
        --system)
            package_mode="system"
            shift
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            die "unknown option: $1"
            ;;
    esac
done

case "${package_mode}" in
    bundled|system)
        ;;
    *)
        die "unknown package mode: ${package_mode}"
        ;;
esac

case "${qt_major}" in
    5|6)
        ;;
    *)
        die "--qt-major must be 5 or 6"
        ;;
esac

command -v cmake >/dev/null 2>&1 || die "missing required tool: cmake"

if [ -z "${output_dir}" ]; then
    if [ "${package_mode}" = "bundled" ]; then
        output_dir="${PROJECT_ROOT}/dist/deb"
    else
        output_dir="${PROJECT_ROOT}/dist/deb-system"
    fi
fi

if [ "${package_mode}" = "bundled" ]; then
    bundled_args=(
        --build-dir "${build_dir}"
        --output-dir "${output_dir}"
        --qt-major "${qt_major}"
    )

    if [ -n "${generator}" ]; then
        bundled_args+=(--generator "${generator}")
    fi
    if [ -n "${bundle_source}" ]; then
        bundled_args+=(--bundle-source "${bundle_source}")
    fi

    "${PROJECT_ROOT}/scripts/build-bundled-deb.sh" "${bundled_args[@]}"
    exit 0
fi

command -v cpack >/dev/null 2>&1 || die "missing required tool: cpack"
command -v dpkg-shlibdeps >/dev/null 2>&1 || die "missing required tool: dpkg-shlibdeps"

cmake_generator="$(detect_generator)"
mkdir -p "${output_dir}"

cmake -S "${PROJECT_ROOT}" -B "${build_dir}" -G "${cmake_generator}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DREVAPLAYER_QT_MAJOR="${qt_major}"

cmake --build "${build_dir}" --parallel
cpack --config "${build_dir}/CPackConfig.cmake" -B "${output_dir}"

latest_package="$(find "${output_dir}" -maxdepth 1 -type f -name "${PACKAGE_NAME}_*.deb" -printf '%T@ %p\n' \
    | sort -nr \
    | awk 'NR == 1 {print $2}')"

[ -n "${latest_package}" ] || die "package was not created in ${output_dir}"
printf 'Reva Player Debian package ready: %s\n' "${latest_package}"
