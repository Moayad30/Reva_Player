#pragma once

#include <QColor>
#include <QString>
#include <QVector>

class QApplication;

namespace revaplayer::application {

struct ThemeOption {
    QString id;
    QString label;
};

struct AccentOption {
    QString id;
    QString label;
};

struct DensityOption {
    QString id;
    QString label;
};

struct ThemeCustomization {
    int radiusPx {10};
    int spacingPx {8};
    int fontScalePercent {100};
    int fontWeightValue {500};
    double letterSpacingPx {0.0};
    int borderContrastPercent {100};
    int shadowStrengthPercent {60};
    int blurStrengthPercent {0};
    int overlayOpacityPercent {82};
};

[[nodiscard]] const QVector<ThemeOption> &availableThemes();
[[nodiscard]] const QVector<AccentOption> &availableAccents();
[[nodiscard]] const QVector<DensityOption> &availableDensities();
[[nodiscard]] QString normalizeThemeId(const QString &themeId);
[[nodiscard]] QString themeLabel(const QString &themeId);
[[nodiscard]] QString normalizeAccentId(const QString &accentId);
[[nodiscard]] QString accentLabel(const QString &accentId);
[[nodiscard]] QString normalizeDensityId(const QString &densityId);
[[nodiscard]] QString densityLabel(const QString &densityId);
[[nodiscard]] QColor resolvedThemeColor(const QString &themeId,
                                        const QString &accentId,
                                        const QString &tokenName);
[[nodiscard]] QString buildThemeCustomizationStyleSheet(const ThemeCustomization &customization,
                                                       const QString &themeId,
                                                       const QString &accentId);
[[nodiscard]] bool applyApplicationTheme(QApplication &application,
                                         const QString &themeId,
                                         const QString &accentId,
                                         const QString &densityId,
                                         const ThemeCustomization &customization = ThemeCustomization {},
                                         QString *errorMessage = nullptr);

}  // namespace revaplayer::application
