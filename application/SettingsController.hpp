#pragma once

#include "domain/CustomCommand.hpp"
#include "domain/PlayerProfile.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

class QByteArray;

namespace revaplayer::infrastructure::storage {
class SqliteStore;
struct WindowStateRecord;
}

namespace revaplayer::application {

class SettingsController final : public QObject {
    Q_OBJECT

public:
    explicit SettingsController(std::unique_ptr<revaplayer::infrastructure::storage::SqliteStore> store,
                                QObject *parent = nullptr);
    ~SettingsController() override;

    [[nodiscard]] bool initialize();
    [[nodiscard]] bool isReady() const;
    [[nodiscard]] QString lastError() const;

    [[nodiscard]] QString lastOpenDirectory() const;
    void setLastOpenDirectory(const QString &directoryPath);

    [[nodiscard]] bool rememberLastOpenDirectory() const;
    void setRememberLastOpenDirectory(bool remember);

    [[nodiscard]] bool rememberWindowState() const;
    void setRememberWindowState(bool remember);
    [[nodiscard]] QString startupWindowMode() const;
    void setStartupWindowMode(const QString &mode);
    [[nodiscard]] QString interfaceLanguage() const;
    void setInterfaceLanguage(const QString &languageId);
    [[nodiscard]] QString uiTheme() const;
    void setUiTheme(const QString &themeId);
    [[nodiscard]] bool showMenuBarInWindowedMode() const;
    void setShowMenuBarInWindowedMode(bool visible);
    [[nodiscard]] bool showStatusBarInWindowedMode() const;
    void setShowStatusBarInWindowedMode(bool visible);
    [[nodiscard]] bool alwaysOnTopEnabled() const;
    void setAlwaysOnTopEnabled(bool enabled);
    [[nodiscard]] bool overlayPanelsOnVideo() const;
    void setOverlayPanelsOnVideo(bool enabled);
    [[nodiscard]] int playlistOverlayPanelWidth() const;
    void setPlaylistOverlayPanelWidth(int width);
    [[nodiscard]] int detailsOverlayPanelWidth() const;
    void setDetailsOverlayPanelWidth(int width);

    [[nodiscard]] bool resumeEnabled() const;
    void setResumeEnabled(bool enabled);

    [[nodiscard]] bool historyEnabled() const;
    void setHistoryEnabled(bool enabled);
    [[nodiscard]] bool clearHistoryOnExit() const;
    void setClearHistoryOnExit(bool enabled);

    [[nodiscard]] revaplayer::domain::PlayerProfile playbackProfile() const;
    void setPlaybackProfile(revaplayer::domain::PlayerProfile profile);
    [[nodiscard]] bool useExternalMpvConfig() const;
    void setUseExternalMpvConfig(bool enabled);

    [[nodiscard]] int startupVolume() const;
    void setStartupVolume(int volume);
    [[nodiscard]] bool rememberLastVolume() const;
    void setRememberLastVolume(bool remember);
    [[nodiscard]] double startupPlaybackSpeed() const;
    void setStartupPlaybackSpeed(double speed);
    [[nodiscard]] QString defaultRepeatMode() const;
    void setDefaultRepeatMode(const QString &mode);
    [[nodiscard]] bool thumbnailPreviewsEnabled() const;
    void setThumbnailPreviewsEnabled(bool enabled);
    [[nodiscard]] int thumbnailPreviewWidth() const;
    void setThumbnailPreviewWidth(int width);
    [[nodiscard]] int thumbnailPopupWidth() const;
    void setThumbnailPopupWidth(int width);
    [[nodiscard]] int thumbnailPopupVerticalOffset() const;
    void setThumbnailPopupVerticalOffset(int pixels);
    [[nodiscard]] int thumbnailPopupScreenPadding() const;
    void setThumbnailPopupScreenPadding(int pixels);
    [[nodiscard]] int shortSeekStepSeconds() const;
    void setShortSeekStepSeconds(int seconds);
    [[nodiscard]] int longSeekStepSeconds() const;
    void setLongSeekStepSeconds(int seconds);
    [[nodiscard]] int volumeStep() const;
    void setVolumeStep(int step);
    [[nodiscard]] bool mouseWheelVolumeEnabled() const;
    void setMouseWheelVolumeEnabled(bool enabled);
    [[nodiscard]] int mouseWheelVolumeStep() const;
    void setMouseWheelVolumeStep(int step);
    [[nodiscard]] QString mouseWheelAction() const;
    void setMouseWheelAction(const QString &actionId);
    [[nodiscard]] int mouseWheelSeekStepSeconds() const;
    void setMouseWheelSeekStepSeconds(int seconds);
    [[nodiscard]] bool mouseNavigationSeekEnabled() const;
    void setMouseNavigationSeekEnabled(bool enabled);
    [[nodiscard]] int mouseNavigationSeekStepSeconds() const;
    void setMouseNavigationSeekStepSeconds(int seconds);
    [[nodiscard]] QString mouseSideButtonsAction() const;
    void setMouseSideButtonsAction(const QString &actionId);
    [[nodiscard]] QString clickAction() const;
    void setClickAction(const QString &actionId);
    [[nodiscard]] QString doubleClickAction() const;
    void setDoubleClickAction(const QString &actionId);
    [[nodiscard]] QString middleClickAction() const;
    void setMiddleClickAction(const QString &actionId);
    [[nodiscard]] bool actionFeedbackOverlayEnabled() const;
    void setActionFeedbackOverlayEnabled(bool enabled);
    [[nodiscard]] bool expressiveControlLabelsEnabled() const;
    void setExpressiveControlLabelsEnabled(bool enabled);
    [[nodiscard]] bool fullscreenAutoHideEnabled() const;
    void setFullscreenAutoHideEnabled(bool enabled);
    [[nodiscard]] int fullscreenRevealMargin() const;
    void setFullscreenRevealMargin(int pixels);
    [[nodiscard]] bool fullscreenEdgePanelRevealEnabled() const;
    void setFullscreenEdgePanelRevealEnabled(bool enabled);
    [[nodiscard]] bool fullscreenSideSelectorEnabled() const;
    void setFullscreenSideSelectorEnabled(bool enabled);
    [[nodiscard]] QString defaultSidePanel() const;
    void setDefaultSidePanel(const QString &panelId);
    [[nodiscard]] bool restoreSidePanelsFromWindowState() const;
    void setRestoreSidePanelsFromWindowState(bool restore);
    [[nodiscard]] bool doubleClickFullscreenEnabled() const;
    void setDoubleClickFullscreenEnabled(bool enabled);
    [[nodiscard]] int historyLimit() const;
    void setHistoryLimit(int limit);
    [[nodiscard]] bool autoLoadSiblingMediaEnabled() const;
    void setAutoLoadSiblingMediaEnabled(bool enabled);
    [[nodiscard]] bool showPlaylistPanelOnFolderLoad() const;
    void setShowPlaylistPanelOnFolderLoad(bool enabled);
    [[nodiscard]] bool naturalSortFolderPlaylistEnabled() const;
    void setNaturalSortFolderPlaylistEnabled(bool enabled);
    [[nodiscard]] bool playlistShowFullPaths() const;
    void setPlaylistShowFullPaths(bool enabled);
    [[nodiscard]] bool playlistShowIndexPrefixes() const;
    void setPlaylistShowIndexPrefixes(bool enabled);
    [[nodiscard]] bool playlistAutoFollowCurrent() const;
    void setPlaylistAutoFollowCurrent(bool enabled);
    [[nodiscard]] bool rotateFolderPlaylistToCurrent() const;
    void setRotateFolderPlaylistToCurrent(bool enabled);
    [[nodiscard]] double videoZoomStep() const;
    void setVideoZoomStep(double step);
    [[nodiscard]] double videoMinimumZoom() const;
    void setVideoMinimumZoom(double factor);
    [[nodiscard]] double videoMaximumZoom() const;
    void setVideoMaximumZoom(double factor);
    [[nodiscard]] QString videoZoomDefaultBehavior() const;
    void setVideoZoomDefaultBehavior(const QString &behaviorId);
    [[nodiscard]] bool videoZoomResetOnFileChange() const;
    void setVideoZoomResetOnFileChange(bool enabled);
    [[nodiscard]] QString videoZoomRememberMode() const;
    void setVideoZoomRememberMode(const QString &modeId);
    [[nodiscard]] double videoPanSensitivity() const;
    void setVideoPanSensitivity(double sensitivity);
    [[nodiscard]] bool videoZoomConstrainPanning() const;
    void setVideoZoomConstrainPanning(bool enabled);
    [[nodiscard]] QString videoZoomWheelBehavior() const;
    void setVideoZoomWheelBehavior(const QString &behaviorId);
    [[nodiscard]] QString videoZoomFullscreenBehavior() const;
    void setVideoZoomFullscreenBehavior(const QString &behaviorId);
    [[nodiscard]] bool subtitleVisible() const;
    void setSubtitleVisible(bool visible);
    [[nodiscard]] double subtitleScale() const;
    void setSubtitleScale(double scale);
    [[nodiscard]] int subtitlePosition() const;
    void setSubtitlePosition(int position);
    [[nodiscard]] QString subtitleFontFamily() const;
    void setSubtitleFontFamily(const QString &fontFamily);
    [[nodiscard]] int subtitleFontSize() const;
    void setSubtitleFontSize(int fontSize);
    [[nodiscard]] QString subtitleAssOverride() const;
    void setSubtitleAssOverride(const QString &mode);
    [[nodiscard]] bool subtitleAutoSelectEnabled() const;
    void setSubtitleAutoSelectEnabled(bool enabled);
    [[nodiscard]] bool subtitlePreferExternal() const;
    void setSubtitlePreferExternal(bool enabled);
    [[nodiscard]] bool subtitleAutoLoadLocalMatches() const;
    void setSubtitleAutoLoadLocalMatches(bool enabled);
    [[nodiscard]] QString subtitlePreferredLanguages() const;
    void setSubtitlePreferredLanguages(const QString &languages);
    [[nodiscard]] double subtitleSyncSmallStep() const;
    void setSubtitleSyncSmallStep(double seconds);
    [[nodiscard]] double subtitleSyncLargeStep() const;
    void setSubtitleSyncLargeStep(double seconds);
    [[nodiscard]] QString subtitleDownloadCommand() const;
    void setSubtitleDownloadCommand(const QString &command);
    [[nodiscard]] int sceneBrowserStepSeconds() const;
    void setSceneBrowserStepSeconds(int seconds);
    [[nodiscard]] int sceneBrowserMaxItems() const;
    void setSceneBrowserMaxItems(int count);
    [[nodiscard]] QString screenshotDirectory() const;
    void setScreenshotDirectory(const QString &directoryPath);
    [[nodiscard]] QString shortcutOverride(const QString &shortcutId) const;
    void setShortcutOverride(const QString &shortcutId, const QString &portableShortcut);
    [[nodiscard]] QVector<revaplayer::domain::CustomCommand> customCommands() const;
    bool setCustomCommands(const QVector<revaplayer::domain::CustomCommand> &commands);
    [[nodiscard]] QString customValue(const QString &key, const QString &defaultValue = {}) const;
    bool setCustomValue(const QString &key, const QString &value);
    bool removeCustomValue(const QString &key);
    [[nodiscard]] QStringList customKeys(const QString &prefix) const;

    [[nodiscard]] bool showPlaylistPanelOnStartup() const;
    void setShowPlaylistPanelOnStartup(bool visible);

    [[nodiscard]] bool showDetailsPanelOnStartup() const;
    void setShowDetailsPanelOnStartup(bool visible);

    [[nodiscard]] std::optional<revaplayer::infrastructure::storage::WindowStateRecord> mainWindowState() const;
    void saveMainWindowState(const QByteArray &geometry,
                             const QByteArray &state,
                             bool maximized,
                             bool fullscreen);
    void clearMainWindowState();
    bool resetApplicationData();
    [[nodiscard]] QString databasePath() const;

private:
    std::unique_ptr<revaplayer::infrastructure::storage::SqliteStore> store_;
};

}  // namespace revaplayer::application
