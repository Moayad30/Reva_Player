#pragma once

#include <QStringList>

#include <memory>
#include <vector>

class QApplication;
class QTranslator;

namespace revaplayer::application {
class BookmarkController;
class HistoryController;
class PlaybackController;
class PlaylistController;
class SettingsController;
class SnapshotController;
}

namespace revaplayer::ui {
class MainWindow;
}

namespace revaplayer::app {

class AppBootstrap final {
public:
    AppBootstrap();
    ~AppBootstrap();

    int run(QApplication &application, const QStringList &arguments);

private:
    void applyTheme(QApplication &application, const QString &themeId) const;
    void loadQtTranslations(QApplication &application, const QString &languageId);

    std::unique_ptr<revaplayer::application::BookmarkController> bookmarkController_;
    std::unique_ptr<revaplayer::application::HistoryController> historyController_;
    std::unique_ptr<revaplayer::application::PlaybackController> playbackController_;
    std::unique_ptr<revaplayer::application::PlaylistController> playlistController_;
    std::unique_ptr<revaplayer::application::SettingsController> settingsController_;
    std::unique_ptr<revaplayer::application::SnapshotController> snapshotController_;
    std::unique_ptr<revaplayer::ui::MainWindow> mainWindow_;
    std::vector<std::unique_ptr<QTranslator>> qtTranslators_;
};

}  // namespace revaplayer::app
