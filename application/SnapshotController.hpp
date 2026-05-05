#pragma once

#include <QObject>
#include <QString>

namespace revaplayer::application {
class PlaybackController;
class SettingsController;
}

namespace revaplayer::application {

class SnapshotController final : public QObject {
    Q_OBJECT

public:
    SnapshotController(revaplayer::application::PlaybackController *playbackController,
                       revaplayer::application::SettingsController *settingsController,
                       QObject *parent = nullptr);

    [[nodiscard]] QString defaultScreenshotDirectory() const;
    [[nodiscard]] QString screenshotDirectory() const;
    bool captureScreenshot(const QString &mediaLabel, QString *outputPath, QString *errorMessage) const;

private:
    [[nodiscard]] QString buildScreenshotFilePath(const QString &mediaLabel, QString *errorMessage) const;

    revaplayer::application::PlaybackController *playbackController_ {nullptr};
    revaplayer::application::SettingsController *settingsController_ {nullptr};
};

}  // namespace revaplayer::application
