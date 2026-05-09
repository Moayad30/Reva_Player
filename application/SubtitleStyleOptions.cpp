#include "application/SubtitleStyleOptions.hpp"

#include "application/UiLanguage.hpp"

#include <algorithm>
#include <cmath>

namespace revaplayer::application {
namespace {

constexpr const char *kDefaultAssOverrideMode = "scale";

const QVector<SubtitleAssOverrideOption> &subtitleOverrideOptionsStorage()
{
    static const QVector<SubtitleAssOverrideOption> options {
        {QStringLiteral("scale"), QStringLiteral("Scale Existing Styles")},
        {QStringLiteral("yes"), QStringLiteral("Apply User Style Where Possible")},
        {QStringLiteral("force"), QStringLiteral("Force User Style")},
        {QStringLiteral("strip"), QStringLiteral("Strip Embedded Styling")},
        {QStringLiteral("no"), QStringLiteral("Keep Embedded Styling")},
    };
    return options;
}

const QVector<SubtitleChoiceOption> &subtitleAutoLoadModeStorage()
{
    static const QVector<SubtitleChoiceOption> options {
        {QStringLiteral("same_name_only"), QStringLiteral("Same file name only")},
        {QStringLiteral("same_name_language"), QStringLiteral("Same name + language suffix")},
        {QStringLiteral("same_folder_any"), QStringLiteral("Any subtitle in same folder")},
        {QStringLiteral("manual_only"), QStringLiteral("Manual only")},
    };
    return options;
}

const QVector<SubtitleChoiceOption> &subtitleEncodingStorage()
{
    static const QVector<SubtitleChoiceOption> options {
        {QStringLiteral("auto"), QStringLiteral("Auto Detect")},
        {QStringLiteral("UTF-8"), QStringLiteral("UTF-8")},
        {QStringLiteral("CP1256"), QStringLiteral("Windows-1256")},
        {QStringLiteral("CP1252"), QStringLiteral("Windows-1252")},
        {QStringLiteral("ISO-8859-6"), QStringLiteral("ISO-8859-6 Arabic")},
        {QStringLiteral("CP1251"), QStringLiteral("Windows-1251")},
        {QStringLiteral("Shift_JIS"), QStringLiteral("Shift_JIS")},
        {QStringLiteral("GB18030"), QStringLiteral("GB18030")},
        {QStringLiteral("Big5"), QStringLiteral("Big5")},
    };
    return options;
}

const QVector<SubtitleChoiceOption> &subtitleBorderStyleStorage()
{
    static const QVector<SubtitleChoiceOption> options {
        {QStringLiteral("outline-and-shadow"), QStringLiteral("Outline and Shadow")},
        {QStringLiteral("background-box"), QStringLiteral("Background Box")},
        {QStringLiteral("opaque-box"), QStringLiteral("Opaque Box")},
    };
    return options;
}

const QVector<SubtitleChoiceOption> &subtitleAlignXStorage()
{
    static const QVector<SubtitleChoiceOption> options {
        {QStringLiteral("center"), QStringLiteral("Center")},
        {QStringLiteral("left"), QStringLiteral("Left")},
        {QStringLiteral("right"), QStringLiteral("Right")},
    };
    return options;
}

const QVector<SubtitleChoiceOption> &subtitleAlignYStorage()
{
    static const QVector<SubtitleChoiceOption> options {
        {QStringLiteral("bottom"), QStringLiteral("Bottom")},
        {QStringLiteral("center"), QStringLiteral("Center")},
        {QStringLiteral("top"), QStringLiteral("Top")},
    };
    return options;
}

const QVector<SubtitleChoiceOption> &subtitleJustifyStorage()
{
    static const QVector<SubtitleChoiceOption> options {
        {QStringLiteral("auto"), QStringLiteral("Auto")},
        {QStringLiteral("left"), QStringLiteral("Left")},
        {QStringLiteral("center"), QStringLiteral("Center")},
        {QStringLiteral("right"), QStringLiteral("Right")},
    };
    return options;
}

const QVector<SubtitleChoiceOption> &subtitleFontProviderStorage()
{
    static const QVector<SubtitleChoiceOption> options {
        {QStringLiteral("auto"), QStringLiteral("Auto")},
        {QStringLiteral("fontconfig"), QStringLiteral("Fontconfig")},
        {QStringLiteral("none"), QStringLiteral("Disabled")},
    };
    return options;
}

const QVector<SubtitleChoiceOption> &subtitleShaperStorage()
{
    static const QVector<SubtitleChoiceOption> options {
        {QStringLiteral("complex"), QStringLiteral("Complex")},
        {QStringLiteral("simple"), QStringLiteral("Simple")},
    };
    return options;
}

const QVector<SubtitleChoiceOption> &subtitleHintingStorage()
{
    static const QVector<SubtitleChoiceOption> options {
        {QStringLiteral("none"), QStringLiteral("None")},
        {QStringLiteral("light"), QStringLiteral("Light")},
        {QStringLiteral("normal"), QStringLiteral("Normal")},
        {QStringLiteral("native"), QStringLiteral("Native")},
    };
    return options;
}

template <typename Option>
QString normalizeChoice(const QString &value,
                        const QVector<Option> &options,
                        const QString &fallback)
{
    const QString normalized = value.trimmed().toLower();
    for (const auto &option : options) {
        if (option.id == normalized) {
            return option.id;
        }
    }
    return fallback;
}

}  // namespace

QVector<SubtitleAssOverrideOption> subtitleAssOverrideOptions()
{
    return subtitleOverrideOptionsStorage();
}

QString normalizeSubtitleAssOverride(const QString &mode)
{
    const QString normalized = mode.trimmed().toLower();
    for (const auto &option : subtitleOverrideOptionsStorage()) {
        if (option.id == normalized) {
            return option.id;
        }
    }

    return QString::fromLatin1(kDefaultAssOverrideMode);
}

QString subtitleAssOverrideLabel(const QString &mode)
{
    const QString normalized = normalizeSubtitleAssOverride(mode);
    for (const auto &option : subtitleOverrideOptionsStorage()) {
        if (option.id == normalized) {
            return revaplayer::application::translateUiText(option.label);
        }
    }

    return revaplayer::application::translateUiText(QStringLiteral("Scale Existing Styles"));
}

QString nextSubtitleAssOverride(const QString &mode)
{
    const QString normalized = normalizeSubtitleAssOverride(mode);
    const auto &options = subtitleOverrideOptionsStorage();
    for (qsizetype index = 0; index < options.size(); ++index) {
        if (options.at(index).id == normalized) {
            return options.at((index + 1) % options.size()).id;
        }
    }

    return QString::fromLatin1(kDefaultAssOverrideMode);
}

QVector<SubtitleChoiceOption> subtitleAutoLoadModeOptions()
{
    return subtitleAutoLoadModeStorage();
}

QString normalizeSubtitleAutoLoadMode(const QString &mode)
{
    const QString normalized = mode.trimmed().toLower();
    if (normalized == QStringLiteral("same_name")
        || normalized == QStringLiteral("exact")) {
        return QStringLiteral("same_name_only");
    }
    if (normalized == QStringLiteral("fuzzy")) {
        return QStringLiteral("same_name_language");
    }
    if (normalized == QStringLiteral("all")) {
        return QStringLiteral("same_folder_any");
    }
    if (normalized == QStringLiteral("disabled")
        || normalized == QStringLiteral("off")
        || normalized == QStringLiteral("no")) {
        return QStringLiteral("manual_only");
    }
    return normalizeChoice(normalized, subtitleAutoLoadModeStorage(), QStringLiteral("same_name_only"));
}

QString subtitleAutoLoadModeMpvValue(const QString &mode)
{
    const QString normalized = normalizeSubtitleAutoLoadMode(mode);
    if (normalized == QStringLiteral("same_folder_any")) {
        return QStringLiteral("all");
    }
    if (normalized == QStringLiteral("same_name_language")) {
        return QStringLiteral("fuzzy");
    }
    if (normalized == QStringLiteral("manual_only")) {
        return QStringLiteral("no");
    }
    return QStringLiteral("exact");
}

QString defaultSubtitleAutoExtensions()
{
    return QStringLiteral("ass,dfxp,idx,lrc,mpl,mpl2,pgs,rt,sami,sbv,scc,smi,srt,ssa,sub,sup,ttml,txt,usf,vtt,webvtt");
}

QString normalizeSubtitleAutoExtensions(const QString &extensions)
{
    QString normalized = extensions.trimmed().toLower();
    if (normalized.isEmpty()) {
        normalized = defaultSubtitleAutoExtensions();
    }
    normalized.replace(QChar(';'), QChar(','));
    normalized.replace(QChar('|'), QChar(','));
    normalized.replace(QChar('\n'), QChar(','));
    normalized.replace(QChar('\t'), QChar(','));
    normalized.replace(QChar(' '), QChar(','));

    QStringList items;
    for (QString token : normalized.split(QChar(','), Qt::SkipEmptyParts)) {
        token = token.trimmed();
        while (token.startsWith(QChar('.'))) {
            token.remove(0, 1);
        }
        if (!token.isEmpty() && !items.contains(token)) {
            items.push_back(token);
        }
    }
    return items.isEmpty() ? defaultSubtitleAutoExtensions() : items.join(QStringLiteral(","));
}

QVector<SubtitleChoiceOption> subtitleEncodingOptions()
{
    return subtitleEncodingStorage();
}

QString normalizeSubtitleEncoding(const QString &encoding)
{
    const QString trimmed = encoding.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("auto");
    }
    for (const auto &option : subtitleEncodingStorage()) {
        if (option.id.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return option.id;
        }
    }
    return trimmed;
}

QVector<SubtitleChoiceOption> subtitleBorderStyleOptions()
{
    return subtitleBorderStyleStorage();
}

QString normalizeSubtitleBorderStyle(const QString &styleId)
{
    return normalizeChoice(styleId, subtitleBorderStyleStorage(), QStringLiteral("outline-and-shadow"));
}

QVector<SubtitleChoiceOption> subtitleAlignXOptions()
{
    return subtitleAlignXStorage();
}

QString normalizeSubtitleAlignX(const QString &alignId)
{
    return normalizeChoice(alignId, subtitleAlignXStorage(), QStringLiteral("center"));
}

QVector<SubtitleChoiceOption> subtitleAlignYOptions()
{
    return subtitleAlignYStorage();
}

QString normalizeSubtitleAlignY(const QString &alignId)
{
    return normalizeChoice(alignId, subtitleAlignYStorage(), QStringLiteral("bottom"));
}

QVector<SubtitleChoiceOption> subtitleJustifyOptions()
{
    return subtitleJustifyStorage();
}

QString normalizeSubtitleJustify(const QString &justifyId)
{
    return normalizeChoice(justifyId, subtitleJustifyStorage(), QStringLiteral("auto"));
}

QVector<SubtitleChoiceOption> subtitleFontProviderOptions()
{
    return subtitleFontProviderStorage();
}

QString normalizeSubtitleFontProvider(const QString &providerId)
{
    return normalizeChoice(providerId, subtitleFontProviderStorage(), QStringLiteral("auto"));
}

QVector<SubtitleChoiceOption> subtitleShaperOptions()
{
    return subtitleShaperStorage();
}

QString normalizeSubtitleShaper(const QString &shaperId)
{
    return normalizeChoice(shaperId, subtitleShaperStorage(), QStringLiteral("complex"));
}

QVector<SubtitleChoiceOption> subtitleHintingOptions()
{
    return subtitleHintingStorage();
}

QString normalizeSubtitleHinting(const QString &hintingId)
{
    return normalizeChoice(hintingId, subtitleHintingStorage(), QStringLiteral("none"));
}

QString normalizeSubtitleColorString(const QString &color, const QString &fallback)
{
    const QColor resolved(color.trimmed());
    if (resolved.isValid()) {
        return resolved.name(QColor::HexArgb);
    }

    const QColor fallbackColor(fallback.trimmed());
    return fallbackColor.isValid()
        ? fallbackColor.name(QColor::HexArgb)
        : QStringLiteral("#FFFFFFFF");
}

double clampSubtitleScale(double scale)
{
    if (!std::isfinite(scale)) {
        return 1.0;
    }

    return std::clamp(scale, 0.25, 5.0);
}

int clampSubtitlePosition(const int position)
{
    return std::clamp(position, 0, 150);
}

int clampSubtitleFontSize(const int fontSize)
{
    return std::clamp(fontSize, 8, 144);
}

int clampSubtitleFontWeight(const int fontWeight)
{
    return std::clamp(fontWeight, 300, 900);
}

double clampSubtitleOutlineSize(const double size)
{
    if (!std::isfinite(size)) {
        return 1.65;
    }
    return std::clamp(size, 0.0, 12.0);
}

double clampSubtitleShadowOffset(const double offset)
{
    if (!std::isfinite(offset)) {
        return 0.0;
    }
    return std::clamp(offset, 0.0, 20.0);
}

double clampSubtitleBlur(const double blur)
{
    if (!std::isfinite(blur)) {
        return 0.0;
    }
    return std::clamp(blur, 0.0, 3.0);
}

double clampSubtitleLineSpacing(const double spacing)
{
    if (!std::isfinite(spacing)) {
        return 0.0;
    }
    return std::clamp(spacing, -1000.0, 1000.0);
}

double clampSubtitleLetterSpacing(const double spacing)
{
    if (!std::isfinite(spacing)) {
        return 0.0;
    }
    return std::clamp(spacing, -10.0, 10.0);
}

double clampSubtitleMaxWidth(const double width)
{
    if (!std::isfinite(width)) {
        return 100.0;
    }
    return std::clamp(width, 30.0, 100.0);
}

int clampSubtitleMarginX(const int margin)
{
    return std::clamp(margin, 0, 300);
}

int clampSubtitleMarginY(const int margin)
{
    return std::clamp(margin, 0, 600);
}

int clampSubtitleBackgroundOpacity(const int opacity)
{
    return std::clamp(opacity, 0, 100);
}

}  // namespace revaplayer::application
