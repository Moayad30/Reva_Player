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

build_dir="${PROJECT_ROOT}/build-appimage"
appdir="${PROJECT_ROOT}/dist/AppDir"
output_dir="${PROJECT_ROOT}/dist/appimage"
qt_major="6"
generator=""
app_version=""
linuxdeploy_path="${LINUXDEPLOY:-}"
linuxdeploy_qt_path="${LINUXDEPLOY_PLUGIN_QT:-}"
appimagetool_path="${APPIMAGETOOL:-}"
runtime_file="${APPIMAGE_RUNTIME:-}"
validate_metadata="1"

usage() {
    cat <<'EOF'
Usage: scripts/build-appimage.sh [options]

Build a release AppImage for Reva Player using CMake + linuxdeploy.

Options:
  --build-dir DIR                CMake build directory
  --appdir DIR                   Intermediate AppDir directory
  --output-dir DIR               Final AppImage output directory
  --version VERSION              Override AppImage version
  --qt-major 5|6                 Qt major version to package (default: 6)
  --generator NAME               Force a CMake generator
  --linuxdeploy PATH             Path to linuxdeploy
  --linuxdeploy-plugin-qt PATH   Path to linuxdeploy Qt plugin
  --appimagetool PATH            Path to appimagetool
  --runtime-file PATH            Path to a pre-downloaded AppImage runtime
  --no-validate                  Skip desktop/metainfo validation
  --help                         Show this help

Environment overrides:
  LINUXDEPLOY
  LINUXDEPLOY_PLUGIN_QT
  APPIMAGETOOL
  APPIMAGE_RUNTIME
  APP_VERSION
EOF
}

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

find_tool() {
    local explicit_path="$1"
    local command_name="$2"

    if [ -n "${explicit_path}" ]; then
        printf '%s\n' "${explicit_path}"
        return 0
    fi

    if command -v "${command_name}" >/dev/null 2>&1; then
        command -v "${command_name}"
        return 0
    fi

    return 1
}

ensure_executable() {
    local path="$1"
    [ -e "${path}" ] || die "missing executable: ${path}"
    if [ ! -x "${path}" ]; then
        chmod +x "${path}"
    fi
}

is_appimage_tool() {
    local tool_path="$1"
    local magic_hex=""

    if [[ "${tool_path}" == *.AppImage ]]; then
        return 0
    fi

    if command -v xxd >/dev/null 2>&1; then
        magic_hex="$(xxd -p -l 3 -s 8 "${tool_path}" 2>/dev/null || true)"
        if [ "${magic_hex}" = "414902" ]; then
            return 0
        fi
    fi

    if command -v file >/dev/null 2>&1 && file -b "${tool_path}" 2>/dev/null | grep -Fq 'AppImage'; then
        return 0
    fi

    return 1
}

appimage_tool_cache_dir=""
extract_appimage_tool() {
    local tool_path="$1"
    local tool_name
    tool_name="$(basename -- "${tool_path}")"
    tool_name="${tool_name//[^A-Za-z0-9._-]/_}"

    if [ -z "${appimage_tool_cache_dir}" ]; then
        appimage_tool_cache_dir="$(mktemp -d)"
    fi

    local extract_dir="${appimage_tool_cache_dir}/${tool_name}"
    local runner_path="${extract_dir}/squashfs-root/AppRun"

    if [ ! -x "${runner_path}" ]; then
        mkdir -p "${extract_dir}"
        (
            cd -- "${extract_dir}"
            "${tool_path}" --appimage-extract >/dev/null
        )
    fi

    [ -x "${runner_path}" ] || die "failed to extract AppImage tool: ${tool_path}"
    printf '%s\n' "${runner_path}"
}

resolve_tool_runner() {
    local tool_path="$1"

    if is_appimage_tool "${tool_path}"; then
        extract_appimage_tool "${tool_path}"
        return 0
    fi

    printf '%s\n' "${tool_path}"
}

detect_version() {
    if [ -n "${app_version}" ]; then
        printf '%s\n' "${app_version}"
        return 0
    fi

    if [ -n "${APP_VERSION:-}" ]; then
        printf '%s\n' "${APP_VERSION}"
        return 0
    fi

    local version
    version="$(sed -nE 's/^project\(RevaPlayer VERSION ([^ )]+).*/\1/p' "${PROJECT_ROOT}/CMakeLists.txt" | head -n 1)"
    [ -n "${version}" ] || die "could not detect project version from CMakeLists.txt"
    printf '%s\n' "${version}"
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

maybe_validate_desktop_assets() {
    if [ "${validate_metadata}" != "1" ]; then
        return 0
    fi

    local desktop_file="$1"
    local metainfo_file="$2"

    if command -v desktop-file-validate >/dev/null 2>&1; then
        desktop-file-validate "${desktop_file}"
    fi

    if command -v appstreamcli >/dev/null 2>&1; then
        appstreamcli validate --no-net "${metainfo_file}"
    fi
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

exec "\${APPDIR}/usr/bin/${real_binary_name}" "\$@"
EOF

    chmod +x "${wrapper_path}"
}

write_qt_conf() {
    local path="$1"

    cat >"${path}" <<'EOF'
[Paths]
Prefix = ../
Plugins = plugins
Translations = translations
Imports = qml
Qml2Imports = qml
EOF
}

linuxdeploy_shim_dir=""
cleanup() {
    if [ -n "${linuxdeploy_shim_dir}" ] && [ -d "${linuxdeploy_shim_dir}" ]; then
        rm -rf "${linuxdeploy_shim_dir}"
    fi
    if [ -n "${appimage_tool_cache_dir}" ] && [ -d "${appimage_tool_cache_dir}" ]; then
        rm -rf "${appimage_tool_cache_dir}"
    fi
}
trap cleanup EXIT

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
             "${destination_root}/iconengines" \
             "${destination_root}/wayland-decoration-client" \
             "${destination_root}/wayland-graphics-integration-client" \
             "${destination_root}/wayland-shell-integration"

    local file_path=""
    for file_path in \
        "${plugins_dir}/platforms/libqwayland.so" \
        "${plugins_dir}/platforms/libqwayland-egl.so" \
        "${plugins_dir}/platforms/libqwayland-generic.so" \
        "${plugins_dir}/platforms/libqminimal.so" \
        "${plugins_dir}/platforms/libqoffscreen.so"; do
        if [ -f "${file_path}" ]; then
            cp -f "${file_path}" "${destination_root}/platforms/"
        fi
    done

    if [ -f "${plugins_dir}/iconengines/libqsvgicon.so" ]; then
        cp -f "${plugins_dir}/iconengines/libqsvgicon.so" "${destination_root}/iconengines/"
    fi

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
        /usr/lib/x86_64-linux-gnu/libQt6DBus.so.6 \
        /usr/lib/x86_64-linux-gnu/libQt5DBus.so.5 \
        /usr/lib/x86_64-linux-gnu/libQt6WaylandClient.so.6 \
        /usr/lib/x86_64-linux-gnu/libQt6WaylandEglClientHwIntegration.so.6; do
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
            libQt6WaylandEglClientHwIntegration.so.6; do
            detected_path="$(ldconfig -p 2>/dev/null | awk -v library_name="${library_name}" '$1 == library_name && detected_path == "" { detected_path = $NF } END { print detected_path }')"
            if [ -f "${detected_path}" ]; then
                cp -f "${detected_path}" "${destination_root}/"
            fi
        done
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
    local plugins_dir="$1"
    local destination_dir="${appdir}/usr/plugins/platformthemes"
    mkdir -p "${destination_dir}"

    for plugin_name in libqgtk3.so KDEPlasmaPlatformTheme6.so KDEPlasmaPlatformTheme5.so; do
        if [ -f "${plugins_dir}/platformthemes/${plugin_name}" ]; then
            cp -f "${plugins_dir}/platformthemes/${plugin_name}" "${destination_dir}/${plugin_name}"
        fi
    done
}

find_system_library() {
    local library_name="$1"
    local detected_path=""

    if command -v ldconfig >/dev/null 2>&1; then
        detected_path="$(ldconfig -p 2>/dev/null | awk -v library_name="${library_name}" '$1 == library_name { print $NF; exit }')"
        if [ -f "${detected_path}" ]; then
            printf '%s\n' "${detected_path}"
            return 0
        fi
    fi

    local root_dir=""
    for root_dir in \
        /lib64 \
        /usr/lib64 \
        /usr/lib64/samba \
        /lib/x86_64-linux-gnu \
        /usr/lib/x86_64-linux-gnu \
        /lib/aarch64-linux-gnu \
        /usr/lib/aarch64-linux-gnu; do
        if [ -f "${root_dir}/${library_name}" ]; then
            printf '%s\n' "${root_dir}/${library_name}"
            return 0
        fi
    done

    return 1
}

restore_system_runtime_payload() {
    if [ -f "${build_dir}/${APP_BINARY_NAME}" ]; then
        cp -f "${build_dir}/${APP_BINARY_NAME}" "${packaged_binary_path}"
    fi

    local file_path=""
    local library_name=""
    local source_path=""
    if [ -d "${appdir}/usr/lib" ]; then
        while IFS= read -r -d '' file_path; do
            library_name="$(basename -- "${file_path}")"
            source_path="$(find_system_library "${library_name}" || true)"
            if [ -f "${source_path}" ]; then
                cp -f "${source_path}" "${file_path}"
            fi
        done < <(find "${appdir}/usr/lib" -maxdepth 1 -type f -print0)
    fi

    if [ -n "${qt_plugins_dir}" ] && [ -d "${qt_plugins_dir}" ] && [ -d "${appdir}/usr/plugins" ]; then
        local relative_path=""
        while IFS= read -r -d '' file_path; do
            relative_path="${file_path#${appdir}/usr/plugins/}"
            source_path="${qt_plugins_dir}/${relative_path}"
            if [ -f "${source_path}" ]; then
                cp -f "${source_path}" "${file_path}"
            fi
        done < <(find "${appdir}/usr/plugins" -type f -name '*.so' -print0)
    fi
}

should_keep_runtime_library() {
    local library_name="$1"

    case "${library_name}" in
        libQt6*.so.6*|\
        libmpv.so.2*|\
        libavcodec.so.*|\
        libavdevice.so.*|\
        libavfilter.so.*|\
        libavformat.so.*|\
        libavutil.so.*|\
        libswresample.so.*|\
        libswscale.so.*|\
        libass.so.*|\
        libbluray.so.*|\
        libcdio*.so.*|\
        libdvd*.so.*|\
        liblcms2.so.*|\
        liblua-5.1.so*|\
        libmujs.so.*|\
        libplacebo.so.*|\
        librubberband.so.*|\
        libsqlite3.so.*|\
        libuchardet.so.*|\
        libzimg.so.*)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

prune_runtime_libraries() {
    local library_dir="${appdir}/usr/lib"
    [ -d "${library_dir}" ] || return 0

    local file_path=""
    local library_name=""
    while IFS= read -r -d '' file_path; do
        library_name="$(basename -- "${file_path}")"
        if ! should_keep_runtime_library "${library_name}"; then
            rm -f "${file_path}"
        fi
    done < <(find "${library_dir}" -maxdepth 1 -type f -print0)
}

prune_appdir_payload() {
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
        ldd_output="$(LD_LIBRARY_PATH="${appdir}/usr/lib:${appdir}/usr/lib64:${appdir}/usr/lib/$(uname -m)-linux-gnu:${appdir}/usr/lib/x86_64-linux-gnu:${appdir}/usr/lib/aarch64-linux-gnu:${LD_LIBRARY_PATH:-}" ldd "${file_path}" 2>&1 || true)"
        if printf '%s\n' "${ldd_output}" | grep -q 'not found'; then
            failures="${failures}
${file_path}
${ldd_output}"
        fi
    done < <(find "${root_dir}" -type f -print0)

    [ -z "${failures}" ] || die "unresolved AppImage runtime dependencies:${failures}"
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --build-dir)
            build_dir="$2"
            shift 2
            ;;
        --appdir)
            appdir="$2"
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
        --qt-major)
            qt_major="$2"
            shift 2
            ;;
        --generator)
            generator="$2"
            shift 2
            ;;
        --linuxdeploy)
            linuxdeploy_path="$2"
            shift 2
            ;;
        --linuxdeploy-plugin-qt)
            linuxdeploy_qt_path="$2"
            shift 2
            ;;
        --appimagetool)
            appimagetool_path="$2"
            shift 2
            ;;
        --runtime-file)
            runtime_file="$2"
            shift 2
            ;;
        --no-validate)
            validate_metadata="0"
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

case "${qt_major}" in
    5|6)
        ;;
    *)
        die "--qt-major must be 5 or 6"
        ;;
esac

app_version="$(detect_version)"
cmake_generator="$(detect_generator)"
linuxdeploy_path="$(find_tool "${linuxdeploy_path}" linuxdeploy || true)"
linuxdeploy_qt_path="$(find_tool "${linuxdeploy_qt_path}" linuxdeploy-plugin-qt || true)"
appimagetool_path="$(find_tool "${appimagetool_path}" appimagetool || true)"
[ -n "${linuxdeploy_path}" ] || die "linuxdeploy not found; set --linuxdeploy or LINUXDEPLOY"
[ -n "${linuxdeploy_qt_path}" ] || die "linuxdeploy-plugin-qt not found; set --linuxdeploy-plugin-qt or LINUXDEPLOY_PLUGIN_QT"
[ -n "${appimagetool_path}" ] || die "appimagetool not found; set --appimagetool or APPIMAGETOOL"
ensure_executable "${linuxdeploy_path}"
ensure_executable "${linuxdeploy_qt_path}"
ensure_executable "${appimagetool_path}"
linuxdeploy_runner="$(resolve_tool_runner "${linuxdeploy_path}")"
linuxdeploy_qt_runner="$(resolve_tool_runner "${linuxdeploy_qt_path}")"
appimagetool_runner="$(resolve_tool_runner "${appimagetool_path}")"

qmake_path="$(detect_qmake || true)"
[ -n "${qmake_path}" ] || die "qmake not found; install Qt build tools for the selected Qt version"
qt_plugins_dir="$(detect_qt_plugins_dir "${qmake_path}" || true)"
qt_translations_dir="$(detect_qt_translations_dir "${qmake_path}" || true)"

readonly desktop_source="${PROJECT_ROOT}/dist/linux/${DESKTOP_ID}"
readonly metainfo_source="${PROJECT_ROOT}/dist/linux/${APP_ID}.metainfo.xml"
readonly icon_source="${PROJECT_ROOT}/resources/icons/${ICON_NAME}.svg"
readonly binary_path="${appdir}/usr/bin/${APP_BINARY_NAME}"
readonly packaged_binary_path="${appdir}/usr/bin/${APP_BINARY_NAME}.bin"
readonly installed_desktop_path="${appdir}/usr/share/applications/${DESKTOP_ID}"
readonly installed_metainfo_path="${appdir}/usr/share/metainfo/${APP_ID}.metainfo.xml"
readonly installed_appdata_path="${appdir}/usr/share/metainfo/${APP_ID}.appdata.xml"
readonly appimage_arch="$(uname -m)"
readonly output_name="${APP_PACKAGE_STEM}-v${app_version}-${appimage_arch}.AppImage"
readonly output_path="${output_dir}/${output_name}"

mkdir -p "${output_dir}"
rm -rf "${appdir}"

cmake -S "${PROJECT_ROOT}" -B "${build_dir}" -G "${cmake_generator}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DREVAPLAYER_QT_MAJOR="${qt_major}"

cmake --build "${build_dir}" --parallel
DESTDIR="${appdir}" cmake --install "${build_dir}"

[ -f "${binary_path}" ] || die "installed binary not found at ${binary_path}"
[ -f "${installed_desktop_path}" ] || die "installed desktop file not found at ${installed_desktop_path}"
[ -f "${installed_metainfo_path}" ] || die "installed metainfo file not found at ${installed_metainfo_path}"

cp -f "${installed_metainfo_path}" "${installed_appdata_path}"

mv "${binary_path}" "${packaged_binary_path}"
write_wrapper_binary "${binary_path}" "${APP_BINARY_NAME}.bin"

printf 'X-AppImage-Version=%s\n' "${app_version}" >>"${installed_desktop_path}"
printf 'X-AppImage-Arch=%s\n' "${appimage_arch}" >>"${installed_desktop_path}"

maybe_validate_desktop_assets "${installed_desktop_path}" "${installed_metainfo_path}"

appimagetool_args=(--no-appstream --comp zstd)
if [ -n "${runtime_file}" ]; then
    [ -f "${runtime_file}" ] || die "runtime file not found: ${runtime_file}"
    appimagetool_args+=(--runtime-file "${runtime_file}")
fi
appimagetool_args+=("${appdir}" "${output_path}")

linuxdeploy_shim_dir="$(mktemp -d)"
cat >"${linuxdeploy_shim_dir}/linuxdeploy-plugin-qt" <<EOF
#!/usr/bin/env bash
set -euo pipefail
"${linuxdeploy_qt_runner}" "\$@"
EOF
chmod +x "${linuxdeploy_shim_dir}/linuxdeploy-plugin-qt"

PATH="${linuxdeploy_shim_dir}:${PATH}" \
NO_STRIP="${NO_STRIP:-1}" \
QMAKE="${qmake_path}" \
"${linuxdeploy_runner}" \
    --appdir "${appdir}" \
    --desktop-file "${installed_desktop_path}" \
    --icon-file "${icon_source}" \
    --executable "${packaged_binary_path}" \
    --plugin qt

if [ -n "${qt_plugins_dir}" ]; then
    copy_optional_platform_theme_plugins "${qt_plugins_dir}"
    copy_optional_qt_runtime_plugins "${qt_plugins_dir}"
fi
copy_supported_qt_translations "${qt_translations_dir}"
copy_optional_qt_runtime_libraries
write_qt_conf "${appdir}/usr/bin/qt.conf"

restore_system_runtime_payload
prune_bundled_qt_plugins "${appdir}/usr/plugins"
prune_bundled_system_libraries "${appdir}/usr/lib"
bundle_recursive_elf_dependencies "${appdir}" "${appdir}/usr/lib"
prune_bundled_system_libraries "${appdir}/usr/lib"
patch_bundle_elf_rpaths "${appdir}/usr"
prune_appdir_payload
if [ "${REVAPLAYER_APPIMAGE_STRIP:-0}" = "1" ]; then
    strip_elf_files "${appdir}"
fi
verify_bundled_elf_dependencies "${appdir}" "${appdir}/usr/lib"

ln -sfn "usr/bin/${APP_BINARY_NAME}" "${appdir}/AppRun"
ln -sfn "usr/share/applications/${DESKTOP_ID}" "${appdir}/${DESKTOP_ID}"
ln -sfn "usr/share/icons/hicolor/scalable/apps/${ICON_NAME}.svg" "${appdir}/${ICON_NAME}.svg"
ln -sfn "${ICON_NAME}.svg" "${appdir}/.DirIcon"

rm -f "${output_path}"
ARCH="${appimage_arch}" "${appimagetool_runner}" "${appimagetool_args[@]}"

printf '%s AppImage ready: %s\n' "${APP_DISPLAY_NAME}" "${output_path}"
