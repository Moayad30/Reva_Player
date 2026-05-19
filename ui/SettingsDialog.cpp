#include "ui/SettingsDialog.hpp"

#include "application/HistoryController.hpp"
#include "application/PlaybackTuning.hpp"
#include "application/SettingsController.hpp"
#include "application/SubtitleStyleOptions.hpp"
#include "application/ThemeStyle.hpp"
#include "application/UiLanguage.hpp"
#include "domain/PlayerProfile.hpp"
#include "ui/FileDialogUtils.hpp"

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QFont>
#include <QFontComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <limits>

namespace revaplayer::ui {
namespace {

QString uiText(const char *sourceText)
{
    return revaplayer::application::translateUiText(QString::fromUtf8(sourceText));
}

void setHoverExplanation(QWidget *widget, const QString &text)
{
    if (widget == nullptr) {
        return;
    }

    widget->setToolTip(text);
    widget->setWhatsThis(text);
}

void setFormRowVisible(QFormLayout *layout, QWidget *field, const bool visible)
{
    if (layout == nullptr || field == nullptr) {
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    layout->setRowVisible(field, visible);
#else
    field->setVisible(visible);
    if (QWidget *label = layout->labelForField(field); label != nullptr) {
        label->setVisible(visible);
    }
#endif
}

QString widgetSearchText(const QWidget *widget)
{
    if (widget == nullptr) {
        return {};
    }

    QStringList parts;
    parts.push_back(widget->windowTitle());
    parts.push_back(widget->toolTip());
    parts.push_back(widget->whatsThis());
    parts.push_back(widget->accessibleName());
    parts.push_back(widget->accessibleDescription());

    if (const auto *groupBox = qobject_cast<const QGroupBox *>(widget); groupBox != nullptr) {
        parts.push_back(groupBox->title());
    }
    if (const auto *label = qobject_cast<const QLabel *>(widget); label != nullptr) {
        parts.push_back(label->text());
    }
    if (const auto *button = qobject_cast<const QAbstractButton *>(widget); button != nullptr) {
        parts.push_back(button->text());
    }
    if (const auto *lineEdit = qobject_cast<const QLineEdit *>(widget); lineEdit != nullptr) {
        parts.push_back(lineEdit->text());
        parts.push_back(lineEdit->placeholderText());
    }
    if (const auto *plainTextEdit = qobject_cast<const QPlainTextEdit *>(widget); plainTextEdit != nullptr) {
        parts.push_back(plainTextEdit->toPlainText());
        parts.push_back(plainTextEdit->placeholderText());
    }
    if (const auto *comboBox = qobject_cast<const QComboBox *>(widget); comboBox != nullptr) {
        parts.push_back(comboBox->currentText());
        for (int index = 0; index < comboBox->count(); ++index) {
            parts.push_back(comboBox->itemText(index));
        }
    }
    if (const auto *fontComboBox = qobject_cast<const QFontComboBox *>(widget); fontComboBox != nullptr) {
        parts.push_back(fontComboBox->currentFont().family());
    }
    if (const auto *spinBox = qobject_cast<const QSpinBox *>(widget); spinBox != nullptr) {
        parts.push_back(spinBox->prefix());
        parts.push_back(spinBox->suffix());
        parts.push_back(spinBox->specialValueText());
    }
    if (const auto *spinBox = qobject_cast<const QDoubleSpinBox *>(widget); spinBox != nullptr) {
        parts.push_back(spinBox->prefix());
        parts.push_back(spinBox->suffix());
        parts.push_back(spinBox->specialValueText());
    }
    if (const auto *tabWidget = qobject_cast<const QTabWidget *>(widget); tabWidget != nullptr) {
        for (int index = 0; index < tabWidget->count(); ++index) {
            parts.push_back(tabWidget->tabText(index));
        }
    }

    parts.removeAll(QString {});
    return parts.join(QChar(' '));
}

bool widgetTreeMatchesSearch(const QWidget *widget, const QString &needle)
{
    if (widget == nullptr) {
        return false;
    }

    if (needle.trimmed().isEmpty()) {
        return true;
    }

    if (widgetSearchText(widget).contains(needle, Qt::CaseInsensitive)) {
        return true;
    }

    const auto childWidgets = widget->findChildren<QWidget *>(QString {}, Qt::FindDirectChildrenOnly);
    return std::any_of(childWidgets.cbegin(), childWidgets.cend(), [&needle](const QWidget *child) {
        return widgetTreeMatchesSearch(child, needle);
    });
}

struct ThumbnailPreviewSizePreset {
    const char *id;
    const char *label;
    int cachedFrameWidth;
    int popupWidth;
};

constexpr ThumbnailPreviewSizePreset kThumbnailPreviewSizePresets[] {
    {"small", "صغير", 360, 320},
    {"normal", "وسط", 560, 480},
    {"large", "كبير", 860, 760},
    {"xlarge", "كبير جدا", 1280, 1120},
    {"cinema", "كبير جدا جدا", 1800, 1600},
};

constexpr auto kDashboardContinueSectionSetting = "ui/dashboard_section_continue";
constexpr auto kDashboardRecentSectionSetting = "ui/dashboard_section_recent";
constexpr auto kDashboardFavoritesSectionSetting = "ui/dashboard_section_favorites";
constexpr auto kDashboardSavedListsSectionSetting = "ui/dashboard_section_saved_lists";
constexpr auto kMouseZoneRightUserDefinedSetting = "input/mouse_zone/right_user_defined";
constexpr auto kShowPlaylistPanelOnFolderLoadUserDefinedSetting = "playlist/show_panel_on_folder_load_user_defined";
constexpr auto kWindowChromeUserDefinedSetting = "ui/window_chrome_user_defined_v1";
constexpr auto kStartupCanvasStyleSetting = "ui/startup_canvas_style";
constexpr auto kControlBarShowOpenButtonSetting = "ui/control_bar/show_open_button";
constexpr auto kControlBarShowStopButtonSetting = "ui/control_bar/show_stop_button";
constexpr auto kControlBarShowPlaylistButtonSetting = "ui/control_bar/show_playlist_button";
constexpr auto kControlBarShowDetailsButtonSetting = "ui/control_bar/show_details_button";
constexpr auto kControlBarShowTimeLabelSetting = "ui/control_bar/show_time_label";
constexpr auto kControlBarShowSpeedButtonSetting = "ui/control_bar/show_speed_button";
constexpr auto kControlBarShowRepeatLoopButtonsSetting = "ui/control_bar/show_repeat_loop_buttons";
constexpr auto kControlBarShowTrackMenusSetting = "ui/control_bar/show_track_menus";
constexpr auto kControlBarShowVolumeControlsSetting = "ui/control_bar/show_volume_controls";
constexpr auto kControlBarShowFullscreenButtonSetting = "ui/control_bar/show_fullscreen_button";
constexpr auto kControlBarTimelineThicknessSetting = "ui/control_bar/timeline_thickness";
constexpr auto kControlBarTimelineHandleSizeSetting = "ui/control_bar/timeline_handle_size";
constexpr auto kControlBarVolumeSliderThicknessSetting = "ui/control_bar/volume_slider_thickness";
constexpr auto kControlBarVolumeSliderWidthSetting = "ui/control_bar/volume_slider_width";
constexpr auto kSessionWidePlaybackSpeedSetting = "playback/session_wide_speed";
constexpr auto kLegacyControlBarShowPanelButtonsSetting = "ui/control_bar/show_panel_buttons";
constexpr int kDefaultControlBarTimelineThickness = 8;
constexpr int kDefaultControlBarTimelineHandleSize = 14;
constexpr int kDefaultControlBarVolumeSliderThickness = 6;
constexpr int kDefaultControlBarVolumeSliderWidth = 88;
constexpr int kPreviousDefaultControlBarVolumeSliderWidth = 132;
constexpr int kLegacyControlBarVolumeSliderWidth = 176;

QString normalizedStartupCanvasStyleId(QString value)
{
    value = value.trimmed().toLower();
    if (value == QStringLiteral("graphite")) {
        return QStringLiteral("black");
    }
    return value == QStringLiteral("black")
            || value == QStringLiteral("warm")
        ? value
        : QStringLiteral("theme");
}

bool rawCustomFlagEnabled(QString value)
{
    value = value.trimmed().toLower();
    if (value.isEmpty()) {
        return false;
    }

    return value != QStringLiteral("0")
        && value != QStringLiteral("false")
        && value != QStringLiteral("no")
        && value != QStringLiteral("off");
}

struct ControlBarPanelButtonSettings final {
    bool showPlaylistButton {true};
    bool showDetailsButton {true};
};

ControlBarPanelButtonSettings resolvedControlBarPanelButtonSettings(
    const revaplayer::application::SettingsController *settingsController)
{
    const QString playlistRaw = settingsController != nullptr
        ? settingsController->customValue(QString::fromLatin1(kControlBarShowPlaylistButtonSetting)).trimmed()
        : QString {};
    const QString detailsRaw = settingsController != nullptr
        ? settingsController->customValue(QString::fromLatin1(kControlBarShowDetailsButtonSetting)).trimmed()
        : QString {};
    const QString legacyPanelRaw = settingsController != nullptr
        ? settingsController->customValue(QString::fromLatin1(kLegacyControlBarShowPanelButtonsSetting)).trimmed()
        : QString {};

    ControlBarPanelButtonSettings resolved;
    if (!playlistRaw.isEmpty()) {
        resolved.showPlaylistButton = rawCustomFlagEnabled(playlistRaw);
        resolved.showDetailsButton = detailsRaw.isEmpty() ? true : rawCustomFlagEnabled(detailsRaw);
        return resolved;
    }

    const bool legacyPanelVisible = legacyPanelRaw.isEmpty() ? true : rawCustomFlagEnabled(legacyPanelRaw);
    resolved.showPlaylistButton = legacyPanelVisible;
    resolved.showDetailsButton = legacyPanelVisible
        && (detailsRaw.isEmpty() ? true : rawCustomFlagEnabled(detailsRaw));
    return resolved;
}

int resolvedControlBarVolumeSliderWidth(const revaplayer::application::SettingsController *settingsController)
{
    const QString rawValue = settingsController != nullptr
        ? settingsController->customValue(QString::fromLatin1(kControlBarVolumeSliderWidthSetting)).trimmed()
        : QString {};
    if (rawValue.isEmpty()) {
        return kDefaultControlBarVolumeSliderWidth;
    }

    bool ok = false;
    const int parsed = rawValue.toInt(&ok);
    if (!ok) {
        return kDefaultControlBarVolumeSliderWidth;
    }
    if (parsed == kPreviousDefaultControlBarVolumeSliderWidth || parsed == kLegacyControlBarVolumeSliderWidth) {
        return kDefaultControlBarVolumeSliderWidth;
    }
    return std::clamp(parsed, 0, 96);
}

const ThumbnailPreviewSizePreset &thumbnailPreviewSizePresetForId(const QString &presetId)
{
    const QString normalizedId = presetId.trimmed().toLower();
    for (const auto &preset : kThumbnailPreviewSizePresets) {
        if (QString::fromLatin1(preset.id) == normalizedId) {
            return preset;
        }
    }

    return kThumbnailPreviewSizePresets[1];
}

QString thumbnailPreviewSizePresetIdForWidths(const int cachedFrameWidth, const int popupWidth)
{
    if (cachedFrameWidth <= 0 && popupWidth <= 0) {
        return QStringLiteral("normal");
    }

    const int effectiveCachedWidth = cachedFrameWidth > 0 ? cachedFrameWidth : kThumbnailPreviewSizePresets[1].cachedFrameWidth;
    const int effectivePopupWidth = popupWidth > 0 ? popupWidth : kThumbnailPreviewSizePresets[1].popupWidth;

    int bestDistance = std::numeric_limits<int>::max();
    QString bestId = QStringLiteral("normal");
    for (const auto &preset : kThumbnailPreviewSizePresets) {
        const int distance = std::abs(effectiveCachedWidth - preset.cachedFrameWidth)
            + std::abs(effectivePopupWidth - preset.popupWidth);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestId = QString::fromLatin1(preset.id);
        }
    }

    return bestId;
}

QColor resolvedSubtitleButtonColor(const QPushButton *button, const QString &fallback)
{
    if (button == nullptr) {
        return QColor(fallback);
    }

    const QColor direct(button->property("subtitleColorValue").toString().trimmed());
    if (direct.isValid()) {
        return direct;
    }

    const QColor textColor(button->text().trimmed());
    if (textColor.isValid()) {
        return textColor;
    }

    return QColor(fallback);
}

void updateSubtitleColorButton(QPushButton *button, const QColor &requestedColor)
{
    if (button == nullptr) {
        return;
    }

    const QColor color = requestedColor.isValid() ? requestedColor : QColor(QStringLiteral("#FFFFFFFF"));
    const QColor foreground = color.lightnessF() > 0.58 ? QColor(QStringLiteral("#111111")) : QColor(QStringLiteral("#f7fbff"));
    const QString colorName = color.name(QColor::HexArgb);
    button->setProperty("subtitleColorValue", colorName);
    button->setText(colorName);
    button->setStyleSheet(
        QStringLiteral("text-align:left; padding: 4px 10px; border-radius: 8px; background:%1; color:%2;")
            .arg(colorName, foreground.name(QColor::HexRgb)));
}

}  // namespace

SettingsDialog::SettingsDialog(revaplayer::application::SettingsController *settingsController,
                               revaplayer::application::HistoryController *historyController,
                               QVector<revaplayer::ui::ShortcutBinding> shortcutBindings,
                               QWidget *parent)
    : QDialog(parent)
    , settingsController_(settingsController)
    , historyController_(historyController)
    , shortcutBindings_(std::move(shortcutBindings))
{
    setObjectName(QStringLiteral("settingsDialog"));
    setWindowTitle(uiText("Preferences"));
    setModal(true);
    setWindowFlag(Qt::WindowMaximizeButtonHint, true);
    setSizeGripEnabled(true);
    setLayoutDirection(Qt::LeftToRight);
    setMinimumSize(1040, 720);
    resize(1180, 820);

    buildUi();
    loadSettings();
}

bool SettingsDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != nullptr && event != nullptr && event->type() == QEvent::Wheel) {
        QWidget *widget = qobject_cast<QWidget *>(watched);
        if (widget != nullptr) {
            const bool isWheelValueControl = qobject_cast<QAbstractSpinBox *>(widget) != nullptr
                || qobject_cast<QComboBox *>(widget) != nullptr
                || qobject_cast<QFontComboBox *>(widget) != nullptr;
            if (isWheelValueControl && !widget->hasFocus()) {
                event->ignore();
                return true;
            }
        }
    }

    return QDialog::eventFilter(watched, event);
}

void SettingsDialog::applySettings()
{
    if (settingsController_ == nullptr) {
        return;
    }

    settingsController_->setRememberWindowState(rememberWindowStateCheckBox_->isChecked());
    settingsController_->setRememberLastOpenDirectory(rememberLastDirectoryCheckBox_->isChecked());
    settingsController_->setInterfaceLanguage(interfaceLanguageComboBox_->currentData().toString());
    settingsController_->setUiTheme(themeComboBox_->currentData().toString());
    settingsController_->setCustomValue(QStringLiteral("ui/accent"), themeAccentComboBox_->currentData().toString());
    settingsController_->setCustomValue(
        QStringLiteral("ui/density"),
        uiDensityComboBox_ != nullptr
            ? uiDensityComboBox_->currentData().toString()
            : QStringLiteral("normal"));
    settingsController_->setCustomValue(
        QString::fromLatin1(kStartupCanvasStyleSetting),
        startupCanvasStyleComboBox_ != nullptr
            ? normalizedStartupCanvasStyleId(startupCanvasStyleComboBox_->currentData().toString())
            : QStringLiteral("theme"));
    settingsController_->setCustomValue(QStringLiteral("ui/mode"), QStringLiteral("simple"));
    settingsController_->setCustomValue(
        QStringLiteral("ui/dashboard_enabled"),
        dashboardEnabledCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(QStringLiteral("ui/dashboard_show_on_idle"), QStringLiteral("1"));
    settingsController_->setCustomValue(
        QString::fromLatin1(kDashboardContinueSectionSetting),
        dashboardShowContinueCheckBox_ != nullptr && dashboardShowContinueCheckBox_->isChecked()
            ? QStringLiteral("1")
            : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(kDashboardRecentSectionSetting),
        dashboardShowRecentCheckBox_ != nullptr && dashboardShowRecentCheckBox_->isChecked()
            ? QStringLiteral("1")
            : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(kDashboardFavoritesSectionSetting),
        dashboardShowFavoritesCheckBox_ != nullptr && dashboardShowFavoritesCheckBox_->isChecked()
            ? QStringLiteral("1")
            : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(kDashboardSavedListsSectionSetting),
        dashboardShowSavedListsCheckBox_ != nullptr && dashboardShowSavedListsCheckBox_->isChecked()
            ? QStringLiteral("1")
            : QStringLiteral("0"));
    settingsController_->setCustomValue(QStringLiteral("ui/radius_px"), QString::number(themeRadiusSpinBox_->value()));
    settingsController_->setCustomValue(QStringLiteral("ui/spacing_px"), QString::number(themeSpacingSpinBox_->value()));
    settingsController_->setCustomValue(QStringLiteral("ui/font_scale_percent"), QString::number(themeFontScaleSpinBox_->value()));
    settingsController_->setCustomValue(QStringLiteral("ui/font_weight_value"), QString::number(themeFontWeightSpinBox_->value()));
    settingsController_->setCustomValue(QStringLiteral("ui/letter_spacing_px"), QString::number(themeLetterSpacingSpinBox_->value(), 'f', 2));
    settingsController_->setCustomValue(QStringLiteral("ui/border_contrast_percent"), QString::number(themeBorderContrastSpinBox_->value()));
    settingsController_->setCustomValue(QStringLiteral("ui/shadow_strength_percent"), QString::number(themeShadowStrengthSpinBox_->value()));
    settingsController_->setCustomValue(QStringLiteral("ui/blur_strength_percent"), QString::number(themeBlurStrengthSpinBox_->value()));
    settingsController_->setCustomValue(QStringLiteral("ui/animation_speed_percent"), QString::number(themeAnimationSpeedSpinBox_->value()));
    settingsController_->setCustomValue(QStringLiteral("ui/animation_easing"), themeAnimationEasingComboBox_->currentData().toString());
    settingsController_->setCustomValue(QStringLiteral("ui/overlay_opacity_percent"), QString::number(themeOverlayOpacitySpinBox_->value()));
    settingsController_->setCustomValue(QStringLiteral("ui/adaptive_enabled"), adaptiveUiCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(QStringLiteral("ui/adaptive_breakpoint_px"), QString::number(adaptiveUiBreakpointSpinBox_->value()));
    settingsController_->setShowMenuBarInWindowedMode(showMenuBarCheckBox_->isChecked());
    settingsController_->setShowStatusBarInWindowedMode(showStatusBarCheckBox_->isChecked());
    settingsController_->setCustomValue(QString::fromLatin1(kWindowChromeUserDefinedSetting), QStringLiteral("1"));
    settingsController_->setAlwaysOnTopEnabled(false);
    settingsController_->setOverlayPanelsOnVideo(overlayPanelsOnVideoCheckBox_->isChecked());
    settingsController_->setResumeEnabled(resumePlaybackCheckBox_->isChecked());
    settingsController_->setHistoryEnabled(rememberHistoryCheckBox_->isChecked());
    settingsController_->setClearHistoryOnExit(clearHistoryOnExitCheckBox_->isChecked());
    settingsController_->setPlaybackProfile(
        revaplayer::domain::playerProfileFromId(profileComboBox_->currentData().toString()));
    settingsController_->setUseExternalMpvConfig(false);
    settingsController_->setShowPlaylistPanelOnStartup(showPlaylistPanelCheckBox_->isChecked());
    settingsController_->setShowDetailsPanelOnStartup(showDetailsPanelCheckBox_->isChecked());
    settingsController_->setRestoreSidePanelsFromWindowState(
        restoreSidePanelsFromWindowStateCheckBox_ != nullptr
        && restoreSidePanelsFromWindowStateCheckBox_->isChecked());
    settingsController_->setRememberLastVolume(
        startupVolumeModeComboBox_->currentData().toString() == QStringLiteral("remember"));
    settingsController_->setStartupVolume(startupVolumeSpinBox_->value());
    settingsController_->setStartupPlaybackSpeed(startupSpeedSpinBox_->value());
    settingsController_->setCustomValue(
        QString::fromLatin1(kSessionWidePlaybackSpeedSetting),
        sessionWideSpeedCheckBox_ != nullptr && sessionWideSpeedCheckBox_->isChecked()
            ? QStringLiteral("1")
            : QStringLiteral("0"));
    settingsController_->setDefaultRepeatMode(defaultRepeatModeComboBox_->currentData().toString());
    settingsController_->setShortSeekStepSeconds(shortSeekStepSpinBox_->value());
    settingsController_->setLongSeekStepSeconds(longSeekStepSpinBox_->value());
    settingsController_->setVolumeStep(volumeStepSpinBox_->value());
    settingsController_->setMouseWheelVolumeEnabled(mouseWheelActionComboBox_->currentData().toString() == QStringLiteral("volume"));
    settingsController_->setMouseWheelVolumeStep(mouseWheelVolumeStepSpinBox_->value());
    settingsController_->setMouseWheelAction(mouseWheelActionComboBox_->currentData().toString());
    settingsController_->setMouseWheelSeekStepSeconds(mouseWheelSeekStepSpinBox_->value());
    settingsController_->setMouseNavigationSeekEnabled(
        mouseSideButtonsActionComboBox_->currentData().toString().startsWith(QStringLiteral("seek")));
    settingsController_->setMouseNavigationSeekStepSeconds(mouseNavigationSeekStepSpinBox_->value());
    settingsController_->setMouseSideButtonsAction(mouseSideButtonsActionComboBox_->currentData().toString());
    settingsController_->setClickAction(clickActionComboBox_->currentData().toString());
    settingsController_->setDoubleClickAction(doubleClickActionComboBox_->currentData().toString());
    settingsController_->setMiddleClickAction(middleClickActionComboBox_->currentData().toString());
    settingsController_->setActionFeedbackOverlayEnabled(actionFeedbackOverlayCheckBox_->isChecked());
    settingsController_->setExpressiveControlLabelsEnabled(expressiveControlLabelsCheckBox_->isChecked());
    settingsController_->setCustomValue(
        QString::fromLatin1(kControlBarShowOpenButtonSetting),
        controlBarShowOpenButtonCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(kControlBarShowStopButtonSetting),
        controlBarShowStopButtonCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(kControlBarShowDetailsButtonSetting),
        controlBarShowDetailsButtonCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(kControlBarShowPlaylistButtonSetting),
        controlBarShowPlaylistButtonCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(kControlBarShowTimeLabelSetting),
        controlBarShowTimeLabelCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(kControlBarShowSpeedButtonSetting),
        controlBarShowSpeedButtonCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(kControlBarShowRepeatLoopButtonsSetting),
        controlBarShowRepeatLoopButtonsCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(kControlBarShowTrackMenusSetting),
        controlBarShowTrackMenusCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(kControlBarShowVolumeControlsSetting),
        controlBarShowVolumeControlsCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(kControlBarShowFullscreenButtonSetting),
        controlBarShowFullscreenButtonCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(kControlBarTimelineThicknessSetting),
        QString::number(controlBarTimelineThicknessSpinBox_->value()));
    settingsController_->setCustomValue(
        QString::fromLatin1(kControlBarTimelineHandleSizeSetting),
        QString::number(controlBarTimelineHandleSizeSpinBox_->value()));
    settingsController_->setCustomValue(
        QString::fromLatin1(kControlBarVolumeSliderThicknessSetting),
        QString::number(controlBarVolumeSliderThicknessSpinBox_->value()));
    settingsController_->setCustomValue(
        QString::fromLatin1(kControlBarVolumeSliderWidthSetting),
        QString::number(controlBarVolumeSliderWidthSpinBox_->value()));
    settingsController_->setFullscreenAutoHideEnabled(fullscreenAutoHideCheckBox_->isChecked());
    settingsController_->setFullscreenRevealMargin(fullscreenRevealMarginSpinBox_->value());
    settingsController_->setFullscreenEdgePanelRevealEnabled(fullscreenEdgePanelsCheckBox_->isChecked());
    settingsController_->setFullscreenSideSelectorEnabled(fullscreenSideSelectorCheckBox_->isChecked());
    settingsController_->setCustomValue(
        QStringLiteral("input/pointer/right_edge_windowed_enabled"),
        windowedEdgePanelsCheckBox_ != nullptr && windowedEdgePanelsCheckBox_->isChecked()
            ? QStringLiteral("1")
            : QStringLiteral("0"));
    settingsController_->setDefaultSidePanel(defaultSidePanelComboBox_->currentData().toString());
    settingsController_->setHistoryLimit(historyLimitSpinBox_->value());
    settingsController_->setThumbnailPreviewsEnabled(thumbnailPreviewsCheckBox_->isChecked());
    settingsController_->setThumbnailPreviewWidth(thumbnailWidthSpinBox_->value());
    settingsController_->setThumbnailPopupWidth(thumbnailPopupWidthSpinBox_->value());
    settingsController_->setThumbnailPopupVerticalOffset(thumbnailPopupOffsetSpinBox_->value());
    settingsController_->setThumbnailPopupScreenPadding(thumbnailPopupPaddingSpinBox_->value());
    settingsController_->setDoubleClickFullscreenEnabled(
        doubleClickActionComboBox_->currentData().toString() == QStringLiteral("fullscreen"));
    settingsController_->setAutoLoadSiblingMediaEnabled(autoLoadSiblingMediaCheckBox_->isChecked());
    settingsController_->setShowPlaylistPanelOnFolderLoad(showPlaylistPanelOnFolderLoadCheckBox_->isChecked());
    settingsController_->setCustomValue(QString::fromLatin1(kShowPlaylistPanelOnFolderLoadUserDefinedSetting), QStringLiteral("1"));
    settingsController_->setPlaylistShowFullPaths(playlistShowFullPathsCheckBox_->isChecked());
    settingsController_->setPlaylistShowIndexPrefixes(playlistShowIndexPrefixesCheckBox_->isChecked());
    settingsController_->setPlaylistAutoFollowCurrent(playlistAutoFollowCurrentCheckBox_->isChecked());
    settingsController_->setRotateFolderPlaylistToCurrent(rotateFolderPlaylistToCurrentCheckBox_->isChecked());
    settingsController_->setVideoZoomStep(videoZoomStepSpinBox_->value());
    settingsController_->setVideoMinimumZoom(videoMinimumZoomSpinBox_->value());
    settingsController_->setVideoMaximumZoom(videoMaximumZoomSpinBox_->value());
    settingsController_->setVideoZoomDefaultBehavior(videoZoomDefaultBehaviorComboBox_->currentData().toString());
    settingsController_->setVideoZoomResetOnFileChange(videoZoomResetOnFileChangeCheckBox_->isChecked());
    settingsController_->setVideoZoomRememberMode(videoZoomRememberModeComboBox_->currentData().toString());
    settingsController_->setVideoPanSensitivity(videoPanSensitivitySpinBox_->value());
    settingsController_->setVideoZoomConstrainPanning(videoZoomConstrainPanningCheckBox_->isChecked());
    settingsController_->setVideoZoomWheelBehavior(videoZoomWheelBehaviorComboBox_->currentData().toString());
    settingsController_->setVideoZoomFullscreenBehavior(videoZoomFullscreenBehaviorComboBox_->currentData().toString());
    settingsController_->setCustomValue(
        QStringLiteral("playlist/progress_mode_enabled"),
        progressTrackingModeCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QStringLiteral("playlist/progress_completion_threshold"),
        QString::number(progressCompletionThresholdSpinBox_->value()));
    settingsController_->setCustomValue(
        QStringLiteral("playlist/progress_show_badges"),
        progressBadgesCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setSubtitleVisible(subtitleVisibleCheckBox_->isChecked());
    settingsController_->setSubtitleScale(subtitleScaleSpinBox_->value());
    settingsController_->setSubtitlePosition(subtitlePositionSpinBox_->value());
    settingsController_->setSubtitleFontFamily(subtitleFontComboBox_->currentFont().family());
    settingsController_->setSubtitleFontSize(subtitleFontSizeSpinBox_->value());
    settingsController_->setSubtitleAssOverride(subtitleAssOverrideComboBox_->currentData().toString());
    settingsController_->setSubtitleAutoSelectEnabled(subtitleAutoSelectCheckBox_->isChecked());
    settingsController_->setSubtitlePreferExternal(subtitlePreferExternalCheckBox_->isChecked());
    const QString subtitleAutoLoadMode = revaplayer::application::normalizeSubtitleAutoLoadMode(
        subtitleAutoLoadModeComboBox_->currentData().toString());
    settingsController_->setSubtitleAutoLoadLocalMatches(
        subtitleAutoLoadLocalMatchesCheckBox_ != nullptr
            ? (subtitleAutoLoadLocalMatchesCheckBox_->isChecked()
                && subtitleAutoLoadMode != QStringLiteral("manual_only"))
            : (subtitleAutoLoadMode != QStringLiteral("manual_only")));
    settingsController_->setSubtitlePreferredLanguages(subtitlePreferredLanguagesEdit_->text());
    settingsController_->setSubtitleSyncSmallStep(subtitleSyncSmallStepSpinBox_->value());
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleAutoLoadModeSetting),
        subtitleAutoLoadMode);
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleAutoExtensionsSetting),
        revaplayer::application::normalizeSubtitleAutoExtensions(
            subtitleAutoExtensionsEdit_ != nullptr
                ? subtitleAutoExtensionsEdit_->text()
                : settingsController_->customValue(
                    QString::fromLatin1(revaplayer::application::kSubtitleAutoExtensionsSetting),
                    revaplayer::application::defaultSubtitleAutoExtensions())));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleRememberTrackChoiceSetting),
        subtitleRememberTrackChoiceCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleCodepageSetting),
        revaplayer::application::normalizeSubtitleEncoding(subtitleEncodingComboBox_->currentData().toString()));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleFixTimingSetting),
        subtitleFixTimingCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleFontWeightSetting),
        QString::number(revaplayer::application::clampSubtitleFontWeight(subtitleFontWeightSpinBox_->value())));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleItalicSetting),
        subtitleItalicCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleTextColorSetting),
        revaplayer::application::normalizeSubtitleColorString(
            subtitleTextColorButton_->property("subtitleColorValue").toString(),
            QStringLiteral("#FFFFFFFF")));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleOutlineColorSetting),
        revaplayer::application::normalizeSubtitleColorString(
            subtitleOutlineColorButton_->property("subtitleColorValue").toString(),
            QStringLiteral("#FF000000")));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleOutlineSizeSetting),
        QString::number(revaplayer::application::clampSubtitleOutlineSize(subtitleOutlineSizeSpinBox_->value()), 'f', 2));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleBorderStyleSetting),
        revaplayer::application::normalizeSubtitleBorderStyle(subtitleBorderStyleComboBox_->currentData().toString()));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleBackgroundEnabledSetting),
        subtitleBackgroundEnabledCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleBackgroundColorSetting),
        revaplayer::application::normalizeSubtitleColorString(
            subtitleBackgroundColorButton_->property("subtitleColorValue").toString(),
            QStringLiteral("#AF000000")));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleBackgroundOpacitySetting),
        QString::number(revaplayer::application::clampSubtitleBackgroundOpacity(subtitleBackgroundOpacitySpinBox_->value())));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleShadowEnabledSetting),
        subtitleShadowEnabledCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleShadowColorSetting),
        revaplayer::application::normalizeSubtitleColorString(
            subtitleShadowColorButton_->property("subtitleColorValue").toString(),
            QStringLiteral("#AF000000")));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleShadowOffsetSetting),
        QString::number(revaplayer::application::clampSubtitleShadowOffset(subtitleShadowOffsetSpinBox_->value()), 'f', 2));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleShadowBlurSetting),
        QString::number(revaplayer::application::clampSubtitleBlur(subtitleShadowBlurSpinBox_->value()), 'f', 2));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleLineSpacingSetting),
        QString::number(revaplayer::application::clampSubtitleLineSpacing(subtitleLineSpacingSpinBox_->value()), 'f', 2));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleLetterSpacingSetting),
        QString::number(revaplayer::application::clampSubtitleLetterSpacing(subtitleLetterSpacingSpinBox_->value()), 'f', 2));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleMaxWidthSetting),
        QString::number(revaplayer::application::clampSubtitleMaxWidth(subtitleMaxWidthSpinBox_->value()), 'f', 2));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleAlignXSetting),
        revaplayer::application::normalizeSubtitleAlignX(subtitleAlignXComboBox_->currentData().toString()));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleAlignYSetting),
        revaplayer::application::normalizeSubtitleAlignY(subtitleAlignYComboBox_->currentData().toString()));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleJustifySetting),
        revaplayer::application::normalizeSubtitleJustify(subtitleJustifyComboBox_->currentData().toString()));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleMarginXSetting),
        QString::number(revaplayer::application::clampSubtitleMarginX(subtitleMarginXSpinBox_->value())));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleMarginYSetting),
        QString::number(revaplayer::application::clampSubtitleMarginY(subtitleMarginYSpinBox_->value())));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleUseMarginsSetting),
        subtitleUseMarginsCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleScaleWithWindowSetting),
        subtitleScaleWithWindowCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleAssForceMarginsSetting),
        subtitleAssForceMarginsCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleAssJustifySetting),
        subtitleAssJustifyCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleFontProviderSetting),
        revaplayer::application::normalizeSubtitleFontProvider(subtitleFontProviderComboBox_->currentData().toString()));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleShaperSetting),
        revaplayer::application::normalizeSubtitleShaper(subtitleShaperComboBox_->currentData().toString()));
    settingsController_->setCustomValue(
        QString::fromLatin1(revaplayer::application::kSubtitleHintingSetting),
        revaplayer::application::normalizeSubtitleHinting(subtitleHintingComboBox_->currentData().toString()));
    settingsController_->setSceneBrowserStepSeconds(sceneBrowserStepSpinBox_->value());
    settingsController_->setSceneBrowserMaxItems(sceneBrowserMaxItemsSpinBox_->value());
    settingsController_->setScreenshotDirectory(screenshotDirectoryEdit_->text());
    settingsController_->setCustomValue(QStringLiteral("input/gestures/enabled"), mouseGesturesCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(QStringLiteral("input/gestures/threshold"), QString::number(mouseGestureThresholdSpinBox_->value()));
    settingsController_->setCustomValue(QStringLiteral("input/gestures/left"), gestureLeftActionComboBox_->currentData().toString());
    settingsController_->setCustomValue(QStringLiteral("input/gestures/right"), gestureRightActionComboBox_->currentData().toString());
    settingsController_->setCustomValue(QStringLiteral("input/gestures/up"), gestureUpActionComboBox_->currentData().toString());
    settingsController_->setCustomValue(QStringLiteral("input/gestures/down"), gestureDownActionComboBox_->currentData().toString());
    settingsController_->setCustomValue(
        QStringLiteral("input/pointer/right_edge_action_fullscreen"),
        pointerRightEdgeFullscreenActionComboBox_->currentData().toString());
    settingsController_->setCustomValue(
        QStringLiteral("input/pointer/right_edge_action_windowed"),
        pointerRightEdgeWindowedActionComboBox_->currentData().toString());
    settingsController_->setCustomValue(
        QStringLiteral("input/pointer/right_edge_leave_action"),
        pointerRightEdgeLeaveActionComboBox_->currentData().toString());
    settingsController_->setCustomValue(
        QStringLiteral("input/pointer/right_edge_margin"),
        QString::number(pointerRightEdgeMarginSpinBox_->value()));
    settingsController_->setCustomValue(
        QStringLiteral("input/pointer/leave_delay_ms"),
        QString::number(pointerLeaveDelaySpinBox_->value()));
    settingsController_->setCustomValue(
        QStringLiteral("input/pointer/keep_controls_visible"),
        pointerKeepControlsVisibleCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QStringLiteral("input/mouse_zone/top"),
        mouseZoneTopActionComboBox_->currentData().toString());
    settingsController_->setCustomValue(
        QStringLiteral("input/mouse_zone/bottom"),
        mouseZoneBottomActionComboBox_->currentData().toString());
    settingsController_->setCustomValue(
        QStringLiteral("input/mouse_zone/left"),
        mouseZoneLeftActionComboBox_->currentData().toString());
    settingsController_->setCustomValue(
        QStringLiteral("input/mouse_zone/right"),
        mouseZoneRightActionComboBox_->currentData().toString());
    settingsController_->setCustomValue(QString::fromLatin1(kMouseZoneRightUserDefinedSetting), QStringLiteral("1"));
    settingsController_->setCustomValue(
        QStringLiteral("input/mouse_zone/center"),
        mouseZoneCenterActionComboBox_->currentData().toString());
    settingsController_->setCustomValue(QStringLiteral("capture/screenshot_format"), screenshotFormatComboBox_->currentData().toString());
    settingsController_->setCustomValue(QStringLiteral("capture/screenshot_template"), screenshotTemplateEdit_->text().trimmed());

    for (const auto &row : shortcutEditors_) {
        const QString currentPortableText = row.editor->keySequence().toString(QKeySequence::PortableText).trimmed();
        const QString defaultPortableText = row.defaultSequence.toString(QKeySequence::PortableText).trimmed();
        settingsController_->setShortcutOverride(
            row.id,
            currentPortableText == defaultPortableText ? QString {} : currentPortableText);
    }
}

void SettingsDialog::buildUi()
{
    settingsTabs_ = new QTabWidget(this);
    auto *tabs = settingsTabs_;
    tabs->setObjectName(QStringLiteral("settingsTabs"));
    tabs->setTabPosition(QTabWidget::North);
    tabs->setDocumentMode(true);
    tabs->setUsesScrollButtons(false);

    settingsSectionTabsScrollArea_ = new QScrollArea(this);
    settingsSectionTabsScrollArea_->setObjectName(QStringLiteral("settingsSectionTabsScrollArea"));
    settingsSectionTabsScrollArea_->setWidgetResizable(false);
    settingsSectionTabsScrollArea_->setFrameShape(QFrame::NoFrame);
    settingsSectionTabsScrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    settingsSectionTabsScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    settingsSectionTabsContainer_ = new QWidget(settingsSectionTabsScrollArea_);
    settingsSectionTabsContainer_->setObjectName(QStringLiteral("settingsSectionTabsContainer"));
    auto *settingsSectionTabsLayout = new QHBoxLayout(settingsSectionTabsContainer_);
    settingsSectionTabsLayout->setContentsMargins(0, 0, 0, 0);
    settingsSectionTabsLayout->setSpacing(8);
    settingsSectionTabsScrollArea_->setWidget(settingsSectionTabsContainer_);

    auto *generalPage = new QWidget();
    auto *generalLayout = new QVBoxLayout(generalPage);
    generalLayout->setContentsMargins(18, 18, 18, 18);
    generalLayout->setSpacing(14);

    auto *advancedPage = new QWidget();
    auto *advancedLayout = new QVBoxLayout(advancedPage);
    advancedLayout->setContentsMargins(18, 18, 18, 18);
    advancedLayout->setSpacing(14);

    auto *sessionGroup = new QGroupBox(uiText("Session"), generalPage);
    auto *sessionLayout = new QVBoxLayout(sessionGroup);
    rememberWindowStateCheckBox_ = new QCheckBox(uiText("Remember window size, position, and dock layout"), sessionGroup);
    rememberLastDirectoryCheckBox_ = new QCheckBox(uiText("Remember the last directory used in Open File"), sessionGroup);
    resumePlaybackCheckBox_ = new QCheckBox(uiText("Resume playback from the last saved position"), sessionGroup);
    sessionLayout->addWidget(rememberWindowStateCheckBox_);
    sessionLayout->addWidget(rememberLastDirectoryCheckBox_);
    resumePlaybackCheckBox_->setVisible(false);

    auto *layoutGroup = new QGroupBox(uiText("Panels"), advancedPage);
    auto *layoutBox = new QVBoxLayout(layoutGroup);
    showPlaylistPanelCheckBox_ = new QCheckBox(uiText("Show playlist panel on startup"), layoutGroup);
    showDetailsPanelCheckBox_ = new QCheckBox(uiText("Show chapters / tracks panel on startup"), layoutGroup);
    restoreSidePanelsFromWindowStateCheckBox_ = new QCheckBox(
        uiText("Restore playlist / details visibility from the saved window state"),
        layoutGroup);
    overlayPanelsOnVideoCheckBox_ = new QCheckBox(uiText("Overlay playlist and details panels on top of the video"), layoutGroup);
    layoutBox->addWidget(showPlaylistPanelCheckBox_);
    layoutBox->addWidget(showDetailsPanelCheckBox_);
    layoutBox->addWidget(restoreSidePanelsFromWindowStateCheckBox_);
    layoutBox->addWidget(overlayPanelsOnVideoCheckBox_);
    setHoverExplanation(
        restoreSidePanelsFromWindowStateCheckBox_,
        uiText("When window state is remembered, reopen whichever side panels were visible in the last session. Disable this to follow the startup panel checkboxes instead."));

    auto *historyGroup = new QGroupBox(uiText("History"), advancedPage);
    auto *historyLayout = new QFormLayout(historyGroup);
    rememberHistoryCheckBox_ = new QCheckBox(uiText("Keep playback history for recent media"), historyGroup);
    clearHistoryOnExitCheckBox_ = new QCheckBox(uiText("Clear history automatically when the application closes"), historyGroup);
    auto *historyNote = new QLabel(
        uiText("Disable history when you want a private session. You can also cap its size or clear it immediately from here."),
        historyGroup);
    historyNote->setWordWrap(true);
    historyLayout->addRow(rememberHistoryCheckBox_);
    historyLayout->addRow(clearHistoryOnExitCheckBox_);

    historyLimitSpinBox_ = new QSpinBox(historyGroup);
    historyLimitSpinBox_->setRange(20, 500);
    historyLimitSpinBox_->setSingleStep(10);
    historyLayout->addRow(uiText("Limit stored history entries"), historyLimitSpinBox_);

    clearHistoryButton_ = new QPushButton(uiText("Clear History Now"), historyGroup);
    historyLayout->addRow(QString(), clearHistoryButton_);
    historyLayout->addRow(historyNote);

    auto *windowChromeGroup = new QGroupBox(uiText("Window Chrome"), generalPage);
    auto *windowChromeLayout = new QVBoxLayout(windowChromeGroup);
    showMenuBarCheckBox_ = new QCheckBox(uiText("Show the menu bar in windowed mode"), windowChromeGroup);
    showStatusBarCheckBox_ = new QCheckBox(uiText("Show the status bar in windowed mode"), windowChromeGroup);
    alwaysOnTopCheckBox_ = new QCheckBox(uiText("Keep the player window above other windows"), windowChromeGroup);
    windowChromeLayout->addWidget(showMenuBarCheckBox_);
    windowChromeLayout->addWidget(showStatusBarCheckBox_);
    alwaysOnTopCheckBox_->setVisible(false);
    windowChromeLayout->addWidget(alwaysOnTopCheckBox_);

    auto *appearanceGroup = new QGroupBox(uiText("Appearance"), generalPage);
    auto *appearanceLayout = new QFormLayout(appearanceGroup);
    themeComboBox_ = new QComboBox(appearanceGroup);
    for (const auto &theme : revaplayer::application::availableThemes()) {
        themeComboBox_->addItem(revaplayer::application::translateUiText(theme.label), theme.id);
    }
    appearanceLayout->addRow(uiText("Application theme"), themeComboBox_);
    themeAccentComboBox_ = new QComboBox(appearanceGroup);
    for (const auto &accent : revaplayer::application::availableAccents()) {
        themeAccentComboBox_->addItem(revaplayer::application::translateUiText(accent.label), accent.id);
    }
    appearanceLayout->addRow(uiText("Accent color"), themeAccentComboBox_);
    uiDensityComboBox_ = nullptr;
    startupCanvasStyleComboBox_ = new QComboBox(appearanceGroup);
    startupCanvasStyleComboBox_->addItem(uiText("Theme gradient"), QStringLiteral("theme"));
    startupCanvasStyleComboBox_->addItem(uiText("Pure black"), QStringLiteral("black"));
    startupCanvasStyleComboBox_->addItem(uiText("Warm surface"), QStringLiteral("warm"));
    appearanceLayout->addRow(uiText("Startup canvas background"), startupCanvasStyleComboBox_);

    auto *themeEditorGroup = new QGroupBox(uiText("Theme Editor"), generalPage);
    auto *themeEditorLayout = new QFormLayout(themeEditorGroup);
    themeRadiusSpinBox_ = new QSpinBox(themeEditorGroup);
    themeRadiusSpinBox_->setRange(4, 28);
    themeSpacingSpinBox_ = new QSpinBox(themeEditorGroup);
    themeSpacingSpinBox_->setRange(4, 20);
    themeFontScaleSpinBox_ = new QSpinBox(themeEditorGroup);
    themeFontScaleSpinBox_->setRange(85, 140);
    themeFontScaleSpinBox_->setSuffix(QStringLiteral("%"));
    themeFontWeightSpinBox_ = new QSpinBox(themeEditorGroup);
    themeFontWeightSpinBox_->setRange(350, 800);
    themeFontWeightSpinBox_->setSingleStep(50);
    themeLetterSpacingSpinBox_ = new QDoubleSpinBox(themeEditorGroup);
    themeLetterSpacingSpinBox_->setRange(-0.4, 2.0);
    themeLetterSpacingSpinBox_->setSingleStep(0.05);
    themeLetterSpacingSpinBox_->setDecimals(2);
    themeBorderContrastSpinBox_ = new QSpinBox(themeEditorGroup);
    themeBorderContrastSpinBox_->setRange(65, 160);
    themeBorderContrastSpinBox_->setSuffix(QStringLiteral("%"));
    themeShadowStrengthSpinBox_ = new QSpinBox(themeEditorGroup);
    themeShadowStrengthSpinBox_->setRange(40, 160);
    themeShadowStrengthSpinBox_->setSuffix(QStringLiteral("%"));
    themeBlurStrengthSpinBox_ = new QSpinBox(themeEditorGroup);
    themeBlurStrengthSpinBox_->setRange(0, 100);
    themeBlurStrengthSpinBox_->setSuffix(QStringLiteral("%"));
    themeOverlayOpacitySpinBox_ = new QSpinBox(themeEditorGroup);
    themeOverlayOpacitySpinBox_->setRange(55, 98);
    themeOverlayOpacitySpinBox_->setSuffix(QStringLiteral("%"));
    themeRadiusSpinBox_->setVisible(false);
    themeSpacingSpinBox_->setVisible(false);
    themeFontWeightSpinBox_->setVisible(false);
    themeLetterSpacingSpinBox_->setVisible(false);
    themeBorderContrastSpinBox_->setVisible(false);
    themeShadowStrengthSpinBox_->setVisible(false);
    themeBlurStrengthSpinBox_->setVisible(false);
    themeOverlayOpacitySpinBox_->setVisible(false);
    // Keep only font size/scale visible for the user in this section.
    themeEditorLayout->addRow(uiText("Font scale"), themeFontScaleSpinBox_);

    auto *motionGroup = new QGroupBox(uiText("Motion & Adaptive UI"), generalPage);
    auto *motionLayout = new QFormLayout(motionGroup);
    themeAnimationSpeedSpinBox_ = new QSpinBox(motionGroup);
    themeAnimationSpeedSpinBox_->setRange(40, 220);
    themeAnimationSpeedSpinBox_->setSuffix(QStringLiteral("%"));
    themeAnimationEasingComboBox_ = new QComboBox(motionGroup);
    themeAnimationEasingComboBox_->addItem(uiText("Cubic"), QStringLiteral("cubic"));
    themeAnimationEasingComboBox_->addItem(uiText("Quart"), QStringLiteral("quart"));
    themeAnimationEasingComboBox_->addItem(uiText("Expo"), QStringLiteral("expo"));
    themeAnimationEasingComboBox_->addItem(uiText("Sine"), QStringLiteral("sine"));
    adaptiveUiCheckBox_ = new QCheckBox(uiText("Adapt the layout automatically when the window becomes narrow"), motionGroup);
    adaptiveUiBreakpointSpinBox_ = new QSpinBox(motionGroup);
    adaptiveUiBreakpointSpinBox_->setRange(760, 2200);
    adaptiveUiBreakpointSpinBox_->setSuffix(QStringLiteral(" px"));
    motionLayout->addRow(uiText("Animation speed"), themeAnimationSpeedSpinBox_);
    motionLayout->addRow(uiText("Panel easing"), themeAnimationEasingComboBox_);
    motionLayout->addRow(adaptiveUiCheckBox_);
    motionLayout->addRow(uiText("Adaptive breakpoint"), adaptiveUiBreakpointSpinBox_);
    motionGroup->setVisible(false);

    auto *interfaceGroup = new QGroupBox(uiText("Interface"), generalPage);
    auto *interfaceLayout = new QFormLayout(interfaceGroup);
    interfaceLanguageComboBox_ = new QComboBox(interfaceGroup);
    for (const auto &language : revaplayer::application::availableUiLanguages()) {
        interfaceLanguageComboBox_->addItem(language.label, language.id);
    }
    interfaceLayout->addRow(uiText("Interface language"), interfaceLanguageComboBox_);
    auto *languageNote = new QLabel(
        uiText("Restart the application after changing the interface language to fully refresh all menus and dialogs."),
        interfaceGroup);
    languageNote->setWordWrap(true);
    interfaceLayout->addRow(languageNote);

    auto *dashboardGroup = new QGroupBox(uiText("Home Dashboard"), generalPage);
    auto *dashboardLayout = new QFormLayout(dashboardGroup);
    dashboardEnabledCheckBox_ = new QCheckBox(
        uiText("Show the new dashboard with continue watching, recent items, favorites, and saved lists"),
        dashboardGroup);
    dashboardShowContinueCheckBox_ = new QCheckBox(uiText("Show Continue Watching section"), dashboardGroup);
    dashboardShowRecentCheckBox_ = new QCheckBox(uiText("Show Recent section"), dashboardGroup);
    dashboardShowFavoritesCheckBox_ = new QCheckBox(uiText("Show Favorites section"), dashboardGroup);
    dashboardShowSavedListsCheckBox_ = new QCheckBox(uiText("Show Saved Lists section"), dashboardGroup);
    dashboardLayout->addRow(dashboardEnabledCheckBox_);
    dashboardLayout->addRow(dashboardShowContinueCheckBox_);
    dashboardLayout->addRow(dashboardShowRecentCheckBox_);
    dashboardLayout->addRow(dashboardShowFavoritesCheckBox_);
    dashboardLayout->addRow(dashboardShowSavedListsCheckBox_);

    storageInfoLabel_ = new QLabel(generalPage);
    storageInfoLabel_->setWordWrap(true);

    generalLayout->addWidget(sessionGroup);
    generalLayout->addWidget(windowChromeGroup);
    generalLayout->addWidget(interfaceGroup);
    generalLayout->addWidget(appearanceGroup);
    generalLayout->addWidget(themeEditorGroup);
    generalLayout->addWidget(dashboardGroup);
    generalLayout->addWidget(storageInfoLabel_);
    generalLayout->addStretch(1);

    auto *playbackPage = new QWidget();
    auto *playbackLayout = new QVBoxLayout(playbackPage);
    playbackLayout->setContentsMargins(18, 18, 18, 18);
    playbackLayout->setSpacing(14);

    auto *volumeGroup = new QGroupBox(uiText("Playback Defaults"), playbackPage);
    auto *volumeLayout = new QFormLayout(volumeGroup);
    profileComboBox_ = new QComboBox(volumeGroup);
    profileComboBox_->addItem(
        revaplayer::application::translateUiText(
            revaplayer::domain::playerProfileLabel(revaplayer::domain::PlayerProfile::Battery)),
        revaplayer::domain::playerProfileId(revaplayer::domain::PlayerProfile::Battery));
    profileComboBox_->addItem(
        revaplayer::application::translateUiText(
            revaplayer::domain::playerProfileLabel(revaplayer::domain::PlayerProfile::Balanced)),
        revaplayer::domain::playerProfileId(revaplayer::domain::PlayerProfile::Balanced));
    profileComboBox_->addItem(
        revaplayer::application::translateUiText(
            revaplayer::domain::playerProfileLabel(revaplayer::domain::PlayerProfile::Quality)),
        revaplayer::domain::playerProfileId(revaplayer::domain::PlayerProfile::Quality));
    volumeLayout->addRow(uiText("Playback profile"), profileComboBox_);
    auto *profileExplanationLabel = new QLabel(
        uiText("Battery reduces thumbnail, playlist, progress, and background refresh work. Balanced keeps previews responsive with moderate refresh rates. Quality makes previews sharper and more immediate while still avoiding busy loops."),
        volumeGroup);
    profileExplanationLabel->setWordWrap(true);
    volumeLayout->addRow(QString {}, profileExplanationLabel);
    externalMpvConfigCheckBox_ = new QCheckBox(uiText("Use external mpv config"), volumeGroup);
    setHoverExplanation(
        externalMpvConfigCheckBox_,
        uiText("When enabled, Reva allows the normal user mpv configuration directory. Leave this off for Reva's isolated bundled mpv setup."));
    externalMpvConfigCheckBox_->setVisible(false);
    volumeLayout->addRow(QString {}, externalMpvConfigCheckBox_);
    startupVolumeModeComboBox_ = new QComboBox(volumeGroup);
    startupVolumeModeComboBox_->addItem(uiText("Remember last volume"), QStringLiteral("remember"));
    startupVolumeModeComboBox_->addItem(uiText("Use fixed startup volume"), QStringLiteral("fixed"));
    volumeLayout->addRow(uiText("Startup volume behavior"), startupVolumeModeComboBox_);

    startupVolumeSpinBox_ = new QSpinBox(volumeGroup);
    startupVolumeSpinBox_->setRange(0, revaplayer::application::kMaximumPlaybackVolume);
    startupVolumeSpinBox_->setSuffix(QStringLiteral("%"));
    volumeLayout->addRow(uiText("Startup volume"), startupVolumeSpinBox_);
    connect(startupVolumeModeComboBox_, &QComboBox::currentIndexChanged, this, [this](const int) {
        startupVolumeSpinBox_->setEnabled(
            startupVolumeModeComboBox_->currentData().toString() == QStringLiteral("fixed"));
    });

    startupSpeedSpinBox_ = new QDoubleSpinBox(volumeGroup);
    startupSpeedSpinBox_->setRange(0.25, 4.0);
    startupSpeedSpinBox_->setSingleStep(0.05);
    startupSpeedSpinBox_->setDecimals(2);
    startupSpeedSpinBox_->setSuffix(QStringLiteral("x"));
    volumeLayout->addRow(uiText("Startup speed"), startupSpeedSpinBox_);

    sessionWideSpeedCheckBox_ = new QCheckBox(uiText("Apply playback speed to all videos during this session"), volumeGroup);
    setHoverExplanation(
        sessionWideSpeedCheckBox_,
        uiText("When enabled, speed changes remain active for newly opened videos until the application closes."));
    volumeLayout->addRow(QString {}, sessionWideSpeedCheckBox_);

    defaultRepeatModeComboBox_ = new QComboBox(volumeGroup);
    defaultRepeatModeComboBox_->addItem(uiText("Repeat Off"), QStringLiteral("off"));
    defaultRepeatModeComboBox_->addItem(uiText("Repeat File"), QStringLiteral("file"));
    defaultRepeatModeComboBox_->addItem(uiText("Repeat Playlist"), QStringLiteral("playlist"));
    volumeLayout->addRow(uiText("Default repeat mode"), defaultRepeatModeComboBox_);

    auto *interactionGroup = new QGroupBox(uiText("Interaction"), playbackPage);
    auto *interactionLayout = new QFormLayout(interactionGroup);
    shortSeekStepSpinBox_ = new QSpinBox(interactionGroup);
    shortSeekStepSpinBox_->setRange(1, 30);
    shortSeekStepSpinBox_->setSuffix(QStringLiteral(" s"));
    interactionLayout->addRow(uiText("Short seek step"), shortSeekStepSpinBox_);

    longSeekStepSpinBox_ = new QSpinBox(interactionGroup);
    longSeekStepSpinBox_->setRange(5, 300);
    longSeekStepSpinBox_->setSingleStep(5);
    longSeekStepSpinBox_->setSuffix(QStringLiteral(" s"));
    interactionLayout->addRow(uiText("Long seek step"), longSeekStepSpinBox_);

    volumeStepSpinBox_ = new QSpinBox(interactionGroup);
    volumeStepSpinBox_->setRange(1, 20);
    volumeStepSpinBox_->setSuffix(QStringLiteral("%"));
    interactionLayout->addRow(uiText("Volume step"), volumeStepSpinBox_);

    auto *videoZoomGroup = new QGroupBox(uiText("Zoom"), playbackPage);
    auto *videoZoomLayout = new QFormLayout(videoZoomGroup);
    videoZoomLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    videoZoomLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    videoZoomLayout->setLabelAlignment(Qt::AlignLeading | Qt::AlignVCenter);
    videoZoomStepSpinBox_ = new QDoubleSpinBox(videoZoomGroup);
    videoZoomStepSpinBox_->setRange(0.05, 1.00);
    videoZoomStepSpinBox_->setDecimals(2);
    videoZoomStepSpinBox_->setSingleStep(0.05);
    videoZoomStepSpinBox_->setSuffix(QStringLiteral("x"));
    videoZoomLayout->addRow(uiText("Zoom step"), videoZoomStepSpinBox_);

    videoMinimumZoomSpinBox_ = new QDoubleSpinBox(videoZoomGroup);
    videoMinimumZoomSpinBox_->setRange(1.0, 12.0);
    videoMinimumZoomSpinBox_->setDecimals(2);
    videoMinimumZoomSpinBox_->setSingleStep(0.10);
    videoMinimumZoomSpinBox_->setSuffix(QStringLiteral("x"));
    videoZoomLayout->addRow(uiText("Minimum zoom"), videoMinimumZoomSpinBox_);

    videoMaximumZoomSpinBox_ = new QDoubleSpinBox(videoZoomGroup);
    videoMaximumZoomSpinBox_->setRange(1.0, 12.0);
    videoMaximumZoomSpinBox_->setDecimals(2);
    videoMaximumZoomSpinBox_->setSingleStep(0.10);
    videoMaximumZoomSpinBox_->setSuffix(QStringLiteral("x"));
    videoZoomLayout->addRow(uiText("Maximum zoom"), videoMaximumZoomSpinBox_);

    videoZoomDefaultBehaviorComboBox_ = new QComboBox(videoZoomGroup);
    videoZoomDefaultBehaviorComboBox_->addItem(uiText("Fit to Frame"), QStringLiteral("fit_to_frame"));
    videoZoomDefaultBehaviorComboBox_->addItem(uiText("Preserve Current Zoom"), QStringLiteral("preserve_current"));
    videoZoomLayout->addRow(uiText("Default behavior"), videoZoomDefaultBehaviorComboBox_);

    videoZoomResetOnFileChangeCheckBox_ = new QCheckBox(uiText("Reset zoom when a different file starts"), videoZoomGroup);
    videoZoomLayout->addRow(videoZoomResetOnFileChangeCheckBox_);

    videoZoomRememberModeComboBox_ = new QComboBox(videoZoomGroup);
    videoZoomRememberModeComboBox_->addItem(uiText("Off"), QStringLiteral("off"));
    videoZoomRememberModeComboBox_->addItem(uiText("Per Session"), QStringLiteral("session"));
    videoZoomRememberModeComboBox_->addItem(uiText("Per File"), QStringLiteral("per_file"));
    videoZoomLayout->addRow(uiText("Remember zoom state"), videoZoomRememberModeComboBox_);

    videoPanSensitivitySpinBox_ = new QDoubleSpinBox(videoZoomGroup);
    videoPanSensitivitySpinBox_->setRange(0.25, 4.0);
    videoPanSensitivitySpinBox_->setDecimals(2);
    videoPanSensitivitySpinBox_->setSingleStep(0.05);
    videoPanSensitivitySpinBox_->setSuffix(QStringLiteral("x"));
    videoZoomLayout->addRow(uiText("Pan sensitivity"), videoPanSensitivitySpinBox_);

    videoZoomConstrainPanningCheckBox_ = new QCheckBox(uiText("Constrain panning to the visible frame bounds"), videoZoomGroup);
    videoZoomLayout->addRow(videoZoomConstrainPanningCheckBox_);

    videoZoomWheelBehaviorComboBox_ = new QComboBox(videoZoomGroup);
    videoZoomWheelBehaviorComboBox_->addItem(uiText("Keep Wheel for Global Mouse Action"), QStringLiteral("global"));
    videoZoomWheelBehaviorComboBox_->addItem(uiText("Zoom with Ctrl + Wheel"), QStringLiteral("zoom_with_ctrl"));
    videoZoomWheelBehaviorComboBox_->addItem(uiText("Zoom with Wheel While Already Zoomed"), QStringLiteral("zoom_when_zoomed"));
    videoZoomLayout->addRow(uiText("Wheel zoom behavior"), videoZoomWheelBehaviorComboBox_);

    videoZoomFullscreenBehaviorComboBox_ = new QComboBox(videoZoomGroup);
    videoZoomFullscreenBehaviorComboBox_->addItem(uiText("Keep Current Zoom"), QStringLiteral("keep"));
    videoZoomFullscreenBehaviorComboBox_->addItem(uiText("Reset on Fullscreen Toggle"), QStringLiteral("reset_on_toggle"));
    videoZoomLayout->addRow(uiText("Fullscreen transition"), videoZoomFullscreenBehaviorComboBox_);

    auto *videoZoomHint = new QLabel(
        uiText("Keyboard zoom shortcuts remain available. These settings control how far zoom can go, how wheel gestures behave, and whether manual zoom is remembered."),
        videoZoomGroup);
    videoZoomHint->setWordWrap(true);
    videoZoomLayout->addRow(videoZoomHint);

    auto *mouseGroup = new QGroupBox(uiText("Mouse Controls"), advancedPage);
    auto *mouseLayout = new QFormLayout(mouseGroup);
    mouseWheelActionComboBox_ = new QComboBox(mouseGroup);
    mouseWheelActionComboBox_->addItem(uiText("Volume"), QStringLiteral("volume"));
    mouseWheelActionComboBox_->addItem(uiText("Seek"), QStringLiteral("seek"));
    mouseWheelActionComboBox_->addItem(uiText("Disabled"), QStringLiteral("none"));
    mouseLayout->addRow(uiText("Wheel action"), mouseWheelActionComboBox_);

    mouseWheelVolumeCheckBox_ = new QCheckBox(uiText("Use mouse wheel over video to adjust volume"), mouseGroup);
    mouseLayout->addRow(uiText("Wheel volume"), mouseWheelVolumeCheckBox_);

    mouseWheelVolumeStepSpinBox_ = new QSpinBox(mouseGroup);
    mouseWheelVolumeStepSpinBox_->setRange(1, 20);
    mouseWheelVolumeStepSpinBox_->setSuffix(QStringLiteral("%"));
    mouseLayout->addRow(uiText("Wheel volume step"), mouseWheelVolumeStepSpinBox_);

    mouseWheelSeekStepSpinBox_ = new QSpinBox(mouseGroup);
    mouseWheelSeekStepSpinBox_->setRange(1, 120);
    mouseWheelSeekStepSpinBox_->setSuffix(QStringLiteral(" s"));
    mouseLayout->addRow(uiText("Wheel seek step"), mouseWheelSeekStepSpinBox_);

    mouseSideButtonsActionComboBox_ = new QComboBox(mouseGroup);
    mouseSideButtonsActionComboBox_->addItem(uiText("Short Seek"), QStringLiteral("seek_short"));
    mouseSideButtonsActionComboBox_->addItem(uiText("Long Seek"), QStringLiteral("seek_long"));
    mouseSideButtonsActionComboBox_->addItem(uiText("Playlist Navigation"), QStringLiteral("playlist"));
    mouseSideButtonsActionComboBox_->addItem(uiText("Chapter Navigation"), QStringLiteral("chapter"));
    mouseSideButtonsActionComboBox_->addItem(uiText("Video Zoom"), QStringLiteral("zoom"));
    mouseSideButtonsActionComboBox_->addItem(uiText("Disabled"), QStringLiteral("none"));
    mouseLayout->addRow(uiText("Side buttons action"), mouseSideButtonsActionComboBox_);

    mouseNavigationSeekCheckBox_ = new QCheckBox(uiText("Use mouse back / forward buttons to seek"), mouseGroup);
    mouseLayout->addRow(uiText("Side buttons seek"), mouseNavigationSeekCheckBox_);

    mouseNavigationSeekStepSpinBox_ = new QSpinBox(mouseGroup);
    mouseNavigationSeekStepSpinBox_->setRange(1, 60);
    mouseNavigationSeekStepSpinBox_->setSuffix(QStringLiteral(" s"));
    mouseLayout->addRow(uiText("Side buttons seek step"), mouseNavigationSeekStepSpinBox_);

    const auto populateButtonActionCombo = [this](QComboBox *comboBox, const bool includeReloadFolderAction) {
        if (comboBox == nullptr) {
            return;
        }

        comboBox->addItem(uiText("Play / Pause"), QStringLiteral("play_pause"));
        comboBox->addItem(uiText("Fullscreen"), QStringLiteral("fullscreen"));
        comboBox->addItem(uiText("Mute"), QStringLiteral("mute"));
        comboBox->addItem(uiText("Toggle Subtitles"), QStringLiteral("subtitles"));
        comboBox->addItem(uiText("Toggle Playlist"), QStringLiteral("playlist"));
        comboBox->addItem(uiText("Toggle Details"), QStringLiteral("details"));
        comboBox->addItem(uiText("Previous Playlist Item"), QStringLiteral("previous_playlist"));
        comboBox->addItem(uiText("Next Playlist Item"), QStringLiteral("next_playlist"));
        comboBox->addItem(uiText("Seek Backward"), QStringLiteral("seek_backward_short"));
        comboBox->addItem(uiText("Seek Forward"), QStringLiteral("seek_forward_short"));
        comboBox->addItem(uiText("Zoom In"), QStringLiteral("zoom_in"));
        comboBox->addItem(uiText("Zoom Out"), QStringLiteral("zoom_out"));
        comboBox->addItem(uiText("Reset Zoom"), QStringLiteral("zoom_reset"));
        if (includeReloadFolderAction) {
            comboBox->addItem(uiText("Reload Current Folder Playlist"), QStringLiteral("reload_folder_playlist"));
        }
        comboBox->addItem(uiText("Disabled"), QStringLiteral("none"));
    };

    clickActionComboBox_ = new QComboBox(mouseGroup);
    populateButtonActionCombo(clickActionComboBox_, false);
    mouseLayout->addRow(uiText("Click action"), clickActionComboBox_);

    doubleClickActionComboBox_ = new QComboBox(mouseGroup);
    populateButtonActionCombo(doubleClickActionComboBox_, true);
    mouseLayout->addRow(uiText("Double-click action"), doubleClickActionComboBox_);

    doubleClickFullscreenCheckBox_ = new QCheckBox(uiText("Enable double-click on video to toggle fullscreen"), mouseGroup);
    mouseLayout->addRow(uiText("Double-click fullscreen"), doubleClickFullscreenCheckBox_);

    middleClickActionComboBox_ = new QComboBox(mouseGroup);
    populateButtonActionCombo(middleClickActionComboBox_, false);
    mouseLayout->addRow(uiText("Middle-click action"), middleClickActionComboBox_);

    const auto populateGestureActionCombo = [this](QComboBox *comboBox) {
        if (comboBox == nullptr) {
            return;
        }
        comboBox->addItem(uiText("Seek Backward"), QStringLiteral("seek_backward_short"));
        comboBox->addItem(uiText("Seek Forward"), QStringLiteral("seek_forward_short"));
        comboBox->addItem(uiText("Volume Up"), QStringLiteral("volume_up"));
        comboBox->addItem(uiText("Volume Down"), QStringLiteral("volume_down"));
        comboBox->addItem(uiText("Speed Up"), QStringLiteral("speed_up"));
        comboBox->addItem(uiText("Speed Down"), QStringLiteral("speed_down"));
        comboBox->addItem(uiText("Show Playlist"), QStringLiteral("playlist"));
        comboBox->addItem(uiText("Show Details"), QStringLiteral("details"));
        comboBox->addItem(uiText("Play / Pause"), QStringLiteral("play_pause"));
        comboBox->addItem(uiText("Subtitle Delay +"), QStringLiteral("subtitle_delay_up"));
        comboBox->addItem(uiText("Subtitle Delay -"), QStringLiteral("subtitle_delay_down"));
        comboBox->addItem(uiText("Disabled"), QStringLiteral("none"));
    };

    const auto populateMouseZoneActionCombo = [this](QComboBox *comboBox) {
        if (comboBox == nullptr) {
            return;
        }
        comboBox->addItem(uiText("Do Nothing"), QStringLiteral("none"));
        comboBox->addItem(uiText("Show Playlist Panel"), QStringLiteral("playlist"));
        comboBox->addItem(uiText("Show Details Panel"), QStringLiteral("details"));
        comboBox->addItem(uiText("Show Controls"), QStringLiteral("controls"));
        comboBox->addItem(uiText("Show Top Bar"), QStringLiteral("top_bar"));
        comboBox->addItem(uiText("Show Home Dashboard"), QStringLiteral("dashboard"));
    };

    auto *gesturesGroup = new QGroupBox(uiText("Mouse Gestures"), advancedPage);
    auto *gesturesLayout = new QFormLayout(gesturesGroup);
    mouseGesturesCheckBox_ = new QCheckBox(uiText("Enable right-drag mouse gestures on top of the video"), gesturesGroup);
    gesturesLayout->addRow(uiText("Availability"), mouseGesturesCheckBox_);
    mouseGestureThresholdSpinBox_ = new QSpinBox(gesturesGroup);
    mouseGestureThresholdSpinBox_->setRange(24, 220);
    mouseGestureThresholdSpinBox_->setSuffix(QStringLiteral(" px"));
    gesturesLayout->addRow(uiText("Gesture threshold"), mouseGestureThresholdSpinBox_);
    gestureLeftActionComboBox_ = new QComboBox(gesturesGroup);
    gestureRightActionComboBox_ = new QComboBox(gesturesGroup);
    gestureUpActionComboBox_ = new QComboBox(gesturesGroup);
    gestureDownActionComboBox_ = new QComboBox(gesturesGroup);
    populateGestureActionCombo(gestureLeftActionComboBox_);
    populateGestureActionCombo(gestureRightActionComboBox_);
    populateGestureActionCombo(gestureUpActionComboBox_);
    populateGestureActionCombo(gestureDownActionComboBox_);
    gesturesLayout->addRow(uiText("Gesture Left"), gestureLeftActionComboBox_);
    gesturesLayout->addRow(uiText("Gesture Right"), gestureRightActionComboBox_);
    gesturesLayout->addRow(uiText("Gesture Up"), gestureUpActionComboBox_);
    gesturesLayout->addRow(uiText("Gesture Down"), gestureDownActionComboBox_);
    gesturesGroup->setVisible(false);

    auto *fullscreenGroup = new QGroupBox(uiText("Fullscreen"), advancedPage);
    auto *fullscreenLayout = new QFormLayout(fullscreenGroup);
    fullscreenAutoHideCheckBox_ = new QCheckBox(uiText("Hide menu, panels, and controls automatically in fullscreen"), fullscreenGroup);
    fullscreenLayout->addRow(fullscreenAutoHideCheckBox_);

    fullscreenRevealMarginSpinBox_ = new QSpinBox(fullscreenGroup);
    fullscreenRevealMarginSpinBox_->setRange(16, 240);
    fullscreenRevealMarginSpinBox_->setSuffix(QStringLiteral(" px"));
    fullscreenLayout->addRow(uiText("Reveal edge margin"), fullscreenRevealMarginSpinBox_);
    setFormRowVisible(fullscreenLayout, fullscreenRevealMarginSpinBox_, false);

    fullscreenEdgePanelsCheckBox_ = new QCheckBox(
        uiText("Open side panels when the pointer reaches the right edge in fullscreen"),
        fullscreenGroup);
    fullscreenLayout->addRow(fullscreenEdgePanelsCheckBox_);

    fullscreenSideSelectorCheckBox_ = new QCheckBox(
        uiText("Show the right-edge quick selector in fullscreen"),
        fullscreenGroup);
    fullscreenLayout->addRow(fullscreenSideSelectorCheckBox_);

    auto *pointerGroup = new QGroupBox(uiText("Pointer Edge Behavior"), advancedPage);
    auto *pointerLayout = new QFormLayout(pointerGroup);
    windowedEdgePanelsCheckBox_ = new QCheckBox(
        uiText("Open side panels when the pointer reaches the right edge while windowed"),
        pointerGroup);
    pointerRightEdgeFullscreenActionComboBox_ = new QComboBox(pointerGroup);
    pointerRightEdgeWindowedActionComboBox_ = new QComboBox(pointerGroup);
    pointerRightEdgeLeaveActionComboBox_ = new QComboBox(pointerGroup);
    defaultSidePanelComboBox_ = new QComboBox(pointerGroup);
    defaultSidePanelComboBox_->addItem(uiText("Show Last Opened Panel"), QStringLiteral("last_opened"));
    defaultSidePanelComboBox_->addItem(uiText("Always Show Playlist Panel"), QStringLiteral("playlist"));
    defaultSidePanelComboBox_->addItem(uiText("Always Show Details Panel"), QStringLiteral("details"));
    const auto populatePointerEdgeActionCombo = [this](QComboBox *comboBox, const bool includeDefault) {
        if (comboBox == nullptr) {
            return;
        }
        if (includeDefault) {
            comboBox->addItem(uiText("Show Default Panel"), QStringLiteral("default"));
        }
        comboBox->addItem(uiText("Show Playlist Panel"), QStringLiteral("playlist"));
        comboBox->addItem(uiText("Show Details Panel"), QStringLiteral("details"));
        comboBox->addItem(uiText("Do Nothing"), QStringLiteral("none"));
    };
    populatePointerEdgeActionCombo(pointerRightEdgeFullscreenActionComboBox_, true);
    populatePointerEdgeActionCombo(pointerRightEdgeWindowedActionComboBox_, true);

    pointerRightEdgeLeaveActionComboBox_->addItem(uiText("Keep Panels Open"), QStringLiteral("keep"));
    pointerRightEdgeLeaveActionComboBox_->addItem(uiText("Hide Side Panels"), QStringLiteral("hide_panel"));
    pointerRightEdgeLeaveActionComboBox_->addItem(uiText("Hide Panels and Chrome"), QStringLiteral("hide_all"));

    pointerRightEdgeMarginSpinBox_ = new QSpinBox(pointerGroup);
    pointerRightEdgeMarginSpinBox_->setRange(8, 220);
    pointerRightEdgeMarginSpinBox_->setSuffix(QStringLiteral(" px"));
    pointerLeaveDelaySpinBox_ = new QSpinBox(pointerGroup);
    pointerLeaveDelaySpinBox_->setRange(0, 4000);
    pointerLeaveDelaySpinBox_->setSingleStep(100);
    pointerLeaveDelaySpinBox_->setSuffix(QStringLiteral(" ms"));
    pointerKeepControlsVisibleCheckBox_ = new QCheckBox(
        uiText("Keep controls visible while a side panel is open"),
        pointerGroup);

    pointerLayout->addRow(windowedEdgePanelsCheckBox_);
    pointerLayout->addRow(uiText("Right-edge action (fullscreen)"), pointerRightEdgeFullscreenActionComboBox_);
    pointerLayout->addRow(uiText("Right-edge action (windowed overlay)"), pointerRightEdgeWindowedActionComboBox_);
    pointerLayout->addRow(uiText("When right-edge reveal opens a panel"), defaultSidePanelComboBox_);
    pointerLayout->addRow(uiText("Right-edge leave action"), pointerRightEdgeLeaveActionComboBox_);
    pointerLayout->addRow(uiText("Right-edge trigger margin"), pointerRightEdgeMarginSpinBox_);
    pointerLayout->addRow(uiText("Pointer leave delay"), pointerLeaveDelaySpinBox_);
    setFormRowVisible(pointerLayout, pointerRightEdgeMarginSpinBox_, false);
    setFormRowVisible(pointerLayout, pointerLeaveDelaySpinBox_, false);
    pointerLayout->addRow(pointerKeepControlsVisibleCheckBox_);
    auto *pointerNote = new QLabel(
        uiText("When the pointer touches the right edge, you can reveal the default panel, force a specific panel, or do nothing. Leave action controls what happens after the pointer leaves the edge or exits the video."),
        pointerGroup);
    pointerNote->setWordWrap(true);
    pointerLayout->addRow(pointerNote);
    setHoverExplanation(
        defaultSidePanelComboBox_,
        uiText("Choose whether right-edge reveal always shows Playlist, always shows Details, or restores the last side panel you left open."));
    setHoverExplanation(
        windowedEdgePanelsCheckBox_,
        uiText("Enable or disable right-edge side-panel reveal while the player is not in fullscreen."));

    auto *mouseZonesGroup = new QGroupBox(uiText("Mouse Zones"), advancedPage);
    auto *mouseZonesLayout = new QFormLayout(mouseZonesGroup);
    mouseZoneTopActionComboBox_ = new QComboBox(mouseZonesGroup);
    mouseZoneBottomActionComboBox_ = new QComboBox(mouseZonesGroup);
    mouseZoneLeftActionComboBox_ = new QComboBox(mouseZonesGroup);
    mouseZoneRightActionComboBox_ = new QComboBox(mouseZonesGroup);
    mouseZoneCenterActionComboBox_ = new QComboBox(mouseZonesGroup);
    populateMouseZoneActionCombo(mouseZoneTopActionComboBox_);
    populateMouseZoneActionCombo(mouseZoneBottomActionComboBox_);
    populateMouseZoneActionCombo(mouseZoneLeftActionComboBox_);
    populateMouseZoneActionCombo(mouseZoneRightActionComboBox_);
    populateMouseZoneActionCombo(mouseZoneCenterActionComboBox_);
    mouseZonesLayout->addRow(uiText("Top zone"), mouseZoneTopActionComboBox_);
    mouseZonesLayout->addRow(uiText("Bottom zone"), mouseZoneBottomActionComboBox_);
    mouseZonesLayout->addRow(uiText("Left zone"), mouseZoneLeftActionComboBox_);
    mouseZonesLayout->addRow(uiText("Right zone"), mouseZoneRightActionComboBox_);
    mouseZonesLayout->addRow(uiText("Center zone"), mouseZoneCenterActionComboBox_);
    auto *mouseZonesNote = new QLabel(
        uiText("Zones react when the pointer enters them inside the video area. Use them for quick UI reveals without depending only on the right edge."),
        mouseZonesGroup);
    mouseZonesNote->setWordWrap(true);
    mouseZonesLayout->addRow(mouseZonesNote);

    auto *feedbackGroup = new QGroupBox(uiText("Feedback & Appearance"), playbackPage);
    auto *feedbackLayout = new QVBoxLayout(feedbackGroup);
    actionFeedbackOverlayCheckBox_ = new QCheckBox(uiText("Show on-screen feedback for volume and seek actions"), feedbackGroup);
    feedbackLayout->addWidget(actionFeedbackOverlayCheckBox_);

    auto *controlBarAppearanceGroup = new QGroupBox(uiText("Control Bar Layout"), playbackPage);
    auto *controlBarAppearanceLayout = new QFormLayout(controlBarAppearanceGroup);

    controlBarTimelineThicknessSpinBox_ = new QSpinBox(controlBarAppearanceGroup);
    controlBarTimelineThicknessSpinBox_->setRange(4, 18);
    controlBarTimelineThicknessSpinBox_->setSuffix(QStringLiteral(" px"));
    controlBarAppearanceLayout->addRow(uiText("Timeline thickness"), controlBarTimelineThicknessSpinBox_);
    setFormRowVisible(controlBarAppearanceLayout, controlBarTimelineThicknessSpinBox_, false);

    controlBarTimelineHandleSizeSpinBox_ = new QSpinBox(controlBarAppearanceGroup);
    controlBarTimelineHandleSizeSpinBox_->setRange(10, 30);
    controlBarTimelineHandleSizeSpinBox_->setSuffix(QStringLiteral(" px"));
    controlBarAppearanceLayout->addRow(uiText("Timeline handle size"), controlBarTimelineHandleSizeSpinBox_);
    setFormRowVisible(controlBarAppearanceLayout, controlBarTimelineHandleSizeSpinBox_, false);

    controlBarVolumeSliderThicknessSpinBox_ = new QSpinBox(controlBarAppearanceGroup);
    controlBarVolumeSliderThicknessSpinBox_->setRange(4, 18);
    controlBarVolumeSliderThicknessSpinBox_->setSuffix(QStringLiteral(" px"));
    controlBarAppearanceLayout->addRow(uiText("Volume slider thickness"), controlBarVolumeSliderThicknessSpinBox_);
    setFormRowVisible(controlBarAppearanceLayout, controlBarVolumeSliderThicknessSpinBox_, false);

    controlBarVolumeSliderWidthSpinBox_ = new QSpinBox(controlBarAppearanceGroup);
    controlBarVolumeSliderWidthSpinBox_->setRange(0, 320);
    controlBarVolumeSliderWidthSpinBox_->setSpecialValueText(uiText("Auto"));
    controlBarVolumeSliderWidthSpinBox_->setSuffix(QStringLiteral(" px"));
    controlBarAppearanceLayout->addRow(uiText("Volume slider width"), controlBarVolumeSliderWidthSpinBox_);
    setFormRowVisible(controlBarAppearanceLayout, controlBarVolumeSliderWidthSpinBox_, false);
    Q_UNUSED(controlBarAppearanceGroup);
    auto *controlBarVisibilityGroup = new QGroupBox(uiText("Control Bar Visibility"), playbackPage);
    auto *controlBarVisibilityLayout = new QVBoxLayout(controlBarVisibilityGroup);
    expressiveControlLabelsCheckBox_ = new QCheckBox(uiText("Use compact icon-only labels on the playback bar"), controlBarVisibilityGroup);
    controlBarShowOpenButtonCheckBox_ = new QCheckBox(uiText("Show Open button"), controlBarVisibilityGroup);
    controlBarShowStopButtonCheckBox_ = new QCheckBox(uiText("Show Stop button"), controlBarVisibilityGroup);
    controlBarShowPlaylistButtonCheckBox_ = new QCheckBox(uiText("Show Playlist button"), controlBarVisibilityGroup);
    controlBarShowDetailsButtonCheckBox_ = new QCheckBox(uiText("Show Details button"), controlBarVisibilityGroup);
    controlBarShowTimeLabelCheckBox_ = new QCheckBox(uiText("Show current time and duration"), controlBarVisibilityGroup);
    controlBarShowSpeedButtonCheckBox_ = new QCheckBox(uiText("Show speed control"), controlBarVisibilityGroup);
    controlBarShowRepeatLoopButtonsCheckBox_ = new QCheckBox(uiText("Show repeat and A-B loop controls"), controlBarVisibilityGroup);
    controlBarShowTrackMenusCheckBox_ = new QCheckBox(uiText("Show quality and subtitle menus"), controlBarVisibilityGroup);
    controlBarShowVolumeControlsCheckBox_ = new QCheckBox(uiText("Show volume controls"), controlBarVisibilityGroup);
    controlBarShowFullscreenButtonCheckBox_ = new QCheckBox(uiText("Show fullscreen button"), controlBarVisibilityGroup);
    for (QCheckBox *checkBox : {expressiveControlLabelsCheckBox_,
                                controlBarShowOpenButtonCheckBox_,
                                controlBarShowStopButtonCheckBox_,
                                controlBarShowPlaylistButtonCheckBox_,
                                controlBarShowDetailsButtonCheckBox_,
                                controlBarShowTimeLabelCheckBox_,
                                controlBarShowSpeedButtonCheckBox_,
                                controlBarShowRepeatLoopButtonsCheckBox_,
                                controlBarShowTrackMenusCheckBox_,
                                controlBarShowVolumeControlsCheckBox_,
                                controlBarShowFullscreenButtonCheckBox_}) {
        controlBarVisibilityLayout->addWidget(checkBox);
    }
    auto *thumbnailsGroup = new QGroupBox(uiText("Preview Thumbnails"), advancedPage);
    auto *thumbnailsLayout = new QFormLayout(thumbnailsGroup);
    thumbnailPreviewsCheckBox_ = new QCheckBox(uiText("Enable timeline preview thumbnails"), thumbnailsGroup);
    thumbnailsLayout->addRow(uiText("Availability"), thumbnailPreviewsCheckBox_);

    thumbnailPreviewSizeComboBox_ = new QComboBox(thumbnailsGroup);
    for (const auto &preset : kThumbnailPreviewSizePresets) {
        thumbnailPreviewSizeComboBox_->addItem(uiText(preset.label), QString::fromLatin1(preset.id));
    }
    setHoverExplanation(
        thumbnailPreviewSizeComboBox_,
        uiText("Choose a fixed preview size for the timeline thumbnail popup."));
    thumbnailsLayout->addRow(uiText("Timeline preview size"), thumbnailPreviewSizeComboBox_);

    thumbnailWidthSpinBox_ = new QSpinBox(thumbnailsGroup);
    thumbnailWidthSpinBox_->setRange(0, 2200);
    thumbnailWidthSpinBox_->setSingleStep(16);
    thumbnailWidthSpinBox_->setSpecialValueText(uiText("Auto"));
    thumbnailWidthSpinBox_->setSuffix(QStringLiteral(" px"));
    thumbnailWidthSpinBox_->hide();

    thumbnailPopupWidthSpinBox_ = new QSpinBox(thumbnailsGroup);
    thumbnailPopupWidthSpinBox_->setRange(0, 1800);
    thumbnailPopupWidthSpinBox_->setSingleStep(16);
    thumbnailPopupWidthSpinBox_->setSpecialValueText(uiText("Auto"));
    thumbnailPopupWidthSpinBox_->setSuffix(QStringLiteral(" px"));
    thumbnailPopupWidthSpinBox_->hide();

    thumbnailPopupOffsetSpinBox_ = new QSpinBox(thumbnailsGroup);
    thumbnailPopupOffsetSpinBox_->setRange(8, 56);
    thumbnailPopupOffsetSpinBox_->setSuffix(QStringLiteral(" px"));
    thumbnailsLayout->addRow(uiText("Timeline popup vertical offset"), thumbnailPopupOffsetSpinBox_);
    setFormRowVisible(thumbnailsLayout, thumbnailPopupOffsetSpinBox_, false);

    thumbnailPopupPaddingSpinBox_ = new QSpinBox(thumbnailsGroup);
    thumbnailPopupPaddingSpinBox_->setRange(0, 32);
    thumbnailPopupPaddingSpinBox_->setSuffix(QStringLiteral(" px"));
    thumbnailsLayout->addRow(uiText("Timeline popup screen padding"), thumbnailPopupPaddingSpinBox_);
    setFormRowVisible(thumbnailsLayout, thumbnailPopupPaddingSpinBox_, false);

    auto *sceneBrowserGroup = new QGroupBox(uiText("Scene Browser"), advancedPage);
    auto *sceneBrowserLayout = new QFormLayout(sceneBrowserGroup);
    sceneBrowserStepSpinBox_ = new QSpinBox(sceneBrowserGroup);
    sceneBrowserStepSpinBox_->setRange(1, 600);
    sceneBrowserStepSpinBox_->setSingleStep(1);
    sceneBrowserStepSpinBox_->setSuffix(QStringLiteral(" s"));
    sceneBrowserLayout->addRow(uiText("Default scene interval"), sceneBrowserStepSpinBox_);
    sceneBrowserMaxItemsSpinBox_ = new QSpinBox(sceneBrowserGroup);
    sceneBrowserMaxItemsSpinBox_->setRange(8, 72);
    sceneBrowserMaxItemsSpinBox_->setSingleStep(4);
    sceneBrowserLayout->addRow(uiText("Maximum generated scenes"), sceneBrowserMaxItemsSpinBox_);

    auto *playlistGroup = new QGroupBox(uiText("Playlist"), playbackPage);
    auto *playlistLayout = new QVBoxLayout(playlistGroup);
    autoLoadSiblingMediaCheckBox_ = new QCheckBox(uiText("Automatically load media from the same folder when opening a single local file"), playlistGroup);
    showPlaylistPanelOnFolderLoadCheckBox_ = new QCheckBox(uiText("Show the playlist panel when a folder playlist is created"), playlistGroup);
    rotateFolderPlaylistToCurrentCheckBox_ = new QCheckBox(uiText("Move the currently opened file to the top when building a folder playlist"), playlistGroup);
    playlistShowFullPathsCheckBox_ = new QCheckBox(uiText("Show full file paths in the playlist instead of short titles"), playlistGroup);
    playlistShowIndexPrefixesCheckBox_ = new QCheckBox(uiText("Show numeric prefixes and current-item markers in the playlist"), playlistGroup);
    playlistAutoFollowCurrentCheckBox_ = new QCheckBox(uiText("Auto-follow the currently playing entry inside the playlist"), playlistGroup);
    playlistLayout->addWidget(autoLoadSiblingMediaCheckBox_);
    playlistLayout->addWidget(showPlaylistPanelOnFolderLoadCheckBox_);
    playlistLayout->addWidget(rotateFolderPlaylistToCurrentCheckBox_);

    auto *progressGroup = new QGroupBox(uiText("Progress Tracking"), playbackPage);
    auto *progressLayout = new QFormLayout(progressGroup);
    progressTrackingModeCheckBox_ = new QCheckBox(
        uiText("Track per-video progress and saved-list completion inside the playlist"),
        progressGroup);
    progressBadgesCheckBox_ = new QCheckBox(
        uiText("Show progress badges like Done, Fav, Saved, custom categories, and subtitle availability"),
        progressGroup);
    progressCompletionThresholdSpinBox_ = new QSpinBox(progressGroup);
    progressCompletionThresholdSpinBox_->setRange(60, 100);
    progressCompletionThresholdSpinBox_->setSuffix(QStringLiteral("%"));
    progressLayout->addRow(progressTrackingModeCheckBox_);
    progressLayout->addRow(progressBadgesCheckBox_);
    progressLayout->addRow(uiText("Mark item completed at"), progressCompletionThresholdSpinBox_);
    auto *progressNote = new QLabel(
        uiText("This mode is ideal for long-running series or any saved folder. The playlist shows per-item progress and saved list tabs can surface overall completion."),
        progressGroup);
    progressNote->setWordWrap(true);
    progressLayout->addRow(progressNote);
    playlistLayout->addWidget(playlistShowFullPathsCheckBox_);
    playlistLayout->addWidget(playlistShowIndexPrefixesCheckBox_);
    playlistLayout->addWidget(playlistAutoFollowCurrentCheckBox_);

    connect(thumbnailPreviewSizeComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int) {
        if (thumbnailPreviewSizeComboBox_ == nullptr
            || thumbnailWidthSpinBox_ == nullptr
            || thumbnailPopupWidthSpinBox_ == nullptr) {
            return;
        }

        const auto preset = thumbnailPreviewSizePresetForId(thumbnailPreviewSizeComboBox_->currentData().toString());
        thumbnailWidthSpinBox_->setValue(preset.cachedFrameWidth);
        thumbnailPopupWidthSpinBox_->setValue(preset.popupWidth);
    });
    connect(thumbnailPreviewsCheckBox_, &QCheckBox::toggled, thumbnailWidthSpinBox_, &QWidget::setEnabled);
    connect(thumbnailPreviewsCheckBox_, &QCheckBox::toggled, thumbnailPopupWidthSpinBox_, &QWidget::setEnabled);
    connect(thumbnailPreviewsCheckBox_, &QCheckBox::toggled, thumbnailPreviewSizeComboBox_, &QWidget::setEnabled);
    connect(thumbnailPreviewsCheckBox_, &QCheckBox::toggled, thumbnailPopupOffsetSpinBox_, &QWidget::setEnabled);
    connect(thumbnailPreviewsCheckBox_, &QCheckBox::toggled, thumbnailPopupPaddingSpinBox_, &QWidget::setEnabled);
    connect(mouseWheelActionComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int) {
        const QString actionId = mouseWheelActionComboBox_->currentData().toString();
        mouseWheelVolumeStepSpinBox_->setEnabled(actionId == QStringLiteral("volume"));
        mouseWheelSeekStepSpinBox_->setEnabled(actionId == QStringLiteral("seek"));
        mouseWheelVolumeCheckBox_->setChecked(actionId == QStringLiteral("volume"));
    });
    connect(mouseWheelVolumeCheckBox_, &QCheckBox::toggled, this, [this](const bool checked) {
        if (checked) {
            mouseWheelActionComboBox_->setCurrentIndex(mouseWheelActionComboBox_->findData(QStringLiteral("volume")));
        } else if (mouseWheelActionComboBox_->currentData().toString() == QStringLiteral("volume")) {
            mouseWheelActionComboBox_->setCurrentIndex(mouseWheelActionComboBox_->findData(QStringLiteral("none")));
        }
    });
    connect(mouseSideButtonsActionComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int) {
        const QString actionId = mouseSideButtonsActionComboBox_->currentData().toString();
        const bool seekAction = actionId.startsWith(QStringLiteral("seek"));
        mouseNavigationSeekStepSpinBox_->setEnabled(seekAction);
        mouseNavigationSeekCheckBox_->setChecked(seekAction);
    });
    connect(mouseNavigationSeekCheckBox_, &QCheckBox::toggled, this, [this](const bool checked) {
        if (checked) {
            mouseSideButtonsActionComboBox_->setCurrentIndex(mouseSideButtonsActionComboBox_->findData(QStringLiteral("seek_short")));
        } else if (mouseSideButtonsActionComboBox_->currentData().toString().startsWith(QStringLiteral("seek"))) {
            mouseSideButtonsActionComboBox_->setCurrentIndex(mouseSideButtonsActionComboBox_->findData(QStringLiteral("none")));
        }
    });
    connect(doubleClickActionComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int) {
        doubleClickFullscreenCheckBox_->setChecked(
            doubleClickActionComboBox_->currentData().toString() == QStringLiteral("fullscreen"));
    });
    connect(doubleClickFullscreenCheckBox_, &QCheckBox::toggled, this, [this](const bool checked) {
        if (checked) {
            doubleClickActionComboBox_->setCurrentIndex(doubleClickActionComboBox_->findData(QStringLiteral("fullscreen")));
        } else if (doubleClickActionComboBox_->currentData().toString() == QStringLiteral("fullscreen")) {
            doubleClickActionComboBox_->setCurrentIndex(doubleClickActionComboBox_->findData(QStringLiteral("none")));
        }
    });
    connect(mouseGesturesCheckBox_, &QCheckBox::toggled, this, [this](const bool checked) {
        mouseGestureThresholdSpinBox_->setEnabled(checked);
        gestureLeftActionComboBox_->setEnabled(checked);
        gestureRightActionComboBox_->setEnabled(checked);
        gestureUpActionComboBox_->setEnabled(checked);
        gestureDownActionComboBox_->setEnabled(checked);
    });
    const auto syncDashboardSectionControls = [this](const bool enabled) {
        if (dashboardShowContinueCheckBox_ != nullptr) {
            dashboardShowContinueCheckBox_->setEnabled(enabled);
        }
        if (dashboardShowRecentCheckBox_ != nullptr) {
            dashboardShowRecentCheckBox_->setEnabled(enabled);
        }
        if (dashboardShowFavoritesCheckBox_ != nullptr) {
            dashboardShowFavoritesCheckBox_->setEnabled(enabled);
        }
        if (dashboardShowSavedListsCheckBox_ != nullptr) {
            dashboardShowSavedListsCheckBox_->setEnabled(enabled);
        }
    };
    connect(dashboardEnabledCheckBox_, &QCheckBox::toggled, this, syncDashboardSectionControls);
    connect(adaptiveUiCheckBox_, &QCheckBox::toggled, adaptiveUiBreakpointSpinBox_, &QWidget::setEnabled);
    connect(progressTrackingModeCheckBox_, &QCheckBox::toggled, progressCompletionThresholdSpinBox_, &QWidget::setEnabled);
    connect(progressTrackingModeCheckBox_, &QCheckBox::toggled, progressBadgesCheckBox_, &QWidget::setEnabled);
    connect(fullscreenAutoHideCheckBox_, &QCheckBox::toggled, fullscreenRevealMarginSpinBox_, &QWidget::setEnabled);
    connect(fullscreenEdgePanelsCheckBox_, &QCheckBox::toggled, fullscreenSideSelectorCheckBox_, &QWidget::setEnabled);
    connect(windowedEdgePanelsCheckBox_, &QCheckBox::toggled, pointerRightEdgeWindowedActionComboBox_, &QWidget::setEnabled);
    connect(autoLoadSiblingMediaCheckBox_, &QCheckBox::toggled, showPlaylistPanelOnFolderLoadCheckBox_, &QWidget::setEnabled);
    connect(autoLoadSiblingMediaCheckBox_, &QCheckBox::toggled, rotateFolderPlaylistToCurrentCheckBox_, &QWidget::setEnabled);
    connect(rememberWindowStateCheckBox_, &QCheckBox::toggled, this, [this](const bool checked) {
        if (restoreSidePanelsFromWindowStateCheckBox_ != nullptr) {
            restoreSidePanelsFromWindowStateCheckBox_->setEnabled(checked);
        }
    });
    connect(rememberHistoryCheckBox_, &QCheckBox::toggled, historyLimitSpinBox_, &QWidget::setEnabled);
    connect(rememberHistoryCheckBox_, &QCheckBox::toggled, clearHistoryOnExitCheckBox_, &QWidget::setEnabled);
    connect(rememberHistoryCheckBox_, &QCheckBox::toggled, clearHistoryButton_, &QWidget::setEnabled);
    connect(clearHistoryButton_, &QPushButton::clicked, this, [this]() {
        if (historyController_ == nullptr || !historyController_->isReady()) {
            QMessageBox::information(this, uiText("History"), uiText("History storage is unavailable"));
            return;
        }

        if (QMessageBox::question(
                this,
                uiText("Clear History"),
                uiText("Remove all recent playback history entries?"))
            != QMessageBox::Yes) {
            return;
        }

        if (!historyController_->clearHistory()) {
            QMessageBox::warning(this, uiText("History"), uiText("Failed to clear playback history"));
            return;
        }

        QMessageBox::information(this, uiText("History"), uiText("Playback history cleared"));
    });

    auto *playbackNote = new QLabel(
        uiText("The profile changes preview generation only; it does not lower video playback quality or disable subtitles. Startup volume, speed, repeat mode, and interaction steps are stored locally and reused on the next launch."),
        playbackPage);
    playbackNote->setWordWrap(true);

    auto *captureGroup = new QGroupBox(uiText("Screenshots"), playbackPage);
    auto *captureLayout = new QFormLayout(captureGroup);
    auto *directoryRow = new QWidget(captureGroup);
    auto *directoryLayout = new QHBoxLayout(directoryRow);
    directoryLayout->setContentsMargins(0, 0, 0, 0);
    directoryLayout->setSpacing(8);

    screenshotDirectoryEdit_ = new QLineEdit(directoryRow);
    screenshotDirectoryEdit_->setPlaceholderText(QStringLiteral("Pictures/Reva Player"));
    auto *browseButton = new QPushButton(uiText("Browse"), directoryRow);
    directoryLayout->addWidget(screenshotDirectoryEdit_, 1);
    directoryLayout->addWidget(browseButton, 0);
    captureLayout->addRow(uiText("Save screenshots to"), directoryRow);

    screenshotFormatComboBox_ = new QComboBox(captureGroup);
    screenshotFormatComboBox_->addItem(QStringLiteral("PNG"), QStringLiteral("png"));
    screenshotFormatComboBox_->addItem(QStringLiteral("JPG"), QStringLiteral("jpg"));
    captureLayout->addRow(uiText("Default format"), screenshotFormatComboBox_);

    screenshotTemplateEdit_ = new QLineEdit(captureGroup);
    screenshotTemplateEdit_->setPlaceholderText(QStringLiteral("{timestamp}-{title}-{index}"));
    captureLayout->addRow(uiText("Filename template"), screenshotTemplateEdit_);
    auto *captureNote = new QLabel(
        uiText("Single screenshots are saved to the selected folder using the chosen format and filename template. You can use {timestamp}, {title}, and {index} inside the name, and the quick capture shortcut is Ctrl+Shift+S."),
        captureGroup);
    captureNote->setWordWrap(true);
    captureLayout->addRow(QString {}, captureNote);

    connect(browseButton, &QPushButton::clicked, this, [this]() {
        const QString initialDirectory = screenshotDirectoryEdit_->text().trimmed();
        const QString selectedDirectory = filedialog::getExistingDirectory(
            this,
            uiText("Choose Screenshot Directory"),
            initialDirectory);
        if (!selectedDirectory.trimmed().isEmpty()) {
            screenshotDirectoryEdit_->setText(selectedDirectory);
        }
    });

    playbackLayout->addWidget(volumeGroup);
    playbackLayout->addWidget(interactionGroup);
    playbackLayout->addWidget(feedbackGroup);
    playbackLayout->addWidget(captureGroup);
    playbackLayout->addWidget(playbackNote);
    playbackLayout->addStretch(1);

    advancedLayout->addWidget(historyGroup);
    advancedLayout->addWidget(layoutGroup);
    advancedLayout->addWidget(mouseGroup);
    gesturesGroup->setVisible(false);
    advancedLayout->addWidget(fullscreenGroup);
    advancedLayout->addWidget(pointerGroup);
    advancedLayout->addWidget(mouseZonesGroup);
    advancedLayout->addWidget(thumbnailsGroup);
    advancedLayout->addWidget(sceneBrowserGroup);
    advancedLayout->addStretch(1);

    auto *subtitleSectionPage = new QWidget();
    subtitleSectionPage->setObjectName(QStringLiteral("settingsSectionPage"));
    auto *subtitleLayout = new QVBoxLayout(subtitleSectionPage);
    subtitleLayout->setContentsMargins(18, 18, 18, 18);
    subtitleLayout->setSpacing(14);

    auto *subtitleBehaviorGroup = new QGroupBox(uiText("Basic Controls"), subtitleSectionPage);
    subtitleBehaviorGroup->setObjectName(QStringLiteral("subtitleBasicGroup"));
    auto *subtitleBehaviorLayout = new QFormLayout(subtitleBehaviorGroup);
    subtitleBehaviorLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    subtitleBehaviorLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    subtitleVisibleCheckBox_ = new QCheckBox(uiText("Show subtitles by default"), subtitleBehaviorGroup);
    setHoverExplanation(subtitleVisibleCheckBox_, uiText("Enable or disable subtitle visibility. The live preview updates immediately."));
    subtitleBehaviorLayout->addRow(uiText("Visibility"), subtitleVisibleCheckBox_);

    subtitleAutoLoadModeComboBox_ = new QComboBox(subtitleBehaviorGroup);
    subtitleAutoLoadModeComboBox_->setObjectName(QStringLiteral("subtitleAutoLoadModeComboBox"));
    for (const auto &option : revaplayer::application::subtitleAutoLoadModeOptions()) {
        subtitleAutoLoadModeComboBox_->addItem(revaplayer::application::translateUiText(option.label), option.id);
    }
    setHoverExplanation(
        subtitleAutoLoadModeComboBox_,
        uiText("Control how Reva Player scans the current folder for external subtitle files before playback selects a track."));
    subtitleBehaviorLayout->addRow(uiText("External subtitle loading"), subtitleAutoLoadModeComboBox_);

    subtitleFontComboBox_ = new QFontComboBox(subtitleBehaviorGroup);
    setHoverExplanation(subtitleFontComboBox_, uiText("Pick the font family that will be used for subtitle preview and playback."));
    subtitleBehaviorLayout->addRow(uiText("Font family"), subtitleFontComboBox_);

    subtitleFontSizeSpinBox_ = new QSpinBox(subtitleBehaviorGroup);
    subtitleFontSizeSpinBox_->setRange(8, 144);
    subtitleFontSizeSpinBox_->setSingleStep(2);
    subtitleFontSizeSpinBox_->setSuffix(QStringLiteral(" px"));
    setHoverExplanation(subtitleFontSizeSpinBox_, uiText("Adjust subtitle font size in pixels and preview the result instantly."));
    subtitleBehaviorLayout->addRow(uiText("Font size"), subtitleFontSizeSpinBox_);

    subtitleTextColorButton_ = new QPushButton(subtitleBehaviorGroup);
    subtitleBehaviorLayout->addRow(uiText("Text color"), subtitleTextColorButton_);

    subtitleOutlineColorButton_ = new QPushButton(subtitleBehaviorGroup);
    subtitleBehaviorLayout->addRow(uiText("Outline color"), subtitleOutlineColorButton_);

    subtitleBackgroundEnabledCheckBox_ = new QCheckBox(uiText("Enable background box"), subtitleBehaviorGroup);
    subtitleBehaviorLayout->addRow(subtitleBackgroundEnabledCheckBox_);

    subtitleBackgroundColorButton_ = new QPushButton(subtitleBehaviorGroup);
    subtitleBehaviorLayout->addRow(uiText("Background color"), subtitleBackgroundColorButton_);

    subtitlePositionSpinBox_ = new QSpinBox(subtitleBehaviorGroup);
    subtitlePositionSpinBox_->setRange(0, 150);
    subtitlePositionSpinBox_->setSuffix(QStringLiteral("%"));
    setHoverExplanation(subtitlePositionSpinBox_, uiText("Move subtitles higher or lower on the frame while the preview updates live."));
    subtitleBehaviorLayout->addRow(uiText("Vertical position"), subtitlePositionSpinBox_);

    resetSubtitleStyleQuickButton_ = new QPushButton(uiText("Reset Subtitle Style"), subtitleBehaviorGroup);
    resetSubtitleStyleQuickButton_->setVisible(false);

    auto *subtitleAdvancedContainer = new QWidget(subtitleSectionPage);
    subtitleAdvancedContainer->setObjectName(QStringLiteral("subtitleAdvancedContainer"));
    auto *subtitleAdvancedContainerLayout = new QVBoxLayout(subtitleAdvancedContainer);
    subtitleAdvancedContainerLayout->setContentsMargins(0, 0, 0, 0);
    subtitleAdvancedContainerLayout->setSpacing(10);

    auto *subtitleAutomationGroup = new QGroupBox(uiText("Advanced Playback Rules"), subtitleAdvancedContainer);
    subtitleAutomationGroup->setObjectName(QStringLiteral("subtitleAdvancedRulesGroup"));
    auto *subtitleAutomationLayout = new QFormLayout(subtitleAutomationGroup);
    subtitleAutomationLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    subtitleAutomationLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);

    subtitleAutoSelectCheckBox_ = new QCheckBox(uiText("Automatically select the best subtitle track"), subtitleAutomationGroup);
    subtitlePreferExternalCheckBox_ = new QCheckBox(uiText("Prefer external subtitle tracks when available"), subtitleAutomationGroup);
    subtitleAutoLoadLocalMatchesCheckBox_ = new QCheckBox(uiText("Enable auto-load subtitles from media folder"), subtitleAutomationGroup);
    subtitleAutoLoadLocalMatchesCheckBox_->setObjectName(QStringLiteral("subtitleAutoLoadLocalMatchesCheckBox"));
    subtitleAutoSelectCheckBox_->setVisible(false);
    subtitlePreferExternalCheckBox_->setVisible(false);
    subtitleAutomationLayout->addRow(subtitleAutoSelectCheckBox_);
    subtitleAutomationLayout->addRow(subtitlePreferExternalCheckBox_);
    subtitleAutomationLayout->addRow(subtitleAutoLoadLocalMatchesCheckBox_);

    subtitlePreferredLanguagesEdit_ = new QLineEdit(subtitleAutomationGroup);
    subtitlePreferredLanguagesEdit_->setPlaceholderText(QStringLiteral("ar,en,ja"));
    subtitlePreferredLanguagesEdit_->setVisible(false);
    subtitleAutomationLayout->addRow(uiText("Preferred languages"), subtitlePreferredLanguagesEdit_);
    if (QWidget *preferredLanguagesLabel = subtitleAutomationLayout->labelForField(subtitlePreferredLanguagesEdit_);
        preferredLanguagesLabel != nullptr) {
        preferredLanguagesLabel->setVisible(false);
    }

    subtitleRememberTrackChoiceCheckBox_ = new QCheckBox(uiText("Remember the last subtitle track choice for each media file"), subtitleAutomationGroup);
    subtitleAutomationLayout->addRow(subtitleRememberTrackChoiceCheckBox_);

    subtitleEncodingComboBox_ = new QComboBox(subtitleAutomationGroup);
    for (const auto &option : revaplayer::application::subtitleEncodingOptions()) {
        subtitleEncodingComboBox_->addItem(revaplayer::application::translateUiText(option.label), option.id);
    }
    subtitleAutomationLayout->addRow(uiText("Character encoding"), subtitleEncodingComboBox_);

    subtitleFixTimingCheckBox_ = new QCheckBox(uiText("Use timing repair for subtitles that drift or contain bad timestamps"), subtitleAutomationGroup);
    subtitleAutomationLayout->addRow(subtitleFixTimingCheckBox_);

    subtitleSyncSmallStepSpinBox_ = new QDoubleSpinBox(subtitleAutomationGroup);
    subtitleSyncSmallStepSpinBox_->setRange(0.05, 10.0);
    subtitleSyncSmallStepSpinBox_->setDecimals(2);
    subtitleSyncSmallStepSpinBox_->setSingleStep(0.05);
    subtitleSyncSmallStepSpinBox_->setSuffix(QStringLiteral(" s"));
    subtitleAutomationLayout->addRow(uiText("Fine sync step"), subtitleSyncSmallStepSpinBox_);

    auto *subtitleStyleGroup = new QGroupBox(uiText("Advanced Style"), subtitleAdvancedContainer);
    subtitleStyleGroup->setObjectName(QStringLiteral("subtitleAdvancedStyleGroup"));
    auto *subtitleStyleLayout = new QFormLayout(subtitleStyleGroup);
    subtitleStyleLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    subtitleStyleLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);

    subtitleAssOverrideComboBox_ = new QComboBox(subtitleStyleGroup);
    for (const auto &option : revaplayer::application::subtitleAssOverrideOptions()) {
        subtitleAssOverrideComboBox_->addItem(revaplayer::application::translateUiText(option.label), option.id);
    }
    setHoverExplanation(subtitleAssOverrideComboBox_, uiText("Choose how strongly your own subtitle style should override embedded ASS styling."));
    subtitleStyleLayout->addRow(uiText("ASS / embedded style handling"), subtitleAssOverrideComboBox_);

    subtitleScaleSpinBox_ = new QDoubleSpinBox(subtitleStyleGroup);
    subtitleScaleSpinBox_->setRange(0.25, 5.0);
    subtitleScaleSpinBox_->setDecimals(2);
    subtitleScaleSpinBox_->setSingleStep(0.05);
    subtitleScaleSpinBox_->setSuffix(QStringLiteral("x"));
    subtitleStyleLayout->addRow(uiText("Scale"), subtitleScaleSpinBox_);

    subtitleFontWeightSpinBox_ = new QSpinBox(subtitleStyleGroup);
    subtitleFontWeightSpinBox_->setRange(300, 900);
    subtitleFontWeightSpinBox_->setSingleStep(100);
    subtitleStyleLayout->addRow(uiText("Font weight"), subtitleFontWeightSpinBox_);

    subtitleItalicCheckBox_ = new QCheckBox(uiText("Italic"), subtitleStyleGroup);
    subtitleStyleLayout->addRow(subtitleItalicCheckBox_);

    subtitleBorderStyleComboBox_ = new QComboBox(subtitleStyleGroup);
    for (const auto &option : revaplayer::application::subtitleBorderStyleOptions()) {
        subtitleBorderStyleComboBox_->addItem(revaplayer::application::translateUiText(option.label), option.id);
    }
    subtitleStyleLayout->addRow(uiText("Border style"), subtitleBorderStyleComboBox_);

    subtitleOutlineSizeSpinBox_ = new QDoubleSpinBox(subtitleStyleGroup);
    subtitleOutlineSizeSpinBox_->setRange(0.0, 12.0);
    subtitleOutlineSizeSpinBox_->setDecimals(2);
    subtitleOutlineSizeSpinBox_->setSingleStep(0.10);
    subtitleOutlineSizeSpinBox_->setSuffix(QStringLiteral(" px"));
    subtitleStyleLayout->addRow(uiText("Outline thickness"), subtitleOutlineSizeSpinBox_);

    subtitleShadowEnabledCheckBox_ = new QCheckBox(uiText("Enable shadow"), subtitleStyleGroup);
    subtitleStyleLayout->addRow(subtitleShadowEnabledCheckBox_);

    subtitleShadowColorButton_ = new QPushButton(subtitleStyleGroup);
    subtitleStyleLayout->addRow(uiText("Shadow color"), subtitleShadowColorButton_);

    subtitleShadowOffsetSpinBox_ = new QDoubleSpinBox(subtitleStyleGroup);
    subtitleShadowOffsetSpinBox_->setRange(0.0, 20.0);
    subtitleShadowOffsetSpinBox_->setDecimals(2);
    subtitleShadowOffsetSpinBox_->setSingleStep(0.10);
    subtitleShadowOffsetSpinBox_->setSuffix(QStringLiteral(" px"));
    subtitleStyleLayout->addRow(uiText("Shadow offset"), subtitleShadowOffsetSpinBox_);

    subtitleShadowBlurSpinBox_ = new QDoubleSpinBox(subtitleStyleGroup);
    subtitleShadowBlurSpinBox_->setRange(0.0, 3.0);
    subtitleShadowBlurSpinBox_->setDecimals(2);
    subtitleShadowBlurSpinBox_->setSingleStep(0.05);
    subtitleStyleLayout->addRow(uiText("Shadow blur"), subtitleShadowBlurSpinBox_);

    subtitleBackgroundOpacitySpinBox_ = new QSpinBox(subtitleStyleGroup);
    subtitleBackgroundOpacitySpinBox_->setRange(0, 100);
    subtitleBackgroundOpacitySpinBox_->setSuffix(QStringLiteral("%"));
    subtitleStyleLayout->addRow(uiText("Background opacity"), subtitleBackgroundOpacitySpinBox_);

    subtitleLineSpacingSpinBox_ = new QDoubleSpinBox(subtitleStyleGroup);
    subtitleLineSpacingSpinBox_->setRange(-1000.0, 1000.0);
    subtitleLineSpacingSpinBox_->setDecimals(2);
    subtitleLineSpacingSpinBox_->setSingleStep(1.0);
    subtitleStyleLayout->addRow(uiText("Line spacing"), subtitleLineSpacingSpinBox_);

    subtitleLetterSpacingSpinBox_ = new QDoubleSpinBox(subtitleStyleGroup);
    subtitleLetterSpacingSpinBox_->setRange(-10.0, 10.0);
    subtitleLetterSpacingSpinBox_->setDecimals(2);
    subtitleLetterSpacingSpinBox_->setSingleStep(0.10);
    subtitleStyleLayout->addRow(uiText("Letter spacing"), subtitleLetterSpacingSpinBox_);

    subtitleMaxWidthSpinBox_ = new QDoubleSpinBox(subtitleStyleGroup);
    subtitleMaxWidthSpinBox_->setRange(30.0, 100.0);
    subtitleMaxWidthSpinBox_->setDecimals(1);
    subtitleMaxWidthSpinBox_->setSingleStep(2.0);
    subtitleMaxWidthSpinBox_->setSuffix(QStringLiteral("%"));
    subtitleStyleLayout->addRow(uiText("Max subtitle width"), subtitleMaxWidthSpinBox_);

    subtitleAlignYComboBox_ = new QComboBox(subtitleStyleGroup);
    for (const auto &option : revaplayer::application::subtitleAlignYOptions()) {
        subtitleAlignYComboBox_->addItem(revaplayer::application::translateUiText(option.label), option.id);
    }
    subtitleStyleLayout->addRow(uiText("Frame alignment"), subtitleAlignYComboBox_);

    subtitleAlignXComboBox_ = new QComboBox(subtitleStyleGroup);
    for (const auto &option : revaplayer::application::subtitleAlignXOptions()) {
        subtitleAlignXComboBox_->addItem(revaplayer::application::translateUiText(option.label), option.id);
    }
    subtitleStyleLayout->addRow(uiText("Horizontal anchor"), subtitleAlignXComboBox_);

    subtitleJustifyComboBox_ = new QComboBox(subtitleStyleGroup);
    for (const auto &option : revaplayer::application::subtitleJustifyOptions()) {
        subtitleJustifyComboBox_->addItem(revaplayer::application::translateUiText(option.label), option.id);
    }
    subtitleStyleLayout->addRow(uiText("Text alignment"), subtitleJustifyComboBox_);

    subtitleUseMarginsCheckBox_ = new QCheckBox(uiText("Keep subtitles inside the renderer margins"), subtitleStyleGroup);
    subtitleStyleLayout->addRow(subtitleUseMarginsCheckBox_);

    subtitleMarginXSpinBox_ = new QSpinBox(subtitleStyleGroup);
    subtitleMarginXSpinBox_->setRange(0, 300);
    subtitleMarginXSpinBox_->setSuffix(QStringLiteral(" px"));
    subtitleStyleLayout->addRow(uiText("Safe area horizontal margin"), subtitleMarginXSpinBox_);

    subtitleMarginYSpinBox_ = new QSpinBox(subtitleStyleGroup);
    subtitleMarginYSpinBox_->setRange(0, 600);
    subtitleMarginYSpinBox_->setSuffix(QStringLiteral(" px"));
    subtitleStyleLayout->addRow(uiText("Safe area bottom / edge margin"), subtitleMarginYSpinBox_);

    subtitleScaleWithWindowCheckBox_ = new QCheckBox(uiText("Scale subtitles with the player window"), subtitleStyleGroup);
    subtitleStyleLayout->addRow(subtitleScaleWithWindowCheckBox_);

    subtitleAssForceMarginsCheckBox_ = new QCheckBox(uiText("Force ASS subtitles to respect the video margins"), subtitleStyleGroup);
    subtitleStyleLayout->addRow(subtitleAssForceMarginsCheckBox_);

    subtitleAssJustifyCheckBox_ = new QCheckBox(uiText("Use ASS-aware justification when possible"), subtitleStyleGroup);
    subtitleStyleLayout->addRow(subtitleAssJustifyCheckBox_);

    subtitleFontProviderComboBox_ = new QComboBox(subtitleStyleGroup);
    for (const auto &option : revaplayer::application::subtitleFontProviderOptions()) {
        subtitleFontProviderComboBox_->addItem(revaplayer::application::translateUiText(option.label), option.id);
    }
    subtitleStyleLayout->addRow(uiText("Font provider"), subtitleFontProviderComboBox_);

    subtitleShaperComboBox_ = new QComboBox(subtitleStyleGroup);
    for (const auto &option : revaplayer::application::subtitleShaperOptions()) {
        subtitleShaperComboBox_->addItem(revaplayer::application::translateUiText(option.label), option.id);
    }
    subtitleStyleLayout->addRow(uiText("Text shaping"), subtitleShaperComboBox_);

    subtitleHintingComboBox_ = new QComboBox(subtitleStyleGroup);
    for (const auto &option : revaplayer::application::subtitleHintingOptions()) {
        subtitleHintingComboBox_->addItem(revaplayer::application::translateUiText(option.label), option.id);
    }
    subtitleStyleLayout->addRow(uiText("Font hinting"), subtitleHintingComboBox_);

    resetSubtitleAppearanceButton_ = new QPushButton(uiText("Reset Subtitle Appearance to Defaults"), subtitleStyleGroup);
    resetSubtitleAppearanceButton_->setVisible(false);

    subtitleAdvancedContainerLayout->addWidget(subtitleAutomationGroup);
    subtitleAdvancedContainerLayout->addWidget(subtitleStyleGroup);

    auto *subtitleTabs = new QTabWidget(subtitleSectionPage);
    auto *subtitleBasicTab = new QWidget(subtitleTabs);
    auto *subtitleBasicLayout = new QVBoxLayout(subtitleBasicTab);
    subtitleBasicLayout->setContentsMargins(0, 0, 0, 0);
    subtitleBasicLayout->setSpacing(10);
    subtitleBasicLayout->addWidget(subtitleBehaviorGroup);

    auto *subtitleAdvancedTab = new QWidget(subtitleTabs);
    auto *subtitleAdvancedTabLayout = new QVBoxLayout(subtitleAdvancedTab);
    subtitleAdvancedTabLayout->setContentsMargins(0, 0, 0, 0);
    subtitleAdvancedTabLayout->setSpacing(10);
    subtitleAdvancedTabLayout->addWidget(subtitleAdvancedContainer);

    subtitleTabs->addTab(subtitleBasicTab, uiText("Basic"));
    subtitleTabs->addTab(subtitleAdvancedTab, uiText("Advanced"));
    subtitleTabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    const auto syncSubtitleTabsHeight = [subtitleTabs]() {
        if (subtitleTabs == nullptr || subtitleTabs->currentWidget() == nullptr || subtitleTabs->tabBar() == nullptr) {
            return;
        }
        const int contentHeight = subtitleTabs->currentWidget()->sizeHint().height();
        const int tabBarHeight = subtitleTabs->tabBar()->sizeHint().height();
        subtitleTabs->setFixedHeight(std::max(120, contentHeight + tabBarHeight + 20));
    };
    connect(subtitleTabs, &QTabWidget::currentChanged, subtitleTabs, [syncSubtitleTabsHeight](const int) {
        syncSubtitleTabsHeight();
    });
    syncSubtitleTabsHeight();

    subtitleLayout->addWidget(subtitleTabs);
    subtitleLayout->addStretch(1);

    const auto syncSubtitleAutoLoadControls = [this]() {
        if (subtitleAutoLoadLocalMatchesCheckBox_ == nullptr || subtitleAutoLoadModeComboBox_ == nullptr) {
            return;
        }

        const QString modeId = subtitleAutoLoadModeComboBox_->currentData().toString().trimmed();
        const bool enabled = modeId != QStringLiteral("manual_only");
        {
            const QSignalBlocker blocker(subtitleAutoLoadLocalMatchesCheckBox_);
            subtitleAutoLoadLocalMatchesCheckBox_->setChecked(enabled);
        }
        if (subtitleRememberTrackChoiceCheckBox_ != nullptr) {
            subtitleRememberTrackChoiceCheckBox_->setEnabled(enabled);
        }
    };
    const auto attachColorPicker = [this](QPushButton *button, const QString &title, const QString &fallback) {
        updateSubtitleColorButton(button, QColor(fallback));
        connect(button, &QPushButton::clicked, this, [this, button, title, fallback]() {
            const QColor chosen = QColorDialog::getColor(
                resolvedSubtitleButtonColor(button, fallback),
                this,
                title,
                QColorDialog::ShowAlphaChannel);
            if (!chosen.isValid()) {
                return;
            }
            updateSubtitleColorButton(button, chosen);
            refreshSubtitlePreviewSample();
            emitSubtitlePreviewState();
        });
    };
    attachColorPicker(subtitleTextColorButton_, uiText("Subtitle Text Color"), QStringLiteral("#FFFFFFFF"));
    attachColorPicker(subtitleOutlineColorButton_, uiText("Subtitle Outline Color"), QStringLiteral("#FF000000"));
    attachColorPicker(subtitleBackgroundColorButton_, uiText("Subtitle Background Color"), QStringLiteral("#AF000000"));
    attachColorPicker(subtitleShadowColorButton_, uiText("Subtitle Shadow Color"), QStringLiteral("#AF000000"));
    connect(subtitleAutoLoadLocalMatchesCheckBox_, &QCheckBox::toggled, this, [this, syncSubtitleAutoLoadControls](const bool checked) {
        if (subtitleAutoLoadModeComboBox_ == nullptr) {
            return;
        }
        const QString targetMode = checked ? QStringLiteral("same_name_only") : QStringLiteral("manual_only");
        if (subtitleAutoLoadModeComboBox_->currentData().toString() != targetMode) {
            const int targetIndex = subtitleAutoLoadModeComboBox_->findData(targetMode);
            if (targetIndex >= 0) {
                subtitleAutoLoadModeComboBox_->setCurrentIndex(targetIndex);
            }
        }
        syncSubtitleAutoLoadControls();
    });
    connect(subtitleAutoLoadModeComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [syncSubtitleAutoLoadControls](const int) {
        syncSubtitleAutoLoadControls();
    });
    const auto refreshSubtitlePreview = [this]() {
        refreshSubtitlePreviewSample();
        emitSubtitlePreviewState();
    };
    connect(subtitleVisibleCheckBox_, &QCheckBox::toggled, this, [refreshSubtitlePreview](const bool) { refreshSubtitlePreview(); });
    connect(subtitleScaleSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [refreshSubtitlePreview](double) { refreshSubtitlePreview(); });
    connect(subtitlePositionSpinBox_, qOverload<int>(&QSpinBox::valueChanged), this, [refreshSubtitlePreview](int) { refreshSubtitlePreview(); });
    connect(subtitleFontComboBox_, &QFontComboBox::currentFontChanged, this, [refreshSubtitlePreview](const QFont &) { refreshSubtitlePreview(); });
    connect(subtitleFontSizeSpinBox_, qOverload<int>(&QSpinBox::valueChanged), this, [refreshSubtitlePreview](int) { refreshSubtitlePreview(); });
    connect(subtitleFontWeightSpinBox_, qOverload<int>(&QSpinBox::valueChanged), this, [refreshSubtitlePreview](int) { refreshSubtitlePreview(); });
    connect(subtitleItalicCheckBox_, &QCheckBox::toggled, this, [refreshSubtitlePreview](const bool) { refreshSubtitlePreview(); });
    connect(subtitleAssOverrideComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [refreshSubtitlePreview](int) { refreshSubtitlePreview(); });
    connect(subtitleAlignXComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [refreshSubtitlePreview](int) { refreshSubtitlePreview(); });
    connect(subtitleAlignYComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [refreshSubtitlePreview](int) { refreshSubtitlePreview(); });
    connect(subtitleJustifyComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [refreshSubtitlePreview](int) { refreshSubtitlePreview(); });
    connect(subtitleOutlineSizeSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [refreshSubtitlePreview](double) { refreshSubtitlePreview(); });
    connect(subtitleBackgroundOpacitySpinBox_, qOverload<int>(&QSpinBox::valueChanged), this, [refreshSubtitlePreview](int) { refreshSubtitlePreview(); });
    connect(subtitleShadowOffsetSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [refreshSubtitlePreview](double) { refreshSubtitlePreview(); });
    connect(subtitleShadowBlurSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [refreshSubtitlePreview](double) { refreshSubtitlePreview(); });
    connect(subtitleLineSpacingSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [refreshSubtitlePreview](double) { refreshSubtitlePreview(); });
    connect(subtitleLetterSpacingSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [refreshSubtitlePreview](double) { refreshSubtitlePreview(); });
    connect(subtitleMarginXSpinBox_, qOverload<int>(&QSpinBox::valueChanged), this, [refreshSubtitlePreview](int) { refreshSubtitlePreview(); });
    connect(subtitleMarginYSpinBox_, qOverload<int>(&QSpinBox::valueChanged), this, [refreshSubtitlePreview](int) { refreshSubtitlePreview(); });
    connect(subtitleBackgroundEnabledCheckBox_, &QCheckBox::toggled, this, [refreshSubtitlePreview](const bool) { refreshSubtitlePreview(); });
    connect(subtitleShadowEnabledCheckBox_, &QCheckBox::toggled, this, [refreshSubtitlePreview](const bool) { refreshSubtitlePreview(); });
    connect(subtitleUseMarginsCheckBox_, &QCheckBox::toggled, this, [refreshSubtitlePreview](const bool) { refreshSubtitlePreview(); });

    const auto resetSubtitleAppearanceToDefaults = [this]() {
        subtitleFontComboBox_->setCurrentFont(QFont(QStringLiteral("sans-serif")));
        subtitleFontSizeSpinBox_->setValue(38);
        subtitleFontWeightSpinBox_->setValue(500);
        subtitleItalicCheckBox_->setChecked(false);
        subtitleScaleSpinBox_->setValue(1.00);
        subtitlePositionSpinBox_->setValue(100);
        subtitleAlignXComboBox_->setCurrentIndex(std::max(0, subtitleAlignXComboBox_->findData(QStringLiteral("center"))));
        subtitleAlignYComboBox_->setCurrentIndex(std::max(0, subtitleAlignYComboBox_->findData(QStringLiteral("bottom"))));
        subtitleJustifyComboBox_->setCurrentIndex(std::max(0, subtitleJustifyComboBox_->findData(QStringLiteral("auto"))));
        subtitleMaxWidthSpinBox_->setValue(92.0);
        subtitleUseMarginsCheckBox_->setChecked(true);
        subtitleMarginXSpinBox_->setValue(19);
        subtitleMarginYSpinBox_->setValue(34);
        subtitleAssOverrideComboBox_->setCurrentIndex(std::max(0, subtitleAssOverrideComboBox_->findData(QStringLiteral("scale"))));
        subtitleBorderStyleComboBox_->setCurrentIndex(std::max(0, subtitleBorderStyleComboBox_->findData(QStringLiteral("outline-and-shadow"))));
        subtitleOutlineSizeSpinBox_->setValue(1.65);
        subtitleShadowEnabledCheckBox_->setChecked(true);
        subtitleShadowOffsetSpinBox_->setValue(0.0);
        subtitleShadowBlurSpinBox_->setValue(0.0);
        subtitleBackgroundEnabledCheckBox_->setChecked(false);
        subtitleBackgroundOpacitySpinBox_->setValue(68);
        subtitleLineSpacingSpinBox_->setValue(0.0);
        subtitleLetterSpacingSpinBox_->setValue(0.0);
        subtitleScaleWithWindowCheckBox_->setChecked(true);
        subtitleAssForceMarginsCheckBox_->setChecked(false);
        subtitleAssJustifyCheckBox_->setChecked(false);
        subtitleFontProviderComboBox_->setCurrentIndex(std::max(0, subtitleFontProviderComboBox_->findData(QStringLiteral("auto"))));
        subtitleShaperComboBox_->setCurrentIndex(std::max(0, subtitleShaperComboBox_->findData(QStringLiteral("complex"))));
        subtitleHintingComboBox_->setCurrentIndex(std::max(0, subtitleHintingComboBox_->findData(QStringLiteral("none"))));
        updateSubtitleColorButton(subtitleTextColorButton_, QColor(QStringLiteral("#FFFFFFFF")));
        updateSubtitleColorButton(subtitleOutlineColorButton_, QColor(QStringLiteral("#FF000000")));
        updateSubtitleColorButton(subtitleBackgroundColorButton_, QColor(QStringLiteral("#AF000000")));
        updateSubtitleColorButton(subtitleShadowColorButton_, QColor(QStringLiteral("#AF000000")));
        refreshSubtitlePreviewSample();
        emitSubtitlePreviewState();
    };
    connect(resetSubtitleAppearanceButton_, &QPushButton::clicked, this, [resetSubtitleAppearanceToDefaults]() {
        resetSubtitleAppearanceToDefaults();
    });
    connect(resetSubtitleStyleQuickButton_, &QPushButton::clicked, this, [resetSubtitleAppearanceToDefaults]() {
        resetSubtitleAppearanceToDefaults();
    });
    connect(videoMinimumZoomSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](const double value) {
        if (videoMaximumZoomSpinBox_ != nullptr && videoMaximumZoomSpinBox_->value() < value) {
            videoMaximumZoomSpinBox_->setValue(value);
        }
    });
    connect(videoMaximumZoomSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](const double value) {
        if (videoMinimumZoomSpinBox_ != nullptr && videoMinimumZoomSpinBox_->value() > value) {
            videoMinimumZoomSpinBox_->setValue(value);
        }
    });
    connect(subtitleBackgroundEnabledCheckBox_, &QCheckBox::toggled, this, [this](const bool enabled) {
        if (subtitleBackgroundColorButton_ != nullptr) {
            subtitleBackgroundColorButton_->setEnabled(enabled);
        }
        if (subtitleBackgroundOpacitySpinBox_ != nullptr) {
            subtitleBackgroundOpacitySpinBox_->setEnabled(enabled);
        }
    });
    connect(subtitleShadowEnabledCheckBox_, &QCheckBox::toggled, this, [this](const bool enabled) {
        if (subtitleShadowColorButton_ != nullptr) {
            subtitleShadowColorButton_->setEnabled(enabled);
        }
        if (subtitleShadowOffsetSpinBox_ != nullptr) {
            subtitleShadowOffsetSpinBox_->setEnabled(enabled);
        }
        if (subtitleShadowBlurSpinBox_ != nullptr) {
            subtitleShadowBlurSpinBox_->setEnabled(enabled);
        }
    });
    connect(subtitleUseMarginsCheckBox_, &QCheckBox::toggled, this, [this](const bool enabled) {
        if (subtitleMarginXSpinBox_ != nullptr) {
            subtitleMarginXSpinBox_->setEnabled(enabled);
        }
        if (subtitleMarginYSpinBox_ != nullptr) {
            subtitleMarginYSpinBox_->setEnabled(enabled);
        }
    });
    syncSubtitleAutoLoadControls();

    auto *shortcutsPage = new QWidget();
    auto *shortcutsPageLayout = new QVBoxLayout(shortcutsPage);
    shortcutsPageLayout->setContentsMargins(18, 18, 18, 18);
    shortcutsPageLayout->setSpacing(14);

    auto *shortcutsInfoLabel = new QLabel(
        uiText("Assign custom shortcuts to the most-used actions. Leaving a field empty falls back to the default shortcut."),
        shortcutsPage);
    shortcutsInfoLabel->setWordWrap(true);

    auto *shortcutsToolbar = new QWidget(shortcutsPage);
    auto *shortcutsToolbarLayout = new QHBoxLayout(shortcutsToolbar);
    shortcutsToolbarLayout->setContentsMargins(0, 0, 0, 0);
    shortcutsToolbarLayout->setSpacing(8);

    shortcutSearchEdit_ = new QLineEdit(shortcutsToolbar);
    shortcutSearchEdit_->setPlaceholderText(uiText("Search shortcuts"));
    shortcutCategoryComboBox_ = new QComboBox(shortcutsToolbar);
    shortcutCategoryComboBox_->addItem(uiText("All Categories"), QStringLiteral("*"));
    {
        QStringList categories;
        for (const auto &binding : shortcutBindings_) {
            const QString category = binding.category.trimmed();
            if (!category.isEmpty() && !categories.contains(category)) {
                categories.push_back(category);
            }
        }
        std::sort(categories.begin(), categories.end(), [](const QString &left, const QString &right) {
            return QString::compare(left, right, Qt::CaseInsensitive) < 0;
        });
        for (const QString &category : categories) {
            shortcutCategoryComboBox_->addItem(category, category);
        }
    }
    shortcutImportButton_ = new QPushButton(uiText("Import"), shortcutsToolbar);
    shortcutExportButton_ = new QPushButton(uiText("Export"), shortcutsToolbar);
    shortcutResetAllButton_ = new QPushButton(uiText("Reset All"), shortcutsToolbar);
    shortcutsToolbarLayout->addWidget(shortcutSearchEdit_, 1);
    shortcutsToolbarLayout->addWidget(shortcutCategoryComboBox_, 0);
    shortcutsToolbarLayout->addWidget(shortcutImportButton_, 0);
    shortcutsToolbarLayout->addWidget(shortcutExportButton_, 0);
    shortcutsToolbarLayout->addWidget(shortcutResetAllButton_, 0);

    shortcutConflictLabel_ = new QLabel(shortcutsPage);
    shortcutConflictLabel_->setWordWrap(true);
    shortcutConflictLabel_->setObjectName(QStringLiteral("shortcutConflictLabel"));

    auto *shortcutsScrollArea = new QScrollArea(shortcutsPage);
    shortcutsScrollArea->setWidgetResizable(true);
    auto *shortcutsContainer = new QWidget(shortcutsScrollArea);
    auto *shortcutsForm = new QFormLayout(shortcutsContainer);
    shortcutsForm->setContentsMargins(0, 0, 0, 0);
    shortcutsForm->setSpacing(10);

    shortcutEditors_.clear();
    shortcutEditors_.reserve(shortcutBindings_.size());
    for (const auto &binding : shortcutBindings_) {
        auto *rowWidget = new QWidget(shortcutsContainer);
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        auto *editor = new QKeySequenceEdit(rowWidget);
        editor->setKeySequence(binding.currentSequence);
        auto *resetButton = new QPushButton(uiText("Reset"), rowWidget);

        rowLayout->addWidget(editor, 1);
        rowLayout->addWidget(resetButton, 0);
        shortcutsForm->addRow(revaplayer::application::translateUiText(binding.label), rowWidget);

        connect(resetButton, &QPushButton::clicked, this, [editor, binding]() {
            editor->setKeySequence(binding.defaultSequence);
            editor->clearFocus();
        });
        connect(editor, &QKeySequenceEdit::keySequenceChanged, this, [this](const QKeySequence &) {
            refreshShortcutEditorState();
        });

        shortcutEditors_.push_back(ShortcutEditorRow {
            binding.id,
            binding.category,
            binding.label,
            binding.defaultSequence,
            rowWidget,
            editor,
        });
    }

    connect(shortcutSearchEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        refreshShortcutEditorState();
    });
    connect(shortcutCategoryComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int) {
        refreshShortcutEditorState();
    });
    connect(shortcutResetAllButton_, &QPushButton::clicked, this, [this]() {
        for (const auto &row : shortcutEditors_) {
            if (row.editor != nullptr) {
                row.editor->setKeySequence(row.defaultSequence);
            }
        }
        refreshShortcutEditorState();
    });
    connect(shortcutExportButton_, &QPushButton::clicked, this, [this]() {
        const QString path = filedialog::getSaveFileName(
            this,
            uiText("Export Shortcut Profile"),
            QStringLiteral("shortcuts-profile.json"),
            uiText("JSON Files (*.json)"));
        if (path.trimmed().isEmpty()) {
            return;
        }

        QJsonArray bindingsArray;
        for (const auto &row : shortcutEditors_) {
            if (row.editor == nullptr) {
                continue;
            }
            QJsonObject bindingObject;
            bindingObject.insert(QStringLiteral("id"), row.id);
            bindingObject.insert(QStringLiteral("category"), row.category);
            bindingObject.insert(QStringLiteral("label"), row.label);
            bindingObject.insert(QStringLiteral("shortcut"), row.editor->keySequence().toString(QKeySequence::PortableText));
            bindingsArray.push_back(bindingObject);
        }

        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || !file.write(QJsonDocument(bindingsArray).toJson(QJsonDocument::Indented)) || !file.commit()) {
            QMessageBox::warning(this, uiText("Shortcuts"), uiText("Could not export the shortcut profile."));
        }
    });
    connect(shortcutImportButton_, &QPushButton::clicked, this, [this]() {
        const QString path = filedialog::getOpenFileName(
            this,
            uiText("Import Shortcut Profile"),
            QStringLiteral("shortcuts-profile.json"),
            uiText("JSON Files (*.json)"));
        if (path.trimmed().isEmpty()) {
            return;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, uiText("Shortcuts"), uiText("Could not open the shortcut profile."));
            return;
        }

        const QJsonArray bindingsArray = QJsonDocument::fromJson(file.readAll()).array();
        for (const QJsonValue &value : bindingsArray) {
            const QJsonObject bindingObject = value.toObject();
            const QString id = bindingObject.value(QStringLiteral("id")).toString();
            const QString shortcut = bindingObject.value(QStringLiteral("shortcut")).toString();
            for (const auto &row : shortcutEditors_) {
                if (row.id == id && row.editor != nullptr) {
                    row.editor->setKeySequence(QKeySequence(shortcut, QKeySequence::PortableText));
                    break;
                }
            }
        }
        refreshShortcutEditorState();
    });
    shortcutsScrollArea->setWidget(shortcutsContainer);
    shortcutsPageLayout->addWidget(shortcutsInfoLabel);
    shortcutsPageLayout->addWidget(shortcutsToolbar, 0);
    shortcutsPageLayout->addWidget(shortcutConflictLabel_, 0);
    shortcutsPageLayout->addWidget(shortcutsScrollArea, 1);
    refreshShortcutEditorState();

    const auto createSectionPage = [](std::initializer_list<QWidget *> sections) {
        auto *page = new QWidget();
        page->setObjectName(QStringLiteral("settingsSectionPage"));
        auto *pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(18, 18, 18, 18);
        pageLayout->setSpacing(14);
        for (QWidget *section : sections) {
            if (section != nullptr) {
                pageLayout->addWidget(section);
            }
        }
        pageLayout->addStretch(1);
        return page;
    };

    QWidget *startupSectionPage = createSectionPage({sessionGroup});
    QWidget *interfaceSectionPage = createSectionPage({windowChromeGroup, interfaceGroup, appearanceGroup, themeEditorGroup, dashboardGroup, storageInfoLabel_});
    QWidget *playbackSectionPage = createSectionPage({volumeGroup, interactionGroup, videoZoomGroup, feedbackGroup, playbackNote});
    QWidget *controlBarSectionPage = createSectionPage({controlBarVisibilityGroup});
    QWidget *panelsSectionPage = createSectionPage({layoutGroup, fullscreenGroup, pointerGroup});
    QWidget *timelineSectionPage = createSectionPage({thumbnailsGroup, sceneBrowserGroup});
    QWidget *playlistSectionPage = createSectionPage({playlistGroup, progressGroup});
    QWidget *mouseSectionPage = createSectionPage({mouseGroup, mouseZonesGroup});
    QWidget *historySectionPage = createSectionPage({historyGroup});
    auto *maintenancePage = new QWidget();
    maintenancePage->setObjectName(QStringLiteral("settingsSectionPage"));
    auto *maintenanceLayout = new QVBoxLayout(maintenancePage);
    maintenanceLayout->setContentsMargins(18, 18, 18, 18);
    maintenanceLayout->setSpacing(14);

    auto *maintenanceGroup = new QGroupBox(uiText("Maintenance & Data"), maintenancePage);
    auto *maintenanceGroupLayout = new QVBoxLayout(maintenanceGroup);
    auto *maintenanceInfoLabel = new QLabel(
        uiText("Use these actions to clear temporary data or fully reset the application. Media files on disk are never deleted."),
        maintenanceGroup);
    maintenanceInfoLabel->setWordWrap(true);

    clearCacheButton_ = new QPushButton(uiText("Clear Cache"), maintenanceGroup);
    auto *clearCacheHintLabel = new QLabel(
        uiText("Removes cached thumbnails and temporary data only. Preferences, history, bookmarks, and saved lists are kept."),
        maintenanceGroup);
    clearCacheHintLabel->setWordWrap(true);

    resetSettingsButton_ = new QPushButton(uiText("Reset Settings"), maintenanceGroup);
    auto *resetSettingsHintLabel = new QLabel(
        uiText("Resets settings to defaults only and keeps history, playlists, bookmarks, resume state, and database data."),
        maintenanceGroup);
    resetSettingsHintLabel->setWordWrap(true);

    factoryResetButton_ = new QPushButton(uiText("Factory Reset"), maintenanceGroup);
    auto *factoryResetHintLabel = new QLabel(
        uiText("Removes all local Reva Player data and restores defaults. This does not delete your actual media files."),
        maintenanceGroup);
    factoryResetHintLabel->setWordWrap(true);

    maintenanceGroupLayout->addWidget(maintenanceInfoLabel);
    maintenanceGroupLayout->addSpacing(6);
    maintenanceGroupLayout->addWidget(clearCacheButton_, 0);
    maintenanceGroupLayout->addWidget(clearCacheHintLabel);
    maintenanceGroupLayout->addSpacing(10);
    maintenanceGroupLayout->addWidget(resetSettingsButton_, 0);
    maintenanceGroupLayout->addWidget(resetSettingsHintLabel);
    maintenanceGroupLayout->addSpacing(10);
    maintenanceGroupLayout->addWidget(factoryResetButton_, 0);
    maintenanceGroupLayout->addWidget(factoryResetHintLabel);

    maintenanceLayout->addWidget(maintenanceGroup);
    maintenanceLayout->addStretch(1);

    connect(clearCacheButton_, &QPushButton::clicked, this, [this]() {
        emit clearCacheRequested();
    });
    connect(resetSettingsButton_, &QPushButton::clicked, this, [this]() {
        if (settingsController_ == nullptr) {
            return;
        }
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            uiText("Reset Settings"),
            uiText("Reset settings to defaults only?\nThis keeps history, playlists, bookmarks, resume state, and database data."),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
        if (!settingsController_->resetSettingsToDefaults()) {
            QMessageBox::warning(
                this,
                uiText("Reset Settings"),
                uiText("Failed to reset settings."));
            return;
        }
        loadSettings();
        emit settingsApplied();
    });
    connect(factoryResetButton_, &QPushButton::clicked, this, [this]() {
        emit factoryResetRequested();
    });

    QWidget *captureSectionPage = createSectionPage({captureGroup});

    settingsPageContents_ = {
        startupSectionPage,
        interfaceSectionPage,
        playbackSectionPage,
        controlBarSectionPage,
        panelsSectionPage,
        timelineSectionPage,
        playlistSectionPage,
        subtitleSectionPage,
        mouseSectionPage,
        historySectionPage,
        maintenancePage,
        captureSectionPage,
        shortcutsPage,
    };

    const auto wrapScrollablePage = [tabs](QWidget *page) {
        auto *scrollArea = new QScrollArea(tabs);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setWidget(page);
        return scrollArea;
    };

    const auto addSectionTab = [tabs, settingsSectionTabsLayout, this](QWidget *page, const QString &label) {
        tabs->addTab(page, label);
        auto *sectionButton = new QPushButton(label, settingsSectionTabsContainer_);
        sectionButton->setObjectName(QStringLiteral("settingsSectionButton"));
        sectionButton->setCheckable(true);
        sectionButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        settingsSectionButtons_.push_back(sectionButton);
        settingsSectionTabsLayout->addWidget(sectionButton, 0);
    };

    addSectionTab(wrapScrollablePage(startupSectionPage), uiText("Startup"));
    addSectionTab(wrapScrollablePage(interfaceSectionPage), uiText("Interface"));
    addSectionTab(wrapScrollablePage(playbackSectionPage), uiText("Playback"));
    addSectionTab(wrapScrollablePage(controlBarSectionPage), uiText("Control Bar"));
    addSectionTab(wrapScrollablePage(panelsSectionPage), uiText("Panels"));
    addSectionTab(wrapScrollablePage(timelineSectionPage), uiText("Timeline"));
    addSectionTab(wrapScrollablePage(playlistSectionPage), uiText("Playlist"));
    auto *subtitleScrollArea = wrapScrollablePage(subtitleSectionPage);
    addSectionTab(subtitleScrollArea, uiText("Subtitles"));
    addSectionTab(wrapScrollablePage(mouseSectionPage), uiText("Mouse"));
    addSectionTab(wrapScrollablePage(historySectionPage), uiText("History"));
    addSectionTab(wrapScrollablePage(maintenancePage), uiText("Maintenance / Data Management"));
    addSectionTab(wrapScrollablePage(captureSectionPage), uiText("Capture"));
    addSectionTab(shortcutsPage, uiText("Shortcuts"));
    subtitleTabIndex_ = tabs->indexOf(subtitleScrollArea);

    if (settingsSectionTabsContainer_ != nullptr) {
        settingsSectionTabsContainer_->adjustSize();
    }

    if (tabs->tabBar() != nullptr) {
        tabs->tabBar()->hide();
    }

    for (qsizetype index = 0; index < settingsSectionButtons_.size(); ++index) {
        QPushButton *sectionButton = settingsSectionButtons_.at(index);
        if (sectionButton == nullptr) {
            continue;
        }

        connect(sectionButton, &QPushButton::clicked, this, [this, index]() {
            if (settingsTabs_ == nullptr || index < 0 || index >= settingsTabs_->count()) {
                return;
            }
            settingsTabs_->setCurrentIndex(static_cast<int>(index));
        });
    }

    connect(tabs, &QTabWidget::currentChanged, this, [this](const int currentIndex) {
        for (qsizetype index = 0; index < settingsSectionButtons_.size(); ++index) {
            if (QPushButton *button = settingsSectionButtons_.at(index); button != nullptr) {
                const QSignalBlocker blocker(button);
                button->setChecked(static_cast<int>(index) == currentIndex);
            }
        }

        if (settingsSectionTabsScrollArea_ != nullptr
            && currentIndex >= 0
            && currentIndex < settingsSectionButtons_.size()) {
            if (QPushButton *currentButton = settingsSectionButtons_.at(currentIndex); currentButton != nullptr) {
                const int viewportWidth = settingsSectionTabsScrollArea_->viewport()->width();
                const int leftEdge = currentButton->x();
                const int rightEdge = currentButton->x() + currentButton->width();
                QScrollBar *scrollBar = settingsSectionTabsScrollArea_->horizontalScrollBar();
                if (scrollBar != nullptr) {
                    const int currentValue = scrollBar->value();
                    const int visibleLeft = currentValue;
                    const int visibleRight = currentValue + viewportWidth;
                    if (leftEdge < visibleLeft) {
                        scrollBar->setValue(leftEdge);
                    } else if (rightEdge > visibleRight) {
                        scrollBar->setValue(std::max(0, rightEdge - viewportWidth));
                    }
                }
            }
        }

    });
    if (!settingsSectionButtons_.isEmpty() && tabs->currentIndex() >= 0 && tabs->currentIndex() < settingsSectionButtons_.size()) {
        if (QPushButton *currentButton = settingsSectionButtons_.at(tabs->currentIndex()); currentButton != nullptr) {
            currentButton->setChecked(true);
        }
    }

    if (settingsSectionTabsScrollArea_ != nullptr && !settingsSectionButtons_.isEmpty()) {
        const int buttonHeight = settingsSectionButtons_.constFirst() != nullptr
            ? settingsSectionButtons_.constFirst()->sizeHint().height()
            : 36;
        const int scrollAreaHeight = buttonHeight
            + settingsSectionTabsScrollArea_->horizontalScrollBar()->sizeHint().height()
            + 8;
        settingsSectionTabsScrollArea_->setFixedHeight(std::max(scrollAreaHeight, 58));
    }

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    if (QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok); okButton != nullptr) {
        okButton->setText(uiText("OK"));
    }
    if (QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel); cancelButton != nullptr) {
        cancelButton->setText(uiText("Cancel"));
    }
    if (QPushButton *applyButton = buttonBox->button(QDialogButtonBox::Apply); applyButton != nullptr) {
        applyButton->setText(uiText("Apply"));
        applyButton_ = applyButton;
        applyButton_->setEnabled(false);
    }
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this]() {
        QString errorMessage;
        if (!applyIfValid(&errorMessage)) {
            QMessageBox::warning(
                this,
                uiText("Invalid Shortcuts"),
                errorMessage);
        }
    });
    const auto markDirty = [this]() {
        settingsDirty_ = true;
        if (applyButton_ != nullptr) {
            applyButton_->setEnabled(true);
        }
    };
    for (QCheckBox *checkBox : findChildren<QCheckBox *>()) {
        connect(checkBox, &QCheckBox::toggled, this, [markDirty](bool) { markDirty(); });
    }
    for (QComboBox *comboBox : findChildren<QComboBox *>()) {
        connect(comboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [markDirty](int) { markDirty(); });
    }
    for (QSpinBox *spinBox : findChildren<QSpinBox *>()) {
        connect(spinBox, qOverload<int>(&QSpinBox::valueChanged), this, [markDirty](int) { markDirty(); });
    }
    for (QDoubleSpinBox *spinBox : findChildren<QDoubleSpinBox *>()) {
        connect(spinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [markDirty](double) { markDirty(); });
    }
    for (QLineEdit *lineEdit : findChildren<QLineEdit *>()) {
        connect(lineEdit, &QLineEdit::textChanged, this, [markDirty](const QString &) { markDirty(); });
    }

    auto *searchRow = new QWidget(this);
    searchRow->setObjectName(QStringLiteral("settingsSearchRow"));
    auto *searchRowLayout = new QHBoxLayout(searchRow);
    searchRowLayout->setContentsMargins(0, 0, 0, 0);
    searchRowLayout->setSpacing(8);
    settingsSearchEdit_ = new QLineEdit(searchRow);
    settingsSearchEdit_->setPlaceholderText(uiText("Search all settings"));
    settingsSearchButton_ = new QPushButton(uiText("Search"), searchRow);
    settingsSearchClearButton_ = new QPushButton(uiText("Clear"), searchRow);
    settingsSearchClearButton_->setEnabled(false);
    searchRowLayout->addWidget(settingsSearchEdit_, 1);
    searchRowLayout->addWidget(settingsSearchButton_, 0);
    searchRowLayout->addWidget(settingsSearchClearButton_, 0);

    connect(settingsSearchEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        refreshSettingsSearchState();
    });
    connect(settingsSearchButton_, &QPushButton::clicked, this, [this]() {
        refreshSettingsSearchState();
    });
    connect(settingsSearchClearButton_, &QPushButton::clicked, this, [this]() {
        if (settingsSearchEdit_ != nullptr) {
            settingsSearchEdit_->clear();
        }
        refreshSettingsSearchState();
    });

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(12);
    rootLayout->addWidget(searchRow, 0);
    rootLayout->addWidget(settingsSectionTabsScrollArea_, 0);
    rootLayout->addWidget(tabs);
    rootLayout->addWidget(buttonBox);

    for (QComboBox *comboBox : findChildren<QComboBox *>()) {
        comboBox->setFocusPolicy(Qt::StrongFocus);
        comboBox->installEventFilter(this);
    }
    for (QFontComboBox *fontComboBox : findChildren<QFontComboBox *>()) {
        fontComboBox->setFocusPolicy(Qt::StrongFocus);
        fontComboBox->installEventFilter(this);
    }
    for (QAbstractSpinBox *spinBox : findChildren<QAbstractSpinBox *>()) {
        spinBox->setFocusPolicy(Qt::StrongFocus);
        spinBox->installEventFilter(this);
    }

}

void SettingsDialog::refreshSubtitlePreviewSample()
{
    emitSubtitlePreviewState();
}

void SettingsDialog::emitSubtitlePreviewState()
{
    if (subtitleVisibleCheckBox_ == nullptr
        || subtitleScaleSpinBox_ == nullptr
        || subtitlePositionSpinBox_ == nullptr
        || subtitleFontComboBox_ == nullptr
        || subtitleFontSizeSpinBox_ == nullptr
        || subtitleAssOverrideComboBox_ == nullptr) {
        return;
    }

    emit subtitlePreviewRequested(
        subtitleVisibleCheckBox_->isChecked(),
        subtitleScaleSpinBox_->value(),
        subtitlePositionSpinBox_->value(),
        subtitleFontComboBox_->currentFont().family(),
        subtitleFontSizeSpinBox_->value(),
        subtitleAssOverrideComboBox_->currentData().toString());
}

void SettingsDialog::accept()
{
    QString errorMessage;
    if (!applyIfValid(&errorMessage)) {
        QMessageBox::warning(
            this,
            uiText("Invalid Shortcuts"),
            errorMessage);
        return;
    }

    QDialog::accept();
}

bool SettingsDialog::applyIfValid(QString *errorMessage)
{
    QString localError;
    if (!validateShortcuts(&localError)) {
        if (errorMessage != nullptr) {
            *errorMessage = localError;
        }
        return false;
    }

    applySettings();
    settingsDirty_ = false;
    if (applyButton_ != nullptr) {
        applyButton_->setEnabled(false);
    }
    emit settingsApplied();
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

void SettingsDialog::loadSettings()
{
    if (settingsController_ == nullptr) {
        storageInfoLabel_->setText(uiText("Settings storage is not available for this session."));
        return;
    }

    rememberWindowStateCheckBox_->setChecked(settingsController_->rememberWindowState());
    rememberLastDirectoryCheckBox_->setChecked(settingsController_->rememberLastOpenDirectory());
    const int languageIndex = interfaceLanguageComboBox_->findData(settingsController_->interfaceLanguage());
    interfaceLanguageComboBox_->setCurrentIndex(languageIndex >= 0 ? languageIndex : 0);
    const int themeIndex = themeComboBox_->findData(settingsController_->uiTheme());
    themeComboBox_->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);
    const int accentIndex = themeAccentComboBox_->findData(
        settingsController_->customValue(QStringLiteral("ui/accent"), QStringLiteral("blue")));
    themeAccentComboBox_->setCurrentIndex(accentIndex >= 0 ? accentIndex : 0);
    if (uiDensityComboBox_ != nullptr) {
        const int densityIndex = uiDensityComboBox_->findData(
            settingsController_->customValue(QStringLiteral("ui/density"), QStringLiteral("normal")));
        uiDensityComboBox_->setCurrentIndex(densityIndex >= 0 ? densityIndex : 0);
    }
    if (startupCanvasStyleComboBox_ != nullptr) {
        const int startupCanvasStyleIndex = startupCanvasStyleComboBox_->findData(
            normalizedStartupCanvasStyleId(
                settingsController_->customValue(
                    QString::fromLatin1(kStartupCanvasStyleSetting),
                    QStringLiteral("theme"))));
        startupCanvasStyleComboBox_->setCurrentIndex(startupCanvasStyleIndex >= 0 ? startupCanvasStyleIndex : 0);
    }
    dashboardEnabledCheckBox_->setChecked(
        settingsController_->customValue(QStringLiteral("ui/dashboard_enabled"), QStringLiteral("1")) != QStringLiteral("0"));
    dashboardShowContinueCheckBox_->setChecked(
        settingsController_->customValue(QString::fromLatin1(kDashboardContinueSectionSetting), QStringLiteral("1")) != QStringLiteral("0"));
    dashboardShowRecentCheckBox_->setChecked(
        settingsController_->customValue(QString::fromLatin1(kDashboardRecentSectionSetting), QStringLiteral("1")) != QStringLiteral("0"));
    dashboardShowFavoritesCheckBox_->setChecked(
        settingsController_->customValue(QString::fromLatin1(kDashboardFavoritesSectionSetting), QStringLiteral("1")) != QStringLiteral("0"));
    dashboardShowSavedListsCheckBox_->setChecked(
        settingsController_->customValue(QString::fromLatin1(kDashboardSavedListsSectionSetting), QStringLiteral("1")) != QStringLiteral("0"));
    themeRadiusSpinBox_->setValue(settingsController_->customValue(QStringLiteral("ui/radius_px"), QStringLiteral("10")).toInt());
    themeSpacingSpinBox_->setValue(settingsController_->customValue(QStringLiteral("ui/spacing_px"), QStringLiteral("8")).toInt());
    themeFontScaleSpinBox_->setValue(settingsController_->customValue(QStringLiteral("ui/font_scale_percent"), QStringLiteral("100")).toInt());
    themeFontWeightSpinBox_->setValue(settingsController_->customValue(QStringLiteral("ui/font_weight_value"), QStringLiteral("500")).toInt());
    themeLetterSpacingSpinBox_->setValue(settingsController_->customValue(QStringLiteral("ui/letter_spacing_px"), QStringLiteral("0.0")).toDouble());
    themeBorderContrastSpinBox_->setValue(settingsController_->customValue(QStringLiteral("ui/border_contrast_percent"), QStringLiteral("100")).toInt());
    themeShadowStrengthSpinBox_->setValue(settingsController_->customValue(QStringLiteral("ui/shadow_strength_percent"), QStringLiteral("60")).toInt());
    themeBlurStrengthSpinBox_->setValue(settingsController_->customValue(QStringLiteral("ui/blur_strength_percent"), QStringLiteral("0")).toInt());
    themeAnimationSpeedSpinBox_->setValue(settingsController_->customValue(QStringLiteral("ui/animation_speed_percent"), QStringLiteral("100")).toInt());
    themeOverlayOpacitySpinBox_->setValue(settingsController_->customValue(QStringLiteral("ui/overlay_opacity_percent"), QStringLiteral("82")).toInt());
    adaptiveUiCheckBox_->setChecked(settingsController_->customValue(QStringLiteral("ui/adaptive_enabled"), QStringLiteral("1")) != QStringLiteral("0"));
    adaptiveUiBreakpointSpinBox_->setValue(settingsController_->customValue(QStringLiteral("ui/adaptive_breakpoint_px"), QStringLiteral("1220")).toInt());
    {
        const int easingIndex = themeAnimationEasingComboBox_->findData(
            settingsController_->customValue(QStringLiteral("ui/animation_easing"), QStringLiteral("cubic")));
        themeAnimationEasingComboBox_->setCurrentIndex(easingIndex >= 0 ? easingIndex : 0);
    }
    showMenuBarCheckBox_->setChecked(settingsController_->showMenuBarInWindowedMode());
    showStatusBarCheckBox_->setChecked(settingsController_->showStatusBarInWindowedMode());
    alwaysOnTopCheckBox_->setChecked(settingsController_->alwaysOnTopEnabled());
    overlayPanelsOnVideoCheckBox_->setChecked(settingsController_->overlayPanelsOnVideo());
    resumePlaybackCheckBox_->setChecked(settingsController_->resumeEnabled());
    rememberHistoryCheckBox_->setChecked(settingsController_->historyEnabled());
    clearHistoryOnExitCheckBox_->setChecked(settingsController_->clearHistoryOnExit());
    const QString profileId = revaplayer::domain::playerProfileId(settingsController_->playbackProfile());
    const int profileIndex = profileComboBox_->findData(profileId);
    profileComboBox_->setCurrentIndex(profileIndex >= 0 ? profileIndex : 1);
    if (externalMpvConfigCheckBox_ != nullptr) {
        externalMpvConfigCheckBox_->setChecked(settingsController_->useExternalMpvConfig());
    }
    showPlaylistPanelCheckBox_->setChecked(settingsController_->showPlaylistPanelOnStartup());
    showDetailsPanelCheckBox_->setChecked(settingsController_->showDetailsPanelOnStartup());
    restoreSidePanelsFromWindowStateCheckBox_->setChecked(settingsController_->restoreSidePanelsFromWindowState());
    const int startupVolumeModeIndex = startupVolumeModeComboBox_->findData(
        settingsController_->rememberLastVolume() ? QStringLiteral("remember") : QStringLiteral("fixed"));
    startupVolumeModeComboBox_->setCurrentIndex(startupVolumeModeIndex >= 0 ? startupVolumeModeIndex : 0);
    startupVolumeSpinBox_->setValue(settingsController_->startupVolume());
    startupVolumeSpinBox_->setEnabled(!settingsController_->rememberLastVolume());
    startupSpeedSpinBox_->setValue(settingsController_->startupPlaybackSpeed());
    if (sessionWideSpeedCheckBox_ != nullptr) {
        sessionWideSpeedCheckBox_->setChecked(rawCustomFlagEnabled(
            settingsController_->customValue(QString::fromLatin1(kSessionWidePlaybackSpeedSetting))));
    }
    const int repeatModeIndex = defaultRepeatModeComboBox_->findData(settingsController_->defaultRepeatMode());
    defaultRepeatModeComboBox_->setCurrentIndex(repeatModeIndex >= 0 ? repeatModeIndex : 0);
    shortSeekStepSpinBox_->setValue(settingsController_->shortSeekStepSeconds());
    longSeekStepSpinBox_->setValue(settingsController_->longSeekStepSeconds());
    volumeStepSpinBox_->setValue(settingsController_->volumeStep());
    const int wheelActionIndex = mouseWheelActionComboBox_->findData(settingsController_->mouseWheelAction());
    mouseWheelActionComboBox_->setCurrentIndex(wheelActionIndex >= 0 ? wheelActionIndex : 0);
    mouseWheelVolumeCheckBox_->setChecked(settingsController_->mouseWheelAction() == QStringLiteral("volume"));
    mouseWheelVolumeStepSpinBox_->setValue(settingsController_->mouseWheelVolumeStep());
    mouseWheelSeekStepSpinBox_->setValue(settingsController_->mouseWheelSeekStepSeconds());
    const int sideButtonsIndex = mouseSideButtonsActionComboBox_->findData(settingsController_->mouseSideButtonsAction());
    mouseSideButtonsActionComboBox_->setCurrentIndex(sideButtonsIndex >= 0 ? sideButtonsIndex : 0);
    mouseNavigationSeekCheckBox_->setChecked(settingsController_->mouseSideButtonsAction().startsWith(QStringLiteral("seek")));
    mouseNavigationSeekStepSpinBox_->setValue(settingsController_->mouseNavigationSeekStepSeconds());
    const int clickActionIndex = clickActionComboBox_->findData(settingsController_->clickAction());
    clickActionComboBox_->setCurrentIndex(clickActionIndex >= 0 ? clickActionIndex : 0);
    const int doubleClickIndex = doubleClickActionComboBox_->findData(settingsController_->doubleClickAction());
    doubleClickActionComboBox_->setCurrentIndex(doubleClickIndex >= 0 ? doubleClickIndex : 0);
    const int middleClickIndex = middleClickActionComboBox_->findData(settingsController_->middleClickAction());
    middleClickActionComboBox_->setCurrentIndex(middleClickIndex >= 0 ? middleClickIndex : 0);
    actionFeedbackOverlayCheckBox_->setChecked(settingsController_->actionFeedbackOverlayEnabled());
    expressiveControlLabelsCheckBox_->setChecked(settingsController_->expressiveControlLabelsEnabled());
    {
        const ControlBarPanelButtonSettings panelButtonSettings = resolvedControlBarPanelButtonSettings(settingsController_);
        controlBarShowOpenButtonCheckBox_->setChecked(
            settingsController_->customValue(QString::fromLatin1(kControlBarShowOpenButtonSetting), QStringLiteral("1")) != QStringLiteral("0"));
        controlBarShowStopButtonCheckBox_->setChecked(
            settingsController_->customValue(QString::fromLatin1(kControlBarShowStopButtonSetting), QStringLiteral("0")) != QStringLiteral("0"));
        controlBarShowPlaylistButtonCheckBox_->setChecked(panelButtonSettings.showPlaylistButton);
        controlBarShowDetailsButtonCheckBox_->setChecked(panelButtonSettings.showDetailsButton);
        controlBarShowTimeLabelCheckBox_->setChecked(
            settingsController_->customValue(QString::fromLatin1(kControlBarShowTimeLabelSetting), QStringLiteral("1")) != QStringLiteral("0"));
        controlBarShowSpeedButtonCheckBox_->setChecked(
            settingsController_->customValue(QString::fromLatin1(kControlBarShowSpeedButtonSetting), QStringLiteral("1")) != QStringLiteral("0"));
        controlBarShowRepeatLoopButtonsCheckBox_->setChecked(
            settingsController_->customValue(QString::fromLatin1(kControlBarShowRepeatLoopButtonsSetting), QStringLiteral("0")) != QStringLiteral("0"));
        controlBarShowTrackMenusCheckBox_->setChecked(
            settingsController_->customValue(QString::fromLatin1(kControlBarShowTrackMenusSetting), QStringLiteral("1")) != QStringLiteral("0"));
        controlBarShowVolumeControlsCheckBox_->setChecked(
            settingsController_->customValue(QString::fromLatin1(kControlBarShowVolumeControlsSetting), QStringLiteral("1")) != QStringLiteral("0"));
        controlBarShowFullscreenButtonCheckBox_->setChecked(
            settingsController_->customValue(QString::fromLatin1(kControlBarShowFullscreenButtonSetting), QStringLiteral("1")) != QStringLiteral("0"));
        controlBarTimelineThicknessSpinBox_->setValue(
            settingsController_->customValue(
                QString::fromLatin1(kControlBarTimelineThicknessSetting),
                QString::number(kDefaultControlBarTimelineThickness)).toInt());
        controlBarTimelineHandleSizeSpinBox_->setValue(
            settingsController_->customValue(
                QString::fromLatin1(kControlBarTimelineHandleSizeSetting),
                QString::number(kDefaultControlBarTimelineHandleSize)).toInt());
        controlBarVolumeSliderThicknessSpinBox_->setValue(
            settingsController_->customValue(
                QString::fromLatin1(kControlBarVolumeSliderThicknessSetting),
                QString::number(kDefaultControlBarVolumeSliderThickness)).toInt());
        controlBarVolumeSliderWidthSpinBox_->setValue(resolvedControlBarVolumeSliderWidth(settingsController_));
    }
    fullscreenAutoHideCheckBox_->setChecked(settingsController_->fullscreenAutoHideEnabled());
    fullscreenRevealMarginSpinBox_->setValue(settingsController_->fullscreenRevealMargin());
    fullscreenEdgePanelsCheckBox_->setChecked(settingsController_->fullscreenEdgePanelRevealEnabled());
    fullscreenSideSelectorCheckBox_->setChecked(settingsController_->fullscreenSideSelectorEnabled());
    windowedEdgePanelsCheckBox_->setChecked(
        settingsController_->customValue(QStringLiteral("input/pointer/right_edge_windowed_enabled"), QStringLiteral("0")) != QStringLiteral("0"));
    const int defaultSidePanelIndex = defaultSidePanelComboBox_->findData(settingsController_->defaultSidePanel());
    defaultSidePanelComboBox_->setCurrentIndex(defaultSidePanelIndex >= 0 ? defaultSidePanelIndex : 0);
    historyLimitSpinBox_->setValue(settingsController_->historyLimit());
    thumbnailPreviewsCheckBox_->setChecked(settingsController_->thumbnailPreviewsEnabled());
    thumbnailWidthSpinBox_->setValue(settingsController_->thumbnailPreviewWidth());
    thumbnailPopupWidthSpinBox_->setValue(settingsController_->thumbnailPopupWidth());
    if (thumbnailPreviewSizeComboBox_ != nullptr) {
        const QString presetId = thumbnailPreviewSizePresetIdForWidths(
            settingsController_->thumbnailPreviewWidth(),
            settingsController_->thumbnailPopupWidth());
        const int presetIndex = thumbnailPreviewSizeComboBox_->findData(presetId);
        thumbnailPreviewSizeComboBox_->setCurrentIndex(presetIndex >= 0 ? presetIndex : 1);
    }
    thumbnailPopupOffsetSpinBox_->setValue(settingsController_->thumbnailPopupVerticalOffset());
    thumbnailPopupPaddingSpinBox_->setValue(settingsController_->thumbnailPopupScreenPadding());
    thumbnailPreviewSizeComboBox_->setEnabled(thumbnailPreviewsCheckBox_->isChecked());
    thumbnailWidthSpinBox_->setEnabled(thumbnailPreviewsCheckBox_->isChecked());
    thumbnailPopupWidthSpinBox_->setEnabled(thumbnailPreviewsCheckBox_->isChecked());
    thumbnailPopupOffsetSpinBox_->setEnabled(thumbnailPreviewsCheckBox_->isChecked());
    thumbnailPopupPaddingSpinBox_->setEnabled(thumbnailPreviewsCheckBox_->isChecked());
    mouseWheelVolumeStepSpinBox_->setEnabled(settingsController_->mouseWheelAction() == QStringLiteral("volume"));
    mouseWheelSeekStepSpinBox_->setEnabled(settingsController_->mouseWheelAction() == QStringLiteral("seek"));
    mouseNavigationSeekStepSpinBox_->setEnabled(settingsController_->mouseSideButtonsAction().startsWith(QStringLiteral("seek")));
    fullscreenRevealMarginSpinBox_->setEnabled(fullscreenAutoHideCheckBox_->isChecked());
    fullscreenSideSelectorCheckBox_->setEnabled(fullscreenEdgePanelsCheckBox_->isChecked());
    pointerRightEdgeWindowedActionComboBox_->setEnabled(windowedEdgePanelsCheckBox_->isChecked());
    restoreSidePanelsFromWindowStateCheckBox_->setEnabled(rememberWindowStateCheckBox_->isChecked());
    historyLimitSpinBox_->setEnabled(rememberHistoryCheckBox_->isChecked());
    clearHistoryOnExitCheckBox_->setEnabled(rememberHistoryCheckBox_->isChecked());
    clearHistoryButton_->setEnabled(rememberHistoryCheckBox_->isChecked());
    doubleClickFullscreenCheckBox_->setChecked(settingsController_->doubleClickAction() == QStringLiteral("fullscreen"));
    autoLoadSiblingMediaCheckBox_->setChecked(settingsController_->autoLoadSiblingMediaEnabled());
    showPlaylistPanelOnFolderLoadCheckBox_->setChecked(settingsController_->showPlaylistPanelOnFolderLoad());
    rotateFolderPlaylistToCurrentCheckBox_->setChecked(settingsController_->rotateFolderPlaylistToCurrent());
    videoZoomStepSpinBox_->setValue(settingsController_->videoZoomStep());
    videoMinimumZoomSpinBox_->setValue(settingsController_->videoMinimumZoom());
    videoMaximumZoomSpinBox_->setValue(settingsController_->videoMaximumZoom());
    {
        const int zoomBehaviorIndex = videoZoomDefaultBehaviorComboBox_->findData(settingsController_->videoZoomDefaultBehavior());
        videoZoomDefaultBehaviorComboBox_->setCurrentIndex(zoomBehaviorIndex >= 0 ? zoomBehaviorIndex : 0);
    }
    videoZoomResetOnFileChangeCheckBox_->setChecked(settingsController_->videoZoomResetOnFileChange());
    {
        const int zoomRememberIndex = videoZoomRememberModeComboBox_->findData(settingsController_->videoZoomRememberMode());
        videoZoomRememberModeComboBox_->setCurrentIndex(zoomRememberIndex >= 0 ? zoomRememberIndex : 0);
    }
    videoPanSensitivitySpinBox_->setValue(settingsController_->videoPanSensitivity());
    videoZoomConstrainPanningCheckBox_->setChecked(settingsController_->videoZoomConstrainPanning());
    {
        const int zoomWheelIndex = videoZoomWheelBehaviorComboBox_->findData(settingsController_->videoZoomWheelBehavior());
        videoZoomWheelBehaviorComboBox_->setCurrentIndex(zoomWheelIndex >= 0 ? zoomWheelIndex : 0);
    }
    {
        const int zoomFullscreenIndex = videoZoomFullscreenBehaviorComboBox_->findData(settingsController_->videoZoomFullscreenBehavior());
        videoZoomFullscreenBehaviorComboBox_->setCurrentIndex(zoomFullscreenIndex >= 0 ? zoomFullscreenIndex : 0);
    }
    progressTrackingModeCheckBox_->setChecked(
        settingsController_->customValue(QStringLiteral("playlist/progress_mode_enabled"), QStringLiteral("1")) != QStringLiteral("0"));
    progressCompletionThresholdSpinBox_->setValue(
        settingsController_->customValue(QStringLiteral("playlist/progress_completion_threshold"), QStringLiteral("92")).toInt());
    progressBadgesCheckBox_->setChecked(
        settingsController_->customValue(QStringLiteral("playlist/progress_show_badges"), QStringLiteral("1")) != QStringLiteral("0"));
    playlistShowFullPathsCheckBox_->setChecked(settingsController_->playlistShowFullPaths());
    playlistShowIndexPrefixesCheckBox_->setChecked(settingsController_->playlistShowIndexPrefixes());
    playlistAutoFollowCurrentCheckBox_->setChecked(settingsController_->playlistAutoFollowCurrent());
    showPlaylistPanelOnFolderLoadCheckBox_->setEnabled(autoLoadSiblingMediaCheckBox_->isChecked());
    rotateFolderPlaylistToCurrentCheckBox_->setEnabled(autoLoadSiblingMediaCheckBox_->isChecked());
    subtitleVisibleCheckBox_->setChecked(settingsController_->subtitleVisible());
    subtitleScaleSpinBox_->setValue(settingsController_->subtitleScale());
    subtitlePositionSpinBox_->setValue(settingsController_->subtitlePosition());
    subtitleFontComboBox_->setCurrentFont(QFont(settingsController_->subtitleFontFamily()));
    subtitleFontSizeSpinBox_->setValue(settingsController_->subtitleFontSize());
    const int overrideIndex = subtitleAssOverrideComboBox_->findData(settingsController_->subtitleAssOverride());
    subtitleAssOverrideComboBox_->setCurrentIndex(overrideIndex >= 0 ? overrideIndex : 0);
    subtitleAutoSelectCheckBox_->setChecked(settingsController_->subtitleAutoSelectEnabled());
    subtitlePreferExternalCheckBox_->setChecked(settingsController_->subtitlePreferExternal());
    subtitleAutoLoadLocalMatchesCheckBox_->setChecked(settingsController_->subtitleAutoLoadLocalMatches());
    subtitlePreferredLanguagesEdit_->setText(settingsController_->subtitlePreferredLanguages());
    subtitleSyncSmallStepSpinBox_->setValue(settingsController_->subtitleSyncSmallStep());
    if (subtitleAutoExtensionsEdit_ != nullptr) {
        subtitleAutoExtensionsEdit_->setText(settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleAutoExtensionsSetting),
            revaplayer::application::defaultSubtitleAutoExtensions()));
    }
    subtitleRememberTrackChoiceCheckBox_->setChecked(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleRememberTrackChoiceSetting),
        QStringLiteral("0")) != QStringLiteral("0"));
    subtitleFixTimingCheckBox_->setChecked(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleFixTimingSetting),
        QStringLiteral("0")) != QStringLiteral("0"));
    subtitleFontWeightSpinBox_->setValue(
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleFontWeightSetting),
            QStringLiteral("500")).toInt());
    subtitleItalicCheckBox_->setChecked(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleItalicSetting),
        QStringLiteral("0")) != QStringLiteral("0"));
    subtitleOutlineSizeSpinBox_->setValue(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleOutlineSizeSetting),
        QStringLiteral("1.65")).toDouble());
    subtitleBackgroundEnabledCheckBox_->setChecked(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleBackgroundEnabledSetting),
        QStringLiteral("0")) != QStringLiteral("0"));
    subtitleBackgroundOpacitySpinBox_->setValue(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleBackgroundOpacitySetting),
        QStringLiteral("68")).toInt());
    subtitleShadowEnabledCheckBox_->setChecked(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleShadowEnabledSetting),
        QStringLiteral("1")) != QStringLiteral("0"));
    subtitleShadowOffsetSpinBox_->setValue(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleShadowOffsetSetting),
        QStringLiteral("0.00")).toDouble());
    subtitleShadowBlurSpinBox_->setValue(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleShadowBlurSetting),
        QStringLiteral("0.00")).toDouble());
    subtitleLineSpacingSpinBox_->setValue(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleLineSpacingSetting),
        QStringLiteral("0.00")).toDouble());
    subtitleLetterSpacingSpinBox_->setValue(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleLetterSpacingSetting),
        QStringLiteral("0.00")).toDouble());
    subtitleMaxWidthSpinBox_->setValue(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleMaxWidthSetting),
        QStringLiteral("92.00")).toDouble());
    subtitleMarginXSpinBox_->setValue(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleMarginXSetting),
        QStringLiteral("19")).toInt());
    subtitleMarginYSpinBox_->setValue(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleMarginYSetting),
        QStringLiteral("34")).toInt());
    subtitleUseMarginsCheckBox_->setChecked(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleUseMarginsSetting),
        QStringLiteral("1")) != QStringLiteral("0"));
    subtitleScaleWithWindowCheckBox_->setChecked(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleScaleWithWindowSetting),
        QStringLiteral("1")) != QStringLiteral("0"));
    subtitleAssForceMarginsCheckBox_->setChecked(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleAssForceMarginsSetting),
        QStringLiteral("0")) != QStringLiteral("0"));
    subtitleAssJustifyCheckBox_->setChecked(settingsController_->customValue(
        QString::fromLatin1(revaplayer::application::kSubtitleAssJustifySetting),
        QStringLiteral("0")) != QStringLiteral("0"));
    updateSubtitleColorButton(
        subtitleTextColorButton_,
        QColor(settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleTextColorSetting),
            QStringLiteral("#FFFFFFFF"))));
    updateSubtitleColorButton(
        subtitleOutlineColorButton_,
        QColor(settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleOutlineColorSetting),
            QStringLiteral("#FF000000"))));
    updateSubtitleColorButton(
        subtitleBackgroundColorButton_,
        QColor(settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleBackgroundColorSetting),
            QStringLiteral("#AF000000"))));
    updateSubtitleColorButton(
        subtitleShadowColorButton_,
        QColor(settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleShadowColorSetting),
            QStringLiteral("#AF000000"))));
    sceneBrowserStepSpinBox_->setValue(settingsController_->sceneBrowserStepSeconds());
    sceneBrowserMaxItemsSpinBox_->setValue(settingsController_->sceneBrowserMaxItems());
    screenshotDirectoryEdit_->setText(settingsController_->screenshotDirectory());
    mouseGesturesCheckBox_->setChecked(
        settingsController_->customValue(QStringLiteral("input/gestures/enabled"), QStringLiteral("1")) != QStringLiteral("0"));
    mouseGestureThresholdSpinBox_->setValue(
        settingsController_->customValue(QStringLiteral("input/gestures/threshold"), QStringLiteral("54")).toInt());
    const auto applyCustomComboValue = [](QComboBox *comboBox, const QString &value, const QString &fallback) {
        if (comboBox == nullptr) {
            return;
        }
        const int valueIndex = comboBox->findData(value);
        if (valueIndex >= 0) {
            comboBox->setCurrentIndex(valueIndex);
            return;
        }
        const int fallbackIndex = comboBox->findData(fallback);
        comboBox->setCurrentIndex(fallbackIndex >= 0 ? fallbackIndex : 0);
    };
    applyCustomComboValue(
        subtitleAutoLoadModeComboBox_,
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleAutoLoadModeSetting),
            QStringLiteral("same_name_only")),
        QStringLiteral("same_name_only"));
    applyCustomComboValue(
        subtitleEncodingComboBox_,
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleCodepageSetting),
            QStringLiteral("auto")),
        QStringLiteral("auto"));
    applyCustomComboValue(
        subtitleBorderStyleComboBox_,
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleBorderStyleSetting),
            QStringLiteral("outline-and-shadow")),
        QStringLiteral("outline-and-shadow"));
    applyCustomComboValue(
        subtitleAlignXComboBox_,
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleAlignXSetting),
            QStringLiteral("center")),
        QStringLiteral("center"));
    applyCustomComboValue(
        subtitleAlignYComboBox_,
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleAlignYSetting),
            QStringLiteral("bottom")),
        QStringLiteral("bottom"));
    applyCustomComboValue(
        subtitleJustifyComboBox_,
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleJustifySetting),
            QStringLiteral("auto")),
        QStringLiteral("auto"));
    applyCustomComboValue(
        subtitleFontProviderComboBox_,
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleFontProviderSetting),
            QStringLiteral("auto")),
        QStringLiteral("auto"));
    applyCustomComboValue(
        subtitleShaperComboBox_,
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleShaperSetting),
            QStringLiteral("complex")),
        QStringLiteral("complex"));
    applyCustomComboValue(
        subtitleHintingComboBox_,
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleHintingSetting),
            QStringLiteral("none")),
        QStringLiteral("none"));
    {
        const bool legacyAutoLoadEnabled = settingsController_->subtitleAutoLoadLocalMatches();
        if (!legacyAutoLoadEnabled && subtitleAutoLoadModeComboBox_ != nullptr) {
            int manualOnlyIndex = subtitleAutoLoadModeComboBox_->findData(QStringLiteral("manual_only"));
            if (manualOnlyIndex < 0) {
                manualOnlyIndex = subtitleAutoLoadModeComboBox_->findData(QStringLiteral("disabled"));
            }
            if (manualOnlyIndex >= 0) {
                subtitleAutoLoadModeComboBox_->setCurrentIndex(manualOnlyIndex);
            }
        }
        const QString autoLoadMode = subtitleAutoLoadModeComboBox_ != nullptr
            ? subtitleAutoLoadModeComboBox_->currentData().toString()
            : QStringLiteral("manual_only");
        const bool effectiveAutoLoadEnabled = legacyAutoLoadEnabled
            && subtitleAutoLoadModeComboBox_ != nullptr
            && autoLoadMode != QStringLiteral("manual_only");
        subtitleAutoLoadLocalMatchesCheckBox_->setChecked(effectiveAutoLoadEnabled);
        if (subtitleAutoExtensionsEdit_ != nullptr) {
            subtitleAutoExtensionsEdit_->setEnabled(effectiveAutoLoadEnabled);
        }
        subtitleRememberTrackChoiceCheckBox_->setEnabled(effectiveAutoLoadEnabled);
    }
    applyCustomComboValue(
        gestureLeftActionComboBox_,
        settingsController_->customValue(QStringLiteral("input/gestures/left"), QStringLiteral("seek_backward_short")),
        QStringLiteral("seek_backward_short"));
    applyCustomComboValue(
        gestureRightActionComboBox_,
        settingsController_->customValue(QStringLiteral("input/gestures/right"), QStringLiteral("seek_forward_short")),
        QStringLiteral("seek_forward_short"));
    applyCustomComboValue(
        gestureUpActionComboBox_,
        settingsController_->customValue(QStringLiteral("input/gestures/up"), QStringLiteral("volume_up")),
        QStringLiteral("volume_up"));
    applyCustomComboValue(
        gestureDownActionComboBox_,
        settingsController_->customValue(QStringLiteral("input/gestures/down"), QStringLiteral("volume_down")),
        QStringLiteral("volume_down"));
    applyCustomComboValue(
        pointerRightEdgeFullscreenActionComboBox_,
        settingsController_->customValue(QStringLiteral("input/pointer/right_edge_action_fullscreen"), QStringLiteral("default")),
        QStringLiteral("default"));
    applyCustomComboValue(
        pointerRightEdgeWindowedActionComboBox_,
        settingsController_->customValue(QStringLiteral("input/pointer/right_edge_action_windowed"), QStringLiteral("none")),
        QStringLiteral("none"));
    applyCustomComboValue(
        pointerRightEdgeLeaveActionComboBox_,
        settingsController_->customValue(QStringLiteral("input/pointer/right_edge_leave_action"), QStringLiteral("hide_panel")),
        QStringLiteral("hide_panel"));
    applyCustomComboValue(
        mouseZoneTopActionComboBox_,
        settingsController_->customValue(QStringLiteral("input/mouse_zone/top"), QStringLiteral("top_bar")),
        QStringLiteral("top_bar"));
    applyCustomComboValue(
        mouseZoneBottomActionComboBox_,
        settingsController_->customValue(QStringLiteral("input/mouse_zone/bottom"), QStringLiteral("controls")),
        QStringLiteral("controls"));
    applyCustomComboValue(
        mouseZoneLeftActionComboBox_,
        settingsController_->customValue(QStringLiteral("input/mouse_zone/left"), QStringLiteral("none")),
        QStringLiteral("none"));
    applyCustomComboValue(
        mouseZoneRightActionComboBox_,
        settingsController_->customValue(QStringLiteral("input/mouse_zone/right"), QStringLiteral("none")),
        QStringLiteral("none"));
    applyCustomComboValue(
        mouseZoneCenterActionComboBox_,
        settingsController_->customValue(QStringLiteral("input/mouse_zone/center"), QStringLiteral("none")),
        QStringLiteral("none"));
    pointerRightEdgeMarginSpinBox_->setValue(
        settingsController_->customValue(QStringLiteral("input/pointer/right_edge_margin"), QStringLiteral("72")).toInt());
    pointerLeaveDelaySpinBox_->setValue(
        settingsController_->customValue(QStringLiteral("input/pointer/leave_delay_ms"), QStringLiteral("900")).toInt());
    pointerKeepControlsVisibleCheckBox_->setChecked(
        settingsController_->customValue(QStringLiteral("input/pointer/keep_controls_visible"), QStringLiteral("1")) != QStringLiteral("0"));
    mouseGestureThresholdSpinBox_->setEnabled(mouseGesturesCheckBox_->isChecked());
    gestureLeftActionComboBox_->setEnabled(mouseGesturesCheckBox_->isChecked());
    gestureRightActionComboBox_->setEnabled(mouseGesturesCheckBox_->isChecked());
    gestureUpActionComboBox_->setEnabled(mouseGesturesCheckBox_->isChecked());
    gestureDownActionComboBox_->setEnabled(mouseGesturesCheckBox_->isChecked());
    const bool dashboardEnabled = dashboardEnabledCheckBox_->isChecked();
    dashboardShowContinueCheckBox_->setEnabled(dashboardEnabled);
    dashboardShowRecentCheckBox_->setEnabled(dashboardEnabled);
    dashboardShowFavoritesCheckBox_->setEnabled(dashboardEnabled);
    dashboardShowSavedListsCheckBox_->setEnabled(dashboardEnabled);
    adaptiveUiBreakpointSpinBox_->setEnabled(adaptiveUiCheckBox_->isChecked());
    progressCompletionThresholdSpinBox_->setEnabled(progressTrackingModeCheckBox_->isChecked());
    progressBadgesCheckBox_->setEnabled(progressTrackingModeCheckBox_->isChecked());
    subtitleBackgroundColorButton_->setEnabled(subtitleBackgroundEnabledCheckBox_->isChecked());
    subtitleBackgroundOpacitySpinBox_->setEnabled(subtitleBackgroundEnabledCheckBox_->isChecked());
    subtitleShadowColorButton_->setEnabled(subtitleShadowEnabledCheckBox_->isChecked());
    subtitleShadowOffsetSpinBox_->setEnabled(subtitleShadowEnabledCheckBox_->isChecked());
    subtitleShadowBlurSpinBox_->setEnabled(subtitleShadowEnabledCheckBox_->isChecked());
    subtitleMarginXSpinBox_->setEnabled(subtitleUseMarginsCheckBox_->isChecked());
    subtitleMarginYSpinBox_->setEnabled(subtitleUseMarginsCheckBox_->isChecked());
    const int screenshotFormatIndex = screenshotFormatComboBox_->findData(
        settingsController_->customValue(QStringLiteral("capture/screenshot_format"), QStringLiteral("png")));
    screenshotFormatComboBox_->setCurrentIndex(screenshotFormatIndex >= 0 ? screenshotFormatIndex : 0);
    screenshotTemplateEdit_->setText(
        settingsController_->customValue(
            QStringLiteral("capture/screenshot_template"),
            QStringLiteral("{timestamp}-{title}-{index}")));
    refreshShortcutEditorState();
    refreshSettingsSearchState();
    refreshSubtitlePreviewSample();
    settingsDirty_ = false;
    if (applyButton_ != nullptr) {
        applyButton_->setEnabled(false);
    }

    if (settingsController_->isReady()) {
        const QString databasePath = settingsController_->databasePath().trimmed();
        storageInfoLabel_->setText(databasePath.isEmpty()
                ? uiText("Settings are stored locally and applied at startup.")
                : uiText("Settings are stored locally in:\n%1").arg(databasePath));
    } else {
        storageInfoLabel_->setText(
            uiText("Settings storage is currently unavailable: %1").arg(settingsController_->lastError()));
    }
}

void SettingsDialog::refreshSettingsSearchState()
{
    const QString query = settingsSearchEdit_ != nullptr
        ? settingsSearchEdit_->text().trimmed()
        : QString {};

    if (settingsSearchClearButton_ != nullptr) {
        settingsSearchClearButton_->setEnabled(!query.isEmpty());
    }

    if (settingsTabs_ == nullptr) {
        return;
    }

    int firstMatchedTab = -1;
    for (int index = 0; index < settingsPageContents_.size() && index < settingsTabs_->count(); ++index) {
        QWidget *page = settingsPageContents_.at(index);
        if (page == nullptr) {
            continue;
        }

        bool pageMatches = query.isEmpty();
        const auto directChildren = page->findChildren<QWidget *>(QString {}, Qt::FindDirectChildrenOnly);
        for (QWidget *child : directChildren) {
            if (child == nullptr) {
                continue;
            }

            const bool childMatches = query.isEmpty() || widgetTreeMatchesSearch(child, query);
            child->setVisible(childMatches);
            pageMatches = pageMatches || childMatches;
        }

        settingsTabs_->setTabEnabled(index, pageMatches);
        if (index < settingsSectionButtons_.size()) {
            if (QPushButton *sectionButton = settingsSectionButtons_.at(index); sectionButton != nullptr) {
                sectionButton->setEnabled(pageMatches);
                sectionButton->setVisible(pageMatches);
            }
        }
        if (pageMatches && firstMatchedTab < 0) {
            firstMatchedTab = index;
        }
    }

    if (firstMatchedTab >= 0 && !settingsTabs_->isTabEnabled(settingsTabs_->currentIndex())) {
        settingsTabs_->setCurrentIndex(firstMatchedTab);
    }

    if (settingsSectionTabsContainer_ != nullptr) {
        settingsSectionTabsContainer_->adjustSize();
    }

}

void SettingsDialog::refreshShortcutEditorState()
{
    const QString searchText = shortcutSearchEdit_ != nullptr
        ? shortcutSearchEdit_->text().trimmed()
        : QString {};
    const QString categoryFilter = shortcutCategoryComboBox_ != nullptr
        ? shortcutCategoryComboBox_->currentData().toString()
        : QStringLiteral("*");

    QHash<QString, QStringList> shortcutsToLabels;
    for (const auto &row : shortcutEditors_) {
        if (row.rowWidget == nullptr || row.editor == nullptr) {
            continue;
        }

        const QString localizedLabel = revaplayer::application::translateUiText(row.label);
        const bool matchesSearch = searchText.isEmpty()
            || localizedLabel.contains(searchText, Qt::CaseInsensitive)
            || row.id.contains(searchText, Qt::CaseInsensitive)
            || row.category.contains(searchText, Qt::CaseInsensitive)
            || row.editor->keySequence().toString(QKeySequence::PortableText).contains(searchText, Qt::CaseInsensitive);
        const bool matchesCategory = categoryFilter == QStringLiteral("*")
            || row.category.compare(categoryFilter, Qt::CaseInsensitive) == 0;
        row.rowWidget->setVisible(matchesSearch && matchesCategory);

        const QString portableText = row.editor->keySequence().toString(QKeySequence::PortableText).trimmed();
        if (!portableText.isEmpty()) {
            shortcutsToLabels[portableText].push_back(localizedLabel);
        }
    }

    if (shortcutConflictLabel_ == nullptr) {
        return;
    }

    QStringList conflicts;
    const auto keys = shortcutsToLabels.keys();
    for (const QString &shortcut : keys) {
        const QStringList labels = shortcutsToLabels.value(shortcut);
        if (labels.size() > 1) {
            conflicts.push_back(uiText("%1 is used by %2").arg(shortcut, labels.join(QStringLiteral(", "))));
        }
    }

    if (conflicts.isEmpty()) {
        shortcutConflictLabel_->setText(uiText("No shortcut conflicts detected."));
        shortcutConflictLabel_->setProperty("conflict", false);
    } else {
        shortcutConflictLabel_->setText(uiText("Conflicts: %1").arg(conflicts.join(QStringLiteral(" | "))));
        shortcutConflictLabel_->setProperty("conflict", true);
    }
    shortcutConflictLabel_->style()->unpolish(shortcutConflictLabel_);
    shortcutConflictLabel_->style()->polish(shortcutConflictLabel_);
}

bool SettingsDialog::validateShortcuts(QString *errorMessage) const
{
    QHash<QString, QString> assignedShortcuts;

    for (const auto &row : shortcutEditors_) {
        if (row.editor == nullptr) {
            continue;
        }

        const QString portableText = row.editor->keySequence().toString(QKeySequence::PortableText).trimmed();
        if (portableText.isEmpty()) {
            continue;
        }

        if (assignedShortcuts.contains(portableText)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("The shortcut %1 is assigned to both \"%2\" and \"%3\".")
                                    .arg(portableText,
                                         revaplayer::application::translateUiText(assignedShortcuts.value(portableText)),
                                         revaplayer::application::translateUiText(row.label));
            }
            return false;
        }

        assignedShortcuts.insert(portableText, row.label);
    }

    return true;
}

}  // namespace revaplayer::ui
