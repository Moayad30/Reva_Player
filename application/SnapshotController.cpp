#include "application/SnapshotController.hpp"

#include "application/PlaybackController.hpp"
#include "application/SnapshotPath.hpp"
#include "application/SettingsController.hpp"

#include <QDateTime>
#include <QDir>
#include <QStandardPaths>

namespace revaplayer::application {
namespace {

constexpr auto kScreenshotDirectoryName = "Reva Player";

}  // namespace

SnapshotController::SnapshotController(revaplayer::application::PlaybackController *playbackController,
                                       revaplayer::application::SettingsController *settingsController,
                                       QObject *parent)
    : QObject(parent)
    , playbackController_(playbackController)
    , settingsController_(settingsController)
{
}

QString SnapshotController::defaultScreenshotDirectory() const
{
    const QString picturesLocation = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (!picturesLocation.trimmed().isEmpty()) {
        return QDir(picturesLocation).filePath(QString::fromLatin1(kScreenshotDirectoryName));
    }

    const QString homeLocation = QDir::homePath();
    return homeLocation.trimmed().isEmpty()
        ? QString::fromLatin1(kScreenshotDirectoryName)
        : QDir(homeLocation).filePath(QString::fromLatin1(kScreenshotDirectoryName));
}

QString SnapshotController::screenshotDirectory() const
{
    if (settingsController_ == nullptr) {
        return defaultScreenshotDirectory();
    }

    const QString configuredDirectory = settingsController_->screenshotDirectory().trimmed();
    return configuredDirectory.isEmpty() ? defaultScreenshotDirectory() : configuredDirectory;
}

bool SnapshotController::captureScreenshot(const QString &mediaLabel,
                                           QString *outputPath,
                                           QString *errorMessage) const
{
    if (outputPath != nullptr) {
        outputPath->clear();
    }
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    if (playbackController_ == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Screenshot capture is not available right now.");
        }
        return false;
    }

    const QString targetFilePath = buildScreenshotFilePath(mediaLabel, errorMessage);
    if (targetFilePath.isEmpty()) {
        return false;
    }

    if (!playbackController_->captureScreenshot(targetFilePath)) {
        if (errorMessage != nullptr && errorMessage->isEmpty()) {
            *errorMessage = QStringLiteral("mpv rejected the screenshot request.");
        }
        return false;
    }

    if (outputPath != nullptr) {
        *outputPath = targetFilePath;
    }

    return true;
}

QString SnapshotController::buildScreenshotFilePath(const QString &mediaLabel, QString *errorMessage) const
{
    const QString targetDirectory = screenshotDirectory().trimmed();
    if (targetDirectory.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No screenshot directory is configured.");
        }
        return {};
    }

    QDir directory(targetDirectory);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not create the screenshot directory.");
        }
        return {};
    }

    return revaplayer::application::buildScreenshotFilePath(
        directory.absolutePath(),
        mediaLabel,
        QDateTime::currentDateTime());
}

}  // namespace revaplayer::application
