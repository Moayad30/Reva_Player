#pragma once

#include <QVector>
#include <QWidget>

class QAction;
class QLabel;
class QMenu;
class QResizeEvent;
class QSlider;
class QToolButton;

namespace revaplayer::ui {

struct TimelineMarker final {
    double timeSeconds {0.0};
    QString label;

    friend bool operator==(const TimelineMarker &, const TimelineMarker &) = default;
};

class ControlBar final : public QWidget {
    Q_OBJECT

public:
    explicit ControlBar(QWidget *parent = nullptr);

    void setPlaybackAvailable(bool available);
    void setPlaylistNavigationAvailable(bool previousAvailable, bool nextAvailable);
    void setPaused(bool paused);
    void setPosition(double positionSeconds, double durationSeconds);
    void setBufferedPosition(double bufferedPositionSeconds);
    void setVolume(int volume);
    void setPlaybackSpeed(double speed);
    void setRepeatMode(const QString &mode);
    void setLoopPoints(double loopStartSeconds, double loopEndSeconds);
    void setTimelineMarkers(const QVector<revaplayer::ui::TimelineMarker> &markers);
    void setExpressiveLabelsEnabled(bool enabled);
    void setTimelineThickness(int pixels);
    void setTimelineHandleSize(int pixels);
    void setVolumeSliderThickness(int pixels);
    void setVolumeSliderPreferredWidth(int pixels);
    void setShowOpenButton(bool visible);
    void setShowStopButton(bool visible);
    void setShowPlaylistButton(bool visible);
    void setShowDetailsButton(bool visible);
    void setShowTimeLabel(bool visible);
    void setShowSpeedButton(bool visible);
    void setShowRepeatLoopButtons(bool visible);
    void setShowTrackMenus(bool visible);
    void setShowVolumeControls(bool visible);
    void setShowFullscreenButton(bool visible);
    void setPanelButtonsChecked(bool playlistVisible, bool detailsVisible);
    void setPanelButtonsEnabled(bool enabled);
    void setPanelButtonsVisible(bool visible);
    void setTrackMenusEnabled(bool qualityEnabled, bool subtitleEnabled);
    [[nodiscard]] QMenu *qualityMenu() const;
    [[nodiscard]] QMenu *subtitleMenu() const;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

signals:
    void openRequested();
    void previousRequested();
    void playPauseRequested();
    void nextRequested();
    void stopRequested();
    void fullscreenRequested();
    void playlistPanelToggled(bool visible);
    void detailsPanelToggled(bool visible);
    void seekRequested(double fraction);
    void previewRequested(double timeSeconds, const QPoint &globalAnchor);
    void previewHidden();
    void volumeRequested(int volume);
    void playbackSpeedRequested(double speed);
    void playbackSpeedAdjusted(double delta);
    void playbackSpeedResetRequested();
    void repeatModeRequested(const QString &mode);
    void loopStartRequested();
    void loopEndRequested();
    void loopClearRequested();

private slots:
    void emitSeekFromSlider();
    void handleSpeedMenuAction(QAction *action);
    void handleRepeatMenuAction(QAction *action);
    void handleLoopMenuAction(QAction *action);

private:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    static QString formatTime(double timeSeconds);
    static QString formatSpeed(double speed);
    void updateButtonLabels();
    void updateButtonIcons();
    void updateSliderStyleSheets();
    void updateVolumePresentation();
    void updateVisualMetrics();
    void updateResponsiveLayout();

    QToolButton *openButton_ {nullptr};
    QToolButton *previousButton_ {nullptr};
    QToolButton *playPauseButton_ {nullptr};
    QToolButton *nextButton_ {nullptr};
    QToolButton *stopButton_ {nullptr};
    QToolButton *fullscreenButton_ {nullptr};
    QToolButton *playlistButton_ {nullptr};
    QToolButton *detailsButton_ {nullptr};
    QWidget *transportGroup_ {nullptr};
    QWidget *timelineGroup_ {nullptr};
    QWidget *volumeGroup_ {nullptr};
    QToolButton *speedButton_ {nullptr};
    QToolButton *repeatButton_ {nullptr};
    QToolButton *loopButton_ {nullptr};
    QToolButton *qualityButton_ {nullptr};
    QToolButton *subtitleButton_ {nullptr};
    QSlider *positionSlider_ {nullptr};
    QLabel *timeLabel_ {nullptr};
    QLabel *volumeIconLabel_ {nullptr};
    QSlider *volumeSlider_ {nullptr};
    QLabel *volumeValueLabel_ {nullptr};
    QMenu *speedMenu_ {nullptr};
    QMenu *repeatMenu_ {nullptr};
    QMenu *loopMenu_ {nullptr};
    QMenu *qualityMenu_ {nullptr};
    QMenu *subtitleMenu_ {nullptr};
    QVector<revaplayer::ui::TimelineMarker> timelineMarkers_;
    double durationSeconds_ {0.0};
    double playbackSpeed_ {1.0};
    QString repeatMode_ {QStringLiteral("off")};
    double loopStartSeconds_ {-1.0};
    double loopEndSeconds_ {-1.0};
    bool playbackAvailable_ {false};
    bool paused_ {true};
    bool expressiveLabelsEnabled_ {true};
    bool qualityMenuEnabled_ {false};
    bool subtitleMenuEnabled_ {false};
    int timelineThickness_ {8};
    int timelineHandleSize_ {14};
    int volumeSliderThickness_ {6};
    int currentVolume_ {100};
    int volumeSliderPreferredWidth_ {0};
    bool showOpenButton_ {true};
    bool showStopButton_ {true};
    bool showPlaylistButton_ {true};
    bool showDetailsButton_ {true};
    bool showTimeLabel_ {true};
    bool showSpeedButton_ {true};
    bool showRepeatLoopButtons_ {true};
    bool showTrackMenus_ {true};
    bool showVolumeControls_ {true};
    bool showFullscreenButton_ {true};
    bool panelButtonsVisibleByContext_ {true};
};

}  // namespace revaplayer::ui
