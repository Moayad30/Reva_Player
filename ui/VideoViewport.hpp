#pragma once

#include <QWidget>

namespace revaplayer::infrastructure::mpv {
class MpvRenderHost;
}

class QLabel;
class QMouseEvent;
class QTimer;
class QWheelEvent;

namespace revaplayer::ui {

class VideoViewport final : public QWidget {
    Q_OBJECT

public:
    explicit VideoViewport(QWidget *parent = nullptr);

    [[nodiscard]] revaplayer::infrastructure::mpv::MpvRenderHost *renderHost() const;
    void setRenderHostVisible(bool visible);
    void setOverlayText(const QString &text);
    void setOverlayVisible(bool visible);
    void showActionOverlay(const QString &text, int timeoutMs = 1500);
    void showVolumeOverlay(int volume,
                           int maximum = 100,
                           int normalMaximum = 100,
                           int timeoutMs = 1500);
    void hideActionOverlay();
    void setMouseGesturesEnabled(bool enabled);
    void setMouseGestureThreshold(int pixels);
    void setVideoPanEnabled(bool enabled);
    void setPointerAutoHideEnabled(bool enabled);

signals:
    void clicked(Qt::MouseButton button);
    void doubleClicked(Qt::MouseButton button);
    void middleClicked();
    void gestureTriggered(const QString &directionId);
    void panDragged(const QPoint &deltaPixels);
    void wheelAdjusted(int steps, Qt::KeyboardModifiers modifiers);
    void backNavigationRequested();
    void forwardNavigationRequested();
    void pointerActivity(const QPoint &localPosition);
    void pointerLeft();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateOverlayGeometry();
    void updateVolumeOverlayMeter();
    void updatePanCursor();
    void resetGestureState();
    void clearPendingClick();
    void showPointer();
    void schedulePointerHide();
    void hidePointer();

    revaplayer::infrastructure::mpv::MpvRenderHost *renderHost_ {nullptr};
    QLabel *overlayLabel_ {nullptr};
    QLabel *actionOverlayLabel_ {nullptr};
    QWidget *volumeOverlayWidget_ {nullptr};
    QLabel *volumeOverlayTextLabel_ {nullptr};
    QLabel *volumeOverlayValueLabel_ {nullptr};
    QWidget *volumeOverlayMeterShell_ {nullptr};
    QWidget *volumeOverlayMeterFill_ {nullptr};
    QWidget *volumeOverlayMeterBoostFill_ {nullptr};
    QWidget *volumeOverlayMeterMarker_ {nullptr};
    QTimer *actionOverlayTimer_ {nullptr};
    QTimer *clickTimer_ {nullptr};
    QTimer *pointerHideTimer_ {nullptr};
    QPoint gestureStartPoint_;
    QPoint panLastPoint_;
    QString currentGestureDirection_;
    int volumeOverlayValue_ {100};
    int volumeOverlayMaximum_ {100};
    int volumeOverlayNormalMaximum_ {100};
    qreal wheelStepAccumulator_ {0.0};
    Qt::MouseButton pendingClickButton_ {Qt::NoButton};
    bool mouseGesturesEnabled_ {true};
    bool gestureTrackingActive_ {false};
    bool videoPanEnabled_ {false};
    bool videoPanActive_ {false};
    bool videoPanMoved_ {false};
    bool pointerAutoHideEnabled_ {false};
    bool pointerHidden_ {false};
    int mouseGestureThreshold_ {54};
};

}  // namespace revaplayer::ui
