#include "app/AppBootstrap.hpp"

#include "application/HistoryController.hpp"
#include "application/BookmarkController.hpp"
#include "application/PlaybackController.hpp"
#include "application/PlaylistController.hpp"
#include "application/SettingsController.hpp"
#include "application/SnapshotController.hpp"
#include "application/ThemeStyle.hpp"
#include "application/UiLanguage.hpp"
#include "infrastructure/mpv/MpvRenderHost.hpp"
#include "infrastructure/storage/SqliteStore.hpp"
#include "ui/MainWindow.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QLibraryInfo>
#include <QLocale>
#include <QTimer>
#include <QTranslator>
namespace revaplayer::app {
namespace {

QString legacyDatabaseEnvVarName()
{
    return QStringLiteral("NEW") + QStringLiteral("POT") + QStringLiteral("PLAYER_DB_PATH");
}

QString configuredDatabasePath()
{
    const QString preferredPath = qEnvironmentVariable("REVAPLAYER_DB_PATH").trimmed();
    if (!preferredPath.isEmpty()) {
        return preferredPath;
    }

    return qEnvironmentVariable(legacyDatabaseEnvVarName().toUtf8().constData()).trimmed();
}

}  // namespace

AppBootstrap::AppBootstrap() = default;

AppBootstrap::~AppBootstrap() = default;

int AppBootstrap::run(QApplication &application, const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Linux desktop media player built with Qt Widgets and libmpv."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption urlOption(
        QStringList {QStringLiteral("u"), QStringLiteral("url")},
        QStringLiteral("Open a media URL on startup."),
        QStringLiteral("url"));

    parser.addOption(urlOption);
    parser.addPositionalArgument(QStringLiteral("media"), QStringLiteral("Media files to open on startup."));
    parser.process(arguments);
    const QString startupUrl = parser.isSet(urlOption) ? parser.value(urlOption).trimmed() : QString {};
    const QStringList startupMedia = parser.positionalArguments();
    const QString databasePath = configuredDatabasePath();
    const auto makeStore = [&databasePath]() {
        return std::make_unique<revaplayer::infrastructure::storage::SqliteStore>(databasePath);
    };

    playbackController_ = std::make_unique<revaplayer::application::PlaybackController>();
    playlistController_ = std::make_unique<revaplayer::application::PlaylistController>();
    bookmarkController_ = std::make_unique<revaplayer::application::BookmarkController>(
        makeStore());
    historyController_ = std::make_unique<revaplayer::application::HistoryController>(
        makeStore());
    settingsController_ = std::make_unique<revaplayer::application::SettingsController>(
        makeStore());
    snapshotController_ = std::make_unique<revaplayer::application::SnapshotController>(
        playbackController_.get(),
        settingsController_.get());

    if (!settingsController_->initialize()) {
        qWarning() << "Settings initialization failed:" << settingsController_->lastError();
    }
    if (settingsController_ != nullptr && playbackController_ != nullptr) {
        playbackController_->setUseExternalMpvConfig(settingsController_->useExternalMpvConfig());
    }
    if (!bookmarkController_->initialize()) {
        qWarning() << "Bookmark initialization failed:" << bookmarkController_->lastError();
    }

    if (!historyController_->initialize()) {
        qWarning() << "History initialization failed:" << historyController_->lastError();
    }
    const QString interfaceLanguage = settingsController_ != nullptr
        ? settingsController_->interfaceLanguage()
        : revaplayer::application::defaultUiLanguageId();
    revaplayer::application::setCurrentUiLanguage(interfaceLanguage);
    loadQtTranslations(application, interfaceLanguage);
    application.setLayoutDirection(revaplayer::application::currentUiLanguageDirection());
    QLocale::setDefault(QLocale(interfaceLanguage));
    applyTheme(
        application,
        settingsController_ != nullptr ? settingsController_->uiTheme() : QStringLiteral("gray"));

    mainWindow_ = std::make_unique<revaplayer::ui::MainWindow>(
        bookmarkController_.get(),
        historyController_.get(),
        playbackController_.get(),
        playlistController_.get(),
        snapshotController_.get(),
        settingsController_.get());

    auto *renderHost = mainWindow_->renderHost();
    playbackController_->initialize(renderHost);
    if (settingsController_ != nullptr && settingsController_->isReady()) {
        playbackController_->setVolume(settingsController_->startupVolume());
        playbackController_->setRepeatMode(settingsController_->defaultRepeatMode());
    }
    mainWindow_->show();

    if (!startupUrl.isEmpty() || !startupMedia.isEmpty()) {
        const auto startRequestedMedia = [this, startupUrl, startupMedia]() {
            if (mainWindow_ != nullptr) {
                mainWindow_->openStartupRequest(startupUrl, startupMedia);
            }
        };

        if (renderHost != nullptr && renderHost->isRendererReady()) {
            startRequestedMedia();
        } else if (renderHost != nullptr) {
            QObject::connect(renderHost, &revaplayer::infrastructure::mpv::MpvRenderHost::rendererReady,
                             &application, startRequestedMedia);
            QTimer::singleShot(0, renderHost, [renderHost]() {
                renderHost->update();
            });
        } else {
            QTimer::singleShot(0, &application, startRequestedMedia);
        }
    }

    return application.exec();
}

void AppBootstrap::loadQtTranslations(QApplication &application, const QString &languageId)
{
    qtTranslators_.clear();

    const QString normalizedLanguage = revaplayer::application::normalizeUiLanguageId(languageId);
    const QLocale locale(normalizedLanguage);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QString translationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    const QString translationsPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif

    for (const QString &catalog : {QStringLiteral("qtbase"), QStringLiteral("qt")}) {
        auto translator = std::make_unique<QTranslator>();
        if (translator->load(locale, catalog, QStringLiteral("_"), translationsPath)) {
            application.installTranslator(translator.get());
            qtTranslators_.push_back(std::move(translator));
        }
    }
}

void AppBootstrap::applyTheme(QApplication &application, const QString &themeId) const
{
    const QString accentId = settingsController_ != nullptr
        ? settingsController_->customValue(QStringLiteral("ui/accent"), QStringLiteral("blue"))
        : QStringLiteral("blue");
    const QString densityId = settingsController_ != nullptr
        ? settingsController_->customValue(QStringLiteral("ui/density"), QStringLiteral("normal"))
        : QStringLiteral("normal");
    QString errorMessage;
    if (!revaplayer::application::applyApplicationTheme(
            application,
            themeId,
            accentId,
            densityId,
            revaplayer::application::ThemeCustomization {
                settingsController_ != nullptr ? settingsController_->customValue(QStringLiteral("ui/radius_px"), QStringLiteral("10")).toInt() : 10,
                settingsController_ != nullptr ? settingsController_->customValue(QStringLiteral("ui/spacing_px"), QStringLiteral("8")).toInt() : 8,
                settingsController_ != nullptr ? settingsController_->customValue(QStringLiteral("ui/font_scale_percent"), QStringLiteral("100")).toInt() : 100,
                settingsController_ != nullptr ? settingsController_->customValue(QStringLiteral("ui/font_weight_value"), QStringLiteral("500")).toInt() : 500,
                settingsController_ != nullptr ? settingsController_->customValue(QStringLiteral("ui/letter_spacing_px"), QStringLiteral("0.0")).toDouble() : 0.0,
                settingsController_ != nullptr ? settingsController_->customValue(QStringLiteral("ui/border_contrast_percent"), QStringLiteral("100")).toInt() : 100,
                settingsController_ != nullptr ? settingsController_->customValue(QStringLiteral("ui/shadow_strength_percent"), QStringLiteral("60")).toInt() : 60,
                settingsController_ != nullptr ? settingsController_->customValue(QStringLiteral("ui/blur_strength_percent"), QStringLiteral("0")).toInt() : 0,
                settingsController_ != nullptr ? settingsController_->customValue(QStringLiteral("ui/overlay_opacity_percent"), QStringLiteral("82")).toInt() : 82,
            },
            &errorMessage)) {
        qWarning() << "Theme application failed:" << errorMessage;
    }
}

}  // namespace revaplayer::app
