#include "ui/VideoViewport.hpp"

#include "application/PlaybackTuning.hpp"
#include "application/UiLanguage.hpp"
#include "infrastructure/mpv/MpvRenderHost.hpp"

#include <algorithm>
#include <cmath>

#include <QEvent>
#include <QColor>
#include <QApplication>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QCursor>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QtGlobal>

namespace revaplayer::ui {
namespace {

QString uiText(const char *sourceText)
{
    return revaplayer::application::translateUiText(QString::fromUtf8(sourceText));
}

QPoint mouseEventLocalPoint(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event != nullptr ? event->position().toPoint() : QPoint {};
#else
    return event != nullptr ? event->pos() : QPoint {};
#endif
}

QString gestureDirectionIdForDelta(const QPoint &delta)
{
    if (std::abs(delta.x()) >= std::abs(delta.y())) {
        return delta.x() >= 0 ? QStringLiteral("right") : QStringLiteral("left");
    }

    return delta.y() >= 0 ? QStringLiteral("down") : QStringLiteral("up");
}

QString gestureOverlayText(const QString &directionId)
{
    if (directionId == QStringLiteral("left")) {
        return uiText("Gesture  \u2190");
    }
    if (directionId == QStringLiteral("right")) {
        return uiText("Gesture  \u2192");
    }
    if (directionId == QStringLiteral("up")) {
        return uiText("Gesture  \u2191");
    }
    if (directionId == QStringLiteral("down")) {
        return uiText("Gesture  \u2193");
    }
    return uiText("Gesture");
}

QColor blendColors(const QColor &from, const QColor &to, const qreal ratio)
{
    const qreal clampedRatio = std::clamp(ratio, 0.0, 1.0);
    return QColor(
        static_cast<int>(std::lround(from.red() + ((to.red() - from.red()) * clampedRatio))),
        static_cast<int>(std::lround(from.green() + ((to.green() - from.green()) * clampedRatio))),
        static_cast<int>(std::lround(from.blue() + ((to.blue() - from.blue()) * clampedRatio))),
        static_cast<int>(std::lround(from.alpha() + ((to.alpha() - from.alpha()) * clampedRatio))));
}

constexpr qreal kTouchpadPixelsPerWheelStep = 30.0;

QColor volumeLowColor()
{
    return QColor(QStringLiteral("#ffb55f"));
}

QColor volumeMidColor()
{
    return QColor(QStringLiteral("#ff9d18"));
}

QColor volumeNormalLimitColor()
{
    return QColor(QStringLiteral("#ff9d18"));
}

QColor volumeBoostEndColor()
{
    return QColor(QStringLiteral("#ff5a00"));
}

QColor volumeBoostStartColor()
{
    return QColor(QStringLiteral("#ff5a00"));
}

QColor volumeColorForNormalLevel(const qreal level)
{
    const qreal normalizedLevel = std::clamp(level, 0.0, 1.0);
    if (normalizedLevel < 0.62) {
        return blendColors(volumeLowColor(), volumeMidColor(), normalizedLevel / 0.62);
    }

    return blendColors(volumeMidColor(), volumeNormalLimitColor(), (normalizedLevel - 0.62) / 0.38);
}

QColor volumeColorForBoostLevel(const qreal level)
{
    return blendColors(volumeBoostStartColor(), volumeBoostEndColor(), std::clamp(level, 0.0, 1.0));
}

}  // namespace

VideoViewport::VideoViewport(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("videoViewport"));
    setAttribute(Qt::WA_StyledBackground, true);
    setContextMenuPolicy(Qt::CustomContextMenu);
    renderHost_ = new revaplayer::infrastructure::mpv::MpvRenderHost(this);
    renderHost_->installEventFilter(this);
    connect(renderHost_, &revaplayer::infrastructure::mpv::MpvRenderHost::doubleClicked,
            this, &VideoViewport::doubleClicked);
    connect(renderHost_, &QWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        emit customContextMenuRequested(renderHost_->mapToParent(position));
    });

    overlayLabel_ = new QLabel(uiText("Open a file or drop media here"), this);
    overlayLabel_->setObjectName(QStringLiteral("viewportOverlayLabel"));
    overlayLabel_->setAlignment(Qt::AlignCenter);
    overlayLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    overlayLabel_->setWordWrap(true);
    overlayLabel_->raise();

    actionOverlayLabel_ = new QLabel(this);
    actionOverlayLabel_->setObjectName(QStringLiteral("viewportActionOverlayLabel"));
    actionOverlayLabel_->setAlignment(Qt::AlignCenter);
    actionOverlayLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    actionOverlayLabel_->setVisible(false);
    actionOverlayLabel_->raise();

    volumeOverlayWidget_ = new QWidget(this);
    volumeOverlayWidget_->setObjectName(QStringLiteral("viewportVolumeOverlayWidget"));
    volumeOverlayWidget_->setAttribute(Qt::WA_StyledBackground, true);
    volumeOverlayWidget_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    volumeOverlayWidget_->setVisible(false);
    auto *volumeOverlayLayout = new QVBoxLayout(volumeOverlayWidget_);
    volumeOverlayLayout->setContentsMargins(8, 4, 8, 4);
    volumeOverlayLayout->setSpacing(6);

    auto *volumeOverlayHeader = new QWidget(volumeOverlayWidget_);
    volumeOverlayHeader->setObjectName(QStringLiteral("viewportVolumeOverlayHeader"));
    volumeOverlayHeader->setAttribute(Qt::WA_StyledBackground, true);
    volumeOverlayHeader->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    volumeOverlayHeader->setAutoFillBackground(false);
    auto *volumeOverlayHeaderLayout = new QVBoxLayout(volumeOverlayHeader);
    volumeOverlayHeaderLayout->setContentsMargins(0, 0, 0, 0);
    volumeOverlayHeaderLayout->setSpacing(0);

    volumeOverlayTextLabel_ = new QLabel(uiText("Volume"), volumeOverlayHeader);
    volumeOverlayTextLabel_->setObjectName(QStringLiteral("viewportVolumeOverlayTextLabel"));
    volumeOverlayTextLabel_->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    volumeOverlayTextLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    volumeOverlayTextLabel_->hide();

    volumeOverlayValueLabel_ = new QLabel(QStringLiteral("100%"), volumeOverlayHeader);
    volumeOverlayValueLabel_->setObjectName(QStringLiteral("viewportVolumeOverlayValueLabel"));
    volumeOverlayValueLabel_->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    volumeOverlayValueLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    volumeOverlayMeterShell_ = new QWidget(volumeOverlayWidget_);
    volumeOverlayMeterShell_->setObjectName(QStringLiteral("viewportVolumeOverlayMeterShell"));
    volumeOverlayMeterShell_->setAttribute(Qt::WA_StyledBackground, true);
    volumeOverlayMeterShell_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    volumeOverlayMeterShell_->setFixedSize(18, 260);

    volumeOverlayMeterBoostFill_ = new QWidget(volumeOverlayMeterShell_);
    volumeOverlayMeterBoostFill_->setObjectName(QStringLiteral("viewportVolumeOverlayMeterBoostFill"));
    volumeOverlayMeterBoostFill_->setAttribute(Qt::WA_StyledBackground, true);
    volumeOverlayMeterBoostFill_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    volumeOverlayMeterBoostFill_->hide();

    volumeOverlayMeterFill_ = new QWidget(volumeOverlayMeterShell_);
    volumeOverlayMeterFill_->setObjectName(QStringLiteral("viewportVolumeOverlayMeterFill"));
    volumeOverlayMeterFill_->setAttribute(Qt::WA_StyledBackground, true);
    volumeOverlayMeterFill_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    volumeOverlayMeterFill_->hide();

    volumeOverlayMeterMarker_ = new QWidget(volumeOverlayMeterShell_);
    volumeOverlayMeterMarker_->setObjectName(QStringLiteral("viewportVolumeOverlayMeterMarker"));
    volumeOverlayMeterMarker_->setAttribute(Qt::WA_StyledBackground, true);
    volumeOverlayMeterMarker_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    volumeOverlayMeterMarker_->setFixedHeight(2);

    QFont volumeOverlayFont = volumeOverlayTextLabel_->font();
    volumeOverlayFont.setBold(true);
    volumeOverlayFont.setPixelSize(11);
    volumeOverlayTextLabel_->setFont(volumeOverlayFont);

    QFont volumeOverlayValueFont = volumeOverlayValueLabel_->font();
    volumeOverlayValueFont.setBold(true);
    volumeOverlayValueFont.setPixelSize(20);
    volumeOverlayValueLabel_->setFont(volumeOverlayValueFont);
    volumeOverlayValueLabel_->setFixedWidth(QFontMetrics(volumeOverlayValueFont).horizontalAdvance(QStringLiteral("150%")) + 2);

    volumeOverlayHeaderLayout->addWidget(volumeOverlayTextLabel_, 0, Qt::AlignHCenter);
    volumeOverlayHeaderLayout->addWidget(volumeOverlayValueLabel_, 0, Qt::AlignHCenter);
    volumeOverlayLayout->addWidget(volumeOverlayHeader, 0, Qt::AlignHCenter);
    volumeOverlayLayout->addWidget(volumeOverlayMeterShell_, 0, Qt::AlignHCenter);
    volumeOverlayWidget_->raise();

    actionOverlayTimer_ = new QTimer(this);
    actionOverlayTimer_->setSingleShot(true);
    connect(actionOverlayTimer_, &QTimer::timeout, this, &VideoViewport::hideActionOverlay);

    clickTimer_ = new QTimer(this);
    clickTimer_->setSingleShot(true);
    connect(clickTimer_, &QTimer::timeout, this, [this]() {
        const Qt::MouseButton button = pendingClickButton_;
        pendingClickButton_ = Qt::NoButton;
        if (button != Qt::NoButton) {
            emit clicked(button);
        }
    });

    pointerHideTimer_ = new QTimer(this);
    pointerHideTimer_->setSingleShot(true);
    connect(pointerHideTimer_, &QTimer::timeout, this, &VideoViewport::hidePointer);

    updateOverlayGeometry();
}

revaplayer::infrastructure::mpv::MpvRenderHost *VideoViewport::renderHost() const
{
    return renderHost_;
}

void VideoViewport::setRenderHostVisible(const bool visible)
{
    if (renderHost_ == nullptr || renderHost_->isVisible() == visible) {
        return;
    }

    renderHost_->setVisible(visible);
    if (visible) {
        renderHost_->update();
    }
    overlayLabel_->raise();
    updateOverlayGeometry();
}

void VideoViewport::setOverlayText(const QString &text)
{
    overlayLabel_->setText(text);
    updateOverlayGeometry();
}

void VideoViewport::setOverlayVisible(const bool visible)
{
    overlayLabel_->setVisible(visible);
}

void VideoViewport::showActionOverlay(const QString &text, const int timeoutMs)
{
    if (actionOverlayLabel_ == nullptr) {
        return;
    }

    if (volumeOverlayWidget_ != nullptr) {
        volumeOverlayWidget_->hide();
    }
    actionOverlayLabel_->setText(text.trimmed());
    actionOverlayLabel_->setVisible(!text.trimmed().isEmpty());
    updateOverlayGeometry();
    actionOverlayLabel_->raise();
    if (actionOverlayTimer_ != nullptr) {
        actionOverlayTimer_->start(std::max(400, timeoutMs));
    }
}

void VideoViewport::showVolumeOverlay(const int volume,
                                      const int maximum,
                                      const int normalMaximum,
                                      const int timeoutMs)
{
    if (volumeOverlayWidget_ == nullptr || volumeOverlayTextLabel_ == nullptr || volumeOverlayValueLabel_ == nullptr) {
        return;
    }

    volumeOverlayValue_ = revaplayer::application::clampPlaybackVolume(volume);
    volumeOverlayMaximum_ = std::max(1, maximum);
    volumeOverlayNormalMaximum_ = std::clamp(normalMaximum, 0, volumeOverlayMaximum_);

    if (actionOverlayLabel_ != nullptr) {
        actionOverlayLabel_->hide();
    }

    volumeOverlayTextLabel_->hide();
    volumeOverlayValueLabel_->setText(QStringLiteral("%1%").arg(volumeOverlayValue_));
    volumeOverlayValueLabel_->setProperty("boosted", volumeOverlayValue_ > volumeOverlayNormalMaximum_);
    volumeOverlayWidget_->setVisible(true);
    updateOverlayGeometry();
    volumeOverlayWidget_->raise();
    if (actionOverlayTimer_ != nullptr) {
        actionOverlayTimer_->start(std::max(500, timeoutMs));
    }
}

void VideoViewport::hideActionOverlay()
{
    if (actionOverlayLabel_ != nullptr) {
        actionOverlayLabel_->hide();
    }
    if (volumeOverlayWidget_ != nullptr) {
        volumeOverlayWidget_->hide();
    }
}

void VideoViewport::setMouseGesturesEnabled(const bool enabled)
{
    mouseGesturesEnabled_ = enabled;
    if (!enabled) {
        hideActionOverlay();
        resetGestureState();
    }
}

void VideoViewport::setMouseGestureThreshold(const int pixels)
{
    mouseGestureThreshold_ = std::clamp(pixels, 24, 220);
}

void VideoViewport::setVideoPanEnabled(const bool enabled)
{
    if (videoPanEnabled_ == enabled) {
        return;
    }

    videoPanEnabled_ = enabled;
    if (!videoPanEnabled_) {
        videoPanActive_ = false;
        videoPanMoved_ = false;
    }
    updatePanCursor();
}

void VideoViewport::setPointerAutoHideEnabled(const bool enabled)
{
    if (pointerAutoHideEnabled_ == enabled) {
        if (enabled) {
            schedulePointerHide();
        }
        return;
    }

    pointerAutoHideEnabled_ = enabled;
    if (pointerAutoHideEnabled_) {
        showPointer();
        schedulePointerHide();
    } else {
        showPointer();
        if (pointerHideTimer_ != nullptr) {
            pointerHideTimer_->stop();
        }
    }
}

bool VideoViewport::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == renderHost_ && event != nullptr) {
        if (event->type() == QEvent::MouseMove) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            showPointer();
            schedulePointerHide();
            emit pointerActivity(mouseEventLocalPoint(mouseEvent));

            if (videoPanEnabled_ && videoPanActive_ && (mouseEvent->buttons() & Qt::LeftButton)) {
                const QPoint localPoint = mouseEventLocalPoint(mouseEvent);
                const QPoint delta = localPoint - panLastPoint_;
                panLastPoint_ = localPoint;
                if (!delta.isNull()) {
                    videoPanMoved_ = true;
                    emit panDragged(delta);
                }
                mouseEvent->accept();
                return true;
            }

            if (mouseGesturesEnabled_ && gestureTrackingActive_ && (mouseEvent->buttons() & Qt::RightButton)) {
                const QPoint delta = mouseEventLocalPoint(mouseEvent) - gestureStartPoint_;
                if (delta.manhattanLength() >= mouseGestureThreshold_) {
                    const QString directionId = gestureDirectionIdForDelta(delta);
                    if (currentGestureDirection_ != directionId) {
                        currentGestureDirection_ = directionId;
                        showActionOverlay(gestureOverlayText(directionId), 1200);
                    }
                    mouseEvent->accept();
                    return true;
                }
            }
        }

        if (event->type() == QEvent::Enter) {
            showPointer();
            schedulePointerHide();
            const QPoint localPoint = renderHost_->mapFromGlobal(QCursor::pos());
            emit pointerActivity(QPoint(
                std::clamp(localPoint.x(), 0, std::max(0, renderHost_->width())),
                std::clamp(localPoint.y(), 0, std::max(0, renderHost_->height()))));
        }

        if (event->type() == QEvent::Leave) {
            showPointer();
            emit pointerLeft();
            hideActionOverlay();
            wheelStepAccumulator_ = 0.0;
            resetGestureState();
            videoPanActive_ = false;
            videoPanMoved_ = false;
            updatePanCursor();
        }

        if (event->type() == QEvent::Wheel) {
            showPointer();
            schedulePointerHide();
            auto *wheelEvent = static_cast<QWheelEvent *>(event);
            if (wheelEvent->phase() == Qt::ScrollBegin) {
                wheelStepAccumulator_ = 0.0;
            }

            const QPoint pixelDelta = wheelEvent->pixelDelta();
            const QPoint angleDelta = wheelEvent->angleDelta();
            qreal effectiveDeltaY = !pixelDelta.isNull()
                ? static_cast<qreal>(pixelDelta.y())
                : static_cast<qreal>(angleDelta.y());
            if (wheelEvent->inverted()) {
                effectiveDeltaY = -effectiveDeltaY;
            }

            if (!qFuzzyIsNull(effectiveDeltaY)) {
                const qreal normalizedStepDelta = !pixelDelta.isNull()
                    ? (effectiveDeltaY / kTouchpadPixelsPerWheelStep)
                    : (effectiveDeltaY / 120.0);
                if ((wheelStepAccumulator_ > 0.0 && normalizedStepDelta < 0.0)
                    || (wheelStepAccumulator_ < 0.0 && normalizedStepDelta > 0.0)) {
                    wheelStepAccumulator_ = 0.0;
                }
                wheelStepAccumulator_ += normalizedStepDelta;

                const int steps = static_cast<int>(std::trunc(wheelStepAccumulator_));
                if (steps != 0) {
                    wheelStepAccumulator_ -= steps;
                    emit wheelAdjusted(steps, wheelEvent->modifiers());
                }
                if (wheelEvent->phase() == Qt::ScrollEnd) {
                    wheelStepAccumulator_ = 0.0;
                }
                wheelEvent->accept();
                return true;
            }
            if (wheelEvent->phase() == Qt::ScrollEnd) {
                wheelStepAccumulator_ = 0.0;
            }
        }

        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            showPointer();
            schedulePointerHide();
            emit pointerActivity(mouseEventLocalPoint(mouseEvent));
            if (mouseEvent->button() == Qt::LeftButton) {
                clearPendingClick();
                if (videoPanEnabled_) {
                    videoPanActive_ = true;
                    videoPanMoved_ = false;
                    panLastPoint_ = mouseEventLocalPoint(mouseEvent);
                    updatePanCursor();
                    mouseEvent->accept();
                    return true;
                }
            }
            if (mouseGesturesEnabled_ && mouseEvent->button() == Qt::RightButton) {
                gestureTrackingActive_ = true;
                gestureStartPoint_ = mouseEventLocalPoint(mouseEvent);
                currentGestureDirection_.clear();
            }
            if (mouseEvent->button() == Qt::MiddleButton) {
                emit middleClicked();
                mouseEvent->accept();
                return true;
            }
            if (mouseEvent->button() == Qt::BackButton) {
                emit backNavigationRequested();
                mouseEvent->accept();
                return true;
            }
            if (mouseEvent->button() == Qt::ForwardButton) {
                emit forwardNavigationRequested();
                mouseEvent->accept();
                return true;
            }
        }

        if (event->type() == QEvent::MouseButtonDblClick) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            clearPendingClick();
            videoPanActive_ = false;
            videoPanMoved_ = false;
            updatePanCursor();
            emit pointerActivity(mouseEventLocalPoint(mouseEvent));
            emit doubleClicked(mouseEvent != nullptr ? mouseEvent->button() : Qt::NoButton);
            event->accept();
            return true;
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                const bool handledPanDrag = videoPanActive_ && videoPanMoved_;
                videoPanActive_ = false;
                videoPanMoved_ = false;
                updatePanCursor();
                if (handledPanDrag) {
                    clearPendingClick();
                    mouseEvent->accept();
                    return true;
                }

                pendingClickButton_ = Qt::LeftButton;
                if (clickTimer_ != nullptr) {
                    clickTimer_->start(QApplication::doubleClickInterval());
                }
                mouseEvent->accept();
                return true;
            }
            if (mouseGesturesEnabled_ && gestureTrackingActive_ && mouseEvent->button() == Qt::RightButton) {
                const QString directionId = currentGestureDirection_;
                hideActionOverlay();
                resetGestureState();
                if (!directionId.isEmpty()) {
                    emit gestureTriggered(directionId);
                    mouseEvent->accept();
                    return true;
                }
            }
        }

        if (event->type() == QEvent::Hide) {
            showPointer();
            hideActionOverlay();
            wheelStepAccumulator_ = 0.0;
            resetGestureState();
            videoPanActive_ = false;
            videoPanMoved_ = false;
            clearPendingClick();
            updatePanCursor();
        }
    }

    return QWidget::eventFilter(watched, event);
}

void VideoViewport::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event == nullptr) {
        return;
    }

    switch (event->type()) {
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        updateVolumeOverlayMeter();
        updateOverlayGeometry();
        break;
    default:
        break;
    }
}

void VideoViewport::mouseDoubleClickEvent(QMouseEvent *event)
{
    clearPendingClick();
    videoPanActive_ = false;
    videoPanMoved_ = false;
    updatePanCursor();
    emit doubleClicked(event != nullptr ? event->button() : Qt::NoButton);
    QWidget::mouseDoubleClickEvent(event);
}

void VideoViewport::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    renderHost_->setGeometry(rect());
    overlayLabel_->raise();
    updateOverlayGeometry();
}

void VideoViewport::updateOverlayGeometry()
{
    if (width() <= 0 || height() <= 0 || overlayLabel_ == nullptr) {
        return;
    }

    const int overlayWidth = std::clamp(width() - 140, 320, 680);
    const int overlayHeightLimit = std::max(140, height() - 120);

    overlayLabel_->setFixedWidth(overlayWidth);
    overlayLabel_->adjustSize();

    QSize overlaySize = overlayLabel_->size();
    overlaySize.setHeight(std::min(overlaySize.height(), overlayHeightLimit));
    overlayLabel_->resize(overlaySize);
    overlayLabel_->move(
        (width() - overlayLabel_->width()) / 2,
        std::max(28, ((height() - overlayLabel_->height()) / 2) - 18));
    overlayLabel_->raise();

    if (actionOverlayLabel_ != nullptr) {
        const int actionOverlayWidth = std::clamp(width() - 120, 220, 420);
        actionOverlayLabel_->setFixedWidth(actionOverlayWidth);
        actionOverlayLabel_->adjustSize();
        actionOverlayLabel_->move(
            std::max(16, width() / 48),
            std::max(16, height() / 36));
        actionOverlayLabel_->raise();
    }

    if (volumeOverlayWidget_ != nullptr && volumeOverlayTextLabel_ != nullptr) {
        const int meterHeight = std::clamp(
            static_cast<int>(std::lround(height() * 0.76)),
            260,
            std::max(260, height() - 64));
        if (volumeOverlayMeterShell_ != nullptr) {
            volumeOverlayMeterShell_->setFixedSize(18, meterHeight);
        }
        volumeOverlayTextLabel_->adjustSize();
        if (volumeOverlayValueLabel_ != nullptr) {
            const QFontMetrics valueMetrics(volumeOverlayValueLabel_->font());
            volumeOverlayValueLabel_->setFixedWidth(
                valueMetrics.horizontalAdvance(QStringLiteral("%1%").arg(volumeOverlayMaximum_)) + 2);
            volumeOverlayValueLabel_->adjustSize();
        }
        volumeOverlayWidget_->adjustSize();
        volumeOverlayWidget_->move(
            std::max(16, width() - volumeOverlayWidget_->width() - std::max(16, width() / 48)),
            std::max(56, (height() / 36) + (actionOverlayLabel_ != nullptr ? actionOverlayLabel_->height() + 10 : 0)));
        updateVolumeOverlayMeter();
        volumeOverlayWidget_->raise();
    }
}

void VideoViewport::updatePanCursor()
{
    if (renderHost_ == nullptr) {
        return;
    }

    if (pointerHidden_) {
        renderHost_->setCursor(Qt::BlankCursor);
        return;
    }

    if (!videoPanEnabled_) {
        renderHost_->unsetCursor();
        return;
    }

    renderHost_->setCursor(videoPanActive_ ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
}

void VideoViewport::showPointer()
{
    if (!pointerHidden_) {
        return;
    }

    pointerHidden_ = false;
    updatePanCursor();
}

void VideoViewport::schedulePointerHide()
{
    if (!pointerAutoHideEnabled_ || pointerHideTimer_ == nullptr) {
        return;
    }

    pointerHideTimer_->start(1600);
}

void VideoViewport::hidePointer()
{
    if (!pointerAutoHideEnabled_ || renderHost_ == nullptr || !renderHost_->underMouse()) {
        return;
    }

    pointerHidden_ = true;
    renderHost_->setCursor(Qt::BlankCursor);
}

void VideoViewport::clearPendingClick()
{
    pendingClickButton_ = Qt::NoButton;
    if (clickTimer_ != nullptr) {
        clickTimer_->stop();
    }
}

void VideoViewport::resetGestureState()
{
    gestureTrackingActive_ = false;
    gestureStartPoint_ = {};
    currentGestureDirection_.clear();
}

void VideoViewport::updateVolumeOverlayMeter()
{
    if (volumeOverlayMeterShell_ == nullptr
        || volumeOverlayMeterFill_ == nullptr
        || volumeOverlayMeterBoostFill_ == nullptr
        || volumeOverlayMeterMarker_ == nullptr) {
        return;
    }

    const QRect shellRect = volumeOverlayMeterShell_->rect().adjusted(4, 4, -4, -4);
    const int availableWidth = std::max(6, shellRect.width());
    const int availableHeight = std::max(1, shellRect.height());
    const int clampedValue = std::clamp(volumeOverlayValue_, 0, volumeOverlayMaximum_);
    const int baseValue = std::min(clampedValue, volumeOverlayNormalMaximum_);
    const int boostValue = std::max(0, clampedValue - volumeOverlayNormalMaximum_);

    const int baseHeight = static_cast<int>(std::lround(
        (static_cast<double>(baseValue) / volumeOverlayMaximum_) * availableHeight));
    const int boostHeight = static_cast<int>(std::lround(
        (static_cast<double>(boostValue) / volumeOverlayMaximum_) * availableHeight));
    const int totalHeight = std::clamp(baseHeight + boostHeight, 0, availableHeight);
    const double boostRatio = volumeOverlayMaximum_ > volumeOverlayNormalMaximum_
        ? std::clamp(
              static_cast<double>(boostValue) / std::max(1, volumeOverlayMaximum_ - volumeOverlayNormalMaximum_),
              0.0,
              1.0)
        : 0.0;
    const double normalLevel = volumeOverlayNormalMaximum_ > 0
        ? std::clamp(static_cast<double>(baseValue) / volumeOverlayNormalMaximum_, 0.0, 1.0)
        : 0.0;
    const QColor baseColor = volumeColorForNormalLevel(normalLevel);
    const QColor boostColor = volumeColorForBoostLevel(boostRatio);
    volumeOverlayMeterFill_->setStyleSheet(
        QStringLiteral("background:%1; border:none; border-radius:999px;")
            .arg(baseColor.name(QColor::HexRgb)));
    volumeOverlayMeterBoostFill_->setStyleSheet(
        QStringLiteral("background:%1; border:none; border-radius:999px;")
            .arg(boostColor.name(QColor::HexRgb)));
    if (volumeOverlayValueLabel_ != nullptr) {
        volumeOverlayValueLabel_->setStyleSheet(QStringLiteral("color:%1;")
                                                    .arg((boostValue > 0 ? boostColor : baseColor).name(QColor::HexRgb)));
    }

    if (totalHeight > 0) {
        if (boostHeight > 0) {
            volumeOverlayMeterBoostFill_->setGeometry(
                shellRect.left(),
                shellRect.bottom() - totalHeight + 1,
                availableWidth,
                boostHeight);
            volumeOverlayMeterBoostFill_->show();
        } else {
            volumeOverlayMeterBoostFill_->hide();
        }

        if (baseHeight > 0) {
            volumeOverlayMeterFill_->setGeometry(
                shellRect.left(),
                shellRect.bottom() - baseHeight + 1,
                availableWidth,
                baseHeight);
            volumeOverlayMeterFill_->show();
        } else {
            volumeOverlayMeterFill_->hide();
        }
    } else {
        volumeOverlayMeterFill_->hide();
        volumeOverlayMeterBoostFill_->hide();
    }

    const int markerOffset = static_cast<int>(std::lround(
        (static_cast<double>(volumeOverlayNormalMaximum_) / volumeOverlayMaximum_) * availableHeight));
    const int markerY = std::clamp(shellRect.bottom() - markerOffset, shellRect.top(), shellRect.bottom() - 1);
    volumeOverlayMeterMarker_->setGeometry(shellRect.left() - 1, markerY, availableWidth + 2, 2);
    volumeOverlayMeterBoostFill_->raise();
    volumeOverlayMeterFill_->raise();
    volumeOverlayMeterMarker_->raise();
}

}  // namespace revaplayer::ui
