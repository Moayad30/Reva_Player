#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace revaplayer::application {

[[nodiscard]] QVector<QStringList> parseCustomCommandScript(const QString &script, QString *errorMessage = nullptr);

}  // namespace revaplayer::application
