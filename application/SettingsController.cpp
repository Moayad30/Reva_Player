#include "application/SettingsController.hpp"

#include "application/PlaybackTuning.hpp"
#include "application/SubtitleStyleOptions.hpp"
#include "application/ThemeStyle.hpp"
#include "application/UiLanguage.hpp"
#include "infrastructure/storage/SqliteStore.hpp"

#include <QHash>

#include <algorithm>
#include <cmath>

namespace revaplayer::application {
namespace {

constexpr auto kLastOpenDirectorySetting = "ui/last_open_directory";
constexpr auto kRememberLastOpenDirectorySetting = "ui/remember_last_open_directory";
constexpr auto kRememberWindowStateSetting = "ui/remember_window_state";
constexpr auto kStartupWindowModeSetting = "ui/startup_window_mode";
constexpr auto kInterfaceLanguageSetting = "ui/interface_language";
constexpr auto kUiThemeSetting = "ui/theme";
constexpr auto kShowMenuBarInWindowedModeSetting = "ui/show_menu_bar_in_windowed_mode";
constexpr auto kShowMenuBarDefaultMigrationAppliedSetting = "migration/ui_show_menu_bar_default_v1_applied";
constexpr auto kShowStatusBarInWindowedModeSetting = "ui/show_status_bar_in_windowed_mode";
constexpr auto kAlwaysOnTopEnabledSetting = "ui/always_on_top_enabled";
constexpr auto kOverlayPanelsOnVideoSetting = "ui/overlay_panels_on_video";
constexpr auto kPlaylistOverlayPanelWidthSetting = "ui/playlist_overlay_panel_width";
constexpr auto kDetailsOverlayPanelWidthSetting = "ui/details_overlay_panel_width";
constexpr auto kShowPlaylistPanelOnStartupSetting = "ui/show_playlist_panel_on_startup";
constexpr auto kShowDetailsPanelOnStartupSetting = "ui/show_details_panel_on_startup";
constexpr auto kResumeEnabledSetting = "playback/resume_enabled";
constexpr auto kHistoryEnabledSetting = "playback/history_enabled";
constexpr auto kClearHistoryOnExitSetting = "playback/clear_history_on_exit";
constexpr auto kPlaybackProfileSetting = "playback/profile";
constexpr auto kUseExternalMpvConfigSetting = "playback/use_external_mpv_config";
constexpr auto kStartupVolumeSetting = "playback/startup_volume";
constexpr auto kStartupVolumeModeSetting = "playback/startup_volume_mode";
constexpr auto kStartupSpeedSetting = "playback/startup_speed";
constexpr auto kDefaultRepeatModeSetting = "playback/default_repeat_mode";
constexpr auto kThumbnailPreviewsEnabledSetting = "playback/thumbnail_previews_enabled";
constexpr auto kThumbnailPreviewWidthSetting = "playback/thumbnail_preview_width";
constexpr auto kThumbnailPopupWidthSetting = "playback/thumbnail_popup_width";
constexpr auto kThumbnailPopupVerticalOffsetSetting = "playback/thumbnail_popup_vertical_offset";
constexpr auto kThumbnailPopupScreenPaddingSetting = "playback/thumbnail_popup_screen_padding";
constexpr auto kShortSeekStepSetting = "playback/short_seek_step_seconds";
constexpr auto kLongSeekStepSetting = "playback/long_seek_step_seconds";
constexpr auto kVolumeStepSetting = "playback/volume_step";
constexpr auto kAutoLoadSiblingMediaSetting = "playlist/auto_load_sibling_media";
constexpr auto kShowPlaylistPanelOnFolderLoadSetting = "playlist/show_panel_on_folder_load";
constexpr auto kNaturalSortFolderPlaylistSetting = "playlist/natural_sort_folder_playlist";
constexpr auto kPlaylistShowFullPathsSetting = "playlist/show_full_paths";
constexpr auto kPlaylistShowIndexPrefixesSetting = "playlist/show_index_prefixes";
constexpr auto kPlaylistAutoFollowCurrentSetting = "playlist/auto_follow_current";
constexpr auto kRotateFolderPlaylistToCurrentSetting = "playlist/rotate_to_current";
constexpr auto kVideoZoomStepSetting = "video/zoom_step";
constexpr auto kVideoMinimumZoomSetting = "video/minimum_zoom";
constexpr auto kVideoMaximumZoomSetting = "video/maximum_zoom";
constexpr auto kVideoZoomDefaultBehaviorSetting = "video/zoom_default_behavior";
constexpr auto kVideoZoomResetOnFileChangeSetting = "video/zoom_reset_on_file_change";
constexpr auto kVideoZoomRememberModeSetting = "video/zoom_remember_mode";
constexpr auto kVideoPanSensitivitySetting = "video/pan_sensitivity";
constexpr auto kVideoZoomConstrainPanningSetting = "video/zoom_constrain_panning";
constexpr auto kVideoZoomWheelBehaviorSetting = "video/zoom_wheel_behavior";
constexpr auto kVideoZoomFullscreenBehaviorSetting = "video/zoom_fullscreen_behavior";
constexpr auto kMouseWheelVolumeEnabledSetting = "input/mouse_wheel_volume_enabled";
constexpr auto kMouseWheelVolumeStepSetting = "input/mouse_wheel_volume_step";
constexpr auto kMouseWheelActionSetting = "input/mouse_wheel_action";
constexpr auto kMouseWheelSeekStepSetting = "input/mouse_wheel_seek_step_seconds";
constexpr auto kMouseNavigationSeekEnabledSetting = "input/mouse_navigation_seek_enabled";
constexpr auto kMouseNavigationSeekStepSetting = "input/mouse_navigation_seek_step_seconds";
constexpr auto kMouseSideButtonsActionSetting = "input/mouse_side_buttons_action";
constexpr auto kClickActionSetting = "input/click_action";
constexpr auto kDoubleClickActionSetting = "input/double_click_action";
constexpr auto kMiddleClickActionSetting = "input/middle_click_action";
constexpr auto kActionFeedbackOverlayEnabledSetting = "ui/action_feedback_overlay_enabled";
constexpr auto kExpressiveControlLabelsEnabledSetting = "ui/expressive_control_labels_enabled";
constexpr auto kFullscreenAutoHideEnabledSetting = "ui/fullscreen_auto_hide_enabled";
constexpr auto kFullscreenRevealMarginSetting = "ui/fullscreen_reveal_margin";
constexpr auto kFullscreenEdgePanelRevealEnabledSetting = "ui/fullscreen_edge_panel_reveal_enabled";
constexpr auto kFullscreenSideSelectorEnabledSetting = "ui/fullscreen_side_selector_enabled";
constexpr auto kDefaultSidePanelSetting = "ui/default_side_panel";
constexpr auto kDefaultSidePanelUserDefinedSetting = "ui/default_side_panel_user_defined";
constexpr auto kRestoreSidePanelsFromWindowStateSetting = "ui/restore_side_panels_from_window_state";
constexpr auto kDoubleClickFullscreenEnabledSetting = "ui/double_click_fullscreen_enabled";
constexpr auto kHistoryLimitSetting = "playback/history_limit";
constexpr auto kSubtitleVisibleSetting = "subtitle/visible";
constexpr auto kSubtitleScaleSetting = "subtitle/scale";
constexpr auto kSubtitlePositionSetting = "subtitle/position";
constexpr auto kSubtitleFontFamilySetting = "subtitle/font_family";
constexpr auto kSubtitleFontSizeSetting = "subtitle/font_size";
constexpr auto kSubtitleAssOverrideSetting = "subtitle/ass_override";
constexpr auto kSubtitleAutoSelectEnabledSetting = "subtitle/auto_select_enabled";
constexpr auto kSubtitlePreferExternalSetting = "subtitle/prefer_external";
constexpr auto kSubtitleAutoLoadLocalMatchesSetting = "subtitle/auto_load_local_matches";
constexpr auto kSubtitlePreferredLanguagesSetting = "subtitle/preferred_languages";
constexpr auto kSubtitleSyncSmallStepSetting = "subtitle/sync_small_step";
constexpr auto kSubtitleDownloadCommandSetting = "subtitle/download_command";
constexpr auto kSceneBrowserStepSetting = "ui/scene_browser_step_seconds";
constexpr auto kSceneBrowserMaxItemsSetting = "ui/scene_browser_max_items";
constexpr auto kScreenshotDirectorySetting = "capture/screenshot_directory";
constexpr auto kMainWindowStateName = "main_window";

QString shortcutSettingKey(const QString &shortcutId)
{
    return QStringLiteral("shortcuts/%1").arg(shortcutId.trimmed());
}

QString normalizeRepeatMode(QString mode)
{
    mode = mode.trimmed().toLower();
    if (mode == QStringLiteral("file") || mode == QStringLiteral("playlist")) {
        return mode;
    }
    return QStringLiteral("off");
}

QString normalizeStartupWindowMode(QString mode)
{
    mode = mode.trimmed().toLower();
    if (mode == QStringLiteral("maximized") || mode == QStringLiteral("fullscreen")) {
        return mode;
    }
    return QStringLiteral("normal");
}

QString normalizeDefaultSidePanel(QString panelId)
{
    panelId = panelId.trimmed().toLower();
    if (panelId == QStringLiteral("playlist")
        || panelId == QStringLiteral("details")
        || panelId == QStringLiteral("last_opened")) {
        return panelId;
    }
    return QStringLiteral("last_opened");
}

QString normalizeWheelAction(QString actionId)
{
    actionId = actionId.trimmed().toLower();
    if (actionId == QStringLiteral("seek") || actionId == QStringLiteral("none")) {
        return actionId;
    }
    return QStringLiteral("volume");
}

bool isSupportedSingleButtonAction(const QString &actionId, const bool includeReloadFolderAction)
{
    return actionId == QStringLiteral("play_pause")
        || actionId == QStringLiteral("fullscreen")
        || actionId == QStringLiteral("mute")
        || actionId == QStringLiteral("subtitles")
        || actionId == QStringLiteral("playlist")
        || actionId == QStringLiteral("details")
        || actionId == QStringLiteral("next_playlist")
        || actionId == QStringLiteral("previous_playlist")
        || actionId == QStringLiteral("seek_forward_short")
        || actionId == QStringLiteral("seek_backward_short")
        || actionId == QStringLiteral("zoom_in")
        || actionId == QStringLiteral("zoom_out")
        || actionId == QStringLiteral("zoom_reset")
        || actionId == QStringLiteral("none")
        || (includeReloadFolderAction && actionId == QStringLiteral("reload_folder_playlist"));
}

QString normalizeSideButtonsAction(QString actionId)
{
    actionId = actionId.trimmed().toLower();
    if (actionId == QStringLiteral("seek_long")
        || actionId == QStringLiteral("chapter")
        || actionId == QStringLiteral("zoom")
        || actionId == QStringLiteral("playlist")
        || actionId == QStringLiteral("none")) {
        return actionId;
    }
    return QStringLiteral("seek_short");
}

QString normalizeClickAction(QString actionId)
{
    actionId = actionId.trimmed().toLower();
    return isSupportedSingleButtonAction(actionId, false) ? actionId : QStringLiteral("none");
}

QString normalizeDoubleClickAction(QString actionId)
{
    actionId = actionId.trimmed().toLower();
    if (isSupportedSingleButtonAction(actionId, true)) {
        return actionId;
    }
    return QStringLiteral("play_pause");
}

QString normalizeMiddleClickAction(QString actionId)
{
    actionId = actionId.trimmed().toLower();
    if (isSupportedSingleButtonAction(actionId, false)) {
        return actionId;
    }
    return QStringLiteral("mute");
}

QString normalizePreferredLanguages(QString languages)
{
    QString normalized = languages.toLower();
    normalized.replace(QChar(';'), QChar(','));
    normalized.replace(QChar('|'), QChar(','));
    normalized.replace(QChar('\n'), QChar(','));
    normalized.replace(QChar('\t'), QChar(','));
    normalized.replace(QChar(' '), QChar(','));

    QStringList items;
    for (QString token : normalized.split(QChar(','), Qt::SkipEmptyParts)) {
        token = token.trimmed();
        if (token.isEmpty() || items.contains(token)) {
            continue;
        }
        items.push_back(token);
    }

    return items.join(QStringLiteral(","));
}

double clampSubtitleSyncStep(const double seconds, const double fallback)
{
    if (!std::isfinite(seconds)) {
        return fallback;
    }

    return std::clamp(seconds, 0.05, 10.0);
}

int clampOverlayPanelWidth(const int width)
{
    return std::clamp(width, 0, 2400);
}

double clampConfiguredVideoZoomStep(const double step)
{
    if (!std::isfinite(step)) {
        return 0.20;
    }
    return std::clamp(step, 0.05, 1.0);
}

double clampConfiguredVideoZoomFactor(const double factor, const double fallback)
{
    if (!std::isfinite(factor)) {
        return fallback;
    }
    return std::clamp(factor, 1.0, 12.0);
}

double clampConfiguredPanSensitivity(const double sensitivity)
{
    if (!std::isfinite(sensitivity)) {
        return 1.0;
    }
    return std::clamp(sensitivity, 0.25, 4.0);
}

QString normalizeVideoZoomDefaultBehavior(QString behaviorId)
{
    behaviorId = behaviorId.trimmed().toLower();
    if (behaviorId == QStringLiteral("preserve_current")) {
        return behaviorId;
    }
    return QStringLiteral("fit_to_frame");
}

QString normalizeVideoZoomRememberMode(QString modeId)
{
    modeId = modeId.trimmed().toLower();
    if (modeId == QStringLiteral("session") || modeId == QStringLiteral("per_file")) {
        return modeId;
    }
    return QStringLiteral("off");
}

QString normalizeVideoZoomWheelBehavior(QString behaviorId)
{
    behaviorId = behaviorId.trimmed().toLower();
    if (behaviorId == QStringLiteral("zoom_when_zoomed") || behaviorId == QStringLiteral("zoom_with_ctrl")) {
        return behaviorId;
    }
    return QStringLiteral("global");
}

QString normalizeVideoZoomFullscreenBehavior(QString behaviorId)
{
    behaviorId = behaviorId.trimmed().toLower();
    if (behaviorId == QStringLiteral("reset_on_toggle")) {
        return behaviorId;
    }
    return QStringLiteral("keep");
}

bool seedDefaultSettings(revaplayer::infrastructure::storage::SqliteStore *store)
{
    if (store == nullptr) {
        return false;
    }

    return store->setDefaultBoolValue(QString::fromLatin1(kRememberLastOpenDirectorySetting), true)
        && store->setDefaultBoolValue(QString::fromLatin1(kRememberWindowStateSetting), true)
        && store->setDefaultStringValue(QString::fromLatin1(kStartupWindowModeSetting), QStringLiteral("normal"))
        && store->setDefaultStringValue(
            QString::fromLatin1(kInterfaceLanguageSetting),
            revaplayer::application::defaultUiLanguageId())
        && store->setDefaultStringValue(QString::fromLatin1(kUiThemeSetting), QStringLiteral("gray"))
        && store->setDefaultBoolValue(QString::fromLatin1(kShowMenuBarInWindowedModeSetting), true)
        && store->setDefaultBoolValue(QString::fromLatin1(kShowStatusBarInWindowedModeSetting), false)
        && store->setDefaultBoolValue(QString::fromLatin1(kAlwaysOnTopEnabledSetting), false)
        && store->setDefaultBoolValue(QString::fromLatin1(kOverlayPanelsOnVideoSetting), true)
        && store->setDefaultStringValue(QString::fromLatin1(kPlaylistOverlayPanelWidthSetting), QStringLiteral("0"))
        && store->setDefaultStringValue(QString::fromLatin1(kDetailsOverlayPanelWidthSetting), QStringLiteral("734"))
        && store->setDefaultBoolValue(QString::fromLatin1(kShowPlaylistPanelOnStartupSetting), false)
        && store->setDefaultBoolValue(QString::fromLatin1(kShowDetailsPanelOnStartupSetting), false)
        && store->setDefaultBoolValue(QString::fromLatin1(kResumeEnabledSetting), true)
        && store->setDefaultBoolValue(QString::fromLatin1(kHistoryEnabledSetting), true)
        && store->setDefaultBoolValue(QString::fromLatin1(kClearHistoryOnExitSetting), false)
        && store->setDefaultStringValue(QString::fromLatin1(kPlaybackProfileSetting), QStringLiteral("balanced"))
        && store->setDefaultBoolValue(QString::fromLatin1(kUseExternalMpvConfigSetting), false)
        && store->setDefaultStringValue(
            QString::fromLatin1(kStartupVolumeSetting),
            QStringLiteral("80"))
        && store->setDefaultStringValue(QString::fromLatin1(kStartupVolumeModeSetting), QStringLiteral("remember"))
        && store->setDefaultStringValue(QString::fromLatin1(kStartupSpeedSetting), QStringLiteral("1.00"))
        && store->setDefaultStringValue(QString::fromLatin1(kDefaultRepeatModeSetting), QStringLiteral("off"))
        && store->setDefaultBoolValue(QString::fromLatin1(kThumbnailPreviewsEnabledSetting), true)
        && store->setDefaultStringValue(QString::fromLatin1(kThumbnailPreviewWidthSetting), QStringLiteral("416"))
        && store->setDefaultStringValue(QString::fromLatin1(kThumbnailPopupWidthSetting), QStringLiteral("352"))
        && store->setDefaultStringValue(QString::fromLatin1(kThumbnailPopupVerticalOffsetSetting), QStringLiteral("18"))
        && store->setDefaultStringValue(QString::fromLatin1(kThumbnailPopupScreenPaddingSetting), QStringLiteral("8"))
        && store->setDefaultStringValue(QString::fromLatin1(kShortSeekStepSetting), QStringLiteral("5"))
        && store->setDefaultStringValue(QString::fromLatin1(kLongSeekStepSetting), QStringLiteral("30"))
        && store->setDefaultStringValue(QString::fromLatin1(kVolumeStepSetting), QStringLiteral("5"))
        && store->setDefaultBoolValue(QString::fromLatin1(kAutoLoadSiblingMediaSetting), true)
        && store->setDefaultBoolValue(QString::fromLatin1(kShowPlaylistPanelOnFolderLoadSetting), false)
        && store->setDefaultBoolValue(QString::fromLatin1(kNaturalSortFolderPlaylistSetting), true)
        && store->setDefaultBoolValue(QString::fromLatin1(kPlaylistShowFullPathsSetting), false)
        && store->setDefaultBoolValue(QString::fromLatin1(kPlaylistShowIndexPrefixesSetting), false)
        && store->setDefaultBoolValue(QString::fromLatin1(kPlaylistAutoFollowCurrentSetting), true)
        && store->setDefaultBoolValue(QString::fromLatin1(kRotateFolderPlaylistToCurrentSetting), true)
        && store->setDefaultStringValue(QString::fromLatin1(kVideoZoomStepSetting), QStringLiteral("0.20"))
        && store->setDefaultStringValue(QString::fromLatin1(kVideoMinimumZoomSetting), QStringLiteral("1.00"))
        && store->setDefaultStringValue(QString::fromLatin1(kVideoMaximumZoomSetting), QStringLiteral("6.00"))
        && store->setDefaultStringValue(QString::fromLatin1(kVideoZoomDefaultBehaviorSetting), QStringLiteral("fit_to_frame"))
        && store->setDefaultBoolValue(QString::fromLatin1(kVideoZoomResetOnFileChangeSetting), true)
        && store->setDefaultStringValue(QString::fromLatin1(kVideoZoomRememberModeSetting), QStringLiteral("off"))
        && store->setDefaultStringValue(QString::fromLatin1(kVideoPanSensitivitySetting), QStringLiteral("1.00"))
        && store->setDefaultBoolValue(QString::fromLatin1(kVideoZoomConstrainPanningSetting), true)
        && store->setDefaultStringValue(QString::fromLatin1(kVideoZoomWheelBehaviorSetting), QStringLiteral("global"))
        && store->setDefaultStringValue(QString::fromLatin1(kVideoZoomFullscreenBehaviorSetting), QStringLiteral("keep"))
        && store->setDefaultBoolValue(QString::fromLatin1(kMouseWheelVolumeEnabledSetting), true)
        && store->setDefaultStringValue(QString::fromLatin1(kMouseWheelVolumeStepSetting), QStringLiteral("4"))
        && store->setDefaultStringValue(QString::fromLatin1(kMouseWheelActionSetting), QStringLiteral("volume"))
        && store->setDefaultStringValue(QString::fromLatin1(kMouseWheelSeekStepSetting), QStringLiteral("10"))
        && store->setDefaultBoolValue(QString::fromLatin1(kMouseNavigationSeekEnabledSetting), true)
        && store->setDefaultStringValue(QString::fromLatin1(kMouseNavigationSeekStepSetting), QStringLiteral("5"))
        && store->setDefaultStringValue(QString::fromLatin1(kMouseSideButtonsActionSetting), QStringLiteral("seek_short"))
        && store->setDefaultStringValue(QString::fromLatin1(kClickActionSetting), QStringLiteral("none"))
        && store->setDefaultStringValue(QString::fromLatin1(kDoubleClickActionSetting), QStringLiteral("fullscreen"))
        && store->setDefaultStringValue(QString::fromLatin1(kMiddleClickActionSetting), QStringLiteral("mute"))
        && store->setDefaultBoolValue(QString::fromLatin1(kActionFeedbackOverlayEnabledSetting), true)
        && store->setDefaultBoolValue(QString::fromLatin1(kExpressiveControlLabelsEnabledSetting), true)
        && store->setDefaultBoolValue(QString::fromLatin1(kFullscreenAutoHideEnabledSetting), true)
        && store->setDefaultStringValue(QString::fromLatin1(kFullscreenRevealMarginSetting), QStringLiteral("72"))
        && store->setDefaultBoolValue(QString::fromLatin1(kFullscreenEdgePanelRevealEnabledSetting), true)
        && store->setDefaultBoolValue(QString::fromLatin1(kFullscreenSideSelectorEnabledSetting), true)
        && store->setDefaultStringValue(QString::fromLatin1(kDefaultSidePanelSetting), QStringLiteral("last_opened"))
        && store->setDefaultBoolValue(QString::fromLatin1(kDefaultSidePanelUserDefinedSetting), false)
        && store->setDefaultBoolValue(QString::fromLatin1(kRestoreSidePanelsFromWindowStateSetting), false)
        && store->setDefaultBoolValue(QString::fromLatin1(kDoubleClickFullscreenEnabledSetting), true)
        && store->setDefaultStringValue(QString::fromLatin1(kHistoryLimitSetting), QStringLiteral("120"))
        && store->setDefaultBoolValue(QString::fromLatin1(kSubtitleVisibleSetting), true)
        && store->setDefaultStringValue(QString::fromLatin1(kSubtitleScaleSetting), QStringLiteral("1.00"))
        && store->setDefaultStringValue(QString::fromLatin1(kSubtitlePositionSetting), QStringLiteral("100"))
        && store->setDefaultStringValue(QString::fromLatin1(kSubtitleFontFamilySetting), QStringLiteral("sans-serif"))
        && store->setDefaultStringValue(QString::fromLatin1(kSubtitleFontSizeSetting), QStringLiteral("38"))
        && store->setDefaultStringValue(QString::fromLatin1(kSubtitleAssOverrideSetting), QStringLiteral("scale"))
        && store->setDefaultBoolValue(QString::fromLatin1(kSubtitleAutoSelectEnabledSetting), true)
        && store->setDefaultBoolValue(QString::fromLatin1(kSubtitlePreferExternalSetting), true)
        && store->setDefaultBoolValue(QString::fromLatin1(kSubtitleAutoLoadLocalMatchesSetting), true)
        && store->setDefaultStringValue(QString::fromLatin1(kSubtitlePreferredLanguagesSetting), QStringLiteral("ar,en"))
        && store->setDefaultStringValue(QString::fromLatin1(kSubtitleSyncSmallStepSetting), QStringLiteral("0.25"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleAutoLoadModeSetting),
            QStringLiteral("same_name_only"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleAutoExtensionsSetting),
            revaplayer::application::defaultSubtitleAutoExtensions())
        && store->setDefaultBoolValue(
            QString::fromLatin1(revaplayer::application::kSubtitleRememberTrackChoiceSetting),
            false)
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleCodepageSetting),
            QStringLiteral("auto"))
        && store->setDefaultBoolValue(
            QString::fromLatin1(revaplayer::application::kSubtitleFixTimingSetting),
            false)
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleFontWeightSetting),
            QStringLiteral("500"))
        && store->setDefaultBoolValue(
            QString::fromLatin1(revaplayer::application::kSubtitleItalicSetting),
            false)
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleTextColorSetting),
            QStringLiteral("#FFFFFFFF"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleOutlineColorSetting),
            QStringLiteral("#FF000000"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleOutlineSizeSetting),
            QStringLiteral("1.65"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleBorderStyleSetting),
            QStringLiteral("outline-and-shadow"))
        && store->setDefaultBoolValue(
            QString::fromLatin1(revaplayer::application::kSubtitleBackgroundEnabledSetting),
            false)
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleBackgroundColorSetting),
            QStringLiteral("#AF000000"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleBackgroundOpacitySetting),
            QStringLiteral("68"))
        && store->setDefaultBoolValue(
            QString::fromLatin1(revaplayer::application::kSubtitleShadowEnabledSetting),
            true)
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleShadowColorSetting),
            QStringLiteral("#AF000000"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleShadowOffsetSetting),
            QStringLiteral("0.00"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleShadowBlurSetting),
            QStringLiteral("0.00"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleLineSpacingSetting),
            QStringLiteral("0.00"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleLetterSpacingSetting),
            QStringLiteral("0.00"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleMaxWidthSetting),
            QStringLiteral("92.00"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleAlignXSetting),
            QStringLiteral("center"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleAlignYSetting),
            QStringLiteral("bottom"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleJustifySetting),
            QStringLiteral("auto"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleMarginXSetting),
            QStringLiteral("19"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleMarginYSetting),
            QStringLiteral("34"))
        && store->setDefaultBoolValue(
            QString::fromLatin1(revaplayer::application::kSubtitleUseMarginsSetting),
            true)
        && store->setDefaultBoolValue(
            QString::fromLatin1(revaplayer::application::kSubtitleScaleWithWindowSetting),
            true)
        && store->setDefaultBoolValue(
            QString::fromLatin1(revaplayer::application::kSubtitleAssForceMarginsSetting),
            false)
        && store->setDefaultBoolValue(
            QString::fromLatin1(revaplayer::application::kSubtitleAssJustifySetting),
            false)
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleFontProviderSetting),
            QStringLiteral("auto"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleShaperSetting),
            QStringLiteral("complex"))
        && store->setDefaultStringValue(
            QString::fromLatin1(revaplayer::application::kSubtitleHintingSetting),
            QStringLiteral("none"))
        && store->setDefaultStringValue(QString::fromLatin1(kSubtitleDownloadCommandSetting), QString {})
        && store->setDefaultStringValue(QString::fromLatin1(kSceneBrowserStepSetting), QStringLiteral("30"))
        && store->setDefaultStringValue(QString::fromLatin1(kSceneBrowserMaxItemsSetting), QStringLiteral("24"));
}

bool applyDefaultSettingMigrations(revaplayer::infrastructure::storage::SqliteStore *store)
{
    if (store == nullptr) {
        return false;
    }

    const QString migrationKey = QString::fromLatin1(kShowMenuBarDefaultMigrationAppliedSetting);
    if (store->containsValue(migrationKey)) {
        return true;
    }

    if (!store->setBoolValue(QString::fromLatin1(kShowMenuBarInWindowedModeSetting), true)) {
        return false;
    }

    return store->setBoolValue(migrationKey, true);
}

}  // namespace

SettingsController::SettingsController(
    std::unique_ptr<revaplayer::infrastructure::storage::SqliteStore> store,
    QObject *parent)
    : QObject(parent)
    , store_(std::move(store))
{
}

SettingsController::~SettingsController() = default;

bool SettingsController::initialize()
{
    return store_ != nullptr
        && store_->initialize()
        && seedDefaultSettings(store_.get())
        && applyDefaultSettingMigrations(store_.get())
        && store_->setBoolValue(QString::fromLatin1(kUseExternalMpvConfigSetting), false)
        && store_->setBoolValue(QString::fromLatin1(kAlwaysOnTopEnabledSetting), false);
}

bool SettingsController::isReady() const
{
    return store_ != nullptr && store_->isInitialized();
}

QString SettingsController::lastError() const
{
    return store_ != nullptr ? store_->lastError() : QStringLiteral("Settings store is not configured.");
}

QString SettingsController::lastOpenDirectory() const
{
    return store_ != nullptr ? store_->stringValue(QString::fromLatin1(kLastOpenDirectorySetting)) : QString {};
}

void SettingsController::setLastOpenDirectory(const QString &directoryPath)
{
    if (store_ != nullptr && !directoryPath.trimmed().isEmpty()) {
        store_->setStringValue(QString::fromLatin1(kLastOpenDirectorySetting), directoryPath.trimmed());
    }
}

bool SettingsController::rememberLastOpenDirectory() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kRememberLastOpenDirectorySetting), true);
}

void SettingsController::setRememberLastOpenDirectory(const bool remember)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kRememberLastOpenDirectorySetting), remember);
    }
}

bool SettingsController::rememberWindowState() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kRememberWindowStateSetting), true);
}

void SettingsController::setRememberWindowState(const bool remember)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kRememberWindowStateSetting), remember);
    }
}

QString SettingsController::startupWindowMode() const
{
    return store_ != nullptr
        ? normalizeStartupWindowMode(store_->stringValue(QString::fromLatin1(kStartupWindowModeSetting), QStringLiteral("normal")))
        : QStringLiteral("normal");
}

void SettingsController::setStartupWindowMode(const QString &mode)
{
    if (store_ != nullptr) {
        store_->setStringValue(QString::fromLatin1(kStartupWindowModeSetting), normalizeStartupWindowMode(mode));
    }
}

QString SettingsController::interfaceLanguage() const
{
    return store_ != nullptr
        ? revaplayer::application::normalizeUiLanguageId(
              store_->stringValue(
                  QString::fromLatin1(kInterfaceLanguageSetting),
                  revaplayer::application::defaultUiLanguageId()))
        : revaplayer::application::defaultUiLanguageId();
}

void SettingsController::setInterfaceLanguage(const QString &languageId)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kInterfaceLanguageSetting),
            revaplayer::application::normalizeUiLanguageId(languageId));
    }
}

QString SettingsController::uiTheme() const
{
    if (store_ == nullptr) {
        return QStringLiteral("gray");
    }

    return revaplayer::application::normalizeThemeId(
        store_->stringValue(QString::fromLatin1(kUiThemeSetting), QStringLiteral("gray")));
}

void SettingsController::setUiTheme(const QString &themeId)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kUiThemeSetting),
            revaplayer::application::normalizeThemeId(themeId));
    }
}

bool SettingsController::showMenuBarInWindowedMode() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kShowMenuBarInWindowedModeSetting), true);
}

void SettingsController::setShowMenuBarInWindowedMode(const bool visible)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kShowMenuBarInWindowedModeSetting), visible);
    }
}

bool SettingsController::showStatusBarInWindowedMode() const
{
    return store_ != nullptr && store_->boolValue(QString::fromLatin1(kShowStatusBarInWindowedModeSetting), false);
}

void SettingsController::setShowStatusBarInWindowedMode(const bool visible)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kShowStatusBarInWindowedModeSetting), visible);
    }
}

bool SettingsController::alwaysOnTopEnabled() const
{
    return false;
}

void SettingsController::setAlwaysOnTopEnabled(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kAlwaysOnTopEnabledSetting), enabled && false);
    }
}

bool SettingsController::overlayPanelsOnVideo() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kOverlayPanelsOnVideoSetting), true);
}

void SettingsController::setOverlayPanelsOnVideo(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kOverlayPanelsOnVideoSetting), enabled);
    }
}

int SettingsController::playlistOverlayPanelWidth() const
{
    if (store_ == nullptr) {
        return 0;
    }

    const QString rawWidth = store_->stringValue(QString::fromLatin1(kPlaylistOverlayPanelWidthSetting), QStringLiteral("0"));
    return clampOverlayPanelWidth(rawWidth.toInt());
}

void SettingsController::setPlaylistOverlayPanelWidth(const int width)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kPlaylistOverlayPanelWidthSetting),
            QString::number(clampOverlayPanelWidth(width)));
    }
}

int SettingsController::detailsOverlayPanelWidth() const
{
    if (store_ == nullptr) {
        return 0;
    }

    const QString rawWidth = store_->stringValue(QString::fromLatin1(kDetailsOverlayPanelWidthSetting), QStringLiteral("0"));
    return clampOverlayPanelWidth(rawWidth.toInt());
}

void SettingsController::setDetailsOverlayPanelWidth(const int width)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kDetailsOverlayPanelWidthSetting),
            QString::number(clampOverlayPanelWidth(width)));
    }
}

bool SettingsController::resumeEnabled() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kResumeEnabledSetting), true);
}

void SettingsController::setResumeEnabled(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kResumeEnabledSetting), enabled);
    }
}

bool SettingsController::historyEnabled() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kHistoryEnabledSetting), true);
}

void SettingsController::setHistoryEnabled(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kHistoryEnabledSetting), enabled);
    }
}

bool SettingsController::clearHistoryOnExit() const
{
    return store_ != nullptr && store_->boolValue(QString::fromLatin1(kClearHistoryOnExitSetting), false);
}

void SettingsController::setClearHistoryOnExit(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kClearHistoryOnExitSetting), enabled);
    }
}

revaplayer::domain::PlayerProfile SettingsController::playbackProfile() const
{
    if (store_ == nullptr) {
        return revaplayer::domain::PlayerProfile::Balanced;
    }

    return revaplayer::domain::playerProfileFromId(
        store_->stringValue(QString::fromLatin1(kPlaybackProfileSetting), QStringLiteral("balanced")));
}

void SettingsController::setPlaybackProfile(const revaplayer::domain::PlayerProfile profile)
{
    if (store_ != nullptr) {
        store_->setStringValue(QString::fromLatin1(kPlaybackProfileSetting), revaplayer::domain::playerProfileId(profile));
    }
}

bool SettingsController::useExternalMpvConfig() const
{
    return false;
}

void SettingsController::setUseExternalMpvConfig(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kUseExternalMpvConfigSetting), enabled && false);
    }
}

int SettingsController::startupVolume() const
{
    if (store_ == nullptr) {
        return revaplayer::application::kDefaultPlaybackVolume;
    }

    const QString rawVolume = store_->stringValue(
        QString::fromLatin1(kStartupVolumeSetting),
        QString::number(revaplayer::application::kDefaultPlaybackVolume));
    return revaplayer::application::clampPlaybackVolume(rawVolume.toInt());
}

void SettingsController::setStartupVolume(const int volume)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kStartupVolumeSetting),
            QString::number(revaplayer::application::clampPlaybackVolume(volume)));
    }
}

bool SettingsController::rememberLastVolume() const
{
    if (store_ == nullptr) {
        return true;
    }

    const QString mode = store_->stringValue(
        QString::fromLatin1(kStartupVolumeModeSetting),
        QStringLiteral("remember")).trimmed().toLower();
    return mode != QStringLiteral("fixed");
}

void SettingsController::setRememberLastVolume(const bool remember)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kStartupVolumeModeSetting),
            remember ? QStringLiteral("remember") : QStringLiteral("fixed"));
    }
}

double SettingsController::startupPlaybackSpeed() const
{
    if (store_ == nullptr) {
        return 1.0;
    }

    const QString rawSpeed = store_->stringValue(QString::fromLatin1(kStartupSpeedSetting), QStringLiteral("1.0"));
    bool ok = false;
    const double parsedSpeed = rawSpeed.toDouble(&ok);
    return std::clamp(ok ? parsedSpeed : 1.0, 0.25, 4.0);
}

void SettingsController::setStartupPlaybackSpeed(const double speed)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kStartupSpeedSetting),
            QString::number(std::clamp(speed, 0.25, 4.0), 'f', 2));
    }
}

QString SettingsController::defaultRepeatMode() const
{
    return store_ != nullptr
        ? normalizeRepeatMode(store_->stringValue(QString::fromLatin1(kDefaultRepeatModeSetting), QStringLiteral("off")))
        : QStringLiteral("off");
}

void SettingsController::setDefaultRepeatMode(const QString &mode)
{
    if (store_ != nullptr) {
        store_->setStringValue(QString::fromLatin1(kDefaultRepeatModeSetting), normalizeRepeatMode(mode));
    }
}

bool SettingsController::thumbnailPreviewsEnabled() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kThumbnailPreviewsEnabledSetting), true);
}

void SettingsController::setThumbnailPreviewsEnabled(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kThumbnailPreviewsEnabledSetting), enabled);
    }
}

int SettingsController::thumbnailPreviewWidth() const
{
    if (store_ == nullptr) {
        return 0;
    }

    const QString rawWidth = store_->stringValue(QString::fromLatin1(kThumbnailPreviewWidthSetting), QStringLiteral("0"));
    return std::clamp(rawWidth.toInt(), 0, 1600);
}

void SettingsController::setThumbnailPreviewWidth(const int width)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kThumbnailPreviewWidthSetting),
            QString::number(std::clamp(width, 0, 1600)));
    }
}

int SettingsController::thumbnailPopupWidth() const
{
    if (store_ == nullptr) {
        return 0;
    }

    const QString rawWidth = store_->stringValue(QString::fromLatin1(kThumbnailPopupWidthSetting), QStringLiteral("0"));
    return std::clamp(rawWidth.toInt(), 0, 1400);
}

void SettingsController::setThumbnailPopupWidth(const int width)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kThumbnailPopupWidthSetting),
            QString::number(std::clamp(width, 0, 1400)));
    }
}

int SettingsController::thumbnailPopupVerticalOffset() const
{
    if (store_ == nullptr) {
        return 18;
    }

    const QString rawOffset = store_->stringValue(
        QString::fromLatin1(kThumbnailPopupVerticalOffsetSetting),
        QStringLiteral("18"));
    return std::clamp(rawOffset.toInt(), 8, 56);
}

void SettingsController::setThumbnailPopupVerticalOffset(const int pixels)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kThumbnailPopupVerticalOffsetSetting),
            QString::number(std::clamp(pixels, 8, 56)));
    }
}

int SettingsController::thumbnailPopupScreenPadding() const
{
    if (store_ == nullptr) {
        return 8;
    }

    const QString rawPadding = store_->stringValue(
        QString::fromLatin1(kThumbnailPopupScreenPaddingSetting),
        QStringLiteral("8"));
    return std::clamp(rawPadding.toInt(), 0, 32);
}

void SettingsController::setThumbnailPopupScreenPadding(const int pixels)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kThumbnailPopupScreenPaddingSetting),
            QString::number(std::clamp(pixels, 0, 32)));
    }
}

int SettingsController::shortSeekStepSeconds() const
{
    if (store_ == nullptr) {
        return 5;
    }

    const QString rawStep = store_->stringValue(QString::fromLatin1(kShortSeekStepSetting), QStringLiteral("5"));
    return std::clamp(rawStep.toInt(), 1, 30);
}

void SettingsController::setShortSeekStepSeconds(const int seconds)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kShortSeekStepSetting),
            QString::number(std::clamp(seconds, 1, 30)));
    }
}

int SettingsController::longSeekStepSeconds() const
{
    if (store_ == nullptr) {
        return 30;
    }

    const QString rawStep = store_->stringValue(QString::fromLatin1(kLongSeekStepSetting), QStringLiteral("30"));
    return std::clamp(rawStep.toInt(), 5, 300);
}

void SettingsController::setLongSeekStepSeconds(const int seconds)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kLongSeekStepSetting),
            QString::number(std::clamp(seconds, 5, 300)));
    }
}

int SettingsController::volumeStep() const
{
    if (store_ == nullptr) {
        return 5;
    }

    const QString rawStep = store_->stringValue(QString::fromLatin1(kVolumeStepSetting), QStringLiteral("5"));
    return std::clamp(rawStep.toInt(), 1, 20);
}

void SettingsController::setVolumeStep(const int step)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kVolumeStepSetting),
            QString::number(std::clamp(step, 1, 20)));
    }
}

bool SettingsController::mouseWheelVolumeEnabled() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kMouseWheelVolumeEnabledSetting), true);
}

void SettingsController::setMouseWheelVolumeEnabled(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kMouseWheelVolumeEnabledSetting), enabled);
    }
}

int SettingsController::mouseWheelVolumeStep() const
{
    if (store_ == nullptr) {
        return 4;
    }

    const QString rawStep = store_->stringValue(QString::fromLatin1(kMouseWheelVolumeStepSetting), QStringLiteral("4"));
    return std::clamp(rawStep.toInt(), 1, 20);
}

void SettingsController::setMouseWheelVolumeStep(const int step)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kMouseWheelVolumeStepSetting),
            QString::number(std::clamp(step, 1, 20)));
    }
}

QString SettingsController::mouseWheelAction() const
{
    if (store_ == nullptr) {
        return QStringLiteral("volume");
    }

    const QString stored = store_->stringValue(QString::fromLatin1(kMouseWheelActionSetting)).trimmed();
    if (stored.isEmpty()) {
        return mouseWheelVolumeEnabled() ? QStringLiteral("volume") : QStringLiteral("none");
    }

    return normalizeWheelAction(stored);
}

void SettingsController::setMouseWheelAction(const QString &actionId)
{
    if (store_ != nullptr) {
        store_->setStringValue(QString::fromLatin1(kMouseWheelActionSetting), normalizeWheelAction(actionId));
    }
}

int SettingsController::mouseWheelSeekStepSeconds() const
{
    if (store_ == nullptr) {
        return 10;
    }

    const QString rawStep = store_->stringValue(QString::fromLatin1(kMouseWheelSeekStepSetting), QStringLiteral("10"));
    return std::clamp(rawStep.toInt(), 1, 120);
}

void SettingsController::setMouseWheelSeekStepSeconds(const int seconds)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kMouseWheelSeekStepSetting),
            QString::number(std::clamp(seconds, 1, 120)));
    }
}

bool SettingsController::mouseNavigationSeekEnabled() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kMouseNavigationSeekEnabledSetting), true);
}

void SettingsController::setMouseNavigationSeekEnabled(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kMouseNavigationSeekEnabledSetting), enabled);
    }
}

int SettingsController::mouseNavigationSeekStepSeconds() const
{
    if (store_ == nullptr) {
        return 5;
    }

    const QString rawStep = store_->stringValue(QString::fromLatin1(kMouseNavigationSeekStepSetting), QStringLiteral("5"));
    return std::clamp(rawStep.toInt(), 1, 60);
}

void SettingsController::setMouseNavigationSeekStepSeconds(const int seconds)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kMouseNavigationSeekStepSetting),
            QString::number(std::clamp(seconds, 1, 60)));
    }
}

QString SettingsController::mouseSideButtonsAction() const
{
    if (store_ == nullptr) {
        return QStringLiteral("seek_short");
    }

    const QString stored = store_->stringValue(QString::fromLatin1(kMouseSideButtonsActionSetting)).trimmed();
    if (stored.isEmpty()) {
        return mouseNavigationSeekEnabled() ? QStringLiteral("seek_short") : QStringLiteral("none");
    }

    return normalizeSideButtonsAction(stored);
}

void SettingsController::setMouseSideButtonsAction(const QString &actionId)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kMouseSideButtonsActionSetting),
            normalizeSideButtonsAction(actionId));
    }
}

QString SettingsController::clickAction() const
{
    if (store_ == nullptr) {
        return QStringLiteral("none");
    }

    const QString stored = store_->stringValue(QString::fromLatin1(kClickActionSetting)).trimmed();
    return stored.isEmpty() ? QStringLiteral("none") : normalizeClickAction(stored);
}

void SettingsController::setClickAction(const QString &actionId)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kClickActionSetting),
            normalizeClickAction(actionId));
    }
}

QString SettingsController::doubleClickAction() const
{
    if (store_ == nullptr) {
        return QStringLiteral("play_pause");
    }

    const QString stored = store_->stringValue(QString::fromLatin1(kDoubleClickActionSetting)).trimmed();
    if (stored.isEmpty()) {
        return QStringLiteral("play_pause");
    }

    return normalizeDoubleClickAction(stored);
}

void SettingsController::setDoubleClickAction(const QString &actionId)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kDoubleClickActionSetting),
            normalizeDoubleClickAction(actionId));
    }
}

QString SettingsController::middleClickAction() const
{
    return store_ != nullptr
        ? normalizeMiddleClickAction(store_->stringValue(QString::fromLatin1(kMiddleClickActionSetting), QStringLiteral("mute")))
        : QStringLiteral("mute");
}

void SettingsController::setMiddleClickAction(const QString &actionId)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kMiddleClickActionSetting),
            normalizeMiddleClickAction(actionId));
    }
}

bool SettingsController::actionFeedbackOverlayEnabled() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kActionFeedbackOverlayEnabledSetting), true);
}

void SettingsController::setActionFeedbackOverlayEnabled(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kActionFeedbackOverlayEnabledSetting), enabled);
    }
}

bool SettingsController::expressiveControlLabelsEnabled() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kExpressiveControlLabelsEnabledSetting), true);
}

void SettingsController::setExpressiveControlLabelsEnabled(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kExpressiveControlLabelsEnabledSetting), enabled);
    }
}

bool SettingsController::fullscreenAutoHideEnabled() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kFullscreenAutoHideEnabledSetting), true);
}

void SettingsController::setFullscreenAutoHideEnabled(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kFullscreenAutoHideEnabledSetting), enabled);
    }
}

int SettingsController::fullscreenRevealMargin() const
{
    if (store_ == nullptr) {
        return 72;
    }

    const QString rawMargin = store_->stringValue(QString::fromLatin1(kFullscreenRevealMarginSetting), QStringLiteral("72"));
    return std::clamp(rawMargin.toInt(), 16, 240);
}

void SettingsController::setFullscreenRevealMargin(const int pixels)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kFullscreenRevealMarginSetting),
            QString::number(std::clamp(pixels, 16, 240)));
    }
}

bool SettingsController::fullscreenEdgePanelRevealEnabled() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kFullscreenEdgePanelRevealEnabledSetting), true);
}

void SettingsController::setFullscreenEdgePanelRevealEnabled(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kFullscreenEdgePanelRevealEnabledSetting), enabled);
    }
}

bool SettingsController::fullscreenSideSelectorEnabled() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kFullscreenSideSelectorEnabledSetting), true);
}

void SettingsController::setFullscreenSideSelectorEnabled(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kFullscreenSideSelectorEnabledSetting), enabled);
    }
}

QString SettingsController::defaultSidePanel() const
{
    if (store_ == nullptr) {
        return QStringLiteral("last_opened");
    }

    const QString storedPanel = normalizeDefaultSidePanel(
        store_->stringValue(QString::fromLatin1(kDefaultSidePanelSetting), QStringLiteral("last_opened")));
    const bool userDefined = store_->boolValue(QString::fromLatin1(kDefaultSidePanelUserDefinedSetting), false);
    if (!userDefined && storedPanel == QStringLiteral("playlist")) {
        return QStringLiteral("last_opened");
    }
    return storedPanel;
}

void SettingsController::setDefaultSidePanel(const QString &panelId)
{
    if (store_ != nullptr) {
        store_->setStringValue(QString::fromLatin1(kDefaultSidePanelSetting), normalizeDefaultSidePanel(panelId));
        store_->setBoolValue(QString::fromLatin1(kDefaultSidePanelUserDefinedSetting), true);
    }
}

bool SettingsController::restoreSidePanelsFromWindowState() const
{
    return store_ != nullptr && store_->boolValue(QString::fromLatin1(kRestoreSidePanelsFromWindowStateSetting), false);
}

void SettingsController::setRestoreSidePanelsFromWindowState(const bool restore)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kRestoreSidePanelsFromWindowStateSetting), restore);
    }
}

bool SettingsController::doubleClickFullscreenEnabled() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kDoubleClickFullscreenEnabledSetting), true);
}

void SettingsController::setDoubleClickFullscreenEnabled(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kDoubleClickFullscreenEnabledSetting), enabled);
    }
}

int SettingsController::historyLimit() const
{
    if (store_ == nullptr) {
        return 120;
    }

    const QString rawLimit = store_->stringValue(QString::fromLatin1(kHistoryLimitSetting), QStringLiteral("120"));
    return std::clamp(rawLimit.toInt(), 20, 500);
}

void SettingsController::setHistoryLimit(const int limit)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kHistoryLimitSetting),
            QString::number(std::clamp(limit, 20, 500)));
    }
}

bool SettingsController::autoLoadSiblingMediaEnabled() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kAutoLoadSiblingMediaSetting), true);
}

void SettingsController::setAutoLoadSiblingMediaEnabled(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kAutoLoadSiblingMediaSetting), enabled);
    }
}

bool SettingsController::showPlaylistPanelOnFolderLoad() const
{
    return store_ != nullptr && store_->boolValue(QString::fromLatin1(kShowPlaylistPanelOnFolderLoadSetting), false);
}

void SettingsController::setShowPlaylistPanelOnFolderLoad(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kShowPlaylistPanelOnFolderLoadSetting), enabled);
    }
}

bool SettingsController::naturalSortFolderPlaylistEnabled() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kNaturalSortFolderPlaylistSetting), true);
}

void SettingsController::setNaturalSortFolderPlaylistEnabled(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kNaturalSortFolderPlaylistSetting), enabled);
    }
}

bool SettingsController::playlistShowFullPaths() const
{
    return store_ != nullptr && store_->boolValue(QString::fromLatin1(kPlaylistShowFullPathsSetting), false);
}

void SettingsController::setPlaylistShowFullPaths(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kPlaylistShowFullPathsSetting), enabled);
    }
}

bool SettingsController::playlistShowIndexPrefixes() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kPlaylistShowIndexPrefixesSetting), true);
}

void SettingsController::setPlaylistShowIndexPrefixes(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kPlaylistShowIndexPrefixesSetting), enabled);
    }
}

bool SettingsController::playlistAutoFollowCurrent() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kPlaylistAutoFollowCurrentSetting), true);
}

void SettingsController::setPlaylistAutoFollowCurrent(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kPlaylistAutoFollowCurrentSetting), enabled);
    }
}

bool SettingsController::rotateFolderPlaylistToCurrent() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kRotateFolderPlaylistToCurrentSetting), true);
}

void SettingsController::setRotateFolderPlaylistToCurrent(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kRotateFolderPlaylistToCurrentSetting), enabled);
    }
}

double SettingsController::videoZoomStep() const
{
    if (store_ == nullptr) {
        return 0.20;
    }

    bool ok = false;
    const double parsed = store_->stringValue(QString::fromLatin1(kVideoZoomStepSetting), QStringLiteral("0.20")).toDouble(&ok);
    return clampConfiguredVideoZoomStep(ok ? parsed : 0.20);
}

void SettingsController::setVideoZoomStep(const double step)
{
    if (store_ != nullptr) {
        store_->setStringValue(QString::fromLatin1(kVideoZoomStepSetting), QString::number(clampConfiguredVideoZoomStep(step), 'f', 2));
    }
}

double SettingsController::videoMinimumZoom() const
{
    if (store_ == nullptr) {
        return 1.0;
    }

    bool ok = false;
    const double parsed = store_->stringValue(QString::fromLatin1(kVideoMinimumZoomSetting), QStringLiteral("1.00")).toDouble(&ok);
    return clampConfiguredVideoZoomFactor(ok ? parsed : 1.0, 1.0);
}

void SettingsController::setVideoMinimumZoom(const double factor)
{
    if (store_ == nullptr) {
        return;
    }

    const double minimum = clampConfiguredVideoZoomFactor(factor, 1.0);
    const double maximum = std::max(minimum, videoMaximumZoom());
    store_->setStringValue(QString::fromLatin1(kVideoMinimumZoomSetting), QString::number(minimum, 'f', 2));
    store_->setStringValue(QString::fromLatin1(kVideoMaximumZoomSetting), QString::number(maximum, 'f', 2));
}

double SettingsController::videoMaximumZoom() const
{
    if (store_ == nullptr) {
        return 6.0;
    }

    bool ok = false;
    const double parsed = store_->stringValue(QString::fromLatin1(kVideoMaximumZoomSetting), QStringLiteral("6.00")).toDouble(&ok);
    return std::max(videoMinimumZoom(), clampConfiguredVideoZoomFactor(ok ? parsed : 6.0, 6.0));
}

void SettingsController::setVideoMaximumZoom(const double factor)
{
    if (store_ != nullptr) {
        const double maximum = std::max(videoMinimumZoom(), clampConfiguredVideoZoomFactor(factor, 6.0));
        store_->setStringValue(QString::fromLatin1(kVideoMaximumZoomSetting), QString::number(maximum, 'f', 2));
    }
}

QString SettingsController::videoZoomDefaultBehavior() const
{
    return store_ != nullptr
        ? normalizeVideoZoomDefaultBehavior(store_->stringValue(QString::fromLatin1(kVideoZoomDefaultBehaviorSetting), QStringLiteral("fit_to_frame")))
        : QStringLiteral("fit_to_frame");
}

void SettingsController::setVideoZoomDefaultBehavior(const QString &behaviorId)
{
    if (store_ != nullptr) {
        store_->setStringValue(QString::fromLatin1(kVideoZoomDefaultBehaviorSetting), normalizeVideoZoomDefaultBehavior(behaviorId));
    }
}

bool SettingsController::videoZoomResetOnFileChange() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kVideoZoomResetOnFileChangeSetting), true);
}

void SettingsController::setVideoZoomResetOnFileChange(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kVideoZoomResetOnFileChangeSetting), enabled);
    }
}

QString SettingsController::videoZoomRememberMode() const
{
    return store_ != nullptr
        ? normalizeVideoZoomRememberMode(store_->stringValue(QString::fromLatin1(kVideoZoomRememberModeSetting), QStringLiteral("off")))
        : QStringLiteral("off");
}

void SettingsController::setVideoZoomRememberMode(const QString &modeId)
{
    if (store_ != nullptr) {
        store_->setStringValue(QString::fromLatin1(kVideoZoomRememberModeSetting), normalizeVideoZoomRememberMode(modeId));
    }
}

double SettingsController::videoPanSensitivity() const
{
    if (store_ == nullptr) {
        return 1.0;
    }

    bool ok = false;
    const double parsed = store_->stringValue(QString::fromLatin1(kVideoPanSensitivitySetting), QStringLiteral("1.00")).toDouble(&ok);
    return clampConfiguredPanSensitivity(ok ? parsed : 1.0);
}

void SettingsController::setVideoPanSensitivity(const double sensitivity)
{
    if (store_ != nullptr) {
        store_->setStringValue(QString::fromLatin1(kVideoPanSensitivitySetting), QString::number(clampConfiguredPanSensitivity(sensitivity), 'f', 2));
    }
}

bool SettingsController::videoZoomConstrainPanning() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kVideoZoomConstrainPanningSetting), true);
}

void SettingsController::setVideoZoomConstrainPanning(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kVideoZoomConstrainPanningSetting), enabled);
    }
}

QString SettingsController::videoZoomWheelBehavior() const
{
    return store_ != nullptr
        ? normalizeVideoZoomWheelBehavior(store_->stringValue(QString::fromLatin1(kVideoZoomWheelBehaviorSetting), QStringLiteral("global")))
        : QStringLiteral("global");
}

void SettingsController::setVideoZoomWheelBehavior(const QString &behaviorId)
{
    if (store_ != nullptr) {
        store_->setStringValue(QString::fromLatin1(kVideoZoomWheelBehaviorSetting), normalizeVideoZoomWheelBehavior(behaviorId));
    }
}

QString SettingsController::videoZoomFullscreenBehavior() const
{
    return store_ != nullptr
        ? normalizeVideoZoomFullscreenBehavior(store_->stringValue(QString::fromLatin1(kVideoZoomFullscreenBehaviorSetting), QStringLiteral("keep")))
        : QStringLiteral("keep");
}

void SettingsController::setVideoZoomFullscreenBehavior(const QString &behaviorId)
{
    if (store_ != nullptr) {
        store_->setStringValue(QString::fromLatin1(kVideoZoomFullscreenBehaviorSetting), normalizeVideoZoomFullscreenBehavior(behaviorId));
    }
}

bool SettingsController::subtitleVisible() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kSubtitleVisibleSetting), true);
}

void SettingsController::setSubtitleVisible(const bool visible)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kSubtitleVisibleSetting), visible);
    }
}

double SettingsController::subtitleScale() const
{
    if (store_ == nullptr) {
        return 1.0;
    }

    bool ok = false;
    const double rawScale = store_->stringValue(QString::fromLatin1(kSubtitleScaleSetting), QStringLiteral("1.0")).toDouble(&ok);
    return revaplayer::application::clampSubtitleScale(ok ? rawScale : 1.0);
}

void SettingsController::setSubtitleScale(const double scale)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kSubtitleScaleSetting),
            QString::number(revaplayer::application::clampSubtitleScale(scale), 'f', 2));
    }
}

int SettingsController::subtitlePosition() const
{
    if (store_ == nullptr) {
        return 100;
    }

    return revaplayer::application::clampSubtitlePosition(
        store_->stringValue(QString::fromLatin1(kSubtitlePositionSetting), QStringLiteral("100")).toInt());
}

void SettingsController::setSubtitlePosition(const int position)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kSubtitlePositionSetting),
            QString::number(revaplayer::application::clampSubtitlePosition(position)));
    }
}

QString SettingsController::subtitleFontFamily() const
{
    if (store_ == nullptr) {
        return QStringLiteral("sans-serif");
    }

    const QString family = store_->stringValue(QString::fromLatin1(kSubtitleFontFamilySetting)).trimmed();
    return family.isEmpty() ? QStringLiteral("sans-serif") : family;
}

void SettingsController::setSubtitleFontFamily(const QString &fontFamily)
{
    if (store_ != nullptr) {
        const QString family = fontFamily.trimmed();
        store_->setStringValue(QString::fromLatin1(kSubtitleFontFamilySetting), family);
    }
}

int SettingsController::subtitleFontSize() const
{
    if (store_ == nullptr) {
        return 38;
    }

    return revaplayer::application::clampSubtitleFontSize(
        store_->stringValue(QString::fromLatin1(kSubtitleFontSizeSetting), QStringLiteral("38")).toInt());
}

void SettingsController::setSubtitleFontSize(const int fontSize)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kSubtitleFontSizeSetting),
            QString::number(revaplayer::application::clampSubtitleFontSize(fontSize)));
    }
}

QString SettingsController::subtitleAssOverride() const
{
    return store_ != nullptr
        ? revaplayer::application::normalizeSubtitleAssOverride(
              store_->stringValue(QString::fromLatin1(kSubtitleAssOverrideSetting), QStringLiteral("scale")))
        : QStringLiteral("scale");
}

void SettingsController::setSubtitleAssOverride(const QString &mode)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kSubtitleAssOverrideSetting),
            revaplayer::application::normalizeSubtitleAssOverride(mode));
    }
}

bool SettingsController::subtitleAutoSelectEnabled() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kSubtitleAutoSelectEnabledSetting), true);
}

void SettingsController::setSubtitleAutoSelectEnabled(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kSubtitleAutoSelectEnabledSetting), enabled);
    }
}

bool SettingsController::subtitlePreferExternal() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kSubtitlePreferExternalSetting), true);
}

void SettingsController::setSubtitlePreferExternal(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kSubtitlePreferExternalSetting), enabled);
    }
}

bool SettingsController::subtitleAutoLoadLocalMatches() const
{
    return store_ == nullptr || store_->boolValue(QString::fromLatin1(kSubtitleAutoLoadLocalMatchesSetting), true);
}

void SettingsController::setSubtitleAutoLoadLocalMatches(const bool enabled)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kSubtitleAutoLoadLocalMatchesSetting), enabled);
    }
}

QString SettingsController::subtitlePreferredLanguages() const
{
    return store_ != nullptr
        ? normalizePreferredLanguages(
              store_->stringValue(QString::fromLatin1(kSubtitlePreferredLanguagesSetting), QStringLiteral("ar,en")))
        : QStringLiteral("ar,en");
}

void SettingsController::setSubtitlePreferredLanguages(const QString &languages)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kSubtitlePreferredLanguagesSetting),
            normalizePreferredLanguages(languages));
    }
}

double SettingsController::subtitleSyncSmallStep() const
{
    if (store_ == nullptr) {
        return 0.25;
    }

    bool ok = false;
    const double parsed = store_->stringValue(QString::fromLatin1(kSubtitleSyncSmallStepSetting), QStringLiteral("0.25")).toDouble(&ok);
    return clampSubtitleSyncStep(ok ? parsed : 0.25, 0.25);
}

void SettingsController::setSubtitleSyncSmallStep(const double seconds)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kSubtitleSyncSmallStepSetting),
            QString::number(clampSubtitleSyncStep(seconds, 0.25), 'f', 2));
    }
}

QString SettingsController::subtitleDownloadCommand() const
{
    return store_ != nullptr ? store_->stringValue(QString::fromLatin1(kSubtitleDownloadCommandSetting)).trimmed() : QString {};
}

void SettingsController::setSubtitleDownloadCommand(const QString &command)
{
    if (store_ != nullptr) {
        store_->setStringValue(QString::fromLatin1(kSubtitleDownloadCommandSetting), command.trimmed());
    }
}

int SettingsController::sceneBrowserStepSeconds() const
{
    if (store_ == nullptr) {
        return 30;
    }

    return std::clamp(
        store_->stringValue(QString::fromLatin1(kSceneBrowserStepSetting), QStringLiteral("30")).toInt(),
        1,
        600);
}

void SettingsController::setSceneBrowserStepSeconds(const int seconds)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kSceneBrowserStepSetting),
            QString::number(std::clamp(seconds, 1, 600)));
    }
}

int SettingsController::sceneBrowserMaxItems() const
{
    if (store_ == nullptr) {
        return 24;
    }

    return std::clamp(
        store_->stringValue(QString::fromLatin1(kSceneBrowserMaxItemsSetting), QStringLiteral("24")).toInt(),
        8,
        72);
}

void SettingsController::setSceneBrowserMaxItems(const int count)
{
    if (store_ != nullptr) {
        store_->setStringValue(
            QString::fromLatin1(kSceneBrowserMaxItemsSetting),
            QString::number(std::clamp(count, 8, 72)));
    }
}

QString SettingsController::screenshotDirectory() const
{
    return store_ != nullptr ? store_->stringValue(QString::fromLatin1(kScreenshotDirectorySetting)) : QString {};
}

void SettingsController::setScreenshotDirectory(const QString &directoryPath)
{
    if (store_ != nullptr) {
        store_->setStringValue(QString::fromLatin1(kScreenshotDirectorySetting), directoryPath.trimmed());
    }
}

QString SettingsController::shortcutOverride(const QString &shortcutId) const
{
    if (store_ == nullptr || shortcutId.trimmed().isEmpty()) {
        return {};
    }

    return store_->stringValue(shortcutSettingKey(shortcutId)).trimmed();
}

void SettingsController::setShortcutOverride(const QString &shortcutId, const QString &portableShortcut)
{
    if (store_ == nullptr || shortcutId.trimmed().isEmpty()) {
        return;
    }

    store_->setStringValue(shortcutSettingKey(shortcutId), portableShortcut.trimmed());
}

QString SettingsController::customValue(const QString &key, const QString &defaultValue) const
{
    return store_ != nullptr && !key.trimmed().isEmpty()
        ? store_->stringValue(key.trimmed(), defaultValue)
        : defaultValue;
}

bool SettingsController::setCustomValue(const QString &key, const QString &value)
{
    if (store_ != nullptr && !key.trimmed().isEmpty()) {
        return store_->setStringValue(key.trimmed(), value);
    }
    return false;
}

bool SettingsController::removeCustomValue(const QString &key)
{
    return store_ != nullptr && !key.trimmed().isEmpty() && store_->removeValue(key.trimmed());
}

QStringList SettingsController::customKeys(const QString &prefix) const
{
    return store_ != nullptr && !prefix.trimmed().isEmpty()
        ? store_->keysWithPrefix(prefix.trimmed())
        : QStringList {};
}

bool SettingsController::showPlaylistPanelOnStartup() const
{
    return store_ != nullptr && store_->boolValue(QString::fromLatin1(kShowPlaylistPanelOnStartupSetting), false);
}

void SettingsController::setShowPlaylistPanelOnStartup(const bool visible)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kShowPlaylistPanelOnStartupSetting), visible);
    }
}

bool SettingsController::showDetailsPanelOnStartup() const
{
    return store_ != nullptr && store_->boolValue(QString::fromLatin1(kShowDetailsPanelOnStartupSetting), false);
}

void SettingsController::setShowDetailsPanelOnStartup(const bool visible)
{
    if (store_ != nullptr) {
        store_->setBoolValue(QString::fromLatin1(kShowDetailsPanelOnStartupSetting), visible);
    }
}

std::optional<revaplayer::infrastructure::storage::WindowStateRecord> SettingsController::mainWindowState() const
{
    return store_ != nullptr
        ? store_->loadWindowState(QString::fromLatin1(kMainWindowStateName))
        : std::nullopt;
}

void SettingsController::saveMainWindowState(const QByteArray &geometry,
                                             const QByteArray &state,
                                             const bool maximized,
                                             const bool fullscreen)
{
    if (store_ != nullptr) {
        store_->saveWindowState(QString::fromLatin1(kMainWindowStateName), geometry, state, maximized, fullscreen);
    }
}

void SettingsController::clearMainWindowState()
{
    if (store_ != nullptr) {
        store_->clearWindowState(QString::fromLatin1(kMainWindowStateName));
    }
}

bool SettingsController::resetApplicationData()
{
    return store_ != nullptr
        && store_->resetApplicationData()
        && seedDefaultSettings(store_.get())
        && applyDefaultSettingMigrations(store_.get());
}

bool SettingsController::resetSettingsToDefaults()
{
    if (store_ == nullptr) {
        return false;
    }

    static const QStringList kPreservedPrefixes {
        QStringLiteral("pinned_course/"),
        QStringLiteral("playlist_snapshot/"),
        QStringLiteral("smart_playlist_rule/"),
        QStringLiteral("playlist_view_preset/"),
    };

    QHash<QString, QString> preservedSettings;
    for (const QString &prefix : kPreservedPrefixes) {
        const QStringList keys = store_->keysWithPrefix(prefix);
        for (const QString &key : keys) {
            preservedSettings.insert(key, store_->stringValue(key));
        }
    }

    if (!store_->resetSettingsOnly()
        || !seedDefaultSettings(store_.get())
        || !applyDefaultSettingMigrations(store_.get())
        || !store_->setBoolValue(QString::fromLatin1(kUseExternalMpvConfigSetting), false)
        || !store_->setBoolValue(QString::fromLatin1(kAlwaysOnTopEnabledSetting), false)) {
        return false;
    }

    for (auto it = preservedSettings.cbegin(); it != preservedSettings.cend(); ++it) {
        if (!store_->setStringValue(it.key(), it.value())) {
            return false;
        }
    }

    return true;
}

QString SettingsController::databasePath() const
{
    return store_ != nullptr ? store_->databasePath() : QString {};
}

}  // namespace revaplayer::application
