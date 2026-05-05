#pragma once

#include <QVector>
#include <QString>
#include <Qt>

namespace revaplayer::application {

struct UiLanguageOption {
    QString id;
    QString label;
    Qt::LayoutDirection direction {Qt::LeftToRight};
};

[[nodiscard]] QString defaultUiLanguageId();
[[nodiscard]] QVector<UiLanguageOption> availableUiLanguages();
[[nodiscard]] QString normalizeUiLanguageId(const QString &languageId);
void setCurrentUiLanguage(const QString &languageId);
[[nodiscard]] QString currentUiLanguage();
[[nodiscard]] Qt::LayoutDirection currentUiLanguageDirection();
[[nodiscard]] bool hasUiTranslation(const QString &languageId, const QString &sourceText);
[[nodiscard]] QString translateUiText(const QString &sourceText);

}  // namespace revaplayer::application
