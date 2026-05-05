#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

namespace revaplayer::application {

struct SubtitleAssOverrideOption final {
    QString id;
    QString label;
};

struct SubtitleChoiceOption final {
    QString id;
    QString label;
};

inline constexpr auto kSubtitleAutoLoadModeSetting = "subtitle/auto_load_mode";
inline constexpr auto kSubtitleAutoExtensionsSetting = "subtitle/auto_extensions";
inline constexpr auto kSubtitleRememberTrackChoiceSetting = "subtitle/remember_track_choice";
inline constexpr auto kSubtitleCodepageSetting = "subtitle/codepage";
inline constexpr auto kSubtitleFixTimingSetting = "subtitle/fix_timing";
inline constexpr auto kSubtitleFontWeightSetting = "subtitle/font_weight";
inline constexpr auto kSubtitleItalicSetting = "subtitle/italic";
inline constexpr auto kSubtitleTextColorSetting = "subtitle/text_color";
inline constexpr auto kSubtitleOutlineColorSetting = "subtitle/outline_color";
inline constexpr auto kSubtitleOutlineSizeSetting = "subtitle/outline_size";
inline constexpr auto kSubtitleBorderStyleSetting = "subtitle/border_style";
inline constexpr auto kSubtitleBackgroundEnabledSetting = "subtitle/background_enabled";
inline constexpr auto kSubtitleBackgroundColorSetting = "subtitle/background_color";
inline constexpr auto kSubtitleBackgroundOpacitySetting = "subtitle/background_opacity";
inline constexpr auto kSubtitleShadowEnabledSetting = "subtitle/shadow_enabled";
inline constexpr auto kSubtitleShadowColorSetting = "subtitle/shadow_color";
inline constexpr auto kSubtitleShadowOffsetSetting = "subtitle/shadow_offset";
inline constexpr auto kSubtitleShadowBlurSetting = "subtitle/shadow_blur";
inline constexpr auto kSubtitleLineSpacingSetting = "subtitle/line_spacing";
inline constexpr auto kSubtitleLetterSpacingSetting = "subtitle/letter_spacing";
inline constexpr auto kSubtitleMaxWidthSetting = "subtitle/max_width";
inline constexpr auto kSubtitleAlignXSetting = "subtitle/align_x";
inline constexpr auto kSubtitleAlignYSetting = "subtitle/align_y";
inline constexpr auto kSubtitleJustifySetting = "subtitle/justify";
inline constexpr auto kSubtitleMarginXSetting = "subtitle/margin_x";
inline constexpr auto kSubtitleMarginYSetting = "subtitle/margin_y";
inline constexpr auto kSubtitleUseMarginsSetting = "subtitle/use_margins";
inline constexpr auto kSubtitleScaleWithWindowSetting = "subtitle/scale_with_window";
inline constexpr auto kSubtitleAssForceMarginsSetting = "subtitle/ass_force_margins";
inline constexpr auto kSubtitleAssJustifySetting = "subtitle/ass_justify";
inline constexpr auto kSubtitleFontProviderSetting = "subtitle/font_provider";
inline constexpr auto kSubtitleShaperSetting = "subtitle/shaper";
inline constexpr auto kSubtitleHintingSetting = "subtitle/hinting";

[[nodiscard]] QVector<SubtitleAssOverrideOption> subtitleAssOverrideOptions();
[[nodiscard]] QString normalizeSubtitleAssOverride(const QString &mode);
[[nodiscard]] QString subtitleAssOverrideLabel(const QString &mode);
[[nodiscard]] QString nextSubtitleAssOverride(const QString &mode);
[[nodiscard]] QVector<SubtitleChoiceOption> subtitleAutoLoadModeOptions();
[[nodiscard]] QString normalizeSubtitleAutoLoadMode(const QString &mode);
[[nodiscard]] QString subtitleAutoLoadModeMpvValue(const QString &mode);
[[nodiscard]] QString defaultSubtitleAutoExtensions();
[[nodiscard]] QString normalizeSubtitleAutoExtensions(const QString &extensions);
[[nodiscard]] QVector<SubtitleChoiceOption> subtitleEncodingOptions();
[[nodiscard]] QString normalizeSubtitleEncoding(const QString &encoding);
[[nodiscard]] QVector<SubtitleChoiceOption> subtitleBorderStyleOptions();
[[nodiscard]] QString normalizeSubtitleBorderStyle(const QString &styleId);
[[nodiscard]] QVector<SubtitleChoiceOption> subtitleAlignXOptions();
[[nodiscard]] QString normalizeSubtitleAlignX(const QString &alignId);
[[nodiscard]] QVector<SubtitleChoiceOption> subtitleAlignYOptions();
[[nodiscard]] QString normalizeSubtitleAlignY(const QString &alignId);
[[nodiscard]] QVector<SubtitleChoiceOption> subtitleJustifyOptions();
[[nodiscard]] QString normalizeSubtitleJustify(const QString &justifyId);
[[nodiscard]] QVector<SubtitleChoiceOption> subtitleFontProviderOptions();
[[nodiscard]] QString normalizeSubtitleFontProvider(const QString &providerId);
[[nodiscard]] QVector<SubtitleChoiceOption> subtitleShaperOptions();
[[nodiscard]] QString normalizeSubtitleShaper(const QString &shaperId);
[[nodiscard]] QVector<SubtitleChoiceOption> subtitleHintingOptions();
[[nodiscard]] QString normalizeSubtitleHinting(const QString &hintingId);
[[nodiscard]] QString normalizeSubtitleColorString(const QString &color, const QString &fallback);
[[nodiscard]] double clampSubtitleScale(double scale);
[[nodiscard]] int clampSubtitlePosition(int position);
[[nodiscard]] int clampSubtitleFontSize(int fontSize);
[[nodiscard]] int clampSubtitleFontWeight(int fontWeight);
[[nodiscard]] double clampSubtitleOutlineSize(double size);
[[nodiscard]] double clampSubtitleShadowOffset(double offset);
[[nodiscard]] double clampSubtitleBlur(double blur);
[[nodiscard]] double clampSubtitleLineSpacing(double spacing);
[[nodiscard]] double clampSubtitleLetterSpacing(double spacing);
[[nodiscard]] double clampSubtitleMaxWidth(double width);
[[nodiscard]] int clampSubtitleMarginX(int margin);
[[nodiscard]] int clampSubtitleMarginY(int margin);
[[nodiscard]] int clampSubtitleBackgroundOpacity(int opacity);

}  // namespace revaplayer::application
