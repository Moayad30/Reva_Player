#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly PROJECT_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
readonly APP_BINARY_NAME="RevaPlayer"
readonly APP_DISPLAY_NAME="Reva Player"
readonly APP_PACKAGE_STEM="RevaPlayer"
readonly APP_ID="io.github.moayad30.revaplayer"
readonly DESKTOP_ID="${APP_ID}.desktop"
readonly ICON_NAME="revaplayer"

source "${SCRIPT_DIR}/lib/bundle-runtime.sh"

build_dir="/tmp/reva-player-build-check"
install_root="/tmp/reva-player-install-check"
source_appimage=""
work_dir="${PROJECT_ROOT}/dist/appimage-repack"
output_dir="${PROJECT_ROOT}/dist/appimage/final"
app_version=""

usage() {
    cat <<'EOF'
Usage: scripts/repack-appimage-fallback.sh [options]

Repack Reva Player into a fresh AppImage by reusing the runtime and bundled
libraries from an existing AppImage when linuxdeploy/appimagetool are not
available locally.

Options:
  --build-dir DIR        Existing CMake build directory (default: /tmp/reva-player-build-check)
  --install-root DIR     Temporary install root (default: /tmp/reva-player-install-check)
  --source-appimage FILE Existing AppImage used as runtime/base payload
  --work-dir DIR         Temporary repack work directory
  --output-dir DIR       Final output directory
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

detect_source_appimage() {
    local candidate=""
    local pattern=""

    for pattern in \
        "${PROJECT_ROOT}/dist/appimage/final/RevaPlayer-v"*.AppImage \
        "${PROJECT_ROOT}/dist/appimage/RevaPlayer-v"*.AppImage \
        "${PROJECT_ROOT}/dist/appimage/RevaPlayer-"*.AppImage; do
        for candidate in ${pattern}; do
            if [ -f "${candidate}" ]; then
                printf '%s\n' "${candidate}"
                return 0
            fi
        done
    done

    return 1
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

write_wrapper_binary() {
    local wrapper_path="$1"
    local real_binary_name="$2"

    cat >"${wrapper_path}" <<EOF
#!/usr/bin/env bash
set -euo pipefail

SELF_PATH="\$(readlink -f "\$0" 2>/dev/null || printf '%s\n' "\$0")"
APPDIR="\$(cd -- "\$(dirname -- "\${SELF_PATH}")/../.." && pwd)"

prepend_path() {
    local candidate="\$1"
    local current_value="\$2"

    if [ ! -d "\${candidate}" ]; then
        printf '%s' "\${current_value}"
        return 0
    fi

    if [ -n "\${current_value}" ]; then
        printf '%s:%s' "\${candidate}" "\${current_value}"
        return 0
    fi

    printf '%s' "\${candidate}"
}

bundled_plugin_exists() {
    local plugin_subpath="\$1"
    for plugin_root in \\
        "\${APPDIR}/usr/plugins" \\
        "\${APPDIR}/usr/lib/plugins"; do
        if [ -f "\${plugin_root}/\${plugin_subpath}" ]; then
            return 0
        fi
    done
    return 1
}

ld_library_path="\${LD_LIBRARY_PATH:-}"
for library_dir in \\
    "\${APPDIR}/usr/lib" \\
    "\${APPDIR}/usr/lib64" \\
    "\${APPDIR}/usr/lib/\$(uname -m)-linux-gnu" \\
    "\${APPDIR}/usr/lib/x86_64-linux-gnu" \\
    "\${APPDIR}/usr/lib/aarch64-linux-gnu" \\
    "\${APPDIR}/lib" \\
    "\${APPDIR}/lib64"; do
    ld_library_path="\$(prepend_path "\${library_dir}" "\${ld_library_path}")"
done

qt_plugin_path="\${QT_PLUGIN_PATH:-}"
for plugin_dir in \\
    "\${APPDIR}/usr/plugins" \\
    "\${APPDIR}/usr/lib/plugins" \\
    "\${APPDIR}/usr/lib/qt6/plugins" \\
    "\${APPDIR}/usr/lib/qt5/plugins"; do
    qt_plugin_path="\$(prepend_path "\${plugin_dir}" "\${qt_plugin_path}")"
done

export PATH="\${APPDIR}/usr/bin:\${PATH:-/usr/bin:/bin}"
export LD_LIBRARY_PATH="\${ld_library_path}"
export QT_PLUGIN_PATH="\${qt_plugin_path}"
export XDG_DATA_DIRS="\$(prepend_path "\${APPDIR}/usr/share" "\${XDG_DATA_DIRS:-/usr/local/share:/usr/share}")"

if [ -z "\${QT_QPA_PLATFORM:-}" ] && { [ -n "\${WAYLAND_DISPLAY:-}" ] || [ "\${XDG_SESSION_TYPE:-}" = "wayland" ]; }; then
    export QT_QPA_PLATFORM="wayland;xcb"
fi

if [ -z "\${QT_QPA_PLATFORMTHEME:-}" ]; then
    case "\${XDG_CURRENT_DESKTOP:-}:\${DESKTOP_SESSION:-}" in
        *KDE*:*|*Plasma*:*|*plasma*:*|*LXQt*:* )
            if bundled_plugin_exists "platformthemes/KDEPlasmaPlatformTheme6.so" || bundled_plugin_exists "platformthemes/KDEPlasmaPlatformTheme5.so"; then
                export QT_QPA_PLATFORMTHEME="kde"
            fi
            ;;
        *GNOME*:*|*gnome*:*|*XFCE*:*|*xfce*:*|*MATE*:*|*mate*:*|*Cinnamon*:*|*X-Cinnamon*:*|*Budgie*:*|*budgie*:*|*Pantheon*:* )
            if bundled_plugin_exists "platformthemes/libqgtk3.so"; then
                export QT_QPA_PLATFORMTHEME="gtk3"
            fi
            ;;
    esac
fi

exec -a "${APP_BINARY_NAME}" "\${APPDIR}/usr/bin/${real_binary_name}" -name "${APP_BINARY_NAME}" "\$@"
EOF

    chmod +x "${wrapper_path}"
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
    [ -d "${plugins_dir}" ] || return 0

    local destination_root="${appdir}/usr/plugins"
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
    local destination_root="${appdir}/usr/lib"
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
        if [ -e "${library_path}" ]; then
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

copy_optional_mpv_thumbnailer() {
    local destination_path="${appdir}/usr/bin/mpv"
    local source_path="${REVAPLAYER_MPV:-}"

    if [ -z "${source_path}" ]; then
        source_path="$(command -v mpv 2>/dev/null || true)"
    fi

    if [ -n "${source_path}" ] && [ -x "${source_path}" ]; then
        cp -f "${source_path}" "${destination_path}"
        chmod 0755 "${destination_path}"
    fi
}

copy_supported_qt_translations() {
    local translations_dir="$1"
    [ -d "${translations_dir}" ] || return 0

    local destination_dir="${appdir}/usr/translations"
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

copy_optional_platform_theme_plugins() {
    local destination_dir="$1"
    mkdir -p "${destination_dir}"

    for plugin_path in \
        /usr/lib/x86_64-linux-gnu/qt6/plugins/platformthemes/libqgtk3.so \
        /usr/lib/x86_64-linux-gnu/qt6/plugins/platformthemes/KDEPlasmaPlatformTheme6.so \
        /usr/lib/x86_64-linux-gnu/qt5/plugins/platformthemes/libqgtk3.so \
        /usr/lib/x86_64-linux-gnu/qt5/plugins/platformthemes/KDEPlasmaPlatformTheme5.so; do
        if [ -f "${plugin_path}" ]; then
            cp -f "${plugin_path}" "${destination_dir}/$(basename -- "${plugin_path}")"
        fi
    done
}

prune_payload() {
    local appdir="$1"
    rm -rf "${appdir}/usr/share/doc" \
           "${appdir}/usr/share/man" \
           "${appdir}/usr/share/gtk-doc" \
           "${appdir}/usr/share/info"

    if [ -d "${appdir}/usr/translations" ]; then
        find "${appdir}/usr/translations" -maxdepth 1 -type f \( -name 'qtbase_*.qm' -o -name 'qt_*.qm' \) \
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
    fi
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
    local appdir="$1"
    command -v file >/dev/null 2>&1 || return 0
    command -v ldd >/dev/null 2>&1 || return 0

    local file_path=""
    local failures=""
    while IFS= read -r -d '' file_path; do
        if ! file -b "${file_path}" 2>/dev/null | grep -Eq 'ELF .* (executable|shared object|pie executable)'; then
            continue
        fi

        local ldd_output=""
        ldd_output="$(LD_LIBRARY_PATH="${appdir}/usr/lib:${appdir}/usr/lib64:${appdir}/usr/lib/$(uname -m)-linux-gnu:${appdir}/usr/lib/x86_64-linux-gnu:${appdir}/usr/lib/aarch64-linux-gnu:${LD_LIBRARY_PATH:-}" ldd "${file_path}" 2>&1 || true)"
        if printf '%s\n' "${ldd_output}" | grep -q 'not found'; then
            failures="${failures}
${file_path}
${ldd_output}"
        fi
    done < <(find "${appdir}" -type f -print0)

    [ -z "${failures}" ] || die "unresolved AppImage runtime dependencies:${failures}"
}

extract_runtime_offset() {
    local source_file="$1"
    local offset=""

    if offset="$("${source_file}" --appimage-offset 2>/dev/null)"; then
        :
    else
        offset=""
    fi

    if [ -z "${offset}" ]; then
        offset="$(grep -abo 'hsqs' "${source_file}" | head -n 1 | cut -d: -f1)"
    fi

    [ -n "${offset}" ] || die "could not find squashfs offset in ${source_file}"
    printf '%s\n' "${offset}"
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
        --source-appimage)
            source_appimage="$2"
            shift 2
            ;;
        --work-dir)
            work_dir="$2"
            shift 2
            ;;
        --output-dir)
            output_dir="$2"
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

ensure_tool unsquashfs
ensure_tool mksquashfs
ensure_tool cmake
ensure_tool head
ensure_tool tail

[ -d "${build_dir}" ] || die "build directory not found: ${build_dir}"
if [ -z "${source_appimage}" ]; then
    source_appimage="$(detect_source_appimage || true)"
fi
[ -n "${source_appimage}" ] || die "could not auto-detect a source AppImage; pass --source-appimage explicitly"
[ -f "${source_appimage}" ] || die "source AppImage not found: ${source_appimage}"

version="$(detect_version)"
arch="$(uname -m)"
output_path="${output_dir}/${APP_PACKAGE_STEM}-v${version}-${arch}.AppImage"
runtime_path="${work_dir}/runtime"
payload_path="${work_dir}/payload.squashfs"
payload_source_path="${work_dir}/source.squashfs"
source_root="${work_dir}/source-root"
appdir="${work_dir}/AppDir"

rm -rf "${install_root}" "${source_root}" "${appdir}" "${runtime_path}" "${payload_path}" "${payload_source_path}"
mkdir -p "${output_dir}" "${work_dir}"

cmake --build "${build_dir}" --parallel
cmake --install "${build_dir}" --prefix "${install_root}/usr"

[ -f "${install_root}/usr/bin/${APP_BINARY_NAME}" ] || die "installed binary not found after cmake --install"

offset="$(extract_runtime_offset "${source_appimage}")"
tail -c +"$((offset + 1))" "${source_appimage}" >"${payload_source_path}"
unsquashfs -d "${source_root}" "${payload_source_path}" >/dev/null
mkdir -p "${appdir}"
cp -a "${source_root}/." "${appdir}/"

mkdir -p "${appdir}/usr/bin" \
         "${appdir}/usr/share/applications" \
         "${appdir}/usr/share/metainfo" \
         "${appdir}/usr/share/icons/hicolor/scalable/apps" \
         "${appdir}/usr/plugins/platformthemes"

find "${appdir}/usr/bin" -maxdepth 1 \( -type f -o -type l \) -delete
rm -f "${appdir}/AppRun" \
      "${appdir}/AppRun.wrapped" \
      "${appdir}/io.github.moayad30.revaplayer.desktop" \
      "${appdir}/${APP_ID}.desktop" \
      "${appdir}/revaplayer.svg"
find "${appdir}" -maxdepth 1 -type f \( -name '*.desktop' -o -name '*.svg' \) -delete

cp -f "${install_root}/usr/bin/${APP_BINARY_NAME}" "${appdir}/usr/bin/${APP_BINARY_NAME}.bin"
write_wrapper_binary "${appdir}/usr/bin/${APP_BINARY_NAME}" "${APP_BINARY_NAME}.bin"

cat >"${appdir}/usr/bin/qt.conf" <<'EOF'
# generated for Reva Player AppImage fallback repack
[Paths]
Prefix = ../
Plugins = plugins
Translations = translations
Imports = qml
Qml2Imports = qml
EOF

cp -f "${install_root}/usr/share/applications/${DESKTOP_ID}" "${appdir}/usr/share/applications/${DESKTOP_ID}"
cp -f "${install_root}/usr/share/metainfo/${APP_ID}.metainfo.xml" "${appdir}/usr/share/metainfo/${APP_ID}.metainfo.xml"
cp -f "${install_root}/usr/share/metainfo/${APP_ID}.metainfo.xml" "${appdir}/usr/share/metainfo/${APP_ID}.appdata.xml"
copy_directory_contents "${install_root}/usr/share/revaplayer" "${appdir}/usr/share/revaplayer"
cp -f "${PROJECT_ROOT}/resources/icons/${ICON_NAME}.svg" "${appdir}/usr/share/icons/hicolor/scalable/apps/${ICON_NAME}.svg"
printf 'X-AppImage-Version=%s\n' "${version}" >>"${appdir}/usr/share/applications/${DESKTOP_ID}"
printf 'X-AppImage-Arch=%s\n' "${arch}" >>"${appdir}/usr/share/applications/${DESKTOP_ID}"

qmake_binary="$(detect_qmake || true)"
qt_plugins_dir="$(detect_qt_plugins_dir "${qmake_binary}" || true)"
qt_translations_dir="$(detect_qt_translations_dir "${qmake_binary}" || true)"
copy_optional_qt_runtime_plugins "${qt_plugins_dir}"
copy_optional_qt_runtime_libraries
copy_supported_qt_translations "${qt_translations_dir}"
copy_optional_platform_theme_plugins "${appdir}/usr/plugins/platformthemes"
copy_optional_mpv_thumbnailer
prune_payload "${appdir}"
strip_elf_files "${appdir}"
verify_no_unresolved_elf_dependencies "${appdir}"

ln -sfn "usr/bin/${APP_BINARY_NAME}" "${appdir}/AppRun"
ln -sfn "usr/share/applications/${DESKTOP_ID}" "${appdir}/${DESKTOP_ID}"
ln -sfn "usr/share/icons/hicolor/scalable/apps/${ICON_NAME}.svg" "${appdir}/${ICON_NAME}.svg"
ln -sfn "${ICON_NAME}.svg" "${appdir}/.DirIcon"

head -c "${offset}" "${source_appimage}" >"${runtime_path}"

rm -f "${payload_path}" "${output_path}"
mksquashfs "${appdir}" "${payload_path}" \
    -noappend \
    -all-root \
    -comp zstd \
    -b 1048576 \
    -Xcompression-level 19 \
    >/dev/null

cat "${runtime_path}" "${payload_path}" >"${output_path}"
chmod +x "${output_path}"

printf '%s AppImage repacked: %s\n' "${APP_DISPLAY_NAME}" "${output_path}"
