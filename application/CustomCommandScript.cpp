#include "application/CustomCommandScript.hpp"

#include <QRegularExpression>

namespace revaplayer::application {

QVector<QStringList> parseCustomCommandScript(const QString &script, QString *errorMessage)
{
    QVector<QStringList> commands;
    const QStringList lines = script.split(QRegularExpression(QStringLiteral("\\r?\\n")), Qt::KeepEmptyParts);

    for (qsizetype index = 0; index < lines.size(); ++index) {
        const QString rawLine = lines.at(index);
        const QString trimmedLine = rawLine.trimmed();
        if (trimmedLine.isEmpty() || trimmedLine.startsWith(QLatin1Char('#'))) {
            continue;
        }

        QStringList parts = rawLine.split(QLatin1Char('|'), Qt::KeepEmptyParts);
        for (QString &part : parts) {
            part = part.trimmed();
        }

        if (parts.isEmpty() || parts.first().isEmpty()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Line %1 does not contain a valid mpv command name.").arg(index + 1);
            }
            return {};
        }

        commands.push_back(parts);
    }

    if (commands.isEmpty() && errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Add at least one mpv command line.");
    }

    return commands;
}

}  // namespace revaplayer::application
