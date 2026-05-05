#include "application/ThemeStyle.hpp"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QHash>
#include <QStyleFactory>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace revaplayer::application {
namespace {

struct ThemePalette {
    const char *appBg;
    const char *appBgElevated;
    const char *appBgAlt;
    const char *surfaceBg;
    const char *surfaceBgAlt;
    const char *surfaceBgSoft;
    const char *textPrimary;
    const char *textMuted;
    const char *textSubtle;
    const char *borderStrong;
    const char *borderSoft;
    const char *borderAccent;
    const char *accent;
    const char *accentHover;
    const char *accentPressed;
    const char *accentSoft;
    const char *buttonBg;
    const char *buttonHover;
    const char *buttonPressed;
    const char *selectBg;
    const char *selectBorder;
    const char *overlayBg;
    const char *overlayBorder;
    const char *osdBg;
    const char *osdBorder;
    const char *sliderGroove;
    const char *sliderProgress;
    const char *sliderHandle;
    const char *searchBg;
};

struct AccentPalette {
    const char *accent;
    const char *accentHover;
    const char *accentPressed;
    const char *accentSoft;
    const char *selectBg;
    const char *selectBorder;
    const char *borderAccent;
    const char *sliderProgress;
};

QColor colorFromCssString(const QString &colorString);

QColor blendColors(const QColor &start, const QColor &end, const qreal amount)
{
    const qreal clampedAmount = std::clamp(amount, 0.0, 1.0);
    return QColor(
        static_cast<int>(std::lround(start.red() + ((end.red() - start.red()) * clampedAmount))),
        static_cast<int>(std::lround(start.green() + ((end.green() - start.green()) * clampedAmount))),
        static_cast<int>(std::lround(start.blue() + ((end.blue() - start.blue()) * clampedAmount))),
        static_cast<int>(std::lround(start.alpha() + ((end.alpha() - start.alpha()) * clampedAmount))));
}

bool paletteLooksLight(const ThemePalette &palette)
{
    const QColor surfaceColor(colorFromCssString(QString::fromLatin1(palette.surfaceBg)));
    return surfaceColor.isValid() && surfaceColor.lightness() >= 190;
}

QColor colorFromCssString(const QString &colorString)
{
    const QString trimmedColor = colorString.trimmed();
    QColor color(trimmedColor);
    if (color.isValid()) {
        return color;
    }

    const bool isRgb = trimmedColor.startsWith(QStringLiteral("rgb("), Qt::CaseInsensitive)
        && trimmedColor.endsWith(QChar(')'));
    const bool isRgba = trimmedColor.startsWith(QStringLiteral("rgba("), Qt::CaseInsensitive)
        && trimmedColor.endsWith(QChar(')'));
    if (!isRgb && !isRgba) {
        return {};
    }

    const int prefixLength = isRgba ? 5 : 4;
    const QStringList parts = trimmedColor.mid(prefixLength, trimmedColor.size() - prefixLength - 1).split(QChar(','));
    if ((isRgb && parts.size() != 3) || (isRgba && parts.size() != 4)) {
        return {};
    }

    auto parseChannel = [](const QString &value, bool *ok) {
        const int channel = value.trimmed().toInt(ok);
        return std::clamp(channel, 0, 255);
    };

    bool redOk = false;
    bool greenOk = false;
    bool blueOk = false;
    const int red = parseChannel(parts.at(0), &redOk);
    const int green = parseChannel(parts.at(1), &greenOk);
    const int blue = parseChannel(parts.at(2), &blueOk);
    if (!redOk || !greenOk || !blueOk) {
        return {};
    }

    qreal alpha = 1.0;
    if (isRgba) {
        QString alphaText = parts.at(3).trimmed();
        bool alphaPercent = false;
        if (alphaText.endsWith(QChar('%'))) {
            alphaPercent = true;
            alphaText.chop(1);
        }

        bool alphaOk = false;
        alpha = alphaText.toDouble(&alphaOk);
        if (!alphaOk) {
            return {};
        }
        alpha = alphaPercent ? alpha / 100.0 : alpha;
    }

    QColor parsedColor(red, green, blue);
    parsedColor.setAlphaF(std::clamp(alpha, 0.0, 1.0));
    return parsedColor;
}

constexpr ThemePalette kNavyPalette {
    "#101114",
    "#121722",
    "#171e2a",
    "#12161d",
    "#171d26",
    "#1a2130",
    "#e8ecf3",
    "#b7c1cf",
    "#8f9bb0",
    "#283244",
    "#232c3a",
    "rgba(110, 141, 188, 0.54)",
    "#5f90ff",
    "#7ba5ff",
    "#3e70de",
    "rgba(95, 144, 255, 0.18)",
    "#1b2432",
    "#23344b",
    "#172536",
    "#27354a",
    "#466696",
    "rgba(8, 12, 20, 0.82)",
    "rgba(120, 152, 208, 0.44)",
    "rgba(8, 13, 20, 0.84)",
    "rgba(109, 157, 227, 0.44)",
    "#1c2431",
    "#4f88ff",
    "#edf3ff",
    "#141b27",
};

constexpr ThemePalette kDarkPalette {
    "#0f1013",
    "#14161b",
    "#1a1d25",
    "#14171d",
    "#1a1e26",
    "#1f2430",
    "#eef1f7",
    "#b5bcc8",
    "#8892a3",
    "#2a313d",
    "#232a35",
    "rgba(101, 132, 182, 0.48)",
    "#6a92ff",
    "#83a5ff",
    "#4f73d8",
    "rgba(106, 146, 255, 0.15)",
    "#1c212b",
    "#283140",
    "#1a202b",
    "#303b4c",
    "#4f6381",
    "rgba(7, 9, 15, 0.84)",
    "rgba(136, 157, 198, 0.40)",
    "rgba(10, 12, 18, 0.84)",
    "rgba(120, 154, 220, 0.40)",
    "#202531",
    "#6a92ff",
    "#eef2f8",
    "#171b22",
};

constexpr ThemePalette kDayPalette {
    "#f3f6fb",
    "#ffffff",
    "#eaf0f7",
    "#fbfcfe",
    "#edf2f8",
    "#e4ebf4",
    "#182232",
    "#5b687a",
    "#7f8b9c",
    "#c7d3e1",
    "#d5deea",
    "rgba(80, 125, 203, 0.30)",
    "#4e7de1",
    "#6a93ec",
    "#395fb0",
    "rgba(78, 125, 225, 0.12)",
    "#eef3fa",
    "#e3ebf8",
    "#dbe4f1",
    "#d9e6fb",
    "#84a3df",
    "rgba(245, 249, 255, 0.92)",
    "rgba(122, 150, 206, 0.34)",
    "rgba(17, 29, 48, 0.90)",
    "rgba(145, 177, 234, 0.42)",
    "#d5deea",
    "#4e7de1",
    "#ffffff",
    "#ffffff",
};

constexpr ThemePalette kMistPalette {
    "#23272d",
    "#2a2f36",
    "#31363e",
    "#2c3138",
    "#363c45",
    "#414852",
    "#eef2f6",
    "#b7c0cb",
    "#8e98a6",
    "#525c68",
    "#454d58",
    "rgba(142, 153, 170, 0.42)",
    "#93a5ba",
    "#a6b6c8",
    "#78889b",
    "rgba(147, 165, 186, 0.14)",
    "#353b44",
    "#424954",
    "#2c323a",
    "#45505d",
    "#6d7c8e",
    "rgba(18, 22, 28, 0.84)",
    "rgba(157, 171, 191, 0.34)",
    "rgba(20, 24, 31, 0.88)",
    "rgba(170, 184, 204, 0.36)",
    "#4a515b",
    "#93a5ba",
    "#f2f5f7",
    "#2f353d",
};

constexpr ThemePalette kGrayPalette {
    "#17181b",
    "#1b1d21",
    "#20232a",
    "#1b1f25",
    "#21262d",
    "#272d35",
    "#edf0f4",
    "#b7bec8",
    "#9099a6",
    "#353c48",
    "#2c333d",
    "rgba(135, 151, 177, 0.45)",
    "#8ba0be",
    "#a4b7d4",
    "#6e819e",
    "rgba(139, 160, 190, 0.16)",
    "#252a32",
    "#313946",
    "#1f252d",
    "#394453",
    "#5f6d84",
    "rgba(12, 14, 18, 0.82)",
    "rgba(145, 159, 186, 0.34)",
    "rgba(14, 16, 20, 0.85)",
    "rgba(150, 166, 193, 0.36)",
    "#2c3139",
    "#8ba0be",
    "#f2f4f8",
    "#20242b",
};

constexpr ThemePalette kSlatePalette {
    "#1b1c1f",
    "#202226",
    "#25282d",
    "#202327",
    "#262a30",
    "#2b3037",
    "#f0f1f3",
    "#c5c9ce",
    "#9ea4ac",
    "#40454d",
    "#353941",
    "rgba(166, 174, 186, 0.36)",
    "#aeb6c2",
    "#c2c8d1",
    "#8f98a5",
    "rgba(174, 182, 194, 0.12)",
    "#2a2d33",
    "#31353c",
    "#24272d",
    "#383d45",
    "#646c78",
    "rgba(16, 17, 20, 0.82)",
    "rgba(175, 182, 193, 0.30)",
    "rgba(18, 19, 23, 0.84)",
    "rgba(183, 190, 201, 0.32)",
    "#343840",
    "#aeb6c2",
    "#f3f4f6",
    "#26292f",
};

constexpr ThemePalette kSugarPalette {
    "#f6f0e6",
    "#fff8ef",
    "#ece1d2",
    "#fcf7f0",
    "#f0e7db",
    "#e7dccf",
    "#2c241c",
    "#6f6255",
    "#978878",
    "#d4c4b2",
    "#e1d4c5",
    "rgba(149, 118, 86, 0.28)",
    "#b88652",
    "#c99a67",
    "#9a6d40",
    "rgba(184, 134, 82, 0.12)",
    "#efe5d8",
    "#e6d8c7",
    "#dcccb9",
    "#e9dccd",
    "#c3a17e",
    "rgba(255, 248, 239, 0.94)",
    "rgba(168, 133, 97, 0.26)",
    "rgba(40, 28, 18, 0.88)",
    "rgba(178, 142, 103, 0.30)",
    "#dcccb9",
    "#b88652",
    "#ffffff",
    "#fffaf3",
};

constexpr ThemePalette kCocoaPalette {
    "#17110d",
    "#1d1510",
    "#241b15",
    "#1b140f",
    "#231a14",
    "#2b2018",
    "#f2e8df",
    "#c4b4a5",
    "#9a897c",
    "#4a392e",
    "#3a2d24",
    "rgba(182, 137, 94, 0.42)",
    "#c58b4d",
    "#d59a60",
    "#a56d37",
    "rgba(197, 139, 77, 0.15)",
    "#2b2119",
    "#35281f",
    "#231914",
    "#433126",
    "#7d5c42",
    "rgba(12, 8, 6, 0.84)",
    "rgba(182, 140, 103, 0.30)",
    "rgba(18, 12, 9, 0.88)",
    "rgba(203, 154, 110, 0.34)",
    "#32261d",
    "#c58b4d",
    "#fff3e4",
    "#201610",
};

constexpr ThemePalette kCappuccinoPalette {
    "#221b17",
    "#2a221d",
    "#332922",
    "#28201a",
    "#312822",
    "#3a3028",
    "#f3ebe3",
    "#c8b9ad",
    "#9d8e82",
    "#54453a",
    "#46392f",
    "rgba(171, 134, 103, 0.40)",
    "#c3976a",
    "#d0a77d",
    "#9f7650",
    "rgba(195, 151, 106, 0.15)",
    "#362c24",
    "#43372d",
    "#2d241d",
    "#514235",
    "#876a52",
    "rgba(16, 11, 8, 0.84)",
    "rgba(182, 148, 118, 0.30)",
    "rgba(20, 14, 10, 0.88)",
    "rgba(208, 167, 125, 0.34)",
    "#403329",
    "#c3976a",
    "#fff4e8",
    "#2a211b",
};

const QVector<ThemeOption> kThemeOptions {
    {QStringLiteral("day"), QStringLiteral("Day")},
    {QStringLiteral("mist"), QStringLiteral("Mist")},
    {QStringLiteral("navy"), QStringLiteral("Navy")},
    {QStringLiteral("dark"), QStringLiteral("Dark")},
    {QStringLiteral("slate"), QStringLiteral("Slate")},
    {QStringLiteral("gray"), QStringLiteral("Gray")},
    {QStringLiteral("sugar"), QStringLiteral("Sugar")},
    {QStringLiteral("cocoa"), QStringLiteral("Walnut")},
    {QStringLiteral("cappuccino"), QStringLiteral("Dune")},
};

const QVector<AccentOption> kAccentOptions {
    {QStringLiteral("blue"), QStringLiteral("Blue")},
    {QStringLiteral("emerald"), QStringLiteral("Emerald")},
    {QStringLiteral("amber"), QStringLiteral("Amber")},
    {QStringLiteral("rose"), QStringLiteral("Rose")},
    {QStringLiteral("graphite"), QStringLiteral("Graphite")},
};

const QVector<DensityOption> kDensityOptions {
    {QStringLiteral("compact"), QStringLiteral("Compact")},
    {QStringLiteral("normal"), QStringLiteral("Normal")},
    {QStringLiteral("comfortable"), QStringLiteral("Comfortable")},
};

constexpr AccentPalette kBlueAccent {
    "#6a92ff",
    "#83a5ff",
    "#4f73d8",
    "rgba(106, 146, 255, 0.15)",
    "#303b4c",
    "#4f6381",
    "rgba(101, 132, 182, 0.48)",
    "#6a92ff",
};

constexpr AccentPalette kEmeraldAccent {
    "#33c38b",
    "#4bd39b",
    "#1ea070",
    "rgba(51, 195, 139, 0.16)",
    "#234236",
    "#3c7e66",
    "rgba(80, 189, 146, 0.40)",
    "#33c38b",
};

constexpr AccentPalette kAmberAccent {
    "#f0b246",
    "#f6c066",
    "#d69527",
    "rgba(240, 178, 70, 0.16)",
    "#4a3a1d",
    "#8f7137",
    "rgba(233, 183, 94, 0.40)",
    "#f0b246",
};

constexpr AccentPalette kRoseAccent {
    "#ef6f92",
    "#f58aa9",
    "#d65679",
    "rgba(239, 111, 146, 0.16)",
    "#4a2633",
    "#8b5666",
    "rgba(221, 117, 146, 0.40)",
    "#ef6f92",
};

constexpr AccentPalette kGraphiteAccent {
    "#9aa7bc",
    "#aeb8c8",
    "#7a879c",
    "rgba(154, 167, 188, 0.16)",
    "#394453",
    "#667384",
    "rgba(160, 172, 191, 0.38)",
    "#9aa7bc",
};

const ThemePalette &paletteFor(const QString &themeId)
{
    if (themeId == QStringLiteral("day")) {
        return kDayPalette;
    }
    if (themeId == QStringLiteral("mist")) {
        return kMistPalette;
    }
    if (themeId == QStringLiteral("sugar")) {
        return kSugarPalette;
    }
    if (themeId == QStringLiteral("dark")) {
        return kDarkPalette;
    }
    if (themeId == QStringLiteral("slate")) {
        return kSlatePalette;
    }
    if (themeId == QStringLiteral("gray")) {
        return kGrayPalette;
    }
    if (themeId == QStringLiteral("cocoa")) {
        return kCocoaPalette;
    }
    if (themeId == QStringLiteral("cappuccino")) {
        return kCappuccinoPalette;
    }
    return kNavyPalette;
}

const AccentPalette &accentPaletteFor(const QString &accentId)
{
    if (accentId == QStringLiteral("emerald")) {
        return kEmeraldAccent;
    }
    if (accentId == QStringLiteral("amber")) {
        return kAmberAccent;
    }
    if (accentId == QStringLiteral("rose")) {
        return kRoseAccent;
    }
    if (accentId == QStringLiteral("graphite")) {
        return kGraphiteAccent;
    }
    return kBlueAccent;
}

QHash<QString, QString> paletteTokens(const ThemePalette &palette, const AccentPalette &accentPalette)
{
    const bool lightPalette = paletteLooksLight(palette);
    const QColor accentColor(QString::fromLatin1(accentPalette.accent));
    const QColor selectColor(QString::fromLatin1(accentPalette.selectBg));
    const QColor textPrimary(QString::fromLatin1(palette.textPrimary));
    const QColor textSelectionBackground = lightPalette
        ? blendColors(accentColor.isValid() ? accentColor : QColor(QStringLiteral("#7aa2ff")),
                      QColor(QStringLiteral("#ffffff")),
                      0.78)
        : (selectColor.isValid() ? selectColor : QColor(QStringLiteral("#303b4c")));
    const QColor textSelectionForeground = textPrimary.isValid()
        ? textPrimary
        : (lightPalette ? QColor(QStringLiteral("#182232")) : QColor(QStringLiteral("#eef1f7")));

    return {
        {QStringLiteral("APP_BG"), QString::fromLatin1(palette.appBg)},
        {QStringLiteral("APP_BG_ELEVATED"), QString::fromLatin1(palette.appBgElevated)},
        {QStringLiteral("APP_BG_ALT"), QString::fromLatin1(palette.appBgAlt)},
        {QStringLiteral("SURFACE_BG"), QString::fromLatin1(palette.surfaceBg)},
        {QStringLiteral("SURFACE_BG_ALT"), QString::fromLatin1(palette.surfaceBgAlt)},
        {QStringLiteral("SURFACE_BG_SOFT"), QString::fromLatin1(palette.surfaceBgSoft)},
        {QStringLiteral("TEXT_PRIMARY"), QString::fromLatin1(palette.textPrimary)},
        {QStringLiteral("TEXT_MUTED"), QString::fromLatin1(palette.textMuted)},
        {QStringLiteral("TEXT_SUBTLE"), QString::fromLatin1(palette.textSubtle)},
        {QStringLiteral("BORDER_STRONG"), QString::fromLatin1(palette.borderStrong)},
        {QStringLiteral("BORDER_SOFT"), QString::fromLatin1(palette.borderSoft)},
        {QStringLiteral("BORDER_ACCENT"), QString::fromLatin1(accentPalette.borderAccent)},
        {QStringLiteral("ACCENT"), QString::fromLatin1(accentPalette.accent)},
        {QStringLiteral("ACCENT_HOVER"), QString::fromLatin1(accentPalette.accentHover)},
        {QStringLiteral("ACCENT_PRESSED"), QString::fromLatin1(accentPalette.accentPressed)},
        {QStringLiteral("ACCENT_SOFT"), QString::fromLatin1(accentPalette.accentSoft)},
        {QStringLiteral("BUTTON_BG"), QString::fromLatin1(palette.buttonBg)},
        {QStringLiteral("BUTTON_HOVER"), QString::fromLatin1(palette.buttonHover)},
        {QStringLiteral("BUTTON_PRESSED"), QString::fromLatin1(palette.buttonPressed)},
        {QStringLiteral("SELECT_BG"), QString::fromLatin1(accentPalette.selectBg)},
        {QStringLiteral("SELECT_BORDER"), QString::fromLatin1(accentPalette.selectBorder)},
        {QStringLiteral("TEXT_SELECT_BG"), textSelectionBackground.name(QColor::HexArgb)},
        {QStringLiteral("TEXT_SELECT_FG"), textSelectionForeground.name(QColor::HexArgb)},
        {QStringLiteral("OVERLAY_BG"), QString::fromLatin1(palette.overlayBg)},
        {QStringLiteral("OVERLAY_BORDER"), QString::fromLatin1(palette.overlayBorder)},
        {QStringLiteral("OSD_BG"), QString::fromLatin1(palette.osdBg)},
        {QStringLiteral("OSD_BORDER"), QString::fromLatin1(palette.osdBorder)},
        {QStringLiteral("SLIDER_GROOVE"), QString::fromLatin1(palette.sliderGroove)},
        {QStringLiteral("SLIDER_PROGRESS"), QString::fromLatin1(accentPalette.sliderProgress)},
        {QStringLiteral("SLIDER_HANDLE"), QString::fromLatin1(palette.sliderHandle)},
        {QStringLiteral("SEARCH_BG"), QString::fromLatin1(palette.searchBg)},
    };
}

QString supplementalDensityStyleSheet(const QString &densityId)
{
    if (densityId == QStringLiteral("compact")) {
        return QStringLiteral(
            "QWidget { font-size: 12px; }"
            "QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QPlainTextEdit, QKeySequenceEdit, QFontComboBox,"
            "QPushButton, QToolButton { min-height: 28px; }"
            "QWidget#controlBar QToolButton, QWidget#fullscreenTopBar QToolButton { min-height: 30px; padding: 3px 8px; }"
            "QMenu::item { min-height: 20px; padding: 6px 10px 6px 12px; }");
    }

    if (densityId == QStringLiteral("comfortable")) {
        return QStringLiteral(
            "QWidget { font-size: 14px; }"
            "QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QPlainTextEdit, QKeySequenceEdit, QFontComboBox,"
            "QPushButton, QToolButton { min-height: 36px; }"
            "QWidget#controlBar QToolButton, QWidget#fullscreenTopBar QToolButton { min-height: 40px; padding: 5px 12px; }"
            "QMenu::item { min-height: 24px; padding: 8px 12px 8px 12px; }");
    }

    return QString {};
}

QString adjustedColor(const QString &colorString, const int percent)
{
    QColor color(colorFromCssString(colorString));
    if (!color.isValid()) {
        return colorString;
    }

    if (percent >= 100) {
        color = color.lighter(std::clamp(percent, 100, 180));
    } else {
        color = color.darker(std::clamp(200 - percent, 100, 180));
    }
    return color.name(QColor::HexArgb);
}

QString loadThemeTemplate(QString *errorMessage)
{
    QFile styleSheetFile(QStringLiteral(":/themes/base.qss"));
    if (!styleSheetFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not load the embedded theme stylesheet.");
        }
        return {};
    }

    return QString::fromUtf8(styleSheetFile.readAll());
}

}  // namespace

const QVector<ThemeOption> &availableThemes()
{
    return kThemeOptions;
}

const QVector<AccentOption> &availableAccents()
{
    return kAccentOptions;
}

const QVector<DensityOption> &availableDensities()
{
    return kDensityOptions;
}

QString normalizeThemeId(const QString &themeId)
{
    const QString normalized = themeId.trimmed().toLower();
    if (normalized == QStringLiteral("night")) {
        return QStringLiteral("day");
    }
    for (const auto &option : kThemeOptions) {
        if (option.id == normalized) {
            return option.id;
        }
    }

    return QStringLiteral("gray");
}

QString themeLabel(const QString &themeId)
{
    const QString normalized = normalizeThemeId(themeId);
    for (const auto &option : kThemeOptions) {
        if (option.id == normalized) {
            return option.label;
        }
    }

    return QStringLiteral("Gray");
}

QString normalizeAccentId(const QString &accentId)
{
    const QString normalized = accentId.trimmed().toLower();
    for (const auto &option : kAccentOptions) {
        if (option.id == normalized) {
            return option.id;
        }
    }
    return QStringLiteral("blue");
}

QString accentLabel(const QString &accentId)
{
    const QString normalized = normalizeAccentId(accentId);
    for (const auto &option : kAccentOptions) {
        if (option.id == normalized) {
            return option.label;
        }
    }
    return QStringLiteral("Blue");
}

QString normalizeDensityId(const QString &densityId)
{
    const QString normalized = densityId.trimmed().toLower();
    for (const auto &option : kDensityOptions) {
        if (option.id == normalized) {
            return option.id;
        }
    }
    return QStringLiteral("normal");
}

QString densityLabel(const QString &densityId)
{
    const QString normalized = normalizeDensityId(densityId);
    for (const auto &option : kDensityOptions) {
        if (option.id == normalized) {
            return option.label;
        }
    }
    return QStringLiteral("Normal");
}

QColor resolvedThemeColor(const QString &themeId,
                          const QString &accentId,
                          const QString &tokenName)
{
    const auto tokens = paletteTokens(
        paletteFor(normalizeThemeId(themeId)),
        accentPaletteFor(normalizeAccentId(accentId)));
    const QColor resolved(colorFromCssString(tokens.value(tokenName)));
    return resolved.isValid() ? resolved : QColor {};
}

QString buildThemeCustomizationStyleSheet(const ThemeCustomization &customization,
                                         const QString &themeId,
                                         const QString &accentId)
{
    const ThemePalette &palette = paletteFor(normalizeThemeId(themeId));
    [[maybe_unused]] const QString normalizedAccentId = normalizeAccentId(accentId);
    const int radius = std::clamp(customization.radiusPx, 4, 28);
    const int spacing = std::clamp(customization.spacingPx, 4, 20);
    const int fontSize = std::clamp(static_cast<int>(std::round(13.0 * std::clamp(customization.fontScalePercent, 85, 140) / 100.0)), 11, 19);
    const int fontWeight = std::clamp(customization.fontWeightValue, 350, 800);
    const double letterSpacing = std::clamp(customization.letterSpacingPx, -0.4, 2.0);
    const int overlayOpacity = std::clamp(customization.overlayOpacityPercent, 55, 98);
    QColor overlayColor(colorFromCssString(QString::fromLatin1(palette.overlayBg)));
    if (overlayColor.isValid()) {
        overlayColor.setAlphaF(std::clamp(overlayOpacity / 100.0, 0.0, 1.0));
    }

    const QString shadowBorder = adjustedColor(QString::fromLatin1(palette.borderStrong), std::clamp(customization.shadowStrengthPercent, 40, 160));
    const QString softBorder = adjustedColor(QString::fromLatin1(palette.borderSoft), std::clamp(customization.borderContrastPercent, 65, 160));
    const QString overlayBackground = overlayColor.isValid() ? overlayColor.name(QColor::HexArgb) : QString::fromLatin1(palette.overlayBg);

    return QStringLiteral(
               "QWidget { font-size: %1px; font-weight: %2; letter-spacing: %3px; }"
               "QMenu, QDialog, QDockWidget, QGroupBox, QListView, QListWidget, QTreeWidget, QScrollArea, QFrame,"
               "QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QPlainTextEdit, QKeySequenceEdit, QFontComboBox,"
               "QPushButton, QToolButton, QTabBar::tab { border-radius: %4px; }"
               "QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QPlainTextEdit, QKeySequenceEdit, QFontComboBox,"
               "QPushButton, QToolButton { padding-top: %5px; padding-bottom: %5px; }"
               "QWidget#controlBar QToolButton#controlOpenButton,"
               "QWidget#controlBar QToolButton#controlPreviousButton,"
               "QWidget#controlBar QToolButton#controlNextButton,"
               "QWidget#controlBar QToolButton#controlStopButton,"
               "QWidget#controlBar QToolButton#controlFullscreenButton { min-height: 40px; padding-top: 0; padding-bottom: 0; }"
               "QWidget#controlBar QToolButton#controlPlaylistButton,"
               "QWidget#controlBar QToolButton#controlDetailsButton { min-width: 52px; min-height: 40px; padding-top: 0; padding-bottom: 0; }"
               "QGroupBox, QWidget#analyticsCard { border: 1px solid %6; }"
               "QWidget#controlBar[overlayMode=\"true\"], QDockWidget#playlistDock, QDockWidget#detailsDock { background: %7; }"
               "QWidget#transportGroup, QWidget#timelineGroup, QWidget#volumeGroup, QWidget#fullscreenTopBar,"
               "QWidget#playlistHeaderBar, QWidget#playlistCoursesRow, QWidget#playlistSearchRow, QWidget#playlistViewsRow,"
               "QWidget#bookmarksHeaderBar, QWidget#analyticsCard { border: 1px solid %8; }")
        .arg(fontSize)
        .arg(fontWeight)
        .arg(letterSpacing, 0, 'f', 2)
        .arg(radius)
        .arg(std::max(4, spacing / 2))
        .arg(softBorder)
        .arg(overlayBackground)
        .arg(shadowBorder);
}

bool applyApplicationTheme(QApplication &application,
                           const QString &themeId,
                           const QString &accentId,
                           const QString &densityId,
                           const ThemeCustomization &customization,
                           QString *errorMessage)
{
    if (auto *fusionStyle = QStyleFactory::create(QStringLiteral("Fusion")); fusionStyle != nullptr) {
        application.setStyle(fusionStyle);
    }

    QString styleSheet = loadThemeTemplate(errorMessage);
    if (styleSheet.isEmpty()) {
        application.setStyleSheet(QString {});
        return false;
    }

    const auto tokens = paletteTokens(
        paletteFor(normalizeThemeId(themeId)),
        accentPaletteFor(normalizeAccentId(accentId)));
    for (auto it = tokens.constBegin(); it != tokens.constEnd(); ++it) {
        styleSheet.replace(QStringLiteral("{{%1}}").arg(it.key()), it.value());
    }

    styleSheet.append(supplementalDensityStyleSheet(normalizeDensityId(densityId)));
    styleSheet.append(buildThemeCustomizationStyleSheet(customization, themeId, accentId));

    application.setStyleSheet(styleSheet);
    return true;
}

}  // namespace revaplayer::application
