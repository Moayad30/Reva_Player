#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly PROJECT_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
readonly APP_BINARY_NAME="RevaPlayer"
readonly APP_DISPLAY_NAME="Reva Player"
readonly APP_ID="io.github.moayad30.revaplayer"
readonly DESKTOP_ID="${APP_ID}.desktop"
readonly ICON_NAME="revaplayer"
readonly PACKAGE_NAME="revaplayer"
readonly PACKAGE_INSTALL_ROOT="/opt/revaplayer"
readonly PACKAGE_RELEASE="1"

build_dir="/tmp/reva-player-rpm-build"
install_root="/tmp/reva-player-install-check"
bundle_source=""
staging_root="${PROJECT_ROOT}/dist/rpm/staging"
output_dir="${PROJECT_ROOT}/dist/rpm"
app_version=""
qt_major="6"
generator=""

usage() {
    cat <<'EOF'
Usage: scripts/build-bundled-rpm.sh [options]

Build a bundled offline-friendly RPM package for Reva Player. The package
installs under /opt/revaplayer, provides /usr/bin/RevaPlayer, and includes
the Qt/libmpv runtime copied from an AppImage AppDir payload.

Options:
  --build-dir DIR        CMake build directory
  --install-root DIR     Temporary install root
  --bundle-source DIR    Runtime bundle source directory
  --staging-root DIR     Working packaging root
  --output-dir DIR       Final .rpm output directory
  --qt-major 5|6         Qt major version to build against (default: 6)
  --generator NAME       Force a CMake generator
  --version VERSION      Override detected project version
  --help                 Show this help
EOF
}

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

ensure_tool() {
    command -v "$1" >/dev/null 2>&1 || die "missing required tool: $1"
}

detect_version() {
    if [ -n "${app_version}" ]; then
        printf '%s\n' "${app_version}"
        return 0
    fi

    local version
    version="$(sed -nE 's/^project\(RevaPlayer VERSION ([^ )]+).*/\1/p' "${PROJECT_ROOT}/CMakeLists.txt" | head -n 1)"
    [ -n "${version}" ] || die "could not detect project version from CMakeLists.txt"
    printf '%s\n' "${version}"
}

detect_rpm_arch() {
    local machine_arch
    machine_arch="$(uname -m)"
    case "${machine_arch}" in
        x86_64|aarch64|ppc64le|s390x)
            printf '%s\n' "${machine_arch}"
            ;;
        armv7l|armhf)
            printf 'armv7hl\n'
            ;;
        i686|i386)
            printf 'i686\n'
            ;;
        *)
            printf '%s\n' "${machine_arch}"
            ;;
    esac
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

absolute_path() {
    local path="$1"

    if [[ "${path}" == /* ]]; then
        printf '%s\n' "${path}"
        return 0
    fi

    printf '%s/%s\n' "${PROJECT_ROOT}" "${path#./}"
}

detect_qmake() {
    local candidate
    for candidate in qmake6 qmake-qt6 qmake; do
        if command -v "${candidate}" >/dev/null 2>&1; then
            command -v "${candidate}"
            return 0
        fi
    done
    return 1
}

detect_qt_plugins_dir() {
    local qmake_binary="$1"
    [ -n "${qmake_binary}" ] || return 1
    "${qmake_binary}" -query QT_INSTALL_PLUGINS 2>/dev/null || return 1
}

detect_qt_translations_dir() {
    local qmake_binary="$1"
    [ -n "${qmake_binary}" ] || return 1
    "${qmake_binary}" -query QT_INSTALL_TRANSLATIONS 2>/dev/null || return 1
}

detect_bundle_source() {
    if [ -n "${bundle_source}" ]; then
        printf '%s\n' "${bundle_source}"
        return 0
    fi

    local candidate=""
    for candidate in \
        "${PROJECT_ROOT}/dist/AppDir/usr" \
        "${PROJECT_ROOT}/dist/appimage-repack/AppDir/usr"; do
        if [ -d "${candidate}" ]; then
            printf '%s\n' "${candidate}"
            return 0
        fi
    done

    return 1
}

copy_directory_contents() {
    local source_dir="$1"
    local destination_dir="$2"

    if [ ! -d "${source_dir}" ]; then
        return 0
    fi

    mkdir -p "${destination_dir}"
    cp -a "${source_dir}/." "${destination_dir}/"
}

copy_optional_qt_runtime_plugins() {
    local plugins_dir="$1"
    local destination_root="$2"
    [ -d "${plugins_dir}" ] || return 0

    mkdir -p "${destination_root}/platforms" \
             "${destination_root}/wayland-decoration-client" \
             "${destination_root}/wayland-graphics-integration-client" \
             "${destination_root}/wayland-shell-integration"

    local file_path=""
    for file_path in \
        "${plugins_dir}/platforms/libqwayland-egl.so" \
        "${plugins_dir}/platforms/libqwayland-generic.so" \
        "${plugins_dir}/platforms/libqminimal.so" \
        "${plugins_dir}/platforms/libqoffscreen.so"; do
        if [ -f "${file_path}" ]; then
            cp -f "${file_path}" "${destination_root}/platforms/"
        fi
    done

    local optional_dir=""
    for optional_dir in \
        "wayland-decoration-client" \
        "wayland-graphics-integration-client" \
        "wayland-shell-integration"; do
        if [ -d "${plugins_dir}/${optional_dir}" ]; then
            copy_directory_contents "${plugins_dir}/${optional_dir}" "${destination_root}/${optional_dir}"
        fi
    done
}

copy_optional_qt_runtime_libraries() {
    local destination_root="$1"
    mkdir -p "${destination_root}"

    local library_path=""
    for library_path in \
        /lib/x86_64-linux-gnu/libQt6DBus.so.6 \
        /lib/x86_64-linux-gnu/libQt5DBus.so.5 \
        /lib/x86_64-linux-gnu/libQt6WaylandClient.so.6 \
        /lib/x86_64-linux-gnu/libQt6WaylandEglClientHwIntegration.so.6 \
        /lib/x86_64-linux-gnu/libwayland-client.so.0 \
        /lib/x86_64-linux-gnu/libdbus-1.so.3 \
        /usr/lib/x86_64-linux-gnu/libQt6DBus.so.6 \
        /usr/lib/x86_64-linux-gnu/libQt5DBus.so.5 \
        /usr/lib/x86_64-linux-gnu/libQt6WaylandClient.so.6 \
        /usr/lib/x86_64-linux-gnu/libQt6WaylandEglClientHwIntegration.so.6 \
        /usr/lib/x86_64-linux-gnu/libwayland-client.so.0 \
        /usr/lib/x86_64-linux-gnu/libdbus-1.so.3; do
        if [ -f "${library_path}" ]; then
            cp -f "${library_path}" "${destination_root}/"
        fi
    done

    if command -v ldconfig >/dev/null 2>&1; then
        local library_name=""
        local detected_path=""
        for library_name in \
            libQt6DBus.so.6 \
            libQt5DBus.so.5 \
            libQt6WaylandClient.so.6 \
            libQt6WaylandEglClientHwIntegration.so.6 \
            libwayland-client.so.0 \
            libdbus-1.so.3; do
            detected_path="$(ldconfig -p 2>/dev/null | awk -v library_name="${library_name}" '$1 == library_name && detected_path == "" { detected_path = $NF } END { print detected_path }')"
            if [ -f "${detected_path}" ]; then
                cp -f "${detected_path}" "${destination_root}/"
            fi
        done
    fi
}

copy_optional_platform_theme_plugins() {
    local plugins_dir="$1"
    local destination_dir="$2"
    [ -d "${plugins_dir}" ] || return 0
    mkdir -p "${destination_dir}"

    for plugin_name in libqgtk3.so KDEPlasmaPlatformTheme6.so KDEPlasmaPlatformTheme5.so; do
        if [ -f "${plugins_dir}/platformthemes/${plugin_name}" ]; then
            cp -f "${plugins_dir}/platformthemes/${plugin_name}" "${destination_dir}/${plugin_name}"
        fi
    done
}

copy_supported_qt_translations() {
    local translations_dir="$1"
    local destination_dir="$2"
    [ -d "${translations_dir}" ] || return 0

    mkdir -p "${destination_dir}"

    local catalog=""
    local locale=""
    for catalog in qtbase qt; do
        for locale in en ar es fr de tr ru zh_CN; do
            if [ -f "${translations_dir}/${catalog}_${locale}.qm" ]; then
                cp -f "${translations_dir}/${catalog}_${locale}.qm" "${destination_dir}/"
            fi
        done
    done
}

prune_translations() {
    local translations_dir="$1"
    if [ ! -d "${translations_dir}" ]; then
        return 0
    fi

    find "${translations_dir}" -maxdepth 1 -type f \( -name 'qtbase_*.qm' -o -name 'qt_*.qm' \) \
        ! -name 'qtbase_en.qm' \
        ! -name 'qtbase_ar.qm' \
        ! -name 'qtbase_es.qm' \
        ! -name 'qtbase_fr.qm' \
        ! -name 'qtbase_de.qm' \
        ! -name 'qtbase_tr.qm' \
        ! -name 'qtbase_ru.qm' \
        ! -name 'qtbase_zh_CN.qm' \
        ! -name 'qt_en.qm' \
        ! -name 'qt_ar.qm' \
        ! -name 'qt_es.qm' \
        ! -name 'qt_fr.qm' \
        ! -name 'qt_de.qm' \
        ! -name 'qt_tr.qm' \
        ! -name 'qt_ru.qm' \
        ! -name 'qt_zh_CN.qm' \
        -delete
}

strip_elf_files() {
    local root_dir="$1"
    command -v file >/dev/null 2>&1 || return 0
    command -v strip >/dev/null 2>&1 || return 0

    local file_path=""
    while IFS= read -r -d '' file_path; do
        if file -b "${file_path}" 2>/dev/null | grep -Eq 'ELF .* (executable|shared object|pie executable)'; then
            strip --strip-unneeded "${file_path}" 2>/dev/null || true
        fi
    done < <(find "${root_dir}" -type f -print0)
}

verify_no_unresolved_elf_dependencies() {
    local root_dir="$1"
    command -v file >/dev/null 2>&1 || return 0
    command -v ldd >/dev/null 2>&1 || return 0

    local file_path=""
    local failures=""
    while IFS= read -r -d '' file_path; do
        if ! file -b "${file_path}" 2>/dev/null | grep -Eq 'ELF .* (executable|shared object|pie executable)'; then
            continue
        fi

        local ldd_output=""
        ldd_output="$(ldd "${file_path}" 2>&1 || true)"
        if printf '%s\n' "${ldd_output}" | grep -q 'not found'; then
            failures="${failures}
${file_path}
${ldd_output}"
        fi
    done < <(find "${root_dir}" -type f -print0)

    [ -z "${failures}" ] || die "unresolved bundled runtime dependencies:${failures}"
}

rewrite_desktop_entry() {
    local source_path="$1"
    local destination_path="$2"

    awk '
        /^Exec=/ { print "Exec=/usr/bin/RevaPlayer %U"; next }
        /^TryExec=/ { print "TryExec=/usr/bin/RevaPlayer"; next }
        { print }
        END {
            print "X-Reva-Bundle=offline-rpm"
        }
    ' "${source_path}" >"${destination_path}"
}

write_wrapper_binary() {
    local wrapper_path="$1"

    cat >"${wrapper_path}" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

SELF_PATH="$(readlink -f "$0" 2>/dev/null || printf '%s\n' "$0")"
APP_ROOT="$(cd -- "$(dirname -- "${SELF_PATH}")/.." && pwd)"

prepend_path() {
    local candidate="$1"
    local current_value="$2"

    if [ ! -d "${candidate}" ]; then
        printf '%s' "${current_value}"
        return 0
    fi

    if [ -n "${current_value}" ]; then
        printf '%s:%s' "${candidate}" "${current_value}"
        return 0
    fi

    printf '%s' "${candidate}"
}

ld_library_path="${LD_LIBRARY_PATH:-}"
for library_dir in \
    "${APP_ROOT}/lib" \
    "${APP_ROOT}/lib/$(uname -m)-linux-gnu" \
    "${APP_ROOT}/lib/x86_64-linux-gnu" \
    "${APP_ROOT}/lib/aarch64-linux-gnu"; do
    ld_library_path="$(prepend_path "${library_dir}" "${ld_library_path}")"
done

qt_plugin_path="${QT_PLUGIN_PATH:-}"
for plugin_dir in \
    "${APP_ROOT}/plugins" \
    "${APP_ROOT}/lib/plugins"; do
    qt_plugin_path="$(prepend_path "${plugin_dir}" "${qt_plugin_path}")"
done

export PATH="${APP_ROOT}/bin:${PATH:-/usr/bin:/bin}"
export LD_LIBRARY_PATH="${ld_library_path}"
export QT_PLUGIN_PATH="${qt_plugin_path}"
export XDG_DATA_DIRS="$(prepend_path "/usr/share" "${XDG_DATA_DIRS:-/usr/local/share:/usr/share}")"
export REVA_USE_HOST_FILE_DIALOGS="${REVA_USE_HOST_FILE_DIALOGS:-1}"

if [ -z "${QT_QPA_PLATFORM:-}" ]; then
    if [ -n "${WAYLAND_DISPLAY:-}" ] || [ "${XDG_SESSION_TYPE:-}" = "wayland" ]; then
        if [ -f "${APP_ROOT}/plugins/platforms/libqwayland-egl.so" ] || [ -f "${APP_ROOT}/plugins/platforms/libqwayland-generic.so" ]; then
            export QT_QPA_PLATFORM="wayland;xcb"
        elif [ -f "${APP_ROOT}/plugins/platforms/libqxcb.so" ]; then
            export QT_QPA_PLATFORM="xcb"
        fi
    elif [ -f "${APP_ROOT}/plugins/platforms/libqxcb.so" ]; then
        export QT_QPA_PLATFORM="xcb"
    fi
fi

exec "${APP_ROOT}/bin/RevaPlayer.bin" "$@"
EOF

    chmod +x "${wrapper_path}"
}

write_qt_conf() {
    local path="$1"

    cat >"${path}" <<'EOF'
[Paths]
Prefix = ..
Plugins = plugins
Translations = translations
Imports = qml
Qml2Imports = qml
EOF
}

normalize_package_permissions() {
    local package_root="$1"
    local bundle_root="$2"

    find "${package_root}" -type d -exec chmod 0755 {} +
    find "${bundle_root}/lib" "${bundle_root}/plugins" "${bundle_root}/translations" "${bundle_root}/share" -type f -exec chmod 0644 {} +

    chmod 0755 "${bundle_root}/bin/${APP_BINARY_NAME}" \
               "${bundle_root}/bin/${APP_BINARY_NAME}.bin" \
               "${package_root}/usr/bin/${APP_BINARY_NAME}" \
               "${package_root}/usr/bin/revaplayer" \
               "${package_root}/usr/bin/reva"
    chmod 0644 "${bundle_root}/bin/qt.conf" \
               "${package_root}/usr/share/applications/${DESKTOP_ID}" \
               "${package_root}/usr/share/metainfo/${APP_ID}.metainfo.xml" \
               "${package_root}/usr/share/icons/hicolor/scalable/apps/${ICON_NAME}.svg" \
               "${package_root}/usr/share/doc/revaplayer/README.md" \
               "${package_root}/usr/share/doc/revaplayer/LICENSE" \
               "${package_root}/usr/share/doc/revaplayer/THIRD_PARTY_NOTICES.md"
}

write_spec_file() {
    local spec_path="$1"
    local payload_root="$2"
    local version="$3"

    cat >"${spec_path}" <<EOF
Name:           ${PACKAGE_NAME}
Version:        ${version}
Release:        ${PACKAGE_RELEASE}%{?dist}
Summary:        Reva Player bundled offline RPM package
License:        GPL-2.0-or-later
URL:            https://github.com/moayad30/Reva_Player
AutoReqProv:    no
Requires:       bash

%description
Reva Player media player bundled with its Qt/libmpv runtime. This RPM installs
under /opt/revaplayer and is intended for RPM-based desktops where offline
installation matters more than strict distribution-native dependency policy.

%prep

%build

%install
rm -rf "%{buildroot}"
mkdir -p "%{buildroot}"
cp -a "${payload_root}/." "%{buildroot}/"

%post
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi

%postun
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi

%files
%defattr(-,root,root,-)
${PACKAGE_INSTALL_ROOT}
/usr/bin/${APP_BINARY_NAME}
/usr/bin/revaplayer
/usr/bin/reva
/usr/share/applications/${DESKTOP_ID}
/usr/share/metainfo/${APP_ID}.metainfo.xml
/usr/share/icons/hicolor/scalable/apps/${ICON_NAME}.svg
/usr/share/doc/revaplayer/README.md
/usr/share/doc/revaplayer/LICENSE
/usr/share/doc/revaplayer/THIRD_PARTY_NOTICES.md
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --build-dir)
            build_dir="$2"
            shift 2
            ;;
        --install-root)
            install_root="$2"
            shift 2
            ;;
        --bundle-source)
            bundle_source="$2"
            shift 2
            ;;
        --staging-root)
            staging_root="$2"
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
        --version)
            app_version="$2"
            shift 2
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

case "${qt_major}" in
    5|6)
        ;;
    *)
        die "--qt-major must be 5 or 6"
        ;;
esac

build_dir="$(absolute_path "${build_dir}")"
install_root="$(absolute_path "${install_root}")"
if [ -n "${bundle_source}" ]; then
    bundle_source="$(absolute_path "${bundle_source}")"
fi
staging_root="$(absolute_path "${staging_root}")"
output_dir="$(absolute_path "${output_dir}")"

ensure_tool cmake
ensure_tool rpmbuild

cmake_generator="$(detect_generator)"
cmake -S "${PROJECT_ROOT}" -B "${build_dir}" -G "${cmake_generator}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DREVAPLAYER_QT_MAJOR="${qt_major}"

bundle_source="$(detect_bundle_source || true)"
[ -d "${bundle_source}" ] || die "bundle source directory not found: ${bundle_source}"

version="$(detect_version)"
rpm_arch="$(detect_rpm_arch)"
package_root="${staging_root}/package-root"
bundle_root="${package_root}${PACKAGE_INSTALL_ROOT}"
rpmbuild_root="${staging_root}/rpmbuild"
spec_path="${rpmbuild_root}/SPECS/${PACKAGE_NAME}.spec"

find "${output_dir}" -type f -name "${PACKAGE_NAME}-${version}-${PACKAGE_RELEASE}*.${rpm_arch}.rpm" -delete 2>/dev/null || true

rm -rf "${install_root}" "${package_root}" "${rpmbuild_root}"
mkdir -p "${output_dir}" \
         "${bundle_root}/bin" \
         "${bundle_root}/lib" \
         "${bundle_root}/plugins" \
         "${bundle_root}/translations" \
         "${bundle_root}/share" \
         "${package_root}/usr/bin" \
         "${package_root}/usr/share/applications" \
         "${package_root}/usr/share/metainfo" \
         "${package_root}/usr/share/icons/hicolor/scalable/apps" \
         "${package_root}/usr/share/doc/revaplayer" \
         "${rpmbuild_root}/BUILD" \
         "${rpmbuild_root}/BUILDROOT" \
         "${rpmbuild_root}/RPMS" \
         "${rpmbuild_root}/RPMDB" \
         "${rpmbuild_root}/SOURCES" \
         "${rpmbuild_root}/SPECS" \
         "${rpmbuild_root}/SRPMS" \
         "${staging_root}/tmp"

cmake --build "${build_dir}" --parallel
cmake --install "${build_dir}" --prefix "${install_root}"

[ -f "${install_root}/bin/${APP_BINARY_NAME}" ] || die "installed binary not found after cmake --install"
[ -f "${install_root}/share/applications/${DESKTOP_ID}" ] || die "desktop file missing after cmake --install"
[ -f "${install_root}/share/metainfo/${APP_ID}.metainfo.xml" ] || die "metainfo file missing after cmake --install"

copy_directory_contents "${bundle_source}/lib" "${bundle_root}/lib"
copy_directory_contents "${bundle_source}/plugins" "${bundle_root}/plugins"
copy_directory_contents "${bundle_source}/translations" "${bundle_root}/translations"
copy_directory_contents "${install_root}/share/revaplayer" "${bundle_root}/share/revaplayer"

[ -f "${bundle_root}/lib/libmpv.so.2" ] || die "bundled libmpv missing from ${bundle_root}/lib"
[ -f "${bundle_root}/plugins/platforms/libqxcb.so" ] || die "bundled Qt xcb platform plugin missing"
[ -f "${bundle_root}/plugins/sqldrivers/libqsqlite.so" ] || die "bundled Qt SQLite plugin missing"

qmake_binary="$(detect_qmake || true)"
qt_plugins_dir="$(detect_qt_plugins_dir "${qmake_binary}" || true)"
qt_translations_dir="$(detect_qt_translations_dir "${qmake_binary}" || true)"
copy_optional_qt_runtime_plugins "${qt_plugins_dir}" "${bundle_root}/plugins"
copy_optional_platform_theme_plugins "${qt_plugins_dir}" "${bundle_root}/plugins/platformthemes"
copy_optional_qt_runtime_libraries "${bundle_root}/lib"
copy_supported_qt_translations "${qt_translations_dir}" "${bundle_root}/translations"
prune_translations "${bundle_root}/translations"

cp -f "${install_root}/bin/${APP_BINARY_NAME}" "${bundle_root}/bin/${APP_BINARY_NAME}.bin"
write_wrapper_binary "${bundle_root}/bin/${APP_BINARY_NAME}"
write_qt_conf "${bundle_root}/bin/qt.conf"

cat >"${package_root}/usr/bin/${APP_BINARY_NAME}" <<EOF
#!/usr/bin/env bash
exec ${PACKAGE_INSTALL_ROOT}/bin/${APP_BINARY_NAME} "\$@"
EOF
chmod 0755 "${package_root}/usr/bin/${APP_BINARY_NAME}"
ln -sfn "${APP_BINARY_NAME}" "${package_root}/usr/bin/revaplayer"
ln -sfn "${APP_BINARY_NAME}" "${package_root}/usr/bin/reva"

rewrite_desktop_entry "${install_root}/share/applications/${DESKTOP_ID}" \
    "${package_root}/usr/share/applications/${DESKTOP_ID}"
cp -f "${install_root}/share/metainfo/${APP_ID}.metainfo.xml" \
    "${package_root}/usr/share/metainfo/${APP_ID}.metainfo.xml"
cp -f "${install_root}/share/icons/hicolor/scalable/apps/${ICON_NAME}.svg" \
    "${package_root}/usr/share/icons/hicolor/scalable/apps/${ICON_NAME}.svg"
cp -f "${PROJECT_ROOT}/README.md" "${package_root}/usr/share/doc/revaplayer/README.md"
cp -f "${PROJECT_ROOT}/LICENSE" "${package_root}/usr/share/doc/revaplayer/LICENSE"
cp -f "${PROJECT_ROOT}/THIRD_PARTY_NOTICES.md" "${package_root}/usr/share/doc/revaplayer/THIRD_PARTY_NOTICES.md"

strip_elf_files "${bundle_root}"
verify_no_unresolved_elf_dependencies "${bundle_root}"
normalize_package_permissions "${package_root}" "${bundle_root}"
write_spec_file "${spec_path}" "${package_root}" "${version}"

rpmbuild -bb \
    --define "_topdir ${rpmbuild_root}" \
    --define "_rpmdir ${output_dir}" \
    --define "_dbpath ${rpmbuild_root}/RPMDB" \
    --define "_tmppath ${staging_root}/tmp" \
    --define "_build_id_links none" \
    "${spec_path}" >/dev/null

latest_package="$(find "${output_dir}" -type f -name "${PACKAGE_NAME}-${version}-${PACKAGE_RELEASE}*.${rpm_arch}.rpm" -printf '%T@ %p\n' \
    | sort -nr \
    | awk 'NR == 1 {print $2}')"

[ -n "${latest_package}" ] || die "RPM package was not created in ${output_dir}"
printf '%s bundled RPM package: %s\n' "${APP_DISPLAY_NAME}" "${latest_package}"
