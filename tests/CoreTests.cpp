#include "application/BookmarkController.hpp"
#include "application/CustomCommandScript.hpp"
#include "application/PlaybackDiagnosticsFormatter.hpp"
#include "application/HistoryController.hpp"
#include "application/PlaylistController.hpp"
#include "application/PlaybackTuning.hpp"
#include "application/SettingsController.hpp"
#include "application/SnapshotPath.hpp"
#include "application/SubtitleStyleOptions.hpp"
#include "application/ThemeStyle.hpp"
#include "application/UiLanguage.hpp"
#include "domain/PlaybackEndReason.hpp"
#include "domain/PlaybackDiagnostics.hpp"
#include "domain/PlayerProfile.hpp"
#include "infrastructure/storage/SqliteStore.hpp"
#include "platform/DesktopIntegration.hpp"
#include "services/media/ThumbnailService.hpp"
#include "services/media/ThumbnailPolicy.hpp"
#include "ui/ControlBar.hpp"
#include "ui/FirstRunDialog.hpp"
#include "ui/SettingsDialog.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTime>
#include <QToolButton>
#include <QUuid>
#include <QUrl>

#include <QtTest/QtTest>

#include <algorithm>
#include <memory>

namespace {

struct PlaybackHistoryRow {
    QString source;
    double positionSeconds {0.0};
    double durationSeconds {0.0};
    bool completed {false};
};

PlaybackHistoryRow loadPlaybackHistoryRow(const QString &databasePath)
{
    const QString connectionName = QStringLiteral("history-test-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(databasePath);

    PlaybackHistoryRow row;
    if (database.open()) {
        QSqlQuery query(database);
        if (query.exec(QStringLiteral(
                "SELECT source, last_position_seconds, duration_seconds, completed "
                "FROM playback_history LIMIT 1"))
            && query.next()) {
            row.source = query.value(0).toString();
            row.positionSeconds = query.value(1).toDouble();
            row.durationSeconds = query.value(2).toDouble();
            row.completed = query.value(3).toInt() != 0;
        }
        database.close();
    }

    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
    return row;
}

std::unique_ptr<revaplayer::infrastructure::storage::SqliteStore> makeStore(const QString &databasePath)
{
    return std::make_unique<revaplayer::infrastructure::storage::SqliteStore>(databasePath);
}

}  // namespace

class CoreTests final : public QObject {
    Q_OBJECT

private slots:
    void settingsPersistence();
    void settingsResetSeedsDefaults();
    void windowStatePersistence();
    void bookmarkCrud();
    void customCommandsPersistence();
    void historyResumeTransitions();
    void historyRecordOpenPreservesExistingProgress();
    void historyMultipleResumeStatesSurviveNavigation();
    void historyRejectsInvalidStoredSources();
    void playbackEndReasonSemantics();
    void playlistModelMapping();
    void playlistDisplayTitlePresentation();
    void playlistSortModes();
    void sqliteMigrationHandlesExistingBookmarkCategory();
    void screenshotPathGeneration();
    void playbackTuningHelpers();
    void videoRotationNormalization();
    void playbackDiagnosticsFormatting();
    void thumbnailPolicyProfiles();
    void subtitleStyleOptions();
    void uiLanguageTranslations();
    void uiLanguageCriticalCoverage();
    void themeResourceAndPaletteCoverage();
    void firstRunDialogDefaultsAndPersistence();
    void settingsDialogSubtitlePreviewVisibility();
    void settingsDialogSubtitleAutoLoadSync();
    void controlBarDesktopChromeVisibility();
    void controlBarButtonsEmitSignals();
    void desktopIntegrationPlatformName();
    void thumbnailServiceUnavailableWithoutBridge();
    void thumbnailServicePendingWorkerTimesOut();
    void thumbnailServiceActiveRequestTimesOut();
    void thumbnailServiceProducesAndCachesPreview();
};

void CoreTests::settingsPersistence()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    const QString databasePath = temporaryDir.filePath(QStringLiteral("settings.sqlite"));

    QCOMPARE(revaplayer::application::availableThemes().size(), 9);
    QCOMPARE(revaplayer::application::availableAccents().size(), 5);
    QCOMPARE(revaplayer::application::availableDensities().size(), 3);
    QCOMPARE(revaplayer::application::availableUiLanguages().size(), 8);
    QCOMPARE(revaplayer::application::normalizeThemeId(QStringLiteral("night")), QStringLiteral("day"));
    QCOMPARE(revaplayer::application::normalizeThemeId(QStringLiteral("unknown-theme")), QStringLiteral("gray"));
    QCOMPARE(revaplayer::application::normalizeUiLanguageId(QStringLiteral("zh_CN")), QStringLiteral("zh_CN"));
    QCOMPARE(revaplayer::application::normalizeUiLanguageId(QStringLiteral("ru")), QStringLiteral("ru"));
    QCOMPARE(revaplayer::application::normalizeUiLanguageId(QStringLiteral("unknown-language")), QStringLiteral("ar"));

    {
        revaplayer::application::SettingsController controller(makeStore(databasePath));
        QVERIFY2(controller.initialize(), qPrintable(controller.lastError()));
        QCOMPARE(controller.uiTheme(), QStringLiteral("gray"));
        controller.setShowMenuBarInWindowedMode(false);
        controller.setRememberWindowState(false);
        controller.setRememberLastOpenDirectory(false);
        controller.setShowStatusBarInWindowedMode(false);
        controller.setStartupWindowMode(QStringLiteral("fullscreen"));
        controller.setInterfaceLanguage(QStringLiteral("ar"));
        controller.setUiTheme(QStringLiteral("gray"));
        controller.setResumeEnabled(false);
        controller.setHistoryEnabled(false);
        controller.setClearHistoryOnExit(true);
        controller.setPlaybackProfile(revaplayer::domain::PlayerProfile::Quality);
        controller.setUseExternalMpvConfig(true);
        controller.setStartupVolume(142);
        controller.setRememberLastVolume(false);
        controller.setStartupPlaybackSpeed(1.35);
        controller.setDefaultRepeatMode(QStringLiteral("playlist"));
        controller.setThumbnailPreviewsEnabled(false);
        controller.setThumbnailPreviewWidth(320);
        controller.setThumbnailPopupWidth(280);
        controller.setThumbnailPopupVerticalOffset(26);
        controller.setThumbnailPopupScreenPadding(14);
        controller.setShortSeekStepSeconds(7);
        controller.setLongSeekStepSeconds(45);
        controller.setVolumeStep(9);
        controller.setMouseWheelVolumeEnabled(false);
        controller.setMouseWheelVolumeStep(6);
        controller.setMouseWheelAction(QStringLiteral("seek"));
        controller.setMouseWheelSeekStepSeconds(18);
        controller.setMouseNavigationSeekEnabled(false);
        controller.setMouseNavigationSeekStepSeconds(14);
        controller.setMouseSideButtonsAction(QStringLiteral("playlist"));
        controller.setClickAction(QStringLiteral("zoom_reset"));
        controller.setDoubleClickAction(QStringLiteral("reload_folder_playlist"));
        controller.setMiddleClickAction(QStringLiteral("subtitles"));
        controller.setActionFeedbackOverlayEnabled(false);
        controller.setExpressiveControlLabelsEnabled(false);
        controller.setFullscreenAutoHideEnabled(false);
        controller.setFullscreenRevealMargin(96);
        controller.setFullscreenEdgePanelRevealEnabled(false);
        controller.setFullscreenSideSelectorEnabled(false);
        controller.setDefaultSidePanel(QStringLiteral("details"));
        controller.setRestoreSidePanelsFromWindowState(true);
        controller.setPlaylistOverlayPanelWidth(522);
        controller.setDetailsOverlayPanelWidth(684);
        controller.setDoubleClickFullscreenEnabled(false);
        controller.setHistoryLimit(80);
        controller.setAutoLoadSiblingMediaEnabled(false);
        controller.setShowPlaylistPanelOnFolderLoad(false);
        controller.setNaturalSortFolderPlaylistEnabled(false);
        controller.setPlaylistShowFullPaths(true);
        controller.setPlaylistShowIndexPrefixes(false);
        controller.setPlaylistAutoFollowCurrent(false);
        controller.setRotateFolderPlaylistToCurrent(false);
        controller.setVideoZoomStep(0.35);
        controller.setVideoMinimumZoom(1.25);
        controller.setVideoMaximumZoom(8.50);
        controller.setVideoZoomDefaultBehavior(QStringLiteral("preserve_current"));
        controller.setVideoZoomResetOnFileChange(false);
        controller.setVideoZoomRememberMode(QStringLiteral("per_file"));
        controller.setVideoPanSensitivity(1.40);
        controller.setVideoZoomConstrainPanning(false);
        controller.setVideoZoomWheelBehavior(QStringLiteral("zoom_with_ctrl"));
        controller.setVideoZoomFullscreenBehavior(QStringLiteral("reset_on_toggle"));
        controller.setSubtitleVisible(false);
        controller.setSubtitleScale(1.40);
        controller.setSubtitlePosition(82);
        controller.setSubtitleFontFamily(QStringLiteral("DejaVu Sans"));
        controller.setSubtitleFontSize(46);
        controller.setSubtitleAssOverride(QStringLiteral("force"));
        controller.setSubtitleAutoSelectEnabled(false);
        controller.setSubtitlePreferExternal(false);
        controller.setSubtitleAutoLoadLocalMatches(false);
        controller.setSubtitlePreferredLanguages(QStringLiteral("ar, en, ja"));
        controller.setSubtitleSyncSmallStep(0.40);
        controller.setSubtitleSyncLargeStep(1.75);
        controller.setSubtitleDownloadCommand(QStringLiteral("subliminal download -l {languages} \"{file}\""));
        controller.setSceneBrowserStepSeconds(2);
        controller.setSceneBrowserMaxItems(36);
        controller.setScreenshotDirectory(temporaryDir.filePath(QStringLiteral("shots")));
        controller.setShortcutOverride(QStringLiteral("play_pause"), QStringLiteral("Ctrl+Alt+P"));
        controller.setCustomValue(QStringLiteral("input/gestures/enabled"), QStringLiteral("1"));
        controller.setCustomValue(QStringLiteral("input/gestures/right"), QStringLiteral("seek_forward_short"));
        controller.setCustomValue(QStringLiteral("ui/accent"), QStringLiteral("emerald"));
        controller.setCustomValue(QStringLiteral("ui/density"), QStringLiteral("comfortable"));
        controller.setCustomValue(QStringLiteral("ui/mode"), QStringLiteral("simple"));
        controller.setCustomValue(QStringLiteral("ui/dashboard_enabled"), QStringLiteral("1"));
        controller.setCustomValue(QStringLiteral("playlist/progress_mode_enabled"), QStringLiteral("1"));
        controller.setCustomValue(QStringLiteral("playlist/progress_completion_threshold"), QStringLiteral("95"));
        controller.setCustomValue(QStringLiteral("input/mouse_zone/right"), QStringLiteral("playlist"));
        controller.setCustomValue(QStringLiteral("capture/screenshot_format"), QStringLiteral("jpg"));
        controller.setCustomValue(QStringLiteral("capture/screenshot_template"), QStringLiteral("{timestamp}-{title}-{index}"));
        controller.setCustomValue(QStringLiteral("tests/custom_key"), QStringLiteral("custom_value"));
        QCOMPARE(QDir::cleanPath(controller.databasePath()), QDir::cleanPath(databasePath));
    }

    {
        revaplayer::application::SettingsController controller(makeStore(databasePath));
        QVERIFY(controller.initialize());
        QCOMPARE(controller.showMenuBarInWindowedMode(), false);
        QCOMPARE(controller.rememberWindowState(), false);
        QCOMPARE(controller.rememberLastOpenDirectory(), false);
        QCOMPARE(controller.showStatusBarInWindowedMode(), false);
        QCOMPARE(controller.startupWindowMode(), QStringLiteral("fullscreen"));
        QCOMPARE(controller.interfaceLanguage(), QStringLiteral("ar"));
        QCOMPARE(controller.uiTheme(), QStringLiteral("gray"));
        QCOMPARE(controller.resumeEnabled(), false);
        QCOMPARE(controller.historyEnabled(), false);
        QCOMPARE(controller.clearHistoryOnExit(), true);
        QCOMPARE(controller.playbackProfile(), revaplayer::domain::PlayerProfile::Quality);
        QCOMPARE(controller.useExternalMpvConfig(), true);
        QCOMPARE(controller.startupVolume(), 142);
        QCOMPARE(controller.rememberLastVolume(), false);
        QCOMPARE(controller.startupPlaybackSpeed(), 1.35);
        QCOMPARE(controller.defaultRepeatMode(), QStringLiteral("playlist"));
        QCOMPARE(controller.thumbnailPreviewsEnabled(), false);
        QCOMPARE(controller.thumbnailPreviewWidth(), 320);
        QCOMPARE(controller.thumbnailPopupWidth(), 280);
        QCOMPARE(controller.thumbnailPopupVerticalOffset(), 26);
        QCOMPARE(controller.thumbnailPopupScreenPadding(), 14);
        QCOMPARE(controller.shortSeekStepSeconds(), 7);
        QCOMPARE(controller.longSeekStepSeconds(), 45);
        QCOMPARE(controller.volumeStep(), 9);
        QCOMPARE(controller.mouseWheelVolumeEnabled(), false);
        QCOMPARE(controller.mouseWheelVolumeStep(), 6);
        QCOMPARE(controller.mouseWheelAction(), QStringLiteral("seek"));
        QCOMPARE(controller.mouseWheelSeekStepSeconds(), 18);
        QCOMPARE(controller.mouseNavigationSeekEnabled(), false);
        QCOMPARE(controller.mouseNavigationSeekStepSeconds(), 14);
        QCOMPARE(controller.mouseSideButtonsAction(), QStringLiteral("playlist"));
        QCOMPARE(controller.clickAction(), QStringLiteral("zoom_reset"));
        QCOMPARE(controller.doubleClickAction(), QStringLiteral("reload_folder_playlist"));
        QCOMPARE(controller.middleClickAction(), QStringLiteral("subtitles"));
        QCOMPARE(controller.actionFeedbackOverlayEnabled(), false);
        QCOMPARE(controller.expressiveControlLabelsEnabled(), false);
        QCOMPARE(controller.fullscreenAutoHideEnabled(), false);
        QCOMPARE(controller.fullscreenRevealMargin(), 96);
        QCOMPARE(controller.fullscreenEdgePanelRevealEnabled(), false);
        QCOMPARE(controller.fullscreenSideSelectorEnabled(), false);
        QCOMPARE(controller.defaultSidePanel(), QStringLiteral("details"));
        QCOMPARE(controller.restoreSidePanelsFromWindowState(), true);
        QCOMPARE(controller.playlistOverlayPanelWidth(), 522);
        QCOMPARE(controller.detailsOverlayPanelWidth(), 684);
        QCOMPARE(controller.doubleClickFullscreenEnabled(), false);
        QCOMPARE(controller.historyLimit(), 80);
        QCOMPARE(controller.autoLoadSiblingMediaEnabled(), false);
        QCOMPARE(controller.showPlaylistPanelOnFolderLoad(), false);
        QCOMPARE(controller.naturalSortFolderPlaylistEnabled(), false);
        QCOMPARE(controller.playlistShowFullPaths(), true);
        QCOMPARE(controller.playlistShowIndexPrefixes(), false);
        QCOMPARE(controller.playlistAutoFollowCurrent(), false);
        QCOMPARE(controller.rotateFolderPlaylistToCurrent(), false);
        QCOMPARE(controller.videoZoomStep(), 0.35);
        QCOMPARE(controller.videoMinimumZoom(), 1.25);
        QCOMPARE(controller.videoMaximumZoom(), 8.50);
        QCOMPARE(controller.videoZoomDefaultBehavior(), QStringLiteral("preserve_current"));
        QCOMPARE(controller.videoZoomResetOnFileChange(), false);
        QCOMPARE(controller.videoZoomRememberMode(), QStringLiteral("per_file"));
        QCOMPARE(controller.videoPanSensitivity(), 1.40);
        QCOMPARE(controller.videoZoomConstrainPanning(), false);
        QCOMPARE(controller.videoZoomWheelBehavior(), QStringLiteral("zoom_with_ctrl"));
        QCOMPARE(controller.videoZoomFullscreenBehavior(), QStringLiteral("reset_on_toggle"));
        QCOMPARE(controller.subtitleVisible(), false);
        QCOMPARE(controller.subtitleScale(), 1.40);
        QCOMPARE(controller.subtitlePosition(), 82);
        QCOMPARE(controller.subtitleFontFamily(), QStringLiteral("DejaVu Sans"));
        QCOMPARE(controller.subtitleFontSize(), 46);
        QCOMPARE(controller.subtitleAssOverride(), QStringLiteral("force"));
        QCOMPARE(controller.subtitleAutoSelectEnabled(), false);
        QCOMPARE(controller.subtitlePreferExternal(), false);
        QCOMPARE(controller.subtitleAutoLoadLocalMatches(), false);
        QCOMPARE(controller.subtitlePreferredLanguages(), QStringLiteral("ar,en,ja"));
        QCOMPARE(controller.subtitleSyncSmallStep(), 0.40);
        QCOMPARE(controller.subtitleSyncLargeStep(), 1.75);
        QCOMPARE(controller.subtitleDownloadCommand(), QStringLiteral("subliminal download -l {languages} \"{file}\""));
        QCOMPARE(controller.sceneBrowserStepSeconds(), 2);
        QCOMPARE(controller.sceneBrowserMaxItems(), 36);
        QCOMPARE(controller.screenshotDirectory(), temporaryDir.filePath(QStringLiteral("shots")));
        QCOMPARE(controller.shortcutOverride(QStringLiteral("play_pause")), QStringLiteral("Ctrl+Alt+P"));
        QCOMPARE(controller.customValue(QStringLiteral("input/gestures/enabled")), QStringLiteral("1"));
        QCOMPARE(controller.customValue(QStringLiteral("input/gestures/right")), QStringLiteral("seek_forward_short"));
        QCOMPARE(controller.customValue(QStringLiteral("ui/accent")), QStringLiteral("emerald"));
        QCOMPARE(controller.customValue(QStringLiteral("ui/density")), QStringLiteral("comfortable"));
        QCOMPARE(controller.customValue(QStringLiteral("ui/mode")), QStringLiteral("simple"));
        QCOMPARE(controller.customValue(QStringLiteral("ui/dashboard_enabled")), QStringLiteral("1"));
        QCOMPARE(controller.customValue(QStringLiteral("playlist/progress_mode_enabled")), QStringLiteral("1"));
        QCOMPARE(controller.customValue(QStringLiteral("playlist/progress_completion_threshold")), QStringLiteral("95"));
        QCOMPARE(controller.customValue(QStringLiteral("input/mouse_zone/right")), QStringLiteral("playlist"));
        QCOMPARE(controller.customValue(QStringLiteral("capture/screenshot_format")), QStringLiteral("jpg"));
        QCOMPARE(controller.customValue(QStringLiteral("capture/screenshot_template")), QStringLiteral("{timestamp}-{title}-{index}"));
        QCOMPARE(controller.customValue(QStringLiteral("tests/custom_key")), QStringLiteral("custom_value"));
        QVERIFY(controller.customKeys(QStringLiteral("tests/")).contains(QStringLiteral("tests/custom_key")));
    }

    {
        const QString legacyDatabasePath = temporaryDir.filePath(QStringLiteral("legacy-settings.sqlite"));
        revaplayer::application::SettingsController controller(makeStore(legacyDatabasePath));
        QVERIFY(controller.initialize());
        controller.setCustomValue(QStringLiteral("ui/default_side_panel"), QStringLiteral("playlist"));
        controller.removeCustomValue(QStringLiteral("ui/default_side_panel_user_defined"));
        QCOMPARE(controller.defaultSidePanel(), QStringLiteral("last_opened"));
    }
}

void CoreTests::settingsResetSeedsDefaults()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    const QString databasePath = temporaryDir.filePath(QStringLiteral("reset.sqlite"));

    revaplayer::application::SettingsController controller(makeStore(databasePath));
    QVERIFY2(controller.initialize(), qPrintable(controller.lastError()));
    controller.setUiTheme(QStringLiteral("dark"));
    controller.setAutoLoadSiblingMediaEnabled(false);
    controller.setCustomValue(QStringLiteral("tests/transient"), QStringLiteral("value"));
    controller.saveMainWindowState(QByteArray("geometry"), QByteArray("state"), true, true);

    QVERIFY(controller.resetApplicationData());
    QCOMPARE(controller.uiTheme(), QStringLiteral("gray"));
    QCOMPARE(controller.autoLoadSiblingMediaEnabled(), true);
    QCOMPARE(controller.showMenuBarInWindowedMode(), true);
    QCOMPARE(controller.showStatusBarInWindowedMode(), false);
    QCOMPARE(controller.rememberLastVolume(), true);
    QCOMPARE(controller.startupVolume(), 80);
    QCOMPARE(controller.thumbnailPreviewWidth(), 416);
    QCOMPARE(controller.thumbnailPopupWidth(), 352);
    QCOMPARE(controller.playlistShowIndexPrefixes(), false);
    QCOMPARE(controller.doubleClickAction(), QStringLiteral("fullscreen"));
    QCOMPARE(controller.customValue(QStringLiteral("tests/transient")), QStringLiteral(""));
    QCOMPARE(controller.customValue(QString::fromLatin1(revaplayer::application::kSubtitleCodepageSetting)), QStringLiteral("auto"));
    QCOMPARE(controller.customValue(QString::fromLatin1(revaplayer::application::kSubtitleBorderStyleSetting)), QStringLiteral("outline-and-shadow"));
    QVERIFY(!controller.mainWindowState().has_value());
}

void CoreTests::windowStatePersistence()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    const QString databasePath = temporaryDir.filePath(QStringLiteral("window.sqlite"));

    revaplayer::application::SettingsController controller(makeStore(databasePath));
    QVERIFY2(controller.initialize(), qPrintable(controller.lastError()));

    const QByteArray geometry("geometry-state");
    const QByteArray state("dock-state");
    controller.saveMainWindowState(geometry, state, true, false);

    const auto record = controller.mainWindowState();
    QVERIFY(record.has_value());
    QCOMPARE(record->geometry, geometry);
    QCOMPARE(record->state, state);
    QCOMPARE(record->maximized, true);
    QCOMPARE(record->fullscreen, false);

    controller.clearMainWindowState();
    QVERIFY(!controller.mainWindowState().has_value());
}

void CoreTests::sqliteMigrationHandlesExistingBookmarkCategory()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    const QString databasePath = temporaryDir.filePath(QStringLiteral("migration.sqlite"));

    const QString connectionName = QStringLiteral("migration-test-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE bookmarks ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "source TEXT NOT NULL,"
            "title TEXT NOT NULL DEFAULT '',"
            "category TEXT NOT NULL DEFAULT '',"
            "note TEXT NOT NULL DEFAULT '',"
            "position_seconds REAL NOT NULL DEFAULT 0,"
            "created_at TEXT NOT NULL,"
            "updated_at TEXT NOT NULL"
            ")")));
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = 2")));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    revaplayer::infrastructure::storage::SqliteStore store(databasePath);
    QVERIFY2(store.initialize(), qPrintable(store.lastError()));
    QVERIFY(store.isInitialized());
    const auto created = store.createBookmark(
        QStringLiteral("/tmp/sample.mp4"),
        QStringLiteral("Migrated Bookmark"),
        12.5,
        QStringLiteral("note"),
        QStringLiteral("Scene"));
    QVERIFY(created.has_value());
    const auto bookmarks = store.loadBookmarks(QStringLiteral("/tmp/sample.mp4"));
    QCOMPARE(bookmarks.size(), 1);
    QCOMPARE(bookmarks.first().category, QStringLiteral("Scene"));
}

void CoreTests::bookmarkCrud()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    const QString databasePath = temporaryDir.filePath(QStringLiteral("bookmarks.sqlite"));
    const QString mediaPath = temporaryDir.filePath(QStringLiteral("sample-video.mkv"));

    QFile mediaFile(mediaPath);
    QVERIFY(mediaFile.open(QIODevice::WriteOnly));
    mediaFile.close();

    revaplayer::application::BookmarkController controller(makeStore(databasePath));
    QVERIFY(controller.initialize());

    const QUrl mediaUrl = QUrl::fromLocalFile(mediaPath);
    const auto createdBookmark = controller.createBookmark(
        mediaUrl.toString(),
        QStringLiteral("Intro"),
        12.5,
        QStringLiteral("Note"),
        QStringLiteral("Scene"));
    QVERIFY(createdBookmark.has_value());

    const QVector<revaplayer::domain::Bookmark> bookmarks = controller.bookmarksFor(mediaPath);
    QCOMPARE(bookmarks.size(), 1);
    QCOMPARE(bookmarks.first().title, QStringLiteral("Intro"));
    QCOMPARE(bookmarks.first().category, QStringLiteral("Scene"));
    QCOMPARE(bookmarks.first().note, QStringLiteral("Note"));
    QCOMPARE(bookmarks.first().positionSeconds, 12.5);

    QVERIFY(controller.deleteBookmark(bookmarks.first().id));
    QCOMPARE(controller.bookmarksFor(mediaUrl.toString()).size(), 0);
}

void CoreTests::customCommandsPersistence()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    const QString databasePath = temporaryDir.filePath(QStringLiteral("commands.sqlite"));

    {
        revaplayer::application::SettingsController controller(makeStore(databasePath));
        QVERIFY2(controller.initialize(), qPrintable(controller.lastError()));

        QVector<revaplayer::domain::CustomCommand> commands;
        commands.push_back(revaplayer::domain::CustomCommand {
            .id = -1,
            .name = QStringLiteral("Cinema"),
            .script = QStringLiteral("set|speed|1.15\nshow-text|Cinema Mode"),
        });
        commands.push_back(revaplayer::domain::CustomCommand {
            .id = -1,
            .name = QStringLiteral("Subtitle Up"),
            .script = QStringLiteral("add|sub-pos|-5"),
        });

        QVERIFY(controller.setCustomCommands(commands));
    }

    {
        revaplayer::application::SettingsController controller(makeStore(databasePath));
        QVERIFY2(controller.initialize(), qPrintable(controller.lastError()));
        const QVector<revaplayer::domain::CustomCommand> commands = controller.customCommands();
        QCOMPARE(commands.size(), 2);
        QCOMPARE(commands.at(0).name, QStringLiteral("Cinema"));
        QCOMPARE(commands.at(0).script, QStringLiteral("set|speed|1.15\nshow-text|Cinema Mode"));
        QCOMPARE(commands.at(1).name, QStringLiteral("Subtitle Up"));
        QCOMPARE(commands.at(1).script, QStringLiteral("add|sub-pos|-5"));
    }
}

void CoreTests::historyResumeTransitions()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    const QString databasePath = temporaryDir.filePath(QStringLiteral("history.sqlite"));
    const QString mediaPath = temporaryDir.filePath(QStringLiteral("episode-01.mp4"));

    QFile mediaFile(mediaPath);
    QVERIFY(mediaFile.open(QIODevice::WriteOnly));
    mediaFile.close();

    revaplayer::application::HistoryController controller(makeStore(databasePath));
    QVERIFY(controller.initialize());

    const QString mediaUrl = QUrl::fromLocalFile(mediaPath).toString();
    controller.recordMediaOpened(mediaUrl, QStringLiteral("Episode 01"), 120.0, true);
    controller.savePlaybackProgress(mediaUrl, QStringLiteral("Episode 01"), 48.0, 120.0, true, true);

    const auto resumeState = controller.resumeStateFor(mediaPath);
    QVERIFY(resumeState.has_value());
    QCOMPARE(resumeState->positionSeconds, 48.0);
    QCOMPARE(resumeState->durationSeconds, 120.0);

    PlaybackHistoryRow row = loadPlaybackHistoryRow(databasePath);
    QCOMPARE(row.positionSeconds, 48.0);
    QCOMPARE(row.durationSeconds, 120.0);
    QCOMPARE(row.completed, false);

    controller.savePlaybackProgress(mediaUrl, QStringLiteral("Episode 01"), 61.0, 120.0, true, false);
    QVERIFY(!controller.resumeStateFor(mediaPath).has_value());

    row = loadPlaybackHistoryRow(databasePath);
    QCOMPARE(row.positionSeconds, 61.0);
    QCOMPARE(row.completed, false);

    controller.markPlaybackCompleted(mediaUrl, QStringLiteral("Episode 01"), 120.0, true, false);
    QVERIFY(!controller.resumeStateFor(mediaPath).has_value());

    row = loadPlaybackHistoryRow(databasePath);
    QCOMPARE(row.positionSeconds, 120.0);
    QCOMPARE(row.durationSeconds, 120.0);
    QCOMPARE(row.completed, true);

    const auto recentHistory = controller.recentHistory();
    QCOMPARE(recentHistory.size(), 1);
    QCOMPARE(recentHistory.first().title, QStringLiteral("Episode 01"));
    QCOMPARE(recentHistory.first().completed, true);

    for (int index = 2; index <= 6; ++index) {
        const QString episodePath = temporaryDir.filePath(QStringLiteral("episode-%1.mp4").arg(index));
        QFile episodeFile(episodePath);
        QVERIFY(episodeFile.open(QIODevice::WriteOnly));
        episodeFile.close();
        controller.recordMediaOpened(
            QUrl::fromLocalFile(episodePath).toString(),
            QStringLiteral("Episode %1").arg(index),
            120.0,
            true);
    }
    QVERIFY(controller.trimHistory(3));
    QCOMPARE(controller.recentHistory(10).size(), 3);

    QVERIFY(controller.clearHistory());
    QCOMPARE(controller.recentHistory().size(), 0);
}

void CoreTests::historyRecordOpenPreservesExistingProgress()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    const QString databasePath = temporaryDir.filePath(QStringLiteral("history-preserve.sqlite"));
    const QString mediaDirectoryPath = temporaryDir.filePath(QStringLiteral("مسار عربي With Spaces"));
    QVERIFY(QDir().mkpath(mediaDirectoryPath));
    const QString mediaPath = QDir(mediaDirectoryPath).filePath(QStringLiteral("حلقة 01 test.mp4"));

    QFile mediaFile(mediaPath);
    QVERIFY(mediaFile.open(QIODevice::WriteOnly));
    mediaFile.close();

    revaplayer::application::HistoryController controller(makeStore(databasePath));
    QVERIFY(controller.initialize());

    const QString mediaUrl = QUrl::fromLocalFile(mediaPath).toString();
    controller.savePlaybackProgress(mediaUrl, QStringLiteral("Episode"), 305.0, 900.0, true, true);
    controller.recordMediaOpened(mediaUrl, QStringLiteral("Episode"), 900.0, true);

    const auto resumeState = controller.resumeStateFor(mediaPath);
    QVERIFY(resumeState.has_value());
    QCOMPARE(resumeState->positionSeconds, 305.0);
    QCOMPARE(resumeState->durationSeconds, 900.0);

    PlaybackHistoryRow row = loadPlaybackHistoryRow(databasePath);
    QCOMPARE(row.source, QFileInfo(mediaPath).canonicalFilePath());
    QCOMPARE(row.positionSeconds, 305.0);
    QCOMPARE(row.durationSeconds, 900.0);
    QCOMPARE(row.completed, false);

    controller.savePlaybackProgress(mediaUrl, QStringLiteral("Episode"), 2.0, 900.0, true, true);
    const auto earlyResumeState = controller.resumeStateFor(mediaPath);
    QVERIFY(earlyResumeState.has_value());
    QCOMPARE(earlyResumeState->positionSeconds, 2.0);
    QCOMPARE(earlyResumeState->durationSeconds, 900.0);

    row = loadPlaybackHistoryRow(databasePath);
    QCOMPARE(row.positionSeconds, 2.0);
    QCOMPARE(row.durationSeconds, 900.0);
    QCOMPARE(row.completed, false);

    controller.recordMediaOpened(mediaUrl, QStringLiteral("Episode"), 900.0, true);
    row = loadPlaybackHistoryRow(databasePath);
    QCOMPARE(row.positionSeconds, 2.0);
    QCOMPARE(row.durationSeconds, 900.0);
    QCOMPARE(row.completed, false);

    controller.markPlaybackCompleted(mediaUrl, QStringLiteral("Episode"), 900.0, true, true);
    controller.recordMediaOpened(mediaUrl, QStringLiteral("Episode"), 900.0, true);

    row = loadPlaybackHistoryRow(databasePath);
    QCOMPARE(row.positionSeconds, 900.0);
    QCOMPARE(row.durationSeconds, 900.0);
    QCOMPARE(row.completed, true);
}

void CoreTests::historyMultipleResumeStatesSurviveNavigation()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    const QString databasePath = temporaryDir.filePath(QStringLiteral("history-navigation.sqlite"));
    const QString firstMediaPath = temporaryDir.filePath(QStringLiteral("episode-01.mp4"));
    const QString secondMediaPath = temporaryDir.filePath(QStringLiteral("episode-02.mp4"));

    QFile firstMediaFile(firstMediaPath);
    QVERIFY(firstMediaFile.open(QIODevice::WriteOnly));
    firstMediaFile.close();
    QFile secondMediaFile(secondMediaPath);
    QVERIFY(secondMediaFile.open(QIODevice::WriteOnly));
    secondMediaFile.close();

    revaplayer::application::HistoryController controller(makeStore(databasePath));
    QVERIFY(controller.initialize());

    const QString firstMediaUrl = QUrl::fromLocalFile(firstMediaPath).toString();
    const QString secondMediaUrl = QUrl::fromLocalFile(secondMediaPath).toString();
    controller.savePlaybackProgress(firstMediaUrl, QStringLiteral("Episode 01"), 123.0, 600.0, true, true);
    controller.savePlaybackProgress(secondMediaUrl, QStringLiteral("Episode 02"), 240.0, 900.0, true, true);

    controller.recordMediaOpened(firstMediaUrl, QStringLiteral("Episode 01"), 600.0, true);
    controller.recordMediaOpened(secondMediaUrl, QStringLiteral("Episode 02"), 900.0, true);

    const auto firstResume = controller.resumeStateFor(firstMediaPath);
    QVERIFY(firstResume.has_value());
    QCOMPARE(firstResume->positionSeconds, 123.0);
    QCOMPARE(firstResume->durationSeconds, 600.0);

    const auto secondResume = controller.resumeStateFor(secondMediaPath);
    QVERIFY(secondResume.has_value());
    QCOMPARE(secondResume->positionSeconds, 240.0);
    QCOMPARE(secondResume->durationSeconds, 900.0);

    controller.savePlaybackProgress(firstMediaUrl, QStringLiteral("Episode 01"), 180.0, 600.0, true, true);
    const auto updatedFirstResume = controller.resumeStateFor(firstMediaPath);
    QVERIFY(updatedFirstResume.has_value());
    QCOMPARE(updatedFirstResume->positionSeconds, 180.0);

    const auto preservedSecondResume = controller.resumeStateFor(secondMediaPath);
    QVERIFY(preservedSecondResume.has_value());
    QCOMPARE(preservedSecondResume->positionSeconds, 240.0);
}

void CoreTests::historyRejectsInvalidStoredSources()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    const QString databasePath = temporaryDir.filePath(QStringLiteral("history-invalid.sqlite"));
    const QString validMediaPath = temporaryDir.filePath(QStringLiteral("episode-02.mp4"));
    const QString missingMediaPath = temporaryDir.filePath(QStringLiteral("missing.mp4"));

    QFile validFile(validMediaPath);
    QVERIFY(validFile.open(QIODevice::WriteOnly));
    validFile.close();

    revaplayer::application::HistoryController controller(makeStore(databasePath));
    QVERIFY(controller.initialize());

    controller.recordMediaOpened(QUrl::fromLocalFile(validMediaPath).toString(), QStringLiteral("Valid"), 20.0, true);
    controller.recordMediaOpened(QUrl::fromLocalFile(missingMediaPath).toString(), QStringLiteral("Missing"), 20.0, true);
    controller.recordMediaOpened(QStringLiteral("https://example.com/watch?v=42"), QStringLiteral("Remote"), 20.0, true);

    const auto history = controller.recentHistory(10);
    QCOMPARE(history.size(), 2);
    QVERIFY(std::any_of(history.cbegin(), history.cend(), [&validMediaPath](const auto &entry) {
        return entry.source == validMediaPath;
    }));
    QVERIFY(std::any_of(history.cbegin(), history.cend(), [](const auto &entry) {
        return entry.source == QStringLiteral("https://example.com/watch?v=42");
    }));
    QVERIFY(std::none_of(history.cbegin(), history.cend(), [&missingMediaPath](const auto &entry) {
        return entry.source == missingMediaPath;
    }));
    QCOMPARE(controller.recentHistory(10).size(), 2);
}

void CoreTests::playlistModelMapping()
{
    revaplayer::application::PlaylistController controller;

    QVector<revaplayer::domain::PlaylistEntry> entries;
    entries.push_back(revaplayer::domain::PlaylistEntry {0, QStringLiteral("First"), QStringLiteral("/tmp/first.mp4"), true});
    entries.push_back(revaplayer::domain::PlaylistEntry {1, QStringLiteral("Second"), QStringLiteral("/tmp/second.mp4"), false});

    controller.setEntries(entries, 0);
    QAbstractItemModel *model = controller.model();
    QVERIFY(model != nullptr);
    QCOMPARE(model->rowCount(), 2);
    QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("▶ 01  •  First"));
    QCOMPARE(controller.playlistIndexFor(model->index(1, 0)), 1);
    QVERIFY(model->index(0, 0).data(Qt::FontRole).value<QFont>().bold());

    controller.setDisplayOptions(true, false);
    QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("/tmp/first.mp4"));
    QVERIFY(model->index(0, 0).data(Qt::UserRole + 2).toString().contains(QStringLiteral("First")));

    QPersistentModelIndex preservedIndex = model->index(1, 0);
    QVERIFY(preservedIndex.isValid());
    QSignalSpy modelResetSpy(model, &QAbstractItemModel::modelReset);

    QHash<QString, revaplayer::application::PlaylistPresentationData> presentationData;
    revaplayer::application::PlaylistPresentationData firstPresentation;
    firstPresentation.durationSeconds = 120.0;
    firstPresentation.lastPositionSeconds = 36.0;
    firstPresentation.watchedPercent = 30;
    firstPresentation.completed = false;
    firstPresentation.secondaryText = QStringLiteral("notes preview");
    firstPresentation.secondaryBadges = QStringList {
        QStringLiteral(".mp4"),
        QStringLiteral("720p"),
        QStringLiteral("1280x720"),
    };
    presentationData.insert(QStringLiteral("/tmp/first.mp4"), firstPresentation);

    controller.setPresentationData(presentationData);

    QCOMPARE(modelResetSpy.count(), 0);
    QVERIFY(preservedIndex.isValid());
    QCOMPARE(preservedIndex.data(revaplayer::application::PlaylistRoles::SourceRole).toString(), QStringLiteral("/tmp/second.mp4"));
    QCOMPARE(model->index(0, 0).data(revaplayer::application::PlaylistRoles::ProgressRole).toInt(), 30);
    QCOMPARE(model->index(0, 0).data(revaplayer::application::PlaylistRoles::DurationSecondsRole).toDouble(), 120.0);
    QCOMPARE(model->index(0, 0).data(revaplayer::application::PlaylistRoles::SecondaryTextRole).toString(), QStringLiteral("notes preview"));
    QCOMPARE(
        model->index(0, 0).data(revaplayer::application::PlaylistRoles::SecondaryBadgeListRole).toStringList(),
        (QStringList {QStringLiteral(".mp4"), QStringLiteral("720p"), QStringLiteral("1280x720")}));

    QVector<revaplayer::domain::PlaylistEntry> browserEntries;
    browserEntries.push_back(revaplayer::domain::PlaylistEntry {0, QStringLiteral("Back"), QStringLiteral("reva-folder-back://%2Ftmp"), false});
    browserEntries.push_back(revaplayer::domain::PlaylistEntry {1, QStringLiteral("Subfolder"), QStringLiteral("reva-folder://%2Ftmp%2Fcourse"), false});
    browserEntries.push_back(revaplayer::domain::PlaylistEntry {2, QStringLiteral("Video"), QStringLiteral("/tmp/course/video.mp4"), false});
    controller.setEntries(browserEntries, -1);

    QVERIFY(!model->index(0, 0).data(revaplayer::application::PlaylistRoles::ReorderableRole).toBool());
    QVERIFY(!model->index(1, 0).data(revaplayer::application::PlaylistRoles::ReorderableRole).toBool());
    QVERIFY(model->index(2, 0).data(revaplayer::application::PlaylistRoles::ReorderableRole).toBool());
    QVERIFY(!(model->flags(model->index(0, 0)) & Qt::ItemIsDragEnabled));
    QVERIFY(!(model->flags(model->index(1, 0)) & Qt::ItemIsDragEnabled));
    QVERIFY(model->flags(model->index(2, 0)) & Qt::ItemIsDragEnabled);
}

void CoreTests::playlistDisplayTitlePresentation()
{
    revaplayer::application::PlaylistController controller;

    QVector<revaplayer::domain::PlaylistEntry> entries;
    entries.push_back(revaplayer::domain::PlaylistEntry {0, QStringLiteral("track01.mp3"), QStringLiteral("/tmp/track01.mp3"), false});
    controller.setEntries(entries, -1);

    QHash<QString, revaplayer::application::PlaylistPresentationData> presentationData;
    revaplayer::application::PlaylistPresentationData audioPresentation;
    audioPresentation.displayTitle = QStringLiteral("Artist Song");
    audioPresentation.secondaryText = QStringLiteral("Artist  •  Album  •  Folder");
    presentationData.insert(QStringLiteral("/tmp/track01.mp3"), audioPresentation);
    controller.setPresentationData(presentationData);

    QAbstractItemModel *model = controller.model();
    QVERIFY(model != nullptr);
    QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("01  •  Artist Song"));
    QCOMPARE(model->index(0, 0).data(revaplayer::application::PlaylistRoles::TitleRole).toString(), QStringLiteral("Artist Song"));
    QVERIFY(model->index(0, 0).data(revaplayer::application::PlaylistRoles::SearchRole).toString().contains(QStringLiteral("Artist Song")));
}

void CoreTests::playbackEndReasonSemantics()
{
    using revaplayer::domain::PlaybackEndReason;
    using revaplayer::domain::playbackEndReasonIsFailure;
    using revaplayer::domain::playbackEndReasonRepresentsCompletion;

    QVERIFY(playbackEndReasonRepresentsCompletion(PlaybackEndReason::ReachedEndOfFile));
    QVERIFY(!playbackEndReasonRepresentsCompletion(PlaybackEndReason::Stopped));
    QVERIFY(!playbackEndReasonRepresentsCompletion(PlaybackEndReason::Redirected));
    QVERIFY(!playbackEndReasonRepresentsCompletion(PlaybackEndReason::BackendQuit));
    QVERIFY(!playbackEndReasonRepresentsCompletion(PlaybackEndReason::Error));
    QVERIFY(!playbackEndReasonRepresentsCompletion(PlaybackEndReason::Unknown));

    QVERIFY(playbackEndReasonIsFailure(PlaybackEndReason::Error));
    QVERIFY(!playbackEndReasonIsFailure(PlaybackEndReason::ReachedEndOfFile));
    QVERIFY(!playbackEndReasonIsFailure(PlaybackEndReason::Stopped));
}

void CoreTests::playlistSortModes()
{
    revaplayer::application::PlaylistController controller;

    QVector<revaplayer::domain::PlaylistEntry> entries;
    entries.push_back(revaplayer::domain::PlaylistEntry {0, QStringLiteral("Bravo"), QStringLiteral("/tmp/bravo.mp4"), false});
    entries.push_back(revaplayer::domain::PlaylistEntry {1, QStringLiteral("Alpha"), QStringLiteral("/tmp/alpha.mp4"), false});
    entries.push_back(revaplayer::domain::PlaylistEntry {2, QStringLiteral("Charlie"), QStringLiteral("/tmp/charlie.mp4"), false});
    controller.setEntries(entries, -1);

    QHash<QString, revaplayer::application::PlaylistPresentationData> presentationData;

    revaplayer::application::PlaylistPresentationData bravoPresentation;
    bravoPresentation.durationSeconds = 40.0;
    bravoPresentation.watchedPercent = 10;
    presentationData.insert(QStringLiteral("/tmp/bravo.mp4"), bravoPresentation);

    revaplayer::application::PlaylistPresentationData alphaPresentation;
    alphaPresentation.durationSeconds = 120.0;
    alphaPresentation.watchedPercent = 100;
    alphaPresentation.completed = true;
    presentationData.insert(QStringLiteral("/tmp/alpha.mp4"), alphaPresentation);

    revaplayer::application::PlaylistPresentationData charliePresentation;
    charliePresentation.durationSeconds = 80.0;
    charliePresentation.watchedPercent = 65;
    presentationData.insert(QStringLiteral("/tmp/charlie.mp4"), charliePresentation);

    controller.setPresentationData(presentationData);

    QAbstractItemModel *model = controller.model();
    QVERIFY(model != nullptr);

    controller.setSortMode(QStringLiteral("title"));
    QCOMPARE(model->index(0, 0).data(revaplayer::application::PlaylistRoles::SourceRole).toString(), QStringLiteral("/tmp/alpha.mp4"));
    QCOMPARE(model->index(1, 0).data(revaplayer::application::PlaylistRoles::SourceRole).toString(), QStringLiteral("/tmp/bravo.mp4"));
    QCOMPARE(model->index(2, 0).data(revaplayer::application::PlaylistRoles::SourceRole).toString(), QStringLiteral("/tmp/charlie.mp4"));

    controller.setSortMode(QStringLiteral("duration"));
    QCOMPARE(model->index(0, 0).data(revaplayer::application::PlaylistRoles::SourceRole).toString(), QStringLiteral("/tmp/alpha.mp4"));
    QCOMPARE(model->index(1, 0).data(revaplayer::application::PlaylistRoles::SourceRole).toString(), QStringLiteral("/tmp/charlie.mp4"));
    QCOMPARE(model->index(2, 0).data(revaplayer::application::PlaylistRoles::SourceRole).toString(), QStringLiteral("/tmp/bravo.mp4"));

    controller.setSortMode(QStringLiteral("progress"));
    QCOMPARE(model->index(0, 0).data(revaplayer::application::PlaylistRoles::SourceRole).toString(), QStringLiteral("/tmp/alpha.mp4"));
    QCOMPARE(model->index(1, 0).data(revaplayer::application::PlaylistRoles::SourceRole).toString(), QStringLiteral("/tmp/charlie.mp4"));
    QCOMPARE(model->index(2, 0).data(revaplayer::application::PlaylistRoles::SourceRole).toString(), QStringLiteral("/tmp/bravo.mp4"));
}

void CoreTests::screenshotPathGeneration()
{
    const QDateTime timestamp(QDate(2026, 3, 18), QTime(14, 5, 9));
    const QString path = revaplayer::application::buildScreenshotFilePath(
        QStringLiteral("/tmp/screenshots"),
        QStringLiteral("Episode 01: Intro/Start"),
        timestamp);

    QCOMPARE(
        path,
        QDir(QStringLiteral("/tmp/screenshots")).filePath(QStringLiteral("20260318-140509-Episode_01_Intro_Start.png")));
    QCOMPARE(revaplayer::application::sanitizeSnapshotFileStem(QStringLiteral("  ")), QStringLiteral("snapshot"));
}

void CoreTests::playbackTuningHelpers()
{
    QCOMPARE(revaplayer::application::clampPlaybackSpeed(0.10), 0.25);
    QCOMPARE(revaplayer::application::clampPlaybackSpeed(4.80), 4.0);
    QCOMPARE(revaplayer::application::formatPlaybackRate(1.0), QStringLiteral("1x"));
    QCOMPARE(revaplayer::application::formatPlaybackRate(1.25), QStringLiteral("1.25x"));
    QCOMPARE(revaplayer::application::clampVideoZoomFactor(0.50), 1.0);
    QCOMPARE(revaplayer::application::clampVideoZoomFactor(9.0), 9.0);
    QCOMPARE(revaplayer::application::clampVideoViewportAlignment(-4.0), -1.0);
    QCOMPARE(revaplayer::application::clampVideoViewportAlignment(3.0), 1.0);
    QCOMPARE(revaplayer::application::videoZoomFactorToLog2(1.0), 0.0);
    QCOMPARE(revaplayer::application::videoZoomFactorToLog2(2.0), 1.0);
    QCOMPARE(revaplayer::application::formatScaleFactor(1.0), QStringLiteral("1x"));
    QCOMPARE(revaplayer::application::formatScaleFactor(1.5), QStringLiteral("1.5x"));
    QCOMPARE(revaplayer::application::formatSignedSeconds(0.50), QStringLiteral("+0.50s"));
    QCOMPARE(revaplayer::application::formatSignedSeconds(-0.25), QStringLiteral("-0.25s"));
}

void CoreTests::videoRotationNormalization()
{
    QCOMPARE(revaplayer::application::normalizeRightAngleRotation(0), 0);
    QCOMPARE(revaplayer::application::normalizeRightAngleRotation(450), 90);
    QCOMPARE(revaplayer::application::normalizeRightAngleRotation(-90), 270);
    QCOMPARE(revaplayer::application::normalizeRightAngleRotation(359), 270);
}

void CoreTests::playbackDiagnosticsFormatting()
{
    const QString previousLanguage = revaplayer::application::currentUiLanguage();
    revaplayer::application::setCurrentUiLanguage(QStringLiteral("en"));

    revaplayer::domain::PlaybackDiagnostics diagnostics;
    diagnostics.source = QStringLiteral("/tmp/movie.mkv");
    diagnostics.fileFormat = QStringLiteral("matroska");
    diagnostics.currentVideoOutput = QStringLiteral("gpu-next");
    diagnostics.currentAudioOutput = QStringLiteral("pipewire");
    diagnostics.hardwareDecoding = QStringLiteral("vaapi");
    diagnostics.videoCodec = QStringLiteral("h264");
    diagnostics.audioCodec = QStringLiteral("aac");
    diagnostics.encodedWidth = 1920;
    diagnostics.encodedHeight = 1080;
    diagnostics.displayWidth = 1920;
    diagnostics.displayHeight = 1080;
    diagnostics.containerFps = 23.976;
    diagnostics.estimatedVideoFps = 23.976;
    diagnostics.videoBitrate = 4500000.0;
    diagnostics.audioBitrate = 192000.0;
    diagnostics.cacheDurationSeconds = 18.5;
    diagnostics.cacheSpeed = 1.25;

    QVector<revaplayer::domain::TrackInfo> tracks;
    tracks.push_back(revaplayer::domain::TrackInfo {
        .id = 1,
        .type = revaplayer::domain::TrackType::Video,
        .title = QStringLiteral("Main Video"),
        .language = QString {},
        .codec = QStringLiteral("h264"),
        .width = 1920,
        .height = 1080,
        .fps = 24.0,
        .selected = true,
        .external = false,
    });
    tracks.push_back(revaplayer::domain::TrackInfo {
        .id = 2,
        .type = revaplayer::domain::TrackType::Audio,
        .title = QStringLiteral("Stereo"),
        .language = QStringLiteral("eng"),
        .codec = QStringLiteral("aac"),
        .width = 0,
        .height = 0,
        .fps = 0.0,
        .selected = true,
        .external = false,
    });

    const QString overlay = revaplayer::application::buildMediaInformationOverlayText(
        diagnostics,
        tracks,
        QStringLiteral("Movie Title"),
        65.0,
        3661.0,
        1.25);
    QVERIFY(overlay.contains(QStringLiteral("Media Information")));
    QVERIFY(overlay.contains(QStringLiteral("File: Movie Title")));
    QVERIFY(overlay.contains(QStringLiteral("Time: 01:05 / 01:01:01")));
    QVERIFY(overlay.contains(QStringLiteral("Speed: 1.25x")));
    QVERIFY(overlay.contains(QStringLiteral("h264")));
    QVERIFY(overlay.contains(QStringLiteral("Subtitle: None")));

    const QString report = revaplayer::application::buildMediaInfoReport(
        QStringLiteral("Movie Title"),
        diagnostics,
        tracks,
        3661.0);
    QVERIFY(report.contains(QStringLiteral("Title: Movie Title")));
    QVERIFY(report.contains(QStringLiteral("Format: matroska")));
    QVERIFY(report.contains(QStringLiteral("Encoded Resolution: 1920x1080")));
    QVERIFY(!report.contains(QStringLiteral("Audio Output: alsa/default")));
    QVERIFY(report.contains(QStringLiteral("Video #1")));
    QVERIFY(report.contains(QStringLiteral("Audio #2")));

    revaplayer::application::setCurrentUiLanguage(previousLanguage);
}

void CoreTests::thumbnailPolicyProfiles()
{
    const auto batteryPolicy = revaplayer::services::media::thumbnailPolicyForProfile(
        revaplayer::domain::PlayerProfile::Battery);
    const auto balancedPolicy = revaplayer::services::media::thumbnailPolicyForProfile(
        revaplayer::domain::PlayerProfile::Balanced);
    const auto qualityPolicy = revaplayer::services::media::thumbnailPolicyForProfile(
        revaplayer::domain::PlayerProfile::Quality);

    QVERIFY(batteryPolicy.debounceIntervalMs > balancedPolicy.debounceIntervalMs);
    QVERIFY(qualityPolicy.debounceIntervalMs < balancedPolicy.debounceIntervalMs);
    QVERIFY(batteryPolicy.pollIntervalMs > balancedPolicy.pollIntervalMs);
    QVERIFY(qualityPolicy.pollIntervalMs < balancedPolicy.pollIntervalMs);
    QVERIFY(batteryPolicy.previewWidth < balancedPolicy.previewWidth);
    QVERIFY(qualityPolicy.previewWidth > balancedPolicy.previewWidth);
    QVERIFY(batteryPolicy.memoryCacheEntries < balancedPolicy.memoryCacheEntries);
    QVERIFY(qualityPolicy.memoryCacheEntries > balancedPolicy.memoryCacheEntries);
}

void CoreTests::subtitleStyleOptions()
{
    const QString previousLanguage = revaplayer::application::currentUiLanguage();
    revaplayer::application::setCurrentUiLanguage(QStringLiteral("en"));

    QString parseError;
    const QVector<QStringList> parsedCommands = revaplayer::application::parseCustomCommandScript(
        QStringLiteral("# comment\nset|speed|1.25\nshow-text|Cinema Mode"),
        &parseError);
    QVERIFY(parseError.isEmpty());
    QCOMPARE(parsedCommands.size(), 2);
    QCOMPARE(parsedCommands.at(0), QStringList({QStringLiteral("set"), QStringLiteral("speed"), QStringLiteral("1.25")}));
    QCOMPARE(parsedCommands.at(1), QStringList({QStringLiteral("show-text"), QStringLiteral("Cinema Mode")}));
    QVERIFY(revaplayer::application::parseCustomCommandScript(QStringLiteral(" |speed|1.0"), &parseError).isEmpty());
    QVERIFY(!parseError.isEmpty());
    QCOMPARE(revaplayer::application::normalizeSubtitleAssOverride(QStringLiteral(" FORCE ")), QStringLiteral("force"));
    QCOMPARE(revaplayer::application::normalizeSubtitleAssOverride(QStringLiteral("unknown")), QStringLiteral("scale"));
    QCOMPARE(revaplayer::application::subtitleAssOverrideLabel(QStringLiteral("strip")), QStringLiteral("Strip Embedded Styling"));
    QCOMPARE(revaplayer::application::nextSubtitleAssOverride(QStringLiteral("scale")), QStringLiteral("yes"));
    QCOMPARE(revaplayer::application::clampSubtitleScale(0.1), 0.25);
    QCOMPARE(revaplayer::application::clampSubtitleScale(8.0), 5.0);
    QCOMPARE(revaplayer::application::clampSubtitlePosition(-10), 0);
    QCOMPARE(revaplayer::application::clampSubtitlePosition(170), 150);
    QCOMPARE(revaplayer::application::clampSubtitleFontSize(3), 8);
    QCOMPARE(revaplayer::application::clampSubtitleFontSize(500), 144);
    QCOMPARE(revaplayer::application::normalizeSubtitleAutoLoadMode(QStringLiteral(" FUZZY ")), QStringLiteral("fuzzy"));
    QCOMPARE(revaplayer::application::subtitleAutoLoadModeMpvValue(QStringLiteral("same_name")), QStringLiteral("exact"));
    QCOMPARE(revaplayer::application::normalizeSubtitleBorderStyle(QStringLiteral("opaque-box")), QStringLiteral("opaque-box"));
    QCOMPARE(revaplayer::application::normalizeSubtitleAlignY(QStringLiteral("middle")), QStringLiteral("bottom"));
    QCOMPARE(revaplayer::application::normalizeSubtitleColorString(QStringLiteral("bad"), QStringLiteral("#FFFFFFFF")).toLower(), QStringLiteral("#ffffffff"));
    QCOMPARE(revaplayer::application::clampSubtitleOutlineSize(33.0), 12.0);
    QCOMPARE(revaplayer::application::clampSubtitleShadowOffset(22.0), 20.0);
    QCOMPARE(revaplayer::application::clampSubtitleBlur(9.0), 3.0);
    QCOMPARE(revaplayer::application::clampSubtitleLetterSpacing(-25.0), -10.0);
    QCOMPARE(revaplayer::application::clampSubtitleBackgroundOpacity(130), 100);

    revaplayer::application::setCurrentUiLanguage(previousLanguage);
}

void CoreTests::uiLanguageTranslations()
{
    const QString previousLanguage = revaplayer::application::currentUiLanguage();
    revaplayer::application::setCurrentUiLanguage(QStringLiteral("ar"));

    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Search")), QStringLiteral("بحث"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Theme Editor")), QStringLiteral("محرر السمة"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Timeline preview size")), QStringLiteral("حجم معاينة الخط الزمني"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Mist")), QStringLiteral("رمادي ضبابي"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Walnut")), QStringLiteral("بني"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Click action")), QStringLiteral("إجراء النقر"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Zoom reset")), QStringLiteral("تمت إعادة التكبير"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Saved window state is enabled, so the last used window mode will be restored.")),
             QStringLiteral("تم تفعيل حفظ حالة النافذة، لذلك ستتم استعادة آخر وضع نافذة مستخدم."));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Create a bookmark at %1")).arg(QStringLiteral("01:15")),
             QStringLiteral("أنشئ إشارة مرجعية عند 01:15"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Open Folder...")), QStringLiteral("فتح مجلد..."));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Back")), QStringLiteral("رجوع"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Add to Favorites")), QStringLiteral("اضافة للمفضل"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Remove from Favorites")), QStringLiteral("إزالة من المفضلة"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Remove from List")), QStringLiteral("إزالة من القائمة"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Reset Progress")), QStringLiteral("تصفير التقدم"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Reset Progress for This List")), QStringLiteral("تصفير تقدم هذه القائمة"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Selected item removed from favorites")), QStringLiteral("تمت إزالة العنصر المحدد من المفضلة"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Add File")), QStringLiteral("إضافة ملف"));
    QCOMPARE(revaplayer::application::subtitleAssOverrideLabel(QStringLiteral("strip")),
             QStringLiteral("إزالة التنسيق المضمن"));
    QCOMPARE(revaplayer::application::currentUiLanguageDirection(), Qt::LeftToRight);

    revaplayer::application::setCurrentUiLanguage(QStringLiteral("zh_CN"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Preferences")), QStringLiteral("偏好设置"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Theme Editor")), QStringLiteral("主题编辑器"));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Open Folder...")), QStringLiteral("打开文件夹..."));
    QCOMPARE(revaplayer::application::translateUiText(QStringLiteral("Startup volume behavior")), QStringLiteral("启动音量行为"));
    QCOMPARE(revaplayer::application::currentUiLanguageDirection(), Qt::LeftToRight);

    revaplayer::application::setCurrentUiLanguage(previousLanguage);
}

void CoreTests::firstRunDialogDefaultsAndPersistence()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    const QString databasePath = temporaryDir.filePath(QStringLiteral("settings.sqlite"));

    revaplayer::application::SettingsController controller(makeStore(databasePath));
    QVERIFY2(controller.initialize(), qPrintable(controller.lastError()));

    const QString previousLanguage = revaplayer::application::currentUiLanguage();
    const Qt::LayoutDirection previousDirection = QApplication::layoutDirection();
    revaplayer::application::setCurrentUiLanguage(controller.interfaceLanguage());
    QApplication::setLayoutDirection(revaplayer::application::currentUiLanguageDirection());

    revaplayer::ui::FirstRunDialog dialog(&controller);
    auto *formLayout = dialog.findChild<QFormLayout *>(QStringLiteral("firstRunFormLayout"));
    auto *languageComboBox = dialog.findChild<QComboBox *>(QStringLiteral("firstRunLanguageComboBox"));
    auto *themeComboBox = dialog.findChild<QComboBox *>(QStringLiteral("firstRunThemeComboBox"));
    auto *dashboardCheckBox = dialog.findChild<QCheckBox *>(QStringLiteral("firstRunDashboardCheckBox"));
    auto *progressCheckBox = dialog.findChild<QCheckBox *>(QStringLiteral("firstRunProgressCheckBox"));

    QVERIFY(formLayout != nullptr);
    QVERIFY(languageComboBox != nullptr);
    QVERIFY(themeComboBox != nullptr);
    QVERIFY(dashboardCheckBox != nullptr);
    QVERIFY(progressCheckBox != nullptr);
    QCOMPARE(formLayout->itemAt(0, QFormLayout::FieldRole)->widget(), static_cast<QWidget *>(languageComboBox));
    QCOMPARE(controller.interfaceLanguage(), QStringLiteral("ar"));
    QCOMPARE(languageComboBox->currentData().toString(), QStringLiteral("ar"));
    QCOMPARE(dialog.layoutDirection(), Qt::LeftToRight);
    QCOMPARE(QApplication::layoutDirection(), Qt::LeftToRight);
    QVERIFY(dashboardCheckBox->isChecked());
    QVERIFY(progressCheckBox->isChecked());
    QCOMPARE(dashboardCheckBox->layoutDirection(), Qt::RightToLeft);
    QCOMPARE(progressCheckBox->layoutDirection(), Qt::RightToLeft);

    languageComboBox->setCurrentIndex(languageComboBox->findData(QStringLiteral("en")));
    themeComboBox->setCurrentIndex(themeComboBox->findData(QStringLiteral("dark")));
    dashboardCheckBox->setChecked(false);
    progressCheckBox->setChecked(false);
    QCoreApplication::processEvents();

    QCOMPARE(dialog.layoutDirection(), Qt::LeftToRight);
    QCOMPARE(QApplication::layoutDirection(), Qt::LeftToRight);

    dialog.applySelections();
    QCOMPARE(controller.interfaceLanguage(), QStringLiteral("en"));
    QCOMPARE(controller.uiTheme(), QStringLiteral("dark"));
    QCOMPARE(controller.customValue(QStringLiteral("ui/mode")), QStringLiteral("simple"));
    QCOMPARE(controller.customValue(QStringLiteral("ui/dashboard_enabled")), QStringLiteral("0"));
    QCOMPARE(controller.customValue(QStringLiteral("ui/dashboard_show_on_idle")), QStringLiteral("0"));
    QCOMPARE(controller.customValue(QStringLiteral("playlist/progress_mode_enabled")), QStringLiteral("0"));

    revaplayer::application::setCurrentUiLanguage(previousLanguage);
    QApplication::setLayoutDirection(previousDirection);
}

void CoreTests::uiLanguageCriticalCoverage()
{
    const QStringList languages {
        QStringLiteral("ar"),
        QStringLiteral("es"),
        QStringLiteral("fr"),
        QStringLiteral("de"),
        QStringLiteral("tr"),
        QStringLiteral("ru"),
        QStringLiteral("zh_CN"),
    };

    const QStringList criticalStrings {
        QStringLiteral("Add Bookmark"),
        QStringLiteral("Create a bookmark at %1"),
        QStringLiteral("Media Info"),
        QStringLiteral("Open a file or drop media here"),
        QStringLiteral("Volume"),
        QStringLiteral("Gesture"),
        QStringLiteral("Subtitle tracks"),
        QStringLiteral("Video quality"),
        QStringLiteral("Toggle Fullscreen"),
        QStringLiteral("Open media"),
        QStringLiteral("Add Favorite Media"),
        QStringLiteral("Add File"),
        QStringLiteral("Add to Favorites"),
        QStringLiteral("Previous playlist item"),
        QStringLiteral("Next playlist item"),
        QStringLiteral("Pause playback"),
        QStringLiteral("Resume playback"),
        QStringLiteral("Stop playback"),
        QStringLiteral("Playback speed (%1)"),
        QStringLiteral("Repeat mode"),
        QStringLiteral("Loop controls"),
        QStringLiteral("Clear A-B Loop"),
        QStringLiteral("Set Loop Start"),
        QStringLiteral("Set Loop End"),
        QStringLiteral("Default behavior"),
        QStringLiteral("Fit to Frame"),
        QStringLiteral("Preserve Current Zoom"),
        QStringLiteral("Reset zoom when a different file starts"),
        QStringLiteral("Constrain panning to the visible frame bounds"),
        QStringLiteral("Wheel zoom behavior"),
        QStringLiteral("Keep Wheel for Global Mouse Action"),
        QStringLiteral("Zoom with Ctrl + Wheel"),
        QStringLiteral("Zoom with Wheel While Already Zoomed"),
        QStringLiteral("Fullscreen transition"),
        QStringLiteral("Keep Current Zoom"),
        QStringLiteral("Reset on Fullscreen Toggle"),
        QStringLiteral("Reset Zoom Settings to Defaults"),
        QStringLiteral("Selected item added to favorites"),
        QStringLiteral("Selected item is already in favorites"),
        QStringLiteral("File added to favorites"),
        QStringLiteral("This file is already in favorites"),
        QStringLiteral("Media Files (*.mkv *.mp4 *.webm *.avi *.mov *.mp3 *.flac *.wav *.m4a *.ogg);;All Files (*)"),
        QStringLiteral("Startup volume behavior"),
        QStringLiteral("Remember last volume"),
        QStringLiteral("Use fixed startup volume"),
        QStringLiteral("Subtitle Files (*.srt *.ass *.ssa *.sub *.vtt *.sup *.idx);;All Files (*)"),
        QStringLiteral("Profile applied: %1"),
        QStringLiteral("Keyboard zoom shortcuts remain available. These settings control how far zoom can go, how wheel gestures behave, and whether manual zoom is remembered."),
    };

    for (const QString &language : languages) {
        for (const QString &sourceText : criticalStrings) {
            QVERIFY2(
                revaplayer::application::hasUiTranslation(language, sourceText),
                qPrintable(QStringLiteral("Missing translation for language '%1': %2").arg(language, sourceText)));
        }
    }
}

void CoreTests::themeResourceAndPaletteCoverage()
{
    auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);

    const QString previousStyleSheet = application->styleSheet();
    const QStringList tokenNames {
        QStringLiteral("APP_BG"),
        QStringLiteral("APP_BG_ELEVATED"),
        QStringLiteral("APP_BG_ALT"),
        QStringLiteral("SURFACE_BG"),
        QStringLiteral("SURFACE_BG_ALT"),
        QStringLiteral("SURFACE_BG_SOFT"),
        QStringLiteral("TEXT_PRIMARY"),
        QStringLiteral("TEXT_MUTED"),
        QStringLiteral("TEXT_SUBTLE"),
        QStringLiteral("BORDER_STRONG"),
        QStringLiteral("BORDER_SOFT"),
        QStringLiteral("BORDER_ACCENT"),
        QStringLiteral("ACCENT"),
        QStringLiteral("ACCENT_HOVER"),
        QStringLiteral("ACCENT_PRESSED"),
        QStringLiteral("ACCENT_SOFT"),
        QStringLiteral("BUTTON_BG"),
        QStringLiteral("BUTTON_HOVER"),
        QStringLiteral("BUTTON_PRESSED"),
        QStringLiteral("SELECT_BG"),
        QStringLiteral("SELECT_BORDER"),
        QStringLiteral("TEXT_SELECT_BG"),
        QStringLiteral("TEXT_SELECT_FG"),
        QStringLiteral("OVERLAY_BG"),
        QStringLiteral("OVERLAY_BORDER"),
        QStringLiteral("OSD_BG"),
        QStringLiteral("OSD_BORDER"),
        QStringLiteral("SLIDER_GROOVE"),
        QStringLiteral("SLIDER_PROGRESS"),
        QStringLiteral("SLIDER_HANDLE"),
        QStringLiteral("SEARCH_BG"),
    };

    for (const auto &theme : revaplayer::application::availableThemes()) {
        for (const auto &accent : revaplayer::application::availableAccents()) {
            for (const QString &tokenName : tokenNames) {
                QVERIFY2(
                    revaplayer::application::resolvedThemeColor(theme.id, accent.id, tokenName).isValid(),
                    qPrintable(QStringLiteral("Invalid theme token %1 for %2/%3").arg(tokenName, theme.id, accent.id)));
            }

            for (const auto &density : revaplayer::application::availableDensities()) {
                QString errorMessage;
                QVERIFY2(
                    revaplayer::application::applyApplicationTheme(
                        *application,
                        theme.id,
                        accent.id,
                        density.id,
                        revaplayer::application::ThemeCustomization {},
                        &errorMessage),
                    qPrintable(errorMessage));
                QVERIFY2(
                    !application->styleSheet().contains(QStringLiteral("{{")),
                    qPrintable(QStringLiteral("Unresolved stylesheet token for %1/%2/%3").arg(theme.id, accent.id, density.id)));
            }
        }
    }

    application->setStyleSheet(previousStyleSheet);
}

void CoreTests::settingsDialogSubtitlePreviewVisibility()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    const QString databasePath = temporaryDir.filePath(QStringLiteral("settings.sqlite"));

    revaplayer::application::SettingsController controller(makeStore(databasePath));
    QVERIFY2(controller.initialize(), qPrintable(controller.lastError()));

    revaplayer::ui::SettingsDialog dialog(&controller);
    auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("settingsTabs"));
    QVERIFY(tabs != nullptr);

    auto *previewGroup = dialog.findChild<QGroupBox *>(QStringLiteral("subtitlePreviewGroup"));
    QVERIFY(previewGroup != nullptr);

    const QString playlistTabText = revaplayer::application::translateUiText(QStringLiteral("Playlist"));
    const QString subtitlesTabText = revaplayer::application::translateUiText(QStringLiteral("Subtitles"));

    int playlistIndex = -1;
    int subtitlesIndex = -1;
    for (int index = 0; index < tabs->count(); ++index) {
        const QString tabText = tabs->tabText(index);
        if (tabText == playlistTabText) {
            playlistIndex = index;
        } else if (tabText == subtitlesTabText) {
            subtitlesIndex = index;
        }
    }

    QVERIFY(playlistIndex >= 0);
    QVERIFY(subtitlesIndex >= 0);

    QWidget *playlistTabPage = tabs->widget(playlistIndex);
    QWidget *subtitlesTabPage = tabs->widget(subtitlesIndex);
    QVERIFY(playlistTabPage != nullptr);
    QVERIFY(subtitlesTabPage != nullptr);

    QVERIFY(subtitlesTabPage->isAncestorOf(previewGroup));
    QVERIFY(!playlistTabPage->isAncestorOf(previewGroup));

    tabs->setCurrentIndex(playlistIndex);
    QCoreApplication::processEvents();
    QVERIFY(previewGroup->isHidden());

    tabs->setCurrentIndex(subtitlesIndex);
    QCoreApplication::processEvents();
    QVERIFY(!previewGroup->isHidden());
}

void CoreTests::settingsDialogSubtitleAutoLoadSync()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());
    const QString databasePath = temporaryDir.filePath(QStringLiteral("settings.sqlite"));

    revaplayer::application::SettingsController controller(makeStore(databasePath));
    QVERIFY2(controller.initialize(), qPrintable(controller.lastError()));
    controller.setSubtitleAutoLoadLocalMatches(true);
    controller.setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleAutoLoadModeSetting),
        QStringLiteral("same_name"));

    revaplayer::ui::SettingsDialog dialog(&controller);
    auto *checkBox = dialog.findChild<QCheckBox *>(QStringLiteral("subtitleAutoLoadLocalMatchesCheckBox"));
    auto *comboBox = dialog.findChild<QComboBox *>(QStringLiteral("subtitleAutoLoadModeComboBox"));
    auto *extensionsEdit = dialog.findChild<QLineEdit *>(QStringLiteral("subtitleAutoExtensionsEdit"));
    QVERIFY(checkBox != nullptr);
    QVERIFY(comboBox != nullptr);
    QVERIFY(extensionsEdit != nullptr);

    QVERIFY(checkBox->isChecked());
    QVERIFY(extensionsEdit->isEnabled());

    comboBox->setCurrentIndex(comboBox->findData(QStringLiteral("disabled")));
    QCoreApplication::processEvents();
    QVERIFY(!checkBox->isChecked());
    QVERIFY(!extensionsEdit->isEnabled());

    checkBox->setChecked(true);
    QCoreApplication::processEvents();
    QVERIFY(comboBox->currentData().toString() != QStringLiteral("disabled"));
    QVERIFY(checkBox->isChecked());
    QVERIFY(extensionsEdit->isEnabled());
}

void CoreTests::controlBarDesktopChromeVisibility()
{
    revaplayer::ui::ControlBar controlBar;
    controlBar.setPlaybackAvailable(true);
    controlBar.setTrackMenusEnabled(true, true);
    controlBar.resize(1120, 72);
    controlBar.show();
    QCoreApplication::processEvents();

    auto *playlistButton = controlBar.findChild<QToolButton *>(QStringLiteral("controlPlaylistButton"));
    auto *detailsButton = controlBar.findChild<QToolButton *>(QStringLiteral("controlDetailsButton"));
    auto *fullscreenButton = controlBar.findChild<QToolButton *>(QStringLiteral("controlFullscreenButton"));
    auto *qualityButton = controlBar.findChild<QToolButton *>(QStringLiteral("controlQualityButton"));
    auto *subtitleButton = controlBar.findChild<QToolButton *>(QStringLiteral("controlSubtitleButton"));
    auto *volumeSlider = controlBar.findChild<QSlider *>(QStringLiteral("controlVolumeSlider"));
    auto *positionSlider = controlBar.findChild<QSlider *>(QStringLiteral("controlPositionSlider"));
    auto *timeLabel = controlBar.findChild<QLabel *>(QStringLiteral("controlTimeLabel"));
    auto *volumeGroup = controlBar.findChild<QWidget *>(QStringLiteral("volumeGroup"));

    QVERIFY(playlistButton != nullptr);
    QVERIFY(detailsButton != nullptr);
    QVERIFY(fullscreenButton != nullptr);
    QVERIFY(qualityButton != nullptr);
    QVERIFY(subtitleButton != nullptr);
    QVERIFY(volumeSlider != nullptr);
    QVERIFY(positionSlider != nullptr);
    QVERIFY(timeLabel != nullptr);
    QVERIFY(volumeGroup != nullptr);
    QVERIFY(!playlistButton->isHidden());
    QVERIFY(!detailsButton->isHidden());
    QVERIFY(!fullscreenButton->isHidden());
    QVERIFY(!qualityButton->isHidden());
    QVERIFY(!subtitleButton->isHidden());
    QVERIFY(!volumeSlider->isHidden());
    QVERIFY(!positionSlider->isHidden());
    QVERIFY(!timeLabel->isHidden());
    QVERIFY(!volumeGroup->isHidden());
    QVERIFY(playlistButton->width() > 0);
    QVERIFY(detailsButton->width() > 0);
    QVERIFY(qualityButton->width() > 0);
    QVERIFY(subtitleButton->width() > 0);
    QVERIFY(positionSlider->width() > 0);
    QVERIFY(playlistButton->mapTo(&controlBar, QPoint(0, 0)).x() < positionSlider->mapTo(&controlBar, QPoint(0, 0)).x());
    QVERIFY(detailsButton->mapTo(&controlBar, QPoint(0, 0)).x() < positionSlider->mapTo(&controlBar, QPoint(0, 0)).x());
    QVERIFY(positionSlider->mapTo(&controlBar, QPoint(0, 0)).x() < timeLabel->mapTo(&controlBar, QPoint(0, 0)).x());
    QVERIFY(timeLabel->mapTo(&controlBar, QPoint(0, 0)).x() < fullscreenButton->mapTo(&controlBar, QPoint(0, 0)).x());
    QVERIFY(controlBar.findChild<QToolButton *>(QStringLiteral("controlVolumeDownButton")) == nullptr);
    QVERIFY(controlBar.findChild<QToolButton *>(QStringLiteral("controlVolumeUpButton")) == nullptr);
}

void CoreTests::controlBarButtonsEmitSignals()
{
    revaplayer::ui::ControlBar controlBar;
    controlBar.setPlaybackAvailable(true);
    controlBar.resize(1120, 72);
    controlBar.show();
    QCoreApplication::processEvents();

    auto *playlistButton = controlBar.findChild<QToolButton *>(QStringLiteral("controlPlaylistButton"));
    auto *detailsButton = controlBar.findChild<QToolButton *>(QStringLiteral("controlDetailsButton"));
    auto *fullscreenButton = controlBar.findChild<QToolButton *>(QStringLiteral("controlFullscreenButton"));
    auto *volumeSlider = controlBar.findChild<QSlider *>(QStringLiteral("controlVolumeSlider"));

    QVERIFY(playlistButton != nullptr);
    QVERIFY(detailsButton != nullptr);
    QVERIFY(fullscreenButton != nullptr);
    QVERIFY(volumeSlider != nullptr);

    QSignalSpy playlistSpy(&controlBar, &revaplayer::ui::ControlBar::playlistPanelToggled);
    QSignalSpy detailsSpy(&controlBar, &revaplayer::ui::ControlBar::detailsPanelToggled);
    QSignalSpy fullscreenSpy(&controlBar, &revaplayer::ui::ControlBar::fullscreenRequested);
    QSignalSpy volumeSpy(&controlBar, &revaplayer::ui::ControlBar::volumeRequested);

    QTest::mouseClick(playlistButton, Qt::LeftButton);
    QTest::mouseClick(detailsButton, Qt::LeftButton);
    QTest::mouseClick(fullscreenButton, Qt::LeftButton);
    volumeSlider->setValue(84);
    QCoreApplication::processEvents();

    QCOMPARE(playlistSpy.count(), 1);
    QCOMPARE(detailsSpy.count(), 1);
    QCOMPARE(fullscreenSpy.count(), 1);
    QVERIFY(volumeSpy.count() >= 1);
}

void CoreTests::desktopIntegrationPlatformName()
{
    const revaplayer::platform::DesktopIntegration integration;
    const QString platformName = integration.platformName();
    QVERIFY(!platformName.trimmed().isEmpty());
    QCOMPARE(platformName, QGuiApplication::platformName());
}

void CoreTests::thumbnailServiceUnavailableWithoutBridge()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());

    const QString mediaPath = temporaryDir.filePath(QStringLiteral("sample.mp4"));
    QFile mediaFile(mediaPath);
    QVERIFY(mediaFile.open(QIODevice::WriteOnly));
    mediaFile.close();

    revaplayer::services::media::ThumbnailService service;
    service.setCurrentSource(mediaPath);

    QSignalSpy unavailableSpy(&service, &revaplayer::services::media::ThumbnailService::thumbnailUnavailable);
    service.requestThumbnail(mediaPath, 1.0);

    QCOMPARE(unavailableSpy.count(), 1);
    const QList<QVariant> arguments = unavailableSpy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QFileInfo(mediaPath).canonicalFilePath());
    QCOMPARE(arguments.at(1).toLongLong(), 1000);
    QVERIFY(arguments.at(2).toString().contains(QStringLiteral("not configured")));
}

void CoreTests::thumbnailServicePendingWorkerTimesOut()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());

    const QString mediaPath = temporaryDir.filePath(QStringLiteral("pending-worker.mp4"));
    QFile mediaFile(mediaPath);
    QVERIFY(mediaFile.open(QIODevice::WriteOnly));
    mediaFile.close();

    revaplayer::services::media::ThumbnailService service;
    service.setCommandDispatcher([](const QStringList &) {
        return true;
    });
    service.setCurrentSource(mediaPath);

    QSignalSpy unavailableSpy(&service, &revaplayer::services::media::ThumbnailService::thumbnailUnavailable);
    service.requestThumbnail(mediaPath, 3.0);

    QTRY_COMPARE_WITH_TIMEOUT(unavailableSpy.count(), 1, 2500);
    const QList<QVariant> arguments = unavailableSpy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QFileInfo(mediaPath).canonicalFilePath());
    QCOMPARE(arguments.at(1).toLongLong(), 3000);
    QVERIFY(arguments.at(2).toString().contains(QStringLiteral("timed out")));
}

void CoreTests::thumbnailServiceActiveRequestTimesOut()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());

    const QString mediaPath = temporaryDir.filePath(QStringLiteral("active-worker.mp4"));
    QFile mediaFile(mediaPath);
    QVERIFY(mediaFile.open(QIODevice::WriteOnly));
    mediaFile.close();

    const QString previewBasePath = temporaryDir.filePath(QStringLiteral("missing-preview"));
    revaplayer::services::media::ThumbnailService service;
    service.setCommandDispatcher([](const QStringList &) {
        return true;
    });
    service.setCurrentSource(mediaPath);
    service.handleMpvClientMessage(QStringList {
        QStringLiteral("thumbfast-info"),
        QStringLiteral("{\"available\":true,\"disabled\":false,\"thumbnail\":\"%1\",\"width\":4,\"height\":2}")
            .arg(previewBasePath),
    });

    QSignalSpy unavailableSpy(&service, &revaplayer::services::media::ThumbnailService::thumbnailUnavailable);
    service.requestThumbnail(mediaPath, 4.0);

    QTRY_COMPARE_WITH_TIMEOUT(unavailableSpy.count(), 1, 3000);
    const QList<QVariant> arguments = unavailableSpy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QFileInfo(mediaPath).canonicalFilePath());
    QCOMPARE(arguments.at(1).toLongLong(), 4000);
    QVERIFY(arguments.at(2).toString().contains(QStringLiteral("timed out")));
}

void CoreTests::thumbnailServiceProducesAndCachesPreview()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());

    const QByteArray previousCacheHome = qgetenv("XDG_CACHE_HOME");
    qputenv("XDG_CACHE_HOME", temporaryDir.path().toUtf8());

    const QString mediaPath = temporaryDir.filePath(QStringLiteral("movie.mkv"));
    QFile mediaFile(mediaPath);
    QVERIFY(mediaFile.open(QIODevice::WriteOnly));
    mediaFile.close();

    const QString normalizedSource = QFileInfo(mediaPath).canonicalFilePath();
    const QString previewBasePath = temporaryDir.filePath(QStringLiteral("thumbfast-preview"));
    QStringList dispatchedArguments;
    QImage readyImage;
    {
        revaplayer::services::media::ThumbnailService service;
        service.setPreviewWidthOverride(2);
        service.setCommandDispatcher([&dispatchedArguments](const QStringList &arguments) {
            dispatchedArguments = arguments;
            return true;
        });
        service.setCurrentSource(mediaPath);
        service.handleMpvClientMessage(QStringList {
            QStringLiteral("thumbfast-info"),
            QStringLiteral("{\"available\":true,\"disabled\":false,\"thumbnail\":\"%1\",\"width\":4,\"height\":2}")
                .arg(previewBasePath),
        });

        QSignalSpy readySpy(&service, &revaplayer::services::media::ThumbnailService::thumbnailReady);
        QSignalSpy unavailableSpy(&service, &revaplayer::services::media::ThumbnailService::thumbnailUnavailable);

        service.requestThumbnail(mediaPath, 1.0);

        QCOMPARE(dispatchedArguments.size(), 5);
        QCOMPARE(dispatchedArguments.at(0), QStringLiteral("script-message"));
        QCOMPARE(dispatchedArguments.at(1), QStringLiteral("thumb"));
        QCOMPARE(dispatchedArguments.at(2), QStringLiteral("1.000"));

        QImage sourceImage(4, 2, QImage::Format_ARGB32);
        sourceImage.fill(QColor(0x22, 0x88, 0xdd, 0xff));
        QFile previewFile(previewBasePath + QStringLiteral(".bgra"));
        QVERIFY(previewFile.open(QIODevice::WriteOnly));
        QCOMPARE(previewFile.write(reinterpret_cast<const char *>(sourceImage.constBits()),
                                   static_cast<qint64>(sourceImage.sizeInBytes())),
                 static_cast<qint64>(sourceImage.sizeInBytes()));
        previewFile.close();

        QTRY_COMPARE(readySpy.count(), 1);
        QCOMPARE(unavailableSpy.count(), 0);

        const QList<QVariant> readyArguments = readySpy.takeFirst();
        QCOMPARE(readyArguments.at(0).toString(), normalizedSource);
        QCOMPARE(readyArguments.at(1).toLongLong(), 1000);

        readyImage = qvariant_cast<QImage>(readyArguments.at(2));
        QVERIFY(!readyImage.isNull());
        QCOMPARE(readyImage.width(), 2);
        QCOMPARE(readyImage.height(), 1);

        QImage cachedImage;
        QVERIFY(service.loadCachedThumbnail(mediaPath, 1.0, &cachedImage));
        QCOMPARE(cachedImage.size(), readyImage.size());
    }

    revaplayer::services::media::ThumbnailService secondService;
    secondService.setPreviewWidthOverride(2);
    QImage diskCachedImage;
    QVERIFY(secondService.loadCachedThumbnail(mediaPath, 1.0, &diskCachedImage));
    QCOMPARE(diskCachedImage.size(), readyImage.size());

    if (previousCacheHome.isEmpty()) {
        qunsetenv("XDG_CACHE_HOME");
    } else {
        qputenv("XDG_CACHE_HOME", previousCacheHome);
    }
}

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("RevaPlayer_tests"));

    CoreTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "CoreTests.moc"
