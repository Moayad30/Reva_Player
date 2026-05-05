find_path(MPV_INCLUDE_DIR
    NAMES mpv/client.h
)

find_library(MPV_LIBRARY
    NAMES mpv
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(mpv
    REQUIRED_VARS
        MPV_INCLUDE_DIR
        MPV_LIBRARY
    FAIL_MESSAGE
        "Could not find libmpv development files. Install libmpv-dev and reconfigure."
)

if(mpv_FOUND AND NOT TARGET mpv::mpv)
    add_library(mpv::mpv UNKNOWN IMPORTED)
    set_target_properties(mpv::mpv PROPERTIES
        IMPORTED_LOCATION "${MPV_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MPV_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(MPV_INCLUDE_DIR MPV_LIBRARY)

