cmake_minimum_required(VERSION 3.24)

project(RevaPlayer VERSION 1.0.0 LANGUAGES CXX)

option(BUILD_TESTING "Build tests" ON)
if (BUILD_TESTING)
    enable_testing()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install path prefix." FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerWarnings)

set(REVAPLAYER_QT_MAJOR "6" CACHE STRING "Qt major version to build against")
set_property(CACHE REVAPLAYER_QT_MAJOR PROPERTY STRINGS 6 5)

if (REVAPLAYER_QT_MAJOR STREQUAL "6")
    find_package(Qt6 REQUIRED COMPONENTS Widgets Sql DBus OpenGLWidgets)
    set(QT_VERSION_MAJOR 6)
elseif (REVAPLAYER_QT_MAJOR STREQUAL "5")
    find_package(Qt5 REQUIRED COMPONENTS Widgets Sql DBus OpenGL)
    set(QT_VERSION_MAJOR 5)
else()
    message(FATAL_ERROR "REVAPLAYER_QT_MAJOR must be 6 or 5.")
endif()
find_package(mpv REQUIRED)

add_executable(RevaPlayer
    app/main.cpp
    app/AppBootstrap.cpp
    app/AppBootstrap.hpp
    application/HistoryController.cpp
    application/HistoryController.hpp
    application/BookmarkController.cpp
    application/BookmarkController.hpp
    application/CustomCommandScript.cpp
    application/CustomCommandScript.hpp
    application/PlaybackDiagnosticsFormatter.cpp
    application/PlaybackDiagnosticsFormatter.hpp
    application/PlaybackTuning.cpp
    application/PlaybackTuning.hpp
    application/PlaybackController.cpp
    application/PlaybackController.hpp
    application/PlaylistController.cpp
    application/PlaylistController.hpp
    application/SettingsController.cpp
    application/SettingsController.hpp
    application/SubtitleStyleOptions.cpp
    application/SubtitleStyleOptions.hpp
    application/ThemeStyle.cpp
    application/ThemeStyle.hpp
    application/UiLanguage.cpp
    application/UiLanguage.hpp
    application/SnapshotController.cpp
    application/SnapshotController.hpp
    application/SnapshotPath.cpp
    application/SnapshotPath.hpp
    domain/Bookmark.hpp
    domain/ChapterInfo.hpp
    domain/CustomCommand.hpp
    domain/MediaItem.hpp
    domain/PlaybackEndReason.hpp
    domain/PlaybackDiagnostics.hpp
    domain/PlayerProfile.hpp
    domain/TrackInfo.hpp
    infrastructure/mpv/MpvCore.cpp
    infrastructure/mpv/MpvCore.hpp
    infrastructure/mpv/MpvEventBridge.cpp
    infrastructure/mpv/MpvEventBridge.hpp
    infrastructure/mpv/MpvPropertyCache.cpp
    infrastructure/mpv/MpvPropertyCache.hpp
    infrastructure/mpv/MpvRenderHost.cpp
    infrastructure/mpv/MpvRenderHost.hpp
    infrastructure/storage/SqliteStore.cpp
    infrastructure/storage/SqliteStore.hpp
    platform/DesktopIntegration.cpp
    platform/DesktopIntegration.hpp
    services/media/ThumbnailService.cpp
    services/media/ThumbnailService.hpp
    services/media/MetadataScanService.cpp
    services/media/MetadataScanService.hpp
    services/media/ThumbnailPolicy.cpp
    services/media/ThumbnailPolicy.hpp
    ui/ControlBar.cpp
    ui/ControlBar.hpp
    ui/BookmarkDialog.cpp
    ui/BookmarkDialog.hpp
    ui/FileDialogUtils.cpp
    ui/FileDialogUtils.hpp
    ui/FirstRunDialog.cpp
    ui/FirstRunDialog.hpp
    ui/HoverPreviewPopup.cpp
    ui/HoverPreviewPopup.hpp
    ui/MainWindow.cpp
    ui/MainWindow.hpp
    ui/MediaInfoDialog.cpp
    ui/MediaInfoDialog.hpp
    ui/ShortcutBinding.hpp
    ui/SettingsDialog.cpp
    ui/SettingsDialog.hpp
    ui/MediaInformationOverlay.cpp
    ui/MediaInformationOverlay.hpp
    ui/VideoViewport.cpp
    ui/VideoViewport.hpp
    resources/resources.qrc
)

target_include_directories(RevaPlayer
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(RevaPlayer
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Sql
        Qt${QT_VERSION_MAJOR}::DBus
        mpv::mpv
)

if (QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(RevaPlayer PRIVATE Qt6::OpenGLWidgets)
else()
    target_link_libraries(RevaPlayer PRIVATE Qt5::OpenGL)
endif()

revaplayer_set_project_warnings(RevaPlayer)

if (BUILD_TESTING)
    if (QT_VERSION_MAJOR EQUAL 6)
        find_package(Qt6 REQUIRED COMPONENTS Test)
    else()
        find_package(Qt5 REQUIRED COMPONENTS Test)
    endif()

    add_executable(RevaPlayer_tests
        tests/CoreTests.cpp
        application/BookmarkController.cpp
        application/BookmarkController.hpp
        application/CustomCommandScript.cpp
        application/CustomCommandScript.hpp
        application/PlaybackDiagnosticsFormatter.cpp
        application/PlaybackDiagnosticsFormatter.hpp
        application/HistoryController.cpp
        application/HistoryController.hpp
        application/PlaybackTuning.cpp
        application/PlaybackTuning.hpp
        application/PlaylistController.cpp
        application/PlaylistController.hpp
        application/SettingsController.cpp
        application/SettingsController.hpp
        application/SubtitleStyleOptions.cpp
        application/SubtitleStyleOptions.hpp
        application/ThemeStyle.cpp
        application/ThemeStyle.hpp
        application/UiLanguage.cpp
        application/UiLanguage.hpp
        application/SnapshotPath.cpp
        application/SnapshotPath.hpp
        domain/Bookmark.hpp
        domain/CustomCommand.hpp
        domain/MediaItem.hpp
        domain/PlaybackEndReason.hpp
        domain/PlaybackDiagnostics.hpp
        domain/PlayerProfile.hpp
        infrastructure/storage/SqliteStore.cpp
        infrastructure/storage/SqliteStore.hpp
        platform/DesktopIntegration.cpp
        platform/DesktopIntegration.hpp
        services/media/ThumbnailService.cpp
        services/media/ThumbnailService.hpp
        services/media/ThumbnailPolicy.cpp
        services/media/ThumbnailPolicy.hpp
        ui/ControlBar.cpp
        ui/ControlBar.hpp
        ui/FileDialogUtils.cpp
        ui/FileDialogUtils.hpp
        ui/FirstRunDialog.cpp
        ui/FirstRunDialog.hpp
        ui/SettingsDialog.cpp
        ui/SettingsDialog.hpp
        ui/ShortcutBinding.hpp
        resources/resources.qrc
        )

    target_include_directories(RevaPlayer_tests
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(RevaPlayer_tests
        PRIVATE
            Qt${QT_VERSION_MAJOR}::Test
            Qt${QT_VERSION_MAJOR}::Widgets
            Qt${QT_VERSION_MAJOR}::Sql
    )

    revaplayer_set_project_warnings(RevaPlayer_tests)
    add_test(NAME RevaPlayer_tests COMMAND RevaPlayer_tests)
    set_tests_properties(RevaPlayer_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_DATA_HOME=/tmp/revaplayer-tests"
    )
endif()

install(TARGETS RevaPlayer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    dist/linux/io.github.moayad30.revaplayer.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

install(FILES
    dist/linux/io.github.moayad30.revaplayer.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo
)

install(FILES
    resources/icons/revaplayer.svg
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
)

install(FILES
    README.md
    LICENSE
    THIRD_PARTY_NOTICES.md
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/revaplayer
)

install(FILES
    resources/mpv/thumbfast.lua
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/revaplayer/mpv/scripts
)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "revaplayer")
set(CPACK_PACKAGE_VENDOR "Reva Player")
set(CPACK_PACKAGE_CONTACT "maintainers@revaplayer.local")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Linux desktop media player built with Qt Widgets and libmpv.")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "RevaPlayer")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "revaplayer-${PROJECT_VERSION}-linux")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_STRIP_FILES ON)

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_NAME "revaplayer")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Reva Player")
set(CPACK_DEBIAN_PACKAGE_SECTION "video")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "kdialog | zenity")

if (QT_VERSION_MAJOR EQUAL 6)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6sql6-sqlite, qt6-wayland")
else()
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5sql5-sqlite, qtwayland5")
endif()

include(CPack)
