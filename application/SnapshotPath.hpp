#pragma once

#include <QDateTime>
#include <QString>

namespace revaplayer::application {

[[nodiscard]] QString sanitizeSnapshotFileStem(const QString &rawLabel);
[[nodiscard]] QString buildScreenshotFilePath(const QString &directoryPath,
                                              const QString &mediaLabel,
                                              const QDateTime &timestamp);

}  // namespace revaplayer::application
