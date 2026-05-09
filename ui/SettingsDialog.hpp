#pragma once

#include "ui/ShortcutBinding.hpp"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFontComboBox;
class QLineEdit;
class QLabel;
class QKeySequenceEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QTabWidget;

namespace revaplayer::application {
class HistoryController;
class SettingsController;
}

namespace revaplayer::ui {

class SettingsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(revaplayer::application::SettingsController *settingsController,
                            revaplayer::application::HistoryController *historyController = nullptr,
                            QVector<revaplayer::ui::ShortcutBinding> shortcutBindings = {},
                            QWidget *parent = nullptr);

    void applySettings();

signals:
    void settingsApplied();
    void clearCacheRequested();
    void factoryResetRequested();
    void loadSubtitleFileRequested();
    void subtitlePreviewRequested(bool visible,
                                  double scale,
                                  int position,
                                  const QString &fontFamily,
                                  int fontSize,
                                  const QString &assOverride);

protected:
    void accept() override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct ShortcutEditorRow {
        QString id;
        QString category;
        QString label;
        QKeySequence defaultSequence;
        QWidget *rowWidget {nullptr};
        QKeySequenceEdit *editor {nullptr};
    };

    void buildUi();
    void loadSettings();
    void refreshShortcutEditorState();
    void refreshSettingsSearchState();
    void refreshSubtitlePreviewSample();
    void emitSubtitlePreviewState();
    [[nodiscard]] bool applyIfValid(QString *errorMessage = nullptr);
    [[nodiscard]] bool validateShortcuts(QString *errorMessage) const;

    revaplayer::application::SettingsController *settingsController_ {nullptr};
    revaplayer::application::HistoryController *historyController_ {nullptr};
    QVector<revaplayer::ui::ShortcutBinding> shortcutBindings_;
    QVector<ShortcutEditorRow> shortcutEditors_;
    QVector<QWidget *> settingsPageContents_;
    QCheckBox *rememberWindowStateCheckBox_ {nullptr};
    QCheckBox *rememberLastDirectoryCheckBox_ {nullptr};
    QCheckBox *resumePlaybackCheckBox_ {nullptr};
    QCheckBox *rememberHistoryCheckBox_ {nullptr};
    QCheckBox *clearHistoryOnExitCheckBox_ {nullptr};
    QCheckBox *externalMpvConfigCheckBox_ {nullptr};
    QCheckBox *fullscreenEdgePanelsCheckBox_ {nullptr};
    QCheckBox *fullscreenSideSelectorCheckBox_ {nullptr};
    QCheckBox *windowedEdgePanelsCheckBox_ {nullptr};
    QCheckBox *showMenuBarCheckBox_ {nullptr};
    QCheckBox *showStatusBarCheckBox_ {nullptr};
    QCheckBox *alwaysOnTopCheckBox_ {nullptr};
    QCheckBox *overlayPanelsOnVideoCheckBox_ {nullptr};
    QCheckBox *showPlaylistPanelCheckBox_ {nullptr};
    QCheckBox *showDetailsPanelCheckBox_ {nullptr};
    QCheckBox *restoreSidePanelsFromWindowStateCheckBox_ {nullptr};
    QCheckBox *playlistShowFullPathsCheckBox_ {nullptr};
    QCheckBox *playlistShowIndexPrefixesCheckBox_ {nullptr};
    QCheckBox *playlistAutoFollowCurrentCheckBox_ {nullptr};
    QCheckBox *rotateFolderPlaylistToCurrentCheckBox_ {nullptr};
    QCheckBox *thumbnailPreviewsCheckBox_ {nullptr};
    QCheckBox *subtitleVisibleCheckBox_ {nullptr};
    QCheckBox *subtitleAutoSelectCheckBox_ {nullptr};
    QCheckBox *subtitlePreferExternalCheckBox_ {nullptr};
    QCheckBox *subtitleAutoLoadLocalMatchesCheckBox_ {nullptr};
    QCheckBox *mouseWheelVolumeCheckBox_ {nullptr};
    QCheckBox *mouseNavigationSeekCheckBox_ {nullptr};
    QCheckBox *doubleClickFullscreenCheckBox_ {nullptr};
    QCheckBox *mouseGesturesCheckBox_ {nullptr};
    QCheckBox *actionFeedbackOverlayCheckBox_ {nullptr};
    QCheckBox *expressiveControlLabelsCheckBox_ {nullptr};
    QCheckBox *fullscreenAutoHideCheckBox_ {nullptr};
    QCheckBox *pointerKeepControlsVisibleCheckBox_ {nullptr};
    QCheckBox *dashboardEnabledCheckBox_ {nullptr};
    QCheckBox *dashboardShowContinueCheckBox_ {nullptr};
    QCheckBox *dashboardShowRecentCheckBox_ {nullptr};
    QCheckBox *dashboardShowFavoritesCheckBox_ {nullptr};
    QCheckBox *dashboardShowSavedListsCheckBox_ {nullptr};
    QCheckBox *progressTrackingModeCheckBox_ {nullptr};
    QCheckBox *progressBadgesCheckBox_ {nullptr};
    QCheckBox *adaptiveUiCheckBox_ {nullptr};
    QCheckBox *autoLoadSiblingMediaCheckBox_ {nullptr};
    QCheckBox *showPlaylistPanelOnFolderLoadCheckBox_ {nullptr};
    QCheckBox *videoZoomResetOnFileChangeCheckBox_ {nullptr};
    QCheckBox *videoZoomConstrainPanningCheckBox_ {nullptr};
    QCheckBox *controlBarShowOpenButtonCheckBox_ {nullptr};
    QCheckBox *controlBarShowStopButtonCheckBox_ {nullptr};
    QCheckBox *controlBarShowPlaylistButtonCheckBox_ {nullptr};
    QCheckBox *controlBarShowDetailsButtonCheckBox_ {nullptr};
    QCheckBox *controlBarShowTimeLabelCheckBox_ {nullptr};
    QCheckBox *controlBarShowSpeedButtonCheckBox_ {nullptr};
    QCheckBox *controlBarShowRepeatLoopButtonsCheckBox_ {nullptr};
    QCheckBox *controlBarShowTrackMenusCheckBox_ {nullptr};
    QCheckBox *controlBarShowVolumeControlsCheckBox_ {nullptr};
    QCheckBox *controlBarShowFullscreenButtonCheckBox_ {nullptr};
    QComboBox *themeComboBox_ {nullptr};
    QComboBox *themeAccentComboBox_ {nullptr};
    QComboBox *uiDensityComboBox_ {nullptr};
    QComboBox *startupCanvasStyleComboBox_ {nullptr};
    QComboBox *themeAnimationEasingComboBox_ {nullptr};
    QComboBox *interfaceLanguageComboBox_ {nullptr};
    QComboBox *profileComboBox_ {nullptr};
    QComboBox *defaultRepeatModeComboBox_ {nullptr};
    QComboBox *defaultSidePanelComboBox_ {nullptr};
    QComboBox *videoZoomDefaultBehaviorComboBox_ {nullptr};
    QComboBox *videoZoomRememberModeComboBox_ {nullptr};
    QComboBox *videoZoomWheelBehaviorComboBox_ {nullptr};
    QComboBox *videoZoomFullscreenBehaviorComboBox_ {nullptr};
    QComboBox *subtitleAssOverrideComboBox_ {nullptr};
    QComboBox *subtitleAutoLoadModeComboBox_ {nullptr};
    QComboBox *subtitleEncodingComboBox_ {nullptr};
    QComboBox *subtitleBorderStyleComboBox_ {nullptr};
    QComboBox *subtitleAlignXComboBox_ {nullptr};
    QComboBox *subtitleAlignYComboBox_ {nullptr};
    QComboBox *subtitleJustifyComboBox_ {nullptr};
    QComboBox *subtitleFontProviderComboBox_ {nullptr};
    QComboBox *subtitleShaperComboBox_ {nullptr};
    QComboBox *subtitleHintingComboBox_ {nullptr};
    QComboBox *mouseWheelActionComboBox_ {nullptr};
    QComboBox *mouseSideButtonsActionComboBox_ {nullptr};
    QComboBox *clickActionComboBox_ {nullptr};
    QComboBox *doubleClickActionComboBox_ {nullptr};
    QComboBox *middleClickActionComboBox_ {nullptr};
    QComboBox *gestureLeftActionComboBox_ {nullptr};
    QComboBox *gestureRightActionComboBox_ {nullptr};
    QComboBox *gestureUpActionComboBox_ {nullptr};
    QComboBox *gestureDownActionComboBox_ {nullptr};
    QComboBox *pointerRightEdgeFullscreenActionComboBox_ {nullptr};
    QComboBox *pointerRightEdgeWindowedActionComboBox_ {nullptr};
    QComboBox *pointerRightEdgeLeaveActionComboBox_ {nullptr};
    QComboBox *mouseZoneTopActionComboBox_ {nullptr};
    QComboBox *mouseZoneBottomActionComboBox_ {nullptr};
    QComboBox *mouseZoneLeftActionComboBox_ {nullptr};
    QComboBox *mouseZoneRightActionComboBox_ {nullptr};
    QComboBox *mouseZoneCenterActionComboBox_ {nullptr};
    QComboBox *screenshotFormatComboBox_ {nullptr};
    QComboBox *shortcutCategoryComboBox_ {nullptr};
    QFontComboBox *subtitleFontComboBox_ {nullptr};
    QComboBox *startupVolumeModeComboBox_ {nullptr};
    QSpinBox *startupVolumeSpinBox_ {nullptr};
    QDoubleSpinBox *startupSpeedSpinBox_ {nullptr};
    QCheckBox *sessionWideSpeedCheckBox_ {nullptr};
    QComboBox *thumbnailPreviewSizeComboBox_ {nullptr};
    QSpinBox *thumbnailWidthSpinBox_ {nullptr};
    QSpinBox *thumbnailPopupWidthSpinBox_ {nullptr};
    QSpinBox *thumbnailPopupOffsetSpinBox_ {nullptr};
    QSpinBox *thumbnailPopupPaddingSpinBox_ {nullptr};
    QSpinBox *shortSeekStepSpinBox_ {nullptr};
    QSpinBox *longSeekStepSpinBox_ {nullptr};
    QSpinBox *volumeStepSpinBox_ {nullptr};
    QSpinBox *mouseWheelVolumeStepSpinBox_ {nullptr};
    QSpinBox *mouseWheelSeekStepSpinBox_ {nullptr};
    QSpinBox *mouseNavigationSeekStepSpinBox_ {nullptr};
    QSpinBox *mouseGestureThresholdSpinBox_ {nullptr};
    QSpinBox *fullscreenRevealMarginSpinBox_ {nullptr};
    QSpinBox *pointerRightEdgeMarginSpinBox_ {nullptr};
    QSpinBox *pointerLeaveDelaySpinBox_ {nullptr};
    QSpinBox *progressCompletionThresholdSpinBox_ {nullptr};
    QSpinBox *themeRadiusSpinBox_ {nullptr};
    QSpinBox *themeSpacingSpinBox_ {nullptr};
    QSpinBox *themeFontScaleSpinBox_ {nullptr};
    QSpinBox *themeFontWeightSpinBox_ {nullptr};
    QSpinBox *themeBorderContrastSpinBox_ {nullptr};
    QSpinBox *themeShadowStrengthSpinBox_ {nullptr};
    QSpinBox *themeBlurStrengthSpinBox_ {nullptr};
    QSpinBox *themeAnimationSpeedSpinBox_ {nullptr};
    QSpinBox *themeOverlayOpacitySpinBox_ {nullptr};
    QSpinBox *adaptiveUiBreakpointSpinBox_ {nullptr};
    QSpinBox *controlBarTimelineThicknessSpinBox_ {nullptr};
    QSpinBox *controlBarTimelineHandleSizeSpinBox_ {nullptr};
    QSpinBox *controlBarVolumeSliderThicknessSpinBox_ {nullptr};
    QSpinBox *controlBarVolumeSliderWidthSpinBox_ {nullptr};
    QSpinBox *sceneBrowserStepSpinBox_ {nullptr};
    QSpinBox *sceneBrowserMaxItemsSpinBox_ {nullptr};
    QSpinBox *historyLimitSpinBox_ {nullptr};
    QDoubleSpinBox *themeLetterSpacingSpinBox_ {nullptr};
    QDoubleSpinBox *subtitleScaleSpinBox_ {nullptr};
    QDoubleSpinBox *videoZoomStepSpinBox_ {nullptr};
    QDoubleSpinBox *videoMinimumZoomSpinBox_ {nullptr};
    QDoubleSpinBox *videoMaximumZoomSpinBox_ {nullptr};
    QDoubleSpinBox *videoPanSensitivitySpinBox_ {nullptr};
    QDoubleSpinBox *subtitleSyncSmallStepSpinBox_ {nullptr};
    QDoubleSpinBox *subtitleOutlineSizeSpinBox_ {nullptr};
    QDoubleSpinBox *subtitleShadowOffsetSpinBox_ {nullptr};
    QDoubleSpinBox *subtitleShadowBlurSpinBox_ {nullptr};
    QDoubleSpinBox *subtitleLineSpacingSpinBox_ {nullptr};
    QDoubleSpinBox *subtitleLetterSpacingSpinBox_ {nullptr};
    QDoubleSpinBox *subtitleMaxWidthSpinBox_ {nullptr};
    QSpinBox *subtitlePositionSpinBox_ {nullptr};
    QSpinBox *subtitleFontSizeSpinBox_ {nullptr};
    QSpinBox *subtitleFontWeightSpinBox_ {nullptr};
    QSpinBox *subtitleBackgroundOpacitySpinBox_ {nullptr};
    QSpinBox *subtitleMarginXSpinBox_ {nullptr};
    QSpinBox *subtitleMarginYSpinBox_ {nullptr};
    QLineEdit *subtitlePreferredLanguagesEdit_ {nullptr};
    QLineEdit *subtitleAutoExtensionsEdit_ {nullptr};
    QPushButton *resetSubtitleStyleQuickButton_ {nullptr};
    QPushButton *subtitleTextColorButton_ {nullptr};
    QPushButton *subtitleOutlineColorButton_ {nullptr};
    QPushButton *subtitleBackgroundColorButton_ {nullptr};
    QPushButton *subtitleShadowColorButton_ {nullptr};
    QCheckBox *subtitleItalicCheckBox_ {nullptr};
    QCheckBox *subtitleRememberTrackChoiceCheckBox_ {nullptr};
    QCheckBox *subtitleFixTimingCheckBox_ {nullptr};
    QCheckBox *subtitleBackgroundEnabledCheckBox_ {nullptr};
    QCheckBox *subtitleShadowEnabledCheckBox_ {nullptr};
    QCheckBox *subtitleUseMarginsCheckBox_ {nullptr};
    QCheckBox *subtitleScaleWithWindowCheckBox_ {nullptr};
    QCheckBox *subtitleAssForceMarginsCheckBox_ {nullptr};
    QCheckBox *subtitleAssJustifyCheckBox_ {nullptr};
    QLineEdit *screenshotDirectoryEdit_ {nullptr};
    QLineEdit *screenshotTemplateEdit_ {nullptr};
    QLineEdit *settingsSearchEdit_ {nullptr};
    QLineEdit *shortcutSearchEdit_ {nullptr};
    QPushButton *clearHistoryButton_ {nullptr};
    QPushButton *clearCacheButton_ {nullptr};
    QPushButton *resetSettingsButton_ {nullptr};
    QPushButton *factoryResetButton_ {nullptr};
    QPushButton *applyButton_ {nullptr};
    bool settingsDirty_ {false};
    QPushButton *resetSubtitleAppearanceButton_ {nullptr};
    QPushButton *settingsSearchButton_ {nullptr};
    QPushButton *settingsSearchClearButton_ {nullptr};
    QPushButton *shortcutImportButton_ {nullptr};
    QPushButton *shortcutExportButton_ {nullptr};
    QPushButton *shortcutResetAllButton_ {nullptr};
    QLabel *shortcutConflictLabel_ {nullptr};
    QLabel *storageInfoLabel_ {nullptr};
    QScrollArea *settingsSectionTabsScrollArea_ {nullptr};
    QWidget *settingsSectionTabsContainer_ {nullptr};
    QVector<QPushButton *> settingsSectionButtons_;
    QTabWidget *settingsTabs_ {nullptr};
    int subtitleTabIndex_ {-1};
};

}  // namespace revaplayer::ui
