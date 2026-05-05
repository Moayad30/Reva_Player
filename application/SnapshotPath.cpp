#include "application/SnapshotPath.hpp"

#include <QDir>
#include <QRegularExpression>

namespace revaplayer::application {

QString sanitizeSnapshotFileStem(const QString &rawLabel)
{
    QString sanitized = rawLabel.trimmed();
    if (sanitized.isEmpty()) {
        sanitized = QStringLiteral("snapshot");
    }

    sanitized.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|]+)")), QStringLiteral("_"));
    sanitized.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QStringLiteral("_"));
    sanitized.replace(QRegularExpression(QStringLiteral(R"(_+)")), QStringLiteral("_"));
    sanitized = sanitized.left(80).trimmed();

    return sanitized.isEmpty() ? QStringLiteral("snapshot") : sanitized;
}

QString buildScreenshotFilePath(const QString &directoryPath,
                                const QString &mediaLabel,
                                const QDateTime &timestamp)
{
    const QString safeTimestamp = timestamp.isValid()
        ? timestamp.toString(QStringLiteral("yyyyMMdd-HHmmss"))
        : QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    return QDir(directoryPath).filePath(
        QStringLiteral("%1-%2.png").arg(safeTimestamp, sanitizeSnapshotFileStem(mediaLabel)));
}

}  // namespace revaplayer::application
