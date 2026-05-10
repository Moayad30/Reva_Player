# shellcheck shell=bash

bundle_runtime_die() {
    if declare -F die >/dev/null 2>&1; then
        die "$*"
    fi

    printf 'error: %s\n' "$*" >&2
    exit 1
}

bundle_runtime_is_elf() {
    local file_path="$1"

    [ -f "${file_path}" ] || return 1
    command -v file >/dev/null 2>&1 || return 1
    file -b "${file_path}" 2>/dev/null | grep -Eq 'ELF .* (executable|shared object|pie executable)'
}

bundle_runtime_is_portable_glvnd_library_name() {
    local library_name="$1"

    case "${library_name}" in
        libGL.so.1|\
        libGLX.so.0|\
        libEGL.so.1|\
        libGLU.so.1|\
        libglut.so.3|\
        libOpenGL.so.0|\
        libGLdispatch.so.0)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

bundle_runtime_is_portable_runtime_library_name() {
    local library_name="$1"

    case "${library_name}" in
        *)
            return 1
            ;;
    esac
}

bundle_runtime_is_system_library_name() {
    local library_name="$1"

    if bundle_runtime_is_portable_runtime_library_name "${library_name}"; then
        return 1
    fi

    case "${library_name}" in
        linux-vdso.so.*|\
        ld-linux*.so.*|\
        ld64.so.*|\
        libc.so.*|\
        libm.so.*|\
        libmvec.so.*|\
        libpthread.so.*|\
        libdl.so.*|\
        librt.so.*|\
        libutil.so.*|\
        libanl.so.*|\
        libresolv.so.*|\
        libnss_*.so.*|\
        libstdc++.so.*|\
        libgcc_s.so.*|\
        libGL.so.*|\
        libGLX.so.*|\
        libEGL.so.*|\
        libOpenGL.so.*|\
        libGLdispatch.so.*|\
        libGLU.so.*|\
        libglut.so.*|\
        libgbm.so.*|\
        libdrm.so.*|\
        libwayland-client.so.*|\
        libwayland-cursor.so.*|\
        libwayland-egl.so.*|\
        libva.so.*|\
        libva-drm.so.*|\
        libva-wayland.so.*|\
        libva-x11.so.*|\
        libvdpau.so.*|\
        libvulkan.so.*|\
        libOpenCL.so.*|\
        libvpl.so.*|\
        libmfx.so.*|\
        libcuda.so.*|\
        libnvidia-*.so.*|\
        libamd*.so.*|\
        libigc.so.*|\
        libigdgmm.so.*|\
        libX11.so.*|\
        libX11-xcb.so.*|\
        libXau.so.*|\
        libXdmcp.so.*|\
        libXext.so.*|\
        libXfixes.so.*|\
        libXi.so.*|\
        libXpresent.so.*|\
        libXrandr.so.*|\
        libXrender.so.*|\
        libXss.so.*|\
        libXv.so.*|\
        libxcb*.so.*|\
        libxkbcommon.so.*|\
        libxkbcommon-x11.so.*|\
        libfontconfig.so.*|\
        libfreetype.so.*|\
        libharfbuzz.so.*|\
        libglib-2.0.so.*|\
        libgio-2.0.so.*|\
        libgmodule-2.0.so.*|\
        libgobject-2.0.so.*|\
        libcairo.so.*|\
        libcairo-gobject.so.*|\
        libgdk_pixbuf-2.0.so.*|\
        libpango-1.0.so.*|\
        libpangocairo-1.0.so.*|\
        libpangoft2-1.0.so.*|\
        libdbus-1.so.*|\
        libsystemd.so.*|\
        libudev.so.*|\
        libelogind.so.*|\
        libasound.so.*|\
        libpulse.so.*|\
        libpulse-simple.so.*|\
        libpulse-mainloop-glib.so.*|\
        libpulsecommon-*.so|\
        libpipewire-*.so.*|\
        libwireplumber-*.so.*|\
        libjack.so.*|\
        libsmbclient.so.*|\
        libwbclient.so.*|\
        libsamba*.so.*|\
        libsmb*.so.*|\
        libndr*.so.*|\
        libdcerpc*.so.*|\
        libsamdb*.so.*|\
        libldb*.so.*|\
        libtalloc*.so.*|\
        libtdb*.so.*|\
        libtevent*.so.*|\
        *-private-samba.so|\
        *-private-samba.so.*)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

bundle_runtime_is_system_library_path() {
    local library_path="$1"

    case "${library_path}" in
        */ld-linux*.so.*|\
        */ld64.so.*|\
        */dri/*|\
        */vdpau/*|\
        */pulseaudio/*|\
        */pipewire-*/*|\
        */samba/*|\
        */nvidia/*|\
        */cuda/*)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

bundle_runtime_find_system_library() {
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

bundle_runtime_copy_library() {
    local source_path="$1"
    local destination_dir="$2"
    local library_name
    library_name="$(basename -- "${source_path}")"

    bundle_runtime_is_system_library_name "${library_name}" && return 1
    if ! bundle_runtime_is_portable_runtime_library_name "${library_name}"; then
        bundle_runtime_is_system_library_path "${source_path}" && return 1
    fi
    [ -f "${source_path}" ] || return 1

    mkdir -p "${destination_dir}"
    if [ -f "${destination_dir}/${library_name}" ]; then
        return 1
    fi

    cp -L -f "${source_path}" "${destination_dir}/${library_name}"
    chmod 0644 "${destination_dir}/${library_name}" 2>/dev/null || true
    return 0
}

bundle_portable_glvnd_libraries() {
    local destination_dir="$1"
    local library_name=""
    local source_path=""
    local missing=""

    for library_name in libGL.so.1 libGLX.so.0 libEGL.so.1 libGLU.so.1 libglut.so.3 libOpenGL.so.0 libGLdispatch.so.0; do
        source_path="$(bundle_runtime_find_system_library "${library_name}" || true)"
        if [ -f "${source_path}" ]; then
            bundle_runtime_copy_library "${source_path}" "${destination_dir}" || true
        else
            missing="${missing} ${library_name}"
        fi
    done

    [ -z "${missing}" ] || bundle_runtime_die "required GLVND runtime libraries not found:${missing}"
}

bundle_runtime_ld_library_path() {
    local primary_library_dir="$1"
    local extra_library_path="${2:-}"
    local result="${primary_library_dir}"

    if [ -n "${extra_library_path}" ]; then
        result="${result}:${extra_library_path}"
    fi
    if [ -n "${LD_LIBRARY_PATH:-}" ]; then
        result="${result}:${LD_LIBRARY_PATH}"
    fi

    printf '%s\n' "${result}"
}

bundle_runtime_parse_ldd_paths() {
    awk '
        /=>/ && $3 == "not" && $4 == "found" {
            print "MISSING:" $1
            next
        }
        /=>/ && $3 ~ /^\// {
            print $3
            next
        }
        /^[[:space:]]*\// {
            print $1
            next
        }
    '
}

bundle_recursive_elf_dependencies() {
    local root_dir="$1"
    local destination_library_dir="$2"
    local extra_library_path="${3:-}"

    command -v ldd >/dev/null 2>&1 || bundle_runtime_die "missing required tool: ldd"
    command -v file >/dev/null 2>&1 || bundle_runtime_die "missing required tool: file"

    mkdir -p "${destination_library_dir}"

    local search_path
    local pass=0
    local copied_any=0
    local unresolved=""
    local file_path=""
    local dependency=""
    local dependency_name=""
    local source_path=""
    local ldd_output=""

    while [ "${pass}" -lt 40 ]; do
        copied_any=0
        unresolved=""
        search_path="$(bundle_runtime_ld_library_path "${destination_library_dir}" "${extra_library_path}")"

        while IFS= read -r -d '' file_path; do
            bundle_runtime_is_elf "${file_path}" || continue

            ldd_output="$(LD_LIBRARY_PATH="${search_path}" ldd "${file_path}" 2>&1 || true)"
            while IFS= read -r dependency; do
                [ -n "${dependency}" ] || continue

                if [[ "${dependency}" == MISSING:* ]]; then
                    dependency_name="${dependency#MISSING:}"
                    bundle_runtime_is_system_library_name "${dependency_name}" && continue

                    source_path="$(bundle_runtime_find_system_library "${dependency_name}" || true)"
                    if [ -n "${source_path}" ] && [ -f "${source_path}" ]; then
                        if bundle_runtime_copy_library "${source_path}" "${destination_library_dir}"; then
                            copied_any=1
                        fi
                    else
                        unresolved="${unresolved}
${file_path}: ${dependency_name}"
                    fi
                    continue
                fi

                [ -f "${dependency}" ] || continue
                dependency_name="$(basename -- "${dependency}")"
                bundle_runtime_is_system_library_name "${dependency_name}" && continue
                if ! bundle_runtime_is_portable_runtime_library_name "${dependency_name}"; then
                    bundle_runtime_is_system_library_path "${dependency}" && continue
                fi

                case "${dependency}" in
                    "${root_dir}"/*)
                        continue
                        ;;
                esac

                if bundle_runtime_copy_library "${dependency}" "${destination_library_dir}"; then
                    copied_any=1
                fi
            done < <(printf '%s\n' "${ldd_output}" | bundle_runtime_parse_ldd_paths)
        done < <(find "${root_dir}" -type f -print0)

        if [ -n "${unresolved}" ]; then
            bundle_runtime_die "unresolved runtime dependencies:${unresolved}"
        fi
        if [ "${copied_any}" -eq 0 ]; then
            return 0
        fi

        pass=$((pass + 1))
    done

    bundle_runtime_die "recursive dependency bundling did not converge for ${root_dir}"
}

prune_bundled_system_libraries() {
    local library_dir="$1"
    [ -d "${library_dir}" ] || return 0

    local file_path=""
    local library_name=""
    while IFS= read -r -d '' file_path; do
        library_name="$(basename -- "${file_path}")"
        if bundle_runtime_is_portable_runtime_library_name "${library_name}"; then
            continue
        fi
        if bundle_runtime_is_system_library_name "${library_name}" \
            || bundle_runtime_is_system_library_path "${file_path}"; then
            rm -f "${file_path}"
        fi
    done < <(find "${library_dir}" -maxdepth 1 -type f -print0)
}

prune_bundled_qt_plugins() {
    local plugins_root="$1"
    [ -d "${plugins_root}" ] || return 0

    local file_path=""
    local relative_path=""
    while IFS= read -r -d '' file_path; do
        relative_path="${file_path#${plugins_root}/}"
        case "${relative_path}" in
            platforms/libqxcb.so|\
            platforms/libqwayland.so|\
            platforms/libqminimal.so|\
            platforms/libqoffscreen.so|\
            imageformats/libq*.so|\
            iconengines/libqsvgicon.so|\
            sqldrivers/libqsqlite.so|\
            xcbglintegrations/libqxcb-*.so|\
            platforminputcontexts/libcomposeplatforminputcontextplugin.so|\
            platforminputcontexts/libibusplatforminputcontextplugin.so|\
            wayland-decoration-client/*.so|\
            wayland-graphics-integration-client/*.so|\
            wayland-shell-integration/*.so)
                ;;
            *)
                rm -f "${file_path}"
                ;;
        esac
    done < <(find "${plugins_root}" -type f -name '*.so' -print0)

    find "${plugins_root}" -depth -type d -empty -delete
}

patch_bundle_elf_rpaths() {
    local bundle_root="$1"

    command -v patchelf >/dev/null 2>&1 || return 0
    command -v file >/dev/null 2>&1 || return 0

    local file_path=""
    local file_name=""
    local source_path=""
    local rpath=""
    while IFS= read -r -d '' file_path; do
        bundle_runtime_is_elf "${file_path}" || continue
        file_name="$(basename -- "${file_path}")"

        case "${file_name}" in
            libpulsecommon-*.so)
                source_path="$(bundle_runtime_find_system_library "${file_name}" || true)"
                if [ -n "${source_path}" ] && [ -f "${source_path}" ]; then
                    cp -L -f "${source_path}" "${file_path}"
                    chmod 0644 "${file_path}" 2>/dev/null || true
                fi
                continue
                ;;
        esac

        case "${file_path}" in
            */bin/*)
                rpath='$ORIGIN/../lib'
                ;;
            */lib/*)
                rpath='$ORIGIN'
                ;;
            */plugins/*)
                rpath='$ORIGIN/../../lib:$ORIGIN/../lib:$ORIGIN'
                ;;
            *)
                rpath='$ORIGIN/../lib:$ORIGIN/lib'
                ;;
        esac

        patchelf --set-rpath "${rpath}" "${file_path}" 2>/dev/null || true
    done < <(find "${bundle_root}" -type f -print0)
}

verify_bundled_elf_dependencies() {
    local root_dir="$1"
    local library_dir="$2"
    local extra_library_path="${3:-}"

    command -v ldd >/dev/null 2>&1 || return 0
    command -v file >/dev/null 2>&1 || return 0

    local search_path
    search_path="$(bundle_runtime_ld_library_path "${library_dir}" "${extra_library_path}")"

    local file_path=""
    local failures=""
    local ldd_output=""
    while IFS= read -r -d '' file_path; do
        bundle_runtime_is_elf "${file_path}" || continue

        ldd_output="$(LD_LIBRARY_PATH="${search_path}" ldd "${file_path}" 2>&1 || true)"
        if printf '%s\n' "${ldd_output}" | grep -q 'not found'; then
            failures="${failures}
${file_path}
${ldd_output}"
        fi
    done < <(find "${root_dir}" -type f -print0)

    [ -z "${failures}" ] || bundle_runtime_die "unresolved bundled runtime dependencies:${failures}"
}
