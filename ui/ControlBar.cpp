#include "ui/ControlBar.hpp"

#include "application/PlaybackTuning.hpp"
#include "application/UiLanguage.hpp"

#include <QApplication>
#include <QEvent>
#include <QAction>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QToolButton>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

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

QColor blendColors(const QColor &from, const QColor &to, const qreal ratio)
{
    const qreal clampedRatio = std::clamp(ratio, 0.0, 1.0);
    return QColor(
        static_cast<int>(std::lround(from.red() + ((to.red() - from.red()) * clampedRatio))),
        static_cast<int>(std::lround(from.green() + ((to.green() - from.green()) * clampedRatio))),
        static_cast<int>(std::lround(from.blue() + ((to.blue() - from.blue()) * clampedRatio))),
        static_cast<int>(std::lround(from.alpha() + ((to.alpha() - from.alpha()) * clampedRatio))));
}

bool isPaletteDark(const QPalette &palette)
{
    return palette.color(QPalette::Window).lightness() < 128;
}

QColor paletteColor(const QWidget *widget, const QPalette::ColorRole role, const QColor &fallback)
{
    if (widget == nullptr) {
        return fallback;
    }

    const QPalette::ColorGroup group = widget->isEnabled() ? QPalette::Active : QPalette::Disabled;
    const QColor color = widget->palette().color(group, role);
    return color.isValid() ? color : fallback;
}

QColor sliderAccentColor(const QWidget *widget, const QColor &fallback)
{
    Q_UNUSED(widget);

    QColor color = QApplication::palette().color(QPalette::Active, QPalette::Highlight);
    if (!color.isValid()) {
        color = fallback;
    }
    color.setAlpha(255);
    return color;
}

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

int labeledButtonMinimumWidth(const QToolButton *button,
                              const QString &text,
                              const int compactWidth)
{
    if (button == nullptr || text.trimmed().isEmpty()) {
        return compactWidth;
    }

    const QFontMetrics metrics(button->font());
    const int iconWidth = std::max(16, button->iconSize().width());
    const int textWidth = metrics.horizontalAdvance(text.trimmed());
    const int framePadding = std::max(24, button->style()->pixelMetric(QStyle::PM_ButtonMargin, nullptr, button) * 4);
    return std::max(compactWidth, iconWidth + textWidth + framePadding + 10);
}

enum class ControlGlyph {
    Open,
    Previous,
    Play,
    Pause,
    Next,
    Stop,
    Fullscreen,
    Playlist,
    Details,
    Volume,
    RepeatOff,
    RepeatFile,
    RepeatPlaylist,
    LoopIdle,
    LoopStart,
    LoopActive,
    Quality,
    Subtitle,
};

class TimelineSlider final : public QSlider {
public:
    explicit TimelineSlider(const Qt::Orientation orientation, QWidget *parent = nullptr)
        : QSlider(orientation, parent)
    {
    }

    void setDurationSeconds(const double durationSeconds)
    {
        const double normalizedDuration = std::max(0.0, durationSeconds);
        if (std::abs(durationSeconds_ - normalizedDuration) < 0.01) {
            return;
        }
        durationSeconds_ = normalizedDuration;
        update();
    }

    void setMarkers(const QVector<revaplayer::ui::TimelineMarker> &markers)
    {
        if (markers_ == markers) {
            return;
        }
        markers_ = markers;
        update();
    }

    void setBufferedSeconds(const double bufferedSeconds)
    {
        const double normalizedBuffered = std::max(0.0, bufferedSeconds);
        if (std::abs(bufferedSeconds_ - normalizedBuffered) < 0.05) {
            return;
        }
        bufferedSeconds_ = normalizedBuffered;
        update();
    }

    void setVisualMetrics(const int grooveHeight, const int handleSize)
    {
        const int normalizedGroove = std::clamp(grooveHeight, 4, 18);
        const int normalizedHandle = std::clamp(handleSize, normalizedGroove + 4, 30);
        if (grooveHeight_ == normalizedGroove && handleSize_ == normalizedHandle) {
            return;
        }
        grooveHeight_ = normalizedGroove;
        handleSize_ = normalizedHandle;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);

        if (orientation() != Qt::Horizontal) {
            QSlider::paintEvent(event);
            return;
        }

        const int sidePadding = std::max(2, handleSize_ / 2);
        const QRectF trackRect(
            sidePadding,
            (height() - grooveHeight_) / 2.0,
            std::max(1, width() - (sidePadding * 2)),
            grooveHeight_);
        if (!trackRect.isValid() || trackRect.width() <= 0.0) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);

        const QColor accent = sliderAccentColor(this, QColor(QStringLiteral("#2d98ff")));
        const bool darkPalette = isPaletteDark(palette());
        QColor grooveStart = paletteColor(this, QPalette::Base, darkPalette ? QColor(QStringLiteral("#0c121b")) : QColor(QStringLiteral("#f7efe4")));
        QColor grooveEnd = paletteColor(this, QPalette::AlternateBase, darkPalette ? QColor(QStringLiteral("#151d29")) : QColor(QStringLiteral("#fff8ee")));
        grooveStart.setAlpha(255);
        grooveEnd.setAlpha(255);
        QLinearGradient grooveGradient(trackRect.topLeft(), trackRect.topRight());
        grooveGradient.setColorAt(0.0, grooveStart);
        grooveGradient.setColorAt(1.0, grooveEnd);
        painter.setBrush(grooveGradient);
        painter.drawRoundedRect(trackRect, grooveHeight_ / 2.0, grooveHeight_ / 2.0);

        const int sliderSpan = std::max(1, maximum() - minimum());
        const double playedFraction = std::clamp(
            static_cast<double>(value() - minimum()) / sliderSpan,
            0.0,
            1.0);
        QColor progressStart = accent;
        QColor progressEnd = accent;
        progressStart.setAlpha(255);
        progressEnd.setAlpha(255);
        const qreal playedWidth = playedFraction * trackRect.width();
        const auto drawProgressOverlay = [&]() {
            if (playedWidth <= 0.0) {
                return;
            }
            const QRectF progressRect(
                trackRect.left(),
                trackRect.top(),
                playedWidth,
                trackRect.height());
            QLinearGradient progressGradient(progressRect.topLeft(), progressRect.topRight());
            progressGradient.setColorAt(0.0, progressStart);
            progressGradient.setColorAt(1.0, progressEnd);
            painter.setBrush(progressGradient);
            painter.drawRoundedRect(progressRect, grooveHeight_ / 2.0, grooveHeight_ / 2.0);
        };

        const double bufferedFraction = std::clamp(bufferedSeconds_ / durationSeconds_, 0.0, 1.0);
        if (durationSeconds_ > 0.0 && bufferedFraction > playedFraction + 0.002) {
            const qreal playedX = trackRect.left() + (playedFraction * trackRect.width());
            const qreal bufferedX = trackRect.left() + (bufferedFraction * trackRect.width());
            const qreal segmentWidth = std::max<qreal>(0.0, bufferedX - playedX);
            if (segmentWidth > 0.0) {
                QColor bufferedColor = accent.lighter(darkPalette ? 135 : 122);
                bufferedColor.setAlpha(isEnabled() ? 118 : 72);
                const qreal bufferHeight = std::max<qreal>(3.0, trackRect.height() - 2.0);
                const QRectF bufferedRect(
                    playedX,
                    trackRect.center().y() - (bufferHeight / 2.0),
                    segmentWidth,
                    bufferHeight);
                painter.setBrush(bufferedColor);
                painter.drawRoundedRect(bufferedRect, bufferHeight / 2.0, bufferHeight / 2.0);
            }
        }

        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        drawProgressOverlay();

        if (durationSeconds_ > 0.0) {
            for (const auto &marker : markers_) {
                const double fraction = std::clamp(marker.timeSeconds / durationSeconds_, 0.0, 1.0);
                const qreal x = trackRect.left() + (fraction * trackRect.width());
                const QRectF markerRect(x - 1.0, trackRect.center().y() - 5.0, 3.0, 10.0);
                painter.setBrush(marker.label.trimmed().isEmpty()
                                     ? QColor(QStringLiteral("#8da1bc"))
                                     : QColor(QStringLiteral("#f4c97a")));
                painter.drawRoundedRect(markerRect, 1.4, 1.4);
            }
        }

        const QPointF handleCenter(trackRect.left() + playedWidth, trackRect.center().y());
        const QRectF handleRect(
            handleCenter.x() - (handleSize_ / 2.0),
            handleCenter.y() - (handleSize_ / 2.0),
            handleSize_,
            handleSize_);
        QColor handleColor = accent.lighter(darkPalette ? 114 : 106);
        handleColor.setAlpha(255);
        QColor handleBorder = accent.darker(darkPalette ? 135 : 150);
        handleBorder.setAlpha(255);
        painter.setPen(QPen(handleBorder, 1.2));
        painter.setBrush(handleColor);
        painter.drawEllipse(handleRect.adjusted(1.0, 1.0, -1.0, -1.0));
    }

private:
    QVector<revaplayer::ui::TimelineMarker> markers_;
    double durationSeconds_ {0.0};
    double bufferedSeconds_ {0.0};
    int grooveHeight_ {6};
    int handleSize_ {16};
};

class VolumeSlider final : public QSlider {
public:
    explicit VolumeSlider(const Qt::Orientation orientation, QWidget *parent = nullptr)
        : QSlider(orientation, parent)
    {
    }

    void setNormalMaximum(const int normalMaximum)
    {
        const int normalizedMaximum = std::max(0, normalMaximum);
        if (normalMaximum_ == normalizedMaximum) {
            return;
        }
        normalMaximum_ = normalizedMaximum;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        if (orientation() != Qt::Horizontal) {
            QSlider::paintEvent(event);
            return;
        }

        QStyleOptionSlider option;
        initStyleOption(&option);
        const QRect grooveRect = style()->subControlRect(
            QStyle::CC_Slider,
            &option,
            QStyle::SC_SliderGroove,
            this);
        if (!grooveRect.isValid() || grooveRect.width() <= 0) {
            QSlider::paintEvent(event);
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const bool darkPalette = isPaletteDark(palette());

        QColor grooveStart = paletteColor(this, QPalette::Base, darkPalette ? QColor(QStringLiteral("#111720")) : QColor(QStringLiteral("#f6f8fb")));
        QColor grooveEnd = paletteColor(this, QPalette::AlternateBase, darkPalette ? QColor(QStringLiteral("#202838")) : QColor(QStringLiteral("#e8eef6")));
        grooveStart.setAlpha(isEnabled() ? (darkPalette ? 236 : 232) : 168);
        grooveEnd.setAlpha(isEnabled() ? (darkPalette ? 240 : 236) : 172);
        const QColor volumeLow = volumeLowColor();
        const QColor volumeNormalLimit = volumeNormalLimitColor();
        QColor grooveBorder = darkPalette ? QColor(QStringLiteral("#9b5a16")) : QColor(QStringLiteral("#b26512"));
        grooveBorder.setAlpha(isEnabled() ? (darkPalette ? 164 : 144) : 96);
        QColor normalZoneStart = blendColors(grooveStart, volumeLow.darker(150), darkPalette ? 0.42 : 0.34);
        QColor normalZoneEnd = blendColors(grooveEnd, volumeNormalLimit, darkPalette ? 0.50 : 0.42);
        normalZoneStart.setAlpha(isEnabled() ? (darkPalette ? 162 : 134) : 82);
        normalZoneEnd.setAlpha(isEnabled() ? (darkPalette ? 184 : 148) : 94);
        QColor markerColor = QColor(QStringLiteral("#fff4dc"));
        markerColor.setAlpha(isEnabled() ? (darkPalette ? 236 : 220) : 132);

        const QRectF trackRect = grooveRect.adjusted(0, 1, 0, -1);
        const qreal trackRadius = std::max<qreal>(3.0, trackRect.height() / 2.0);
        QLinearGradient grooveGradient(trackRect.topLeft(), trackRect.topRight());
        grooveGradient.setColorAt(0.0, grooveStart);
        grooveGradient.setColorAt(1.0, grooveEnd);
        painter.setPen(QPen(grooveBorder, 1.0));
        painter.setBrush(grooveGradient);
        painter.drawRoundedRect(trackRect, trackRadius, trackRadius);

        const int sliderSpan = std::max(1, maximum() - minimum());
        const qreal playedFraction = std::clamp(
            static_cast<qreal>(value() - minimum()) / static_cast<qreal>(sliderSpan),
            0.0,
            1.0);
        const qreal normalFraction = std::clamp(
            static_cast<qreal>(std::clamp(normalMaximum_, minimum(), maximum()) - minimum())
                / static_cast<qreal>(sliderSpan),
            0.0,
            1.0);
        const int filledWidth = static_cast<int>(std::lround(playedFraction * trackRect.width()));
        const int normalWidth = static_cast<int>(std::lround(normalFraction * trackRect.width()));
        const int baseWidth = std::clamp(filledWidth, 0, normalWidth);
        const int boostWidth = std::max(0, filledWidth - normalWidth);
        const bool boosted = value() > normalMaximum_;
        const qreal boostRatio = maximum() > normalMaximum_
            ? std::clamp(
                  static_cast<qreal>(std::max(0, value() - normalMaximum_))
                      / static_cast<qreal>(std::max(1, maximum() - normalMaximum_)),
                  0.0,
                  1.0)
            : 0.0;
        const qreal normalLevel = normalMaximum_ > minimum()
            ? std::clamp(
                  static_cast<qreal>(std::clamp(value(), minimum(), normalMaximum_) - minimum())
                      / static_cast<qreal>(std::max(1, normalMaximum_ - minimum())),
                  0.0,
                  1.0)
            : playedFraction;
        const QColor normalFillColor = volumeColorForNormalLevel(normalLevel);
        const QColor boostFillColor = volumeColorForBoostLevel(boostRatio);
        painter.setPen(Qt::NoPen);

        if (normalWidth > 0) {
            QLinearGradient normalZoneGradient(trackRect.topLeft(), trackRect.topRight());
            normalZoneGradient.setColorAt(0.0, normalZoneStart);
            normalZoneGradient.setColorAt(1.0, normalZoneEnd);
            painter.setBrush(normalZoneGradient);
            painter.drawRoundedRect(
                QRectF(trackRect.left(), trackRect.top(), normalWidth, trackRect.height()),
                trackRadius,
                trackRadius);
        }
        if (baseWidth > 0) {
            QLinearGradient baseFillGradient(
                trackRect.topLeft(),
                QPointF(trackRect.left() + std::max<qreal>(1.0, baseWidth), trackRect.top()));
            baseFillGradient.setColorAt(0.0, normalFillColor.lighter(104));
            baseFillGradient.setColorAt(1.0, normalFillColor);
            painter.setBrush(baseFillGradient);
            painter.drawRoundedRect(
                QRectF(trackRect.left(), trackRect.top(), baseWidth, trackRect.height()),
                trackRadius,
                trackRadius);
        }
        if (boostWidth > 0) {
            QLinearGradient boostFillGradient(
                QPointF(trackRect.left() + normalWidth, trackRect.top()),
                trackRect.topRight());
            boostFillGradient.setColorAt(0.0, boostFillColor.lighter(105));
            boostFillGradient.setColorAt(1.0, boostFillColor);
            painter.setBrush(boostFillGradient);
            painter.drawRoundedRect(
                QRectF(trackRect.left() + normalWidth, trackRect.top(), boostWidth, trackRect.height()),
                trackRadius,
                trackRadius);
        }

        if (normalMaximum_ > minimum() && normalMaximum_ < maximum()) {
            const int markerX = static_cast<int>(std::lround(trackRect.left() + normalFraction * trackRect.width()));
            painter.setBrush(markerColor);
            painter.drawRoundedRect(
                QRectF(markerX - 1.0, trackRect.top() - 1.0, 2.0, trackRect.height() + 2.0),
                1.0,
                1.0);
        }

        QRect handleRect = style()->subControlRect(
            QStyle::CC_Slider,
            &option,
            QStyle::SC_SliderHandle,
            this);
        if (!handleRect.isValid() || handleRect.width() <= 0 || handleRect.height() <= 0) {
            const int diameter = std::clamp(static_cast<int>(std::lround(trackRect.height() + 8.0)), 12, 22);
            const int centerX = static_cast<int>(std::lround(trackRect.left() + (playedFraction * trackRect.width())));
            const int centerY = static_cast<int>(std::lround(trackRect.center().y()));
            handleRect = QRect(centerX - (diameter / 2), centerY - (diameter / 2), diameter, diameter);
        }
        const QRectF handleRectF = QRectF(handleRect).adjusted(1.0, 1.0, -1.0, -1.0);
        QColor handleStart = boosted
            ? blendColors(QColor(QStringLiteral("#ff7a35")), QColor(QStringLiteral("#da2a0a")), boostRatio)
            : blendColors(QColor(QStringLiteral("#fff4dc")), QColor(QStringLiteral("#ff7a35")), normalLevel);
        QColor handleEnd = boosted ? boostFillColor : normalFillColor;
        QColor handleBorder = boosted ? QColor(QStringLiteral("#8d3300")) : QColor(QStringLiteral("#ad6a1f"));
        handleBorder.setAlpha(boosted ? 190 : 132);

        if (boosted) {
            painter.setBrush(boostFillColor);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(handleRectF.adjusted(-1.0, -1.0, 1.0, 1.0));
        }

        QLinearGradient handleGradient(handleRectF.topLeft(), handleRectF.bottomLeft());
        handleGradient.setColorAt(0.0, handleStart);
        handleGradient.setColorAt(1.0, handleEnd);
        painter.setPen(QPen(handleBorder, 1.0));
        painter.setBrush(handleGradient);
        painter.drawEllipse(handleRectF);
    }

private:
    int normalMaximum_ {revaplayer::application::kDefaultPlaybackVolume};
};

struct ControlBarStyleMetrics final {
    int regularButtonSize {40};
    int primaryButtonSize {44};
    int panelButtonWidth {52};
    int panelButtonHeight {40};
    int timeLabelWidth {108};
    int speedButtonWidth {72};
    int defaultVolumeSliderWidth {88};
};

ControlBarStyleMetrics controlBarMetrics()
{
    return {};
}

QString rgbaCss(const QColor &color)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

QString volumeSliderStyleSheet(const QPalette &palette, const int grooveHeight, const int handleSize)
{
    const bool darkPalette = isPaletteDark(palette);
    QColor grooveColor = darkPalette ? QColor(QStringLiteral("#111720")) : QColor(QStringLiteral("#1d2530"));
    grooveColor.setAlpha(darkPalette ? 232 : 190);
    QColor addPageColor = darkPalette ? QColor(QStringLiteral("#171a21")) : QColor(QStringLiteral("#2e3138"));
    addPageColor.setAlpha(darkPalette ? 244 : 214);
    QColor progressStart = volumeLowColor();
    QColor progressEnd = volumeNormalLimitColor();
    QColor handleColor = volumeMidColor();
    progressStart.setAlpha(255);
    progressEnd.setAlpha(255);
    handleColor.setAlpha(255);
    QColor borderColor = darkPalette ? QColor(QStringLiteral("#9b5a16")) : QColor(QStringLiteral("#b26512"));
    borderColor.setAlpha(darkPalette ? 136 : 104);

    const int safeGrooveHeight = std::clamp(grooveHeight, 4, 18);
    const int safeHandleSize = std::clamp(handleSize, safeGrooveHeight + 4, 30);
    const int handleRadius = std::max(3, safeHandleSize / 2);
    const int handleMargin = -std::max(1, (safeHandleSize - safeGrooveHeight) / 2);

    return QStringLiteral(
               "QSlider::groove:horizontal {"
               " background: %1;"
               " border: 1px solid %2;"
               " height: %3px;"
               " border-radius: %3px;"
               "}"
               "QSlider::sub-page:horizontal {"
               " background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %4, stop:1 %5);"
               " border: none;"
               " border-radius: %3px;"
               "}"
               "QSlider::add-page:horizontal {"
               " background: %6;"
               " border: none;"
               " border-radius: %3px;"
               "}"
               "QSlider::handle:horizontal {"
               " background: %7;"
               " border: 1px solid %8;"
               " border-radius: %9px;"
               " margin: %10px 0;"
               " width: %11px;"
               "}"
               "QSlider::handle:horizontal:hover {"
               " background: %12;"
               " border: 1px solid %13;"
               "}")
        .arg(rgbaCss(grooveColor))
        .arg(rgbaCss(borderColor))
        .arg(std::max(3, safeGrooveHeight / 2))
        .arg(rgbaCss(progressStart))
        .arg(rgbaCss(progressEnd))
        .arg(rgbaCss(addPageColor))
        .arg(rgbaCss(handleColor))
        .arg(rgbaCss(borderColor))
        .arg(handleRadius)
        .arg(handleMargin)
        .arg(safeHandleSize)
        .arg(rgbaCss(handleColor.lighter(darkPalette ? 108 : 116)))
        .arg(rgbaCss(borderColor.lighter(darkPalette ? 112 : 106)));
}

QIcon drawGlyphIcon(const ControlGlyph glyph, const QColor &color, const QSize &size)
{
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPen stroke(color);
    stroke.setJoinStyle(Qt::RoundJoin);
    stroke.setCapStyle(Qt::RoundCap);
    stroke.setWidthF(std::max(1.5, size.width() / 13.0));
    painter.setPen(stroke);
    painter.setBrush(color);

    const QRectF rect(2.0, 2.0, size.width() - 4.0, size.height() - 4.0);
    const auto pointAt = [&rect](const qreal x, const qreal y) {
        return QPointF(rect.left() + rect.width() * x, rect.top() + rect.height() * y);
    };
    const QColor inactiveColor = QColor(color.red(), color.green(), color.blue(), 110);

    switch (glyph) {
    case ControlGlyph::Open: {
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(pointAt(0.14, 0.42), pointAt(0.60, 0.84)), 2.0, 2.0);
        painter.drawLine(pointAt(0.40, 0.60), pointAt(0.82, 0.18));
        painter.drawLine(pointAt(0.58, 0.18), pointAt(0.82, 0.18));
        painter.drawLine(pointAt(0.82, 0.18), pointAt(0.82, 0.42));
        break;
    }
    case ControlGlyph::Previous: {
        painter.drawRect(QRectF(pointAt(0.18, 0.22), QSizeF(rect.width() * 0.10, rect.height() * 0.56)));
        QPainterPath triangle;
        triangle.moveTo(pointAt(0.74, 0.20));
        triangle.lineTo(pointAt(0.34, 0.50));
        triangle.lineTo(pointAt(0.74, 0.80));
        triangle.closeSubpath();
        painter.drawPath(triangle);
        break;
    }
    case ControlGlyph::Play: {
        QPainterPath triangle;
        triangle.moveTo(pointAt(0.28, 0.18));
        triangle.lineTo(pointAt(0.78, 0.50));
        triangle.lineTo(pointAt(0.28, 0.82));
        triangle.closeSubpath();
        painter.drawPath(triangle);
        break;
    }
    case ControlGlyph::Pause:
        painter.drawRoundedRect(QRectF(pointAt(0.26, 0.18), QSizeF(rect.width() * 0.16, rect.height() * 0.64)), 1.8, 1.8);
        painter.drawRoundedRect(QRectF(pointAt(0.58, 0.18), QSizeF(rect.width() * 0.16, rect.height() * 0.64)), 1.8, 1.8);
        break;
    case ControlGlyph::Next: {
        QPainterPath triangle;
        triangle.moveTo(pointAt(0.28, 0.20));
        triangle.lineTo(pointAt(0.68, 0.50));
        triangle.lineTo(pointAt(0.28, 0.80));
        triangle.closeSubpath();
        painter.drawPath(triangle);
        painter.drawRect(QRectF(pointAt(0.72, 0.22), QSizeF(rect.width() * 0.10, rect.height() * 0.56)));
        break;
    }
    case ControlGlyph::Stop:
        painter.drawRoundedRect(QRectF(pointAt(0.24, 0.24), QSizeF(rect.width() * 0.52, rect.height() * 0.52)), 2.0, 2.0);
        break;
    case ControlGlyph::Fullscreen: {
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(pointAt(0.18, 0.38), pointAt(0.18, 0.18));
        painter.drawLine(pointAt(0.18, 0.18), pointAt(0.38, 0.18));
        painter.drawLine(pointAt(0.62, 0.18), pointAt(0.82, 0.18));
        painter.drawLine(pointAt(0.82, 0.18), pointAt(0.82, 0.38));
        painter.drawLine(pointAt(0.18, 0.62), pointAt(0.18, 0.82));
        painter.drawLine(pointAt(0.18, 0.82), pointAt(0.38, 0.82));
        painter.drawLine(pointAt(0.62, 0.82), pointAt(0.82, 0.82));
        painter.drawLine(pointAt(0.82, 0.62), pointAt(0.82, 0.82));
        break;
    }
    case ControlGlyph::Playlist: {
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(pointAt(0.22, 0.28), pointAt(0.82, 0.28));
        painter.drawLine(pointAt(0.22, 0.50), pointAt(0.82, 0.50));
        painter.drawLine(pointAt(0.36, 0.72), pointAt(0.82, 0.72));
        QPainterPath playPath;
        playPath.moveTo(pointAt(0.18, 0.60));
        playPath.lineTo(pointAt(0.18, 0.84));
        playPath.lineTo(pointAt(0.34, 0.72));
        playPath.closeSubpath();
        painter.setBrush(color);
        painter.drawPath(playPath);
        break;
    }
    case ControlGlyph::Details: {
        painter.setBrush(Qt::NoBrush);
        const QRectF cardRect(pointAt(0.18, 0.18), QSizeF(rect.width() * 0.64, rect.height() * 0.64));
        painter.drawRoundedRect(cardRect, 3.0, 3.0);
        painter.drawLine(pointAt(0.30, 0.36), pointAt(0.62, 0.36));
        painter.drawLine(pointAt(0.30, 0.50), pointAt(0.68, 0.50));
        painter.drawLine(pointAt(0.30, 0.64), pointAt(0.56, 0.64));

        painter.setBrush(color);
        painter.drawEllipse(pointAt(0.68, 0.28), rect.width() * 0.13, rect.height() * 0.13);

        QFont infoFont = painter.font();
        infoFont.setBold(true);
        infoFont.setPixelSize(std::max(8, static_cast<int>(size.height() * 0.32)));
        painter.setFont(infoFont);
        painter.setPen(Qt::white);
        painter.drawText(
            QRectF(pointAt(0.55, 0.15), QSizeF(rect.width() * 0.26, rect.height() * 0.26)),
            Qt::AlignCenter,
            QStringLiteral("i"));
        break;
    }
    case ControlGlyph::Volume: {
        painter.setBrush(Qt::NoBrush);
        QPainterPath speaker;
        speaker.moveTo(pointAt(0.20, 0.42));
        speaker.lineTo(pointAt(0.34, 0.42));
        speaker.lineTo(pointAt(0.48, 0.26));
        speaker.lineTo(pointAt(0.48, 0.74));
        speaker.lineTo(pointAt(0.34, 0.58));
        speaker.lineTo(pointAt(0.20, 0.58));
        speaker.closeSubpath();
        painter.drawPath(speaker);
        painter.drawArc(QRectF(pointAt(0.42, 0.24), QSizeF(rect.width() * 0.30, rect.height() * 0.52)), -40 * 16, 80 * 16);
        painter.drawArc(QRectF(pointAt(0.48, 0.14), QSizeF(rect.width() * 0.36, rect.height() * 0.72)), -40 * 16, 80 * 16);
        break;
    }
    case ControlGlyph::RepeatOff:
    case ControlGlyph::RepeatFile:
    case ControlGlyph::RepeatPlaylist: {
        painter.setBrush(Qt::NoBrush);
        painter.drawArc(QRectF(pointAt(0.16, 0.12), QSizeF(rect.width() * 0.54, rect.height() * 0.34)), 28 * 16, 240 * 16);
        painter.drawArc(QRectF(pointAt(0.30, 0.44), QSizeF(rect.width() * 0.54, rect.height() * 0.34)), 208 * 16, 240 * 16);
        painter.drawLine(pointAt(0.71, 0.18), pointAt(0.84, 0.30));
        painter.drawLine(pointAt(0.71, 0.42), pointAt(0.84, 0.30));
        painter.drawLine(pointAt(0.16, 0.70), pointAt(0.29, 0.58));
        painter.drawLine(pointAt(0.16, 0.70), pointAt(0.29, 0.82));

        if (glyph == ControlGlyph::RepeatFile) {
            const QRectF badgeRect(pointAt(0.56, 0.50), QSizeF(rect.width() * 0.24, rect.height() * 0.24));
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawEllipse(badgeRect);

            QFont badgeFont = painter.font();
            badgeFont.setBold(true);
            badgeFont.setPixelSize(std::max(8, static_cast<int>(size.height() * 0.24)));
            painter.setFont(badgeFont);
            painter.setPen(Qt::white);
            painter.drawText(badgeRect, Qt::AlignCenter, QStringLiteral("1"));
        } else if (glyph == ControlGlyph::RepeatPlaylist) {
            painter.setPen(QPen(color, std::max(1.2, size.width() / 15.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawLine(pointAt(0.56, 0.55), pointAt(0.82, 0.55));
            painter.drawLine(pointAt(0.56, 0.67), pointAt(0.82, 0.67));
            painter.drawLine(pointAt(0.62, 0.79), pointAt(0.82, 0.79));
        }
        break;
    }
    case ControlGlyph::LoopIdle:
    case ControlGlyph::LoopStart:
    case ControlGlyph::LoopActive: {
        const QColor leftColor = glyph == ControlGlyph::LoopIdle ? inactiveColor : color;
        const QColor rightColor = glyph == ControlGlyph::LoopActive ? color : inactiveColor;

        painter.setPen(Qt::NoPen);
        painter.setBrush(leftColor);
        painter.drawRoundedRect(QRectF(pointAt(0.20, 0.20), QSizeF(rect.width() * 0.09, rect.height() * 0.58)), 1.8, 1.8);
        painter.setBrush(rightColor);
        painter.drawRoundedRect(QRectF(pointAt(0.71, 0.20), QSizeF(rect.width() * 0.09, rect.height() * 0.58)), 1.8, 1.8);

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(color, std::max(1.3, size.width() / 16.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(pointAt(0.31, 0.32), pointAt(0.69, 0.32));
        painter.drawLine(pointAt(0.67, 0.68), pointAt(0.33, 0.68));
        painter.drawLine(pointAt(0.33, 0.68), pointAt(0.42, 0.58));
        painter.drawLine(pointAt(0.33, 0.68), pointAt(0.42, 0.78));
        break;
    }
    case ControlGlyph::Quality: {
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(pointAt(0.16, 0.24), QSizeF(rect.width() * 0.68, rect.height() * 0.48)), 3.0, 3.0);
        painter.drawLine(pointAt(0.34, 0.76), pointAt(0.66, 0.76));

        QFont badgeFont = painter.font();
        badgeFont.setBold(true);
        badgeFont.setPixelSize(std::max(8, static_cast<int>(size.height() * 0.23)));
        painter.setFont(badgeFont);
        painter.drawText(QRectF(pointAt(0.20, 0.28), QSizeF(rect.width() * 0.60, rect.height() * 0.36)),
                         Qt::AlignCenter,
                         QStringLiteral("HD"));
        break;
    }
    case ControlGlyph::Subtitle: {
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(pointAt(0.14, 0.24), QSizeF(rect.width() * 0.72, rect.height() * 0.48)), 3.0, 3.0);

        QFont badgeFont = painter.font();
        badgeFont.setBold(true);
        badgeFont.setPixelSize(std::max(8, static_cast<int>(size.height() * 0.22)));
        painter.setFont(badgeFont);
        painter.drawText(QRectF(pointAt(0.18, 0.28), QSizeF(rect.width() * 0.64, rect.height() * 0.34)),
                         Qt::AlignCenter,
                         QStringLiteral("CC"));

        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(QRectF(pointAt(0.28, 0.78), QSizeF(rect.width() * 0.18, rect.height() * 0.06)), 1.0, 1.0);
        painter.drawRoundedRect(QRectF(pointAt(0.54, 0.78), QSizeF(rect.width() * 0.18, rect.height() * 0.06)), 1.0, 1.0);
        break;
    }
    }

    return QIcon(pixmap);
}

ControlGlyph repeatGlyph(const QString &mode)
{
    if (mode == QStringLiteral("file")) {
        return ControlGlyph::RepeatFile;
    }
    if (mode == QStringLiteral("playlist")) {
        return ControlGlyph::RepeatPlaylist;
    }
    return ControlGlyph::RepeatOff;
}

ControlGlyph loopGlyph(const double loopStartSeconds, const double loopEndSeconds)
{
    if (loopStartSeconds >= 0.0 && loopEndSeconds >= 0.0) {
        return ControlGlyph::LoopActive;
    }
    if (loopStartSeconds >= 0.0) {
        return ControlGlyph::LoopStart;
    }
    return ControlGlyph::LoopIdle;
}

void refreshWidgetStyle(QWidget *widget)
{
    if (widget == nullptr) {
        return;
    }

    if (QStyle *styleEngine = widget->style(); styleEngine != nullptr) {
        styleEngine->unpolish(widget);
        styleEngine->polish(widget);
    }
    widget->update();
}

}  // namespace

ControlBar::ControlBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("controlBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    openButton_ = new QToolButton(this);
    openButton_->setObjectName(QStringLiteral("controlOpenButton"));
    openButton_->setFixedHeight(40);

    previousButton_ = new QToolButton(this);
    previousButton_->setObjectName(QStringLiteral("controlPreviousButton"));
    previousButton_->setFixedSize(40, 40);

    playPauseButton_ = new QToolButton(this);
    playPauseButton_->setObjectName(QStringLiteral("controlPlayPauseButton"));
    playPauseButton_->setFixedSize(44, 44);

    nextButton_ = new QToolButton(this);
    nextButton_->setObjectName(QStringLiteral("controlNextButton"));
    nextButton_->setFixedSize(40, 40);

    stopButton_ = new QToolButton(this);
    stopButton_->setObjectName(QStringLiteral("controlStopButton"));
    stopButton_->setFixedSize(40, 40);

    fullscreenButton_ = new QToolButton(this);
    fullscreenButton_->setObjectName(QStringLiteral("controlFullscreenButton"));
    fullscreenButton_->setFixedSize(40, 40);

    playlistButton_ = new QToolButton(this);
    playlistButton_->setObjectName(QStringLiteral("controlPlaylistButton"));
    playlistButton_->setCheckable(true);
    playlistButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    playlistButton_->setFixedSize(52, 40);

    detailsButton_ = new QToolButton(this);
    detailsButton_->setObjectName(QStringLiteral("controlDetailsButton"));
    detailsButton_->setCheckable(true);
    detailsButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    detailsButton_->setFixedSize(52, 40);

    positionSlider_ = new TimelineSlider(Qt::Horizontal, this);
    positionSlider_->setRange(0, 1000);
    positionSlider_->setMouseTracking(true);
    positionSlider_->installEventFilter(this);
    positionSlider_->setObjectName(QStringLiteral("controlPositionSlider"));

    timeLabel_ = new QLabel(QStringLiteral("00:00 / 00:00"), this);
    timeLabel_->setObjectName(QStringLiteral("controlTimeLabel"));
    timeLabel_->setMinimumWidth(108);
    timeLabel_->setAlignment(Qt::AlignCenter);
    timeLabel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    speedButton_ = new QToolButton(this);
    speedButton_->setObjectName(QStringLiteral("controlSpeedButton"));
    speedButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    speedButton_->setPopupMode(QToolButton::InstantPopup);
    speedButton_->setMinimumWidth(72);
    speedButton_->setFixedHeight(34);
    speedButton_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    repeatButton_ = new QToolButton(this);
    repeatButton_->setObjectName(QStringLiteral("controlRepeatButton"));
    repeatButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    repeatButton_->setPopupMode(QToolButton::InstantPopup);
    repeatButton_->setFixedSize(40, 40);

    loopButton_ = new QToolButton(this);
    loopButton_->setObjectName(QStringLiteral("controlLoopButton"));
    loopButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    loopButton_->setPopupMode(QToolButton::InstantPopup);
    loopButton_->setFixedSize(40, 40);

    qualityButton_ = new QToolButton(this);
    qualityButton_->setObjectName(QStringLiteral("controlQualityButton"));
    qualityButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    qualityButton_->setPopupMode(QToolButton::InstantPopup);
    qualityButton_->setFixedSize(40, 40);

    subtitleButton_ = new QToolButton(this);
    subtitleButton_->setObjectName(QStringLiteral("controlSubtitleButton"));
    subtitleButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    subtitleButton_->setPopupMode(QToolButton::InstantPopup);
    subtitleButton_->setFixedSize(40, 40);

    speedMenu_ = new QMenu(speedButton_);
    speedButton_->setMenu(speedMenu_);

    QAction *slowerAction = speedMenu_->addAction(uiText("Slower  -0.10x"));
    slowerAction->setData(QStringLiteral("adjust:-0.10"));
    QAction *fasterAction = speedMenu_->addAction(uiText("Faster  +0.10x"));
    fasterAction->setData(QStringLiteral("adjust:+0.10"));
    QAction *resetAction = speedMenu_->addAction(uiText("Reset  1.00x"));
    resetAction->setData(QStringLiteral("reset"));
    speedMenu_->addSeparator();

    const QList<double> quickSpeeds {
        0.25, 0.50, 0.75, 0.90,
        1.00, 1.10, 1.25, 1.50,
        1.75, 2.00, 2.50, 3.00, 4.00,
    };
    for (const double speed : quickSpeeds) {
        QAction *speedAction = speedMenu_->addAction(formatSpeed(speed));
        speedAction->setData(QStringLiteral("set:%1").arg(QString::number(speed, 'f', 2)));
    }

    repeatMenu_ = new QMenu(repeatButton_);
    repeatButton_->setMenu(repeatMenu_);
    QAction *repeatOffAction = repeatMenu_->addAction(uiText("Repeat Off"));
    repeatOffAction->setData(QStringLiteral("off"));
    QAction *repeatFileAction = repeatMenu_->addAction(uiText("Repeat File"));
    repeatFileAction->setData(QStringLiteral("file"));
    QAction *repeatPlaylistAction = repeatMenu_->addAction(uiText("Repeat Playlist"));
    repeatPlaylistAction->setData(QStringLiteral("playlist"));

    loopMenu_ = new QMenu(loopButton_);
    loopButton_->setMenu(loopMenu_);
    QAction *loopStartAction = loopMenu_->addAction(uiText("Set Loop Start"));
    loopStartAction->setData(QStringLiteral("start"));
    QAction *loopEndAction = loopMenu_->addAction(uiText("Set Loop End"));
    loopEndAction->setData(QStringLiteral("end"));
    QAction *clearLoopAction = loopMenu_->addAction(uiText("Clear A-B Loop"));
    clearLoopAction->setData(QStringLiteral("clear"));

    qualityMenu_ = new QMenu(qualityButton_);
    qualityButton_->setMenu(qualityMenu_);

    subtitleMenu_ = new QMenu(subtitleButton_);
    subtitleButton_->setMenu(subtitleMenu_);

    volumeSlider_ = new VolumeSlider(Qt::Horizontal, this);
    volumeSlider_->setRange(0, revaplayer::application::kMaximumPlaybackVolume);
    volumeSlider_->setValue(revaplayer::application::kDefaultPlaybackVolume);
    if (auto *volumeSlider = static_cast<VolumeSlider *>(volumeSlider_); volumeSlider != nullptr) {
        volumeSlider->setNormalMaximum(revaplayer::application::kDefaultPlaybackVolume);
    }
    volumeSlider_->setMinimumWidth(72);
    volumeSlider_->setMaximumWidth(88);
    volumeSlider_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    volumeSlider_->setObjectName(QStringLiteral("controlVolumeSlider"));

    volumeValueLabel_ = new QLabel(QStringLiteral("100%"), this);
    volumeValueLabel_->setObjectName(QStringLiteral("controlVolumeValueLabel"));
    volumeValueLabel_->setAlignment(Qt::AlignCenter);
    volumeValueLabel_->setMinimumWidth(48);
    volumeValueLabel_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    transportGroup_ = new QWidget(this);
    transportGroup_->setObjectName(QStringLiteral("transportGroup"));
    auto *transportLayout = new QHBoxLayout(transportGroup_);
    transportLayout->setContentsMargins(8, 5, 8, 5);
    transportLayout->setSpacing(4);
    transportLayout->addWidget(openButton_);
    transportLayout->addWidget(previousButton_);
    transportLayout->addWidget(playPauseButton_);
    transportLayout->addWidget(nextButton_);
    transportLayout->addWidget(stopButton_);

    timelineGroup_ = new QWidget(this);
    timelineGroup_->setObjectName(QStringLiteral("timelineGroup"));
    auto *timelineLayout = new QHBoxLayout(timelineGroup_);
    timelineLayout->setContentsMargins(10, 6, 10, 6);
    timelineLayout->setSpacing(6);
    timelineLayout->addWidget(playlistButton_, 0);
    timelineLayout->addWidget(detailsButton_, 0);
    timelineLayout->addWidget(positionSlider_, 1);
    timelineLayout->addWidget(timeLabel_, 0);
    timelineLayout->addWidget(fullscreenButton_, 0);
    timelineLayout->addWidget(speedButton_, 0);
    timelineLayout->addWidget(repeatButton_, 0);
    timelineLayout->addWidget(loopButton_, 0);

    volumeGroup_ = new QWidget(this);
    volumeGroup_->setObjectName(QStringLiteral("volumeGroup"));
    auto *volumeLayout = new QHBoxLayout(volumeGroup_);
    volumeLayout->setContentsMargins(8, 5, 8, 5);
    volumeLayout->setSpacing(6);
    volumeIconLabel_ = new QLabel(volumeGroup_);
    volumeIconLabel_->setObjectName(QStringLiteral("controlVolumeIcon"));
    volumeIconLabel_->setFixedSize(18, 18);
    volumeIconLabel_->setAlignment(Qt::AlignCenter);
    volumeLayout->addWidget(qualityButton_, 0);
    volumeLayout->addWidget(subtitleButton_, 0);
    volumeLayout->addWidget(volumeIconLabel_, 0);
    volumeLayout->addWidget(volumeSlider_, 1);
    volumeLayout->addWidget(volumeValueLabel_, 0);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(8);
    layout->addWidget(transportGroup_, 0);
    layout->addWidget(timelineGroup_, 1);
    layout->addWidget(volumeGroup_, 0);

    connect(openButton_, &QToolButton::clicked, this, &ControlBar::openRequested);
    connect(previousButton_, &QToolButton::clicked, this, &ControlBar::previousRequested);
    connect(playPauseButton_, &QToolButton::clicked, this, &ControlBar::playPauseRequested);
    connect(nextButton_, &QToolButton::clicked, this, &ControlBar::nextRequested);
    connect(stopButton_, &QToolButton::clicked, this, &ControlBar::stopRequested);
    connect(fullscreenButton_, &QToolButton::clicked, this, &ControlBar::fullscreenRequested);
    connect(playlistButton_, &QToolButton::toggled, this, &ControlBar::playlistPanelToggled);
    connect(detailsButton_, &QToolButton::toggled, this, &ControlBar::detailsPanelToggled);
    connect(positionSlider_, &QSlider::sliderReleased, this, &ControlBar::emitSeekFromSlider);
    connect(volumeSlider_, &QSlider::valueChanged, this, &ControlBar::volumeRequested);
    connect(volumeSlider_, &QSlider::valueChanged, this, [this](const int volume) {
        currentVolume_ = revaplayer::application::clampPlaybackVolume(volume);
        updateVolumePresentation();
    });
    connect(speedMenu_, &QMenu::triggered, this, &ControlBar::handleSpeedMenuAction);
    connect(repeatMenu_, &QMenu::triggered, this, &ControlBar::handleRepeatMenuAction);
    connect(loopMenu_, &QMenu::triggered, this, &ControlBar::handleLoopMenuAction);

    setPlaybackAvailable(false);
    setPlaylistNavigationAvailable(false, false);
    setPanelButtonsEnabled(true);
    setPlaybackSpeed(1.0);
    setRepeatMode(QStringLiteral("off"));
    setLoopPoints(-1.0, -1.0);
    setTrackMenusEnabled(false, false);
    updateVolumePresentation();
    updateVisualMetrics();
    updateButtonLabels();
    updateResponsiveLayout();
}

void ControlBar::setPlaybackAvailable(const bool available)
{
    playbackAvailable_ = available;
    playPauseButton_->setEnabled(available);
    stopButton_->setEnabled(available);
    positionSlider_->setEnabled(available);
    speedButton_->setEnabled(available);
    repeatButton_->setEnabled(available);
    loopButton_->setEnabled(available);
    if (!available) {
        qualityButton_->setEnabled(false);
        subtitleButton_->setEnabled(false);
    }

    if (!available) {
        setPaused(true);
        setPosition(0.0, 0.0);
        setBufferedPosition(0.0);
        setPlaybackSpeed(1.0);
        setLoopPoints(-1.0, -1.0);
        emit previewHidden();
    }
}

void ControlBar::setPlaylistNavigationAvailable(const bool previousAvailable, const bool nextAvailable)
{
    previousButton_->setEnabled(previousAvailable);
    nextButton_->setEnabled(nextAvailable);
}

void ControlBar::setPaused(const bool paused)
{
    paused_ = paused;
    updateButtonLabels();
}

void ControlBar::setPosition(const double positionSeconds, const double durationSeconds)
{
    const double safeDuration = std::max(0.0, durationSeconds);
    if (auto *timelineSlider = static_cast<TimelineSlider *>(positionSlider_); timelineSlider != nullptr) {
        if (!qFuzzyCompare(durationSeconds_ + 1.0, safeDuration + 1.0)) {
            timelineSlider->setDurationSeconds(safeDuration);
        }
    }
    durationSeconds_ = safeDuration;

    if (!positionSlider_->isSliderDown()) {
        const int sliderValue = durationSeconds_ > 0.0
            ? static_cast<int>(std::lround((positionSeconds / durationSeconds_) * 1000.0))
            : 0;
        const int clampedSliderValue = std::clamp(sliderValue, 0, 1000);
        const QSignalBlocker blocker(positionSlider_);
        if (positionSlider_->value() != clampedSliderValue) {
            positionSlider_->setValue(clampedSliderValue);
        }
    }

    const QString timeText = QStringLiteral("%1 / %2")
        .arg(formatTime(positionSeconds), formatTime(durationSeconds_));
    if (timeLabel_->text() != timeText) {
        timeLabel_->setText(timeText);
    }
}

void ControlBar::setBufferedPosition(const double bufferedPositionSeconds)
{
    if (auto *timelineSlider = static_cast<TimelineSlider *>(positionSlider_); timelineSlider != nullptr) {
        timelineSlider->setBufferedSeconds(bufferedPositionSeconds);
    }
}

void ControlBar::setVolume(const int volume)
{
    const int clampedVolume = revaplayer::application::clampPlaybackVolume(volume);
    currentVolume_ = clampedVolume;
    const QSignalBlocker blocker(volumeSlider_);
    if (volumeSlider_->value() != currentVolume_) {
        volumeSlider_->setValue(currentVolume_);
    }
    updateVolumePresentation();
}

void ControlBar::setPlaybackSpeed(const double speed)
{
    playbackSpeed_ = std::clamp(speed, 0.25, 4.0);
    if (speedButton_ != nullptr) {
        speedButton_->setText(formatSpeed(playbackSpeed_));
        speedButton_->setToolTip(uiText("Playback speed (%1)").arg(formatSpeed(playbackSpeed_)));
    }
}

void ControlBar::setRepeatMode(const QString &mode)
{
    QString normalized = mode.trimmed().toLower();
    if (normalized != QStringLiteral("file") && normalized != QStringLiteral("playlist")) {
        normalized = QStringLiteral("off");
    }

    repeatMode_ = normalized;
    if (repeatButton_ == nullptr) {
        return;
    }

    const QString label = normalized == QStringLiteral("file")
        ? uiText("Repeat File")
        : (normalized == QStringLiteral("playlist") ? uiText("Repeat Playlist") : uiText("Repeat Off"));
    repeatButton_->setText(QString {});
    repeatButton_->setProperty("active", normalized != QStringLiteral("off"));
    refreshWidgetStyle(repeatButton_);
    updateButtonIcons();
    repeatButton_->setToolTip(QStringLiteral("%1 - %2").arg(uiText("Repeat mode"), label));
}

void ControlBar::setLoopPoints(const double loopStartSeconds, const double loopEndSeconds)
{
    loopStartSeconds_ = loopStartSeconds;
    loopEndSeconds_ = loopEndSeconds;
    if (loopButton_ == nullptr) {
        return;
    }

    loopButton_->setText(QString {});
    loopButton_->setProperty("active", loopStartSeconds_ >= 0.0 || loopEndSeconds_ >= 0.0);
    refreshWidgetStyle(loopButton_);
    updateButtonIcons();
    const QString stateLabel = loopStartSeconds_ >= 0.0 && loopEndSeconds_ >= 0.0
        ? uiText("Clear A-B Loop")
        : (loopStartSeconds_ >= 0.0 ? uiText("Set Loop End") : uiText("Set Loop Start"));
    loopButton_->setToolTip(QStringLiteral("%1 - %2").arg(uiText("Loop controls"), stateLabel));
}

void ControlBar::setTimelineMarkers(const QVector<revaplayer::ui::TimelineMarker> &markers)
{
    timelineMarkers_ = markers;
    if (auto *timelineSlider = static_cast<TimelineSlider *>(positionSlider_); timelineSlider != nullptr) {
        timelineSlider->setMarkers(timelineMarkers_);
    }
}

void ControlBar::setExpressiveLabelsEnabled(const bool enabled)
{
    if (expressiveLabelsEnabled_ == enabled) {
        return;
    }

    expressiveLabelsEnabled_ = enabled;
    updateButtonLabels();
}

void ControlBar::refreshPresentation()
{
    updateVisualMetrics();
    updateButtonIcons();
}

void ControlBar::setTimelineThickness(const int pixels)
{
    const int safePixels = std::clamp(pixels, 4, 18);
    if (timelineThickness_ == safePixels) {
        return;
    }

    timelineThickness_ = safePixels;
    updateSliderStyleSheets();
}

void ControlBar::setTimelineHandleSize(const int pixels)
{
    const int safePixels = std::clamp(pixels, 10, 30);
    if (timelineHandleSize_ == safePixels) {
        return;
    }

    timelineHandleSize_ = safePixels;
    updateSliderStyleSheets();
}

void ControlBar::setVolumeSliderThickness(const int pixels)
{
    const int safePixels = std::clamp(pixels, 4, 18);
    if (volumeSliderThickness_ == safePixels) {
        return;
    }

    volumeSliderThickness_ = safePixels;
    updateSliderStyleSheets();
}

void ControlBar::setVolumeSliderPreferredWidth(const int pixels)
{
    const int safePixels = pixels <= 0 ? 0 : std::clamp(pixels, 52, 96);
    if (volumeSliderPreferredWidth_ == safePixels) {
        return;
    }

    volumeSliderPreferredWidth_ = safePixels;
    updateResponsiveLayout();
}

void ControlBar::setShowOpenButton(const bool visible)
{
    if (showOpenButton_ == visible) {
        return;
    }

    showOpenButton_ = visible;
    updateResponsiveLayout();
}

void ControlBar::setShowStopButton(const bool visible)
{
    if (showStopButton_ == visible) {
        return;
    }

    showStopButton_ = visible;
    updateResponsiveLayout();
}

void ControlBar::setShowPlaylistButton(const bool visible)
{
    if (showPlaylistButton_ == visible) {
        return;
    }

    showPlaylistButton_ = visible;
    updateResponsiveLayout();
}

void ControlBar::setShowDetailsButton(const bool visible)
{
    if (showDetailsButton_ == visible) {
        return;
    }

    showDetailsButton_ = visible;
    updateResponsiveLayout();
}

void ControlBar::setShowTimeLabel(const bool visible)
{
    if (showTimeLabel_ == visible) {
        return;
    }

    showTimeLabel_ = visible;
    updateResponsiveLayout();
}

void ControlBar::setShowSpeedButton(const bool visible)
{
    if (showSpeedButton_ == visible) {
        return;
    }

    showSpeedButton_ = visible;
    updateResponsiveLayout();
}

void ControlBar::setShowRepeatLoopButtons(const bool visible)
{
    if (showRepeatLoopButtons_ == visible) {
        return;
    }

    showRepeatLoopButtons_ = visible;
    updateResponsiveLayout();
}

void ControlBar::setShowTrackMenus(const bool visible)
{
    if (showTrackMenus_ == visible) {
        return;
    }

    showTrackMenus_ = visible;
    updateResponsiveLayout();
}

void ControlBar::setShowVolumeControls(const bool visible)
{
    if (showVolumeControls_ == visible) {
        return;
    }

    showVolumeControls_ = visible;
    updateResponsiveLayout();
}

void ControlBar::setShowFullscreenButton(const bool visible)
{
    if (showFullscreenButton_ == visible) {
        return;
    }

    showFullscreenButton_ = visible;
    updateResponsiveLayout();
}

void ControlBar::setPanelButtonsChecked(const bool playlistVisible, const bool detailsVisible)
{
    if (playlistButton_ != nullptr) {
        const QSignalBlocker blocker(playlistButton_);
        playlistButton_->setChecked(playlistVisible);
    }
    if (detailsButton_ != nullptr) {
        const QSignalBlocker blocker(detailsButton_);
        detailsButton_->setChecked(detailsVisible);
    }
}

void ControlBar::setPanelButtonsEnabled(const bool enabled)
{
    if (playlistButton_ != nullptr) {
        playlistButton_->setEnabled(enabled);
    }
    if (detailsButton_ != nullptr) {
        detailsButton_->setEnabled(enabled);
    }
}

void ControlBar::setPanelButtonsVisible(const bool visible)
{
    if (panelButtonsVisibleByContext_ != visible) {
        panelButtonsVisibleByContext_ = visible;
        updateResponsiveLayout();
    }
}

void ControlBar::setTrackMenusEnabled(const bool qualityEnabled, const bool subtitleEnabled)
{
    qualityMenuEnabled_ = qualityEnabled;
    subtitleMenuEnabled_ = subtitleEnabled;

    if (qualityButton_ != nullptr) {
        qualityButton_->setEnabled(playbackAvailable_ && qualityMenuEnabled_);
    }
    if (subtitleButton_ != nullptr) {
        subtitleButton_->setEnabled(playbackAvailable_ && subtitleMenuEnabled_);
    }
    updateResponsiveLayout();
}

QMenu *ControlBar::qualityMenu() const
{
    return qualityMenu_;
}

QMenu *ControlBar::subtitleMenu() const
{
    return subtitleMenu_;
}

void ControlBar::emitSeekFromSlider()
{
    emit seekRequested(static_cast<double>(positionSlider_->value()) / 1000.0);
}

void ControlBar::handleSpeedMenuAction(QAction *action)
{
    if (action == nullptr) {
        return;
    }

    const QString command = action->data().toString().trimmed();
    if (command == QStringLiteral("reset")) {
        emit playbackSpeedResetRequested();
        return;
    }

    if (command.startsWith(QStringLiteral("adjust:"))) {
        bool ok = false;
        const double delta = command.mid(QStringLiteral("adjust:").size()).toDouble(&ok);
        if (ok && delta != 0.0) {
            emit playbackSpeedAdjusted(delta);
        }
        return;
    }

    if (command.startsWith(QStringLiteral("set:"))) {
        bool ok = false;
        const double speed = command.mid(QStringLiteral("set:").size()).toDouble(&ok);
        if (ok) {
            emit playbackSpeedRequested(speed);
        }
    }
}

void ControlBar::handleRepeatMenuAction(QAction *action)
{
    if (action == nullptr) {
        return;
    }

    const QString mode = action->data().toString().trimmed();
    if (!mode.isEmpty()) {
        emit repeatModeRequested(mode);
    }
}

void ControlBar::handleLoopMenuAction(QAction *action)
{
    if (action == nullptr) {
        return;
    }

    const QString command = action->data().toString().trimmed();
    if (command == QStringLiteral("start")) {
        emit loopStartRequested();
    } else if (command == QStringLiteral("end")) {
        emit loopEndRequested();
    } else if (command == QStringLiteral("clear")) {
        emit loopClearRequested();
    }
}

void ControlBar::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event == nullptr) {
        return;
    }

    switch (event->type()) {
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::EnabledChange:
        updateSliderStyleSheets();
        updateVolumePresentation();
        updateButtonIcons();
        break;
    default:
        break;
    }
}

QSize ControlBar::minimumSizeHint() const
{
    return QSize(150, 58);
}

bool ControlBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == positionSlider_) {
        switch (event->type()) {
        case QEvent::Leave:
        case QEvent::Hide:
            emit previewHidden();
            break;
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress: {
            if (!playbackAvailable_ || durationSeconds_ <= 0.0) {
                emit previewHidden();
                break;
            }

            const auto *mouseEvent = static_cast<QMouseEvent *>(event);
            const int x = std::clamp(mouseEventLocalPoint(mouseEvent).x(), 0, positionSlider_->width());
            const int sliderValue = QStyle::sliderValueFromPosition(
                positionSlider_->minimum(),
                positionSlider_->maximum(),
                x,
                std::max(1, positionSlider_->width()));
            const double fraction = static_cast<double>(sliderValue - positionSlider_->minimum())
                / std::max(1, positionSlider_->maximum() - positionSlider_->minimum());
            const QPoint anchor = positionSlider_->mapToGlobal(QPoint(x, 0));

            if (event->type() == QEvent::MouseButtonPress && mouseEvent->button() == Qt::LeftButton) {
                const QSignalBlocker blocker(positionSlider_);
                positionSlider_->setSliderPosition(sliderValue);
                positionSlider_->setValue(sliderValue);
                emit seekRequested(fraction);
            }

            emit previewRequested(fraction * durationSeconds_, anchor);
            break;
        }
        default:
            break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void ControlBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateResponsiveLayout();
}

QSize ControlBar::sizeHint() const
{
    return QSize(1060, 64);
}

QString ControlBar::formatTime(const double timeSeconds)
{
    const int totalSeconds = std::max(0, static_cast<int>(std::lround(timeSeconds)));
    const int hours = totalSeconds / 3600;
    const int minutes = (totalSeconds % 3600) / 60;
    const int seconds = totalSeconds % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }

    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString ControlBar::formatSpeed(const double speed)
{
    return QStringLiteral("%1x").arg(QString::number(std::clamp(speed, 0.25, 4.0), 'f', 2));
}

void ControlBar::updateSliderStyleSheets()
{
    if (positionSlider_ != nullptr) {
        const int safeTimelineThickness = std::clamp(timelineThickness_, 4, 18);
        const int safeTimelineHandle = std::clamp(timelineHandleSize_, safeTimelineThickness + 4, 30);
        positionSlider_->setFixedHeight(std::max(safeTimelineHandle + 8, safeTimelineThickness + 10));
        positionSlider_->setStyleSheet(QString {});
        if (auto *timelineSlider = static_cast<TimelineSlider *>(positionSlider_); timelineSlider != nullptr) {
            timelineSlider->setVisualMetrics(safeTimelineThickness, safeTimelineHandle);
        }
    }

    if (volumeSlider_ != nullptr) {
        const int volumeGroove = std::clamp(volumeSliderThickness_, 4, 18);
        const int volumeHandle = std::clamp(
            std::max(volumeGroove + 4, timelineHandleSize_ - 2),
            volumeGroove + 4,
            30);
        volumeSlider_->setFixedHeight(std::max(volumeHandle + 8, volumeGroove + 10));
        volumeSlider_->setStyleSheet(volumeSliderStyleSheet(volumeSlider_->palette(), volumeGroove, volumeHandle));
    }
}

void ControlBar::updateVolumePresentation()
{
    const int clampedVolume = revaplayer::application::clampPlaybackVolume(currentVolume_);
    const bool boosted = clampedVolume > revaplayer::application::kDefaultPlaybackVolume;
    const QString volumeText = QStringLiteral("%1%").arg(clampedVolume);
    const QString tooltip = uiText("Volume %1%").arg(clampedVolume);

    if (volumeValueLabel_ != nullptr) {
        if (volumeValueLabel_->text() != volumeText) {
            volumeValueLabel_->setText(volumeText);
        }
        const bool boostedChanged = volumeValueLabel_->property("boosted").toBool() != boosted;
        if (boostedChanged) {
            volumeValueLabel_->setProperty("boosted", boosted);
        }
        if (volumeValueLabel_->toolTip() != tooltip) {
            volumeValueLabel_->setToolTip(tooltip);
        }
        if (boostedChanged) {
            refreshWidgetStyle(volumeValueLabel_);
        }
    }
    if (volumeSlider_ != nullptr) {
        if (volumeSlider_->toolTip() != tooltip) {
            volumeSlider_->setToolTip(tooltip);
        }
    }
    if (volumeIconLabel_ != nullptr) {
        const bool boostedChanged = volumeIconLabel_->property("boosted").toBool() != boosted;
        if (boostedChanged) {
            volumeIconLabel_->setProperty("boosted", boosted);
        }
        if (volumeIconLabel_->toolTip() != tooltip) {
            volumeIconLabel_->setToolTip(tooltip);
        }
        if (boostedChanged) {
            refreshWidgetStyle(volumeIconLabel_);
        }
    }
}

void ControlBar::updateVisualMetrics()
{
    const ControlBarStyleMetrics metrics = controlBarMetrics();
    const bool openIconOnly = expressiveLabelsEnabled_ || property("overlayMode").toBool();

    if (openButton_ != nullptr) {
        if (openIconOnly) {
            openButton_->setFixedSize(metrics.regularButtonSize, metrics.regularButtonSize);
        } else {
            openButton_->setMinimumWidth(40);
            openButton_->setMaximumWidth(QWIDGETSIZE_MAX);
            openButton_->setFixedHeight(metrics.regularButtonSize);
        }
    }
    if (previousButton_ != nullptr) {
        previousButton_->setFixedSize(metrics.regularButtonSize, metrics.regularButtonSize);
    }
    if (playPauseButton_ != nullptr) {
        playPauseButton_->setFixedSize(metrics.primaryButtonSize, metrics.primaryButtonSize);
    }
    if (nextButton_ != nullptr) {
        nextButton_->setFixedSize(metrics.regularButtonSize, metrics.regularButtonSize);
    }
    if (stopButton_ != nullptr) {
        stopButton_->setFixedSize(metrics.regularButtonSize, metrics.regularButtonSize);
    }
    if (fullscreenButton_ != nullptr) {
        fullscreenButton_->setFixedSize(metrics.regularButtonSize, metrics.regularButtonSize);
    }
    if (playlistButton_ != nullptr) {
        playlistButton_->setFixedSize(metrics.panelButtonWidth, metrics.panelButtonHeight);
    }
    if (detailsButton_ != nullptr) {
        detailsButton_->setFixedSize(metrics.panelButtonWidth, metrics.panelButtonHeight);
    }
    if (speedButton_ != nullptr) {
        speedButton_->setFixedHeight(std::max(30, metrics.regularButtonSize - 2));
        speedButton_->setMinimumWidth(metrics.speedButtonWidth);
    }
    for (QToolButton *button : {repeatButton_, loopButton_, qualityButton_, subtitleButton_}) {
        if (button != nullptr) {
            button->setFixedSize(metrics.regularButtonSize, metrics.regularButtonSize);
        }
    }
    if (timeLabel_ != nullptr) {
        timeLabel_->setMinimumWidth(metrics.timeLabelWidth);
        timeLabel_->setMinimumHeight(std::max(30, metrics.regularButtonSize - 2));
    }
    if (volumeIconLabel_ != nullptr) {
        const int iconSide = std::max(16, metrics.regularButtonSize / 2);
        volumeIconLabel_->setFixedSize(iconSide, iconSide);
    }
    if (volumeValueLabel_ != nullptr) {
        volumeValueLabel_->setMinimumHeight(std::max(28, metrics.regularButtonSize - 4));
        volumeValueLabel_->setMinimumWidth(52);
    }

    updateSliderStyleSheets();
    updateVolumePresentation();
    updateButtonLabels();
    updateResponsiveLayout();
}

void ControlBar::updateButtonLabels()
{
    const bool openIconOnly = expressiveLabelsEnabled_ || property("overlayMode").toBool();
    const QString openText = openIconOnly ? QString {} : uiText("Open");

    openButton_->setText(openText);
    playlistButton_->setText(QString {});
    detailsButton_->setText(QString {});
    if (openIconOnly) {
        openButton_->setFixedWidth(controlBarMetrics().regularButtonSize);
    } else {
        openButton_->setMinimumWidth(labeledButtonMinimumWidth(openButton_, openText, 40));
        openButton_->setMaximumWidth(QWIDGETSIZE_MAX);
    }
    playlistButton_->setMinimumWidth(controlBarMetrics().panelButtonWidth);
    detailsButton_->setMinimumWidth(controlBarMetrics().panelButtonWidth);

    openButton_->setToolButtonStyle(openIconOnly ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
    playlistButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    detailsButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    previousButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    playPauseButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    nextButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    stopButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    fullscreenButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    repeatButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    loopButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    qualityButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    subtitleButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);

    openButton_->setToolTip(uiText("Open media"));
    previousButton_->setToolTip(uiText("Previous playlist item"));
    playPauseButton_->setToolTip(paused_ ? uiText("Resume playback") : uiText("Pause playback"));
    nextButton_->setToolTip(uiText("Next playlist item"));
    stopButton_->setToolTip(uiText("Stop playback"));
    fullscreenButton_->setToolTip(uiText("Toggle Fullscreen"));
    playlistButton_->setToolTip(uiText("Show or hide playlist panel"));
    detailsButton_->setToolTip(uiText("Show or hide details panel"));
    if (qualityButton_ != nullptr) {
        qualityButton_->setToolTip(uiText("Video quality"));
    }
    if (subtitleButton_ != nullptr) {
        subtitleButton_->setToolTip(uiText("Subtitle tracks"));
    }
    if (volumeValueLabel_ != nullptr) {
        volumeValueLabel_->setToolTip(uiText("Volume %1%").arg(revaplayer::application::clampPlaybackVolume(currentVolume_)));
    }
    if (repeatButton_ != nullptr) {
        setRepeatMode(repeatMode_);
    }
    if (loopButton_ != nullptr) {
        setLoopPoints(loopStartSeconds_, loopEndSeconds_);
    }

    updateButtonIcons();
    updateResponsiveLayout();
}

void ControlBar::updateResponsiveLayout()
{
    const ControlBarStyleMetrics metrics = controlBarMetrics();
    const int availableWidth = width();
    const bool compact = availableWidth < 980;
    const bool narrow = availableWidth < 840;
    const bool tight = availableWidth < 700;
    const bool condensed = availableWidth < 560;
    const bool minimal = availableWidth < 460;
    const bool playlistButtonVisible = showPlaylistButton_ && panelButtonsVisibleByContext_;
    const bool detailsButtonVisible = showDetailsButton_ && panelButtonsVisibleByContext_;
    const bool qualityButtonVisible = showTrackMenus_ && !condensed;
    const bool subtitleButtonVisible = showTrackMenus_ && !condensed;
    const bool volumeIconVisible = showVolumeControls_ && !tight;
    const bool volumeSliderVisible = showVolumeControls_ && !tight;
    const bool volumeValueVisible = showVolumeControls_ && !narrow;

    if (openButton_ != nullptr) {
        openButton_->setVisible(showOpenButton_ && !tight);
    }
    if (stopButton_ != nullptr) {
        stopButton_->setVisible(showStopButton_ && !narrow);
    }
    if (fullscreenButton_ != nullptr) {
        fullscreenButton_->setVisible(showFullscreenButton_ && !minimal);
    }
    if (repeatButton_ != nullptr) {
        repeatButton_->setVisible(showRepeatLoopButtons_ && !tight);
    }
    if (loopButton_ != nullptr) {
        loopButton_->setVisible(showRepeatLoopButtons_ && !tight);
    }
    if (playlistButton_ != nullptr) {
        playlistButton_->setVisible(playlistButtonVisible);
    }
    if (detailsButton_ != nullptr) {
        detailsButton_->setVisible(detailsButtonVisible);
    }
    if (timeLabel_ != nullptr) {
        timeLabel_->setVisible(showTimeLabel_ && !minimal);
        const int baseTimeWidth = metrics.timeLabelWidth;
        timeLabel_->setMinimumWidth(minimal ? std::max(66, baseTimeWidth - 34)
                                            : (condensed ? std::max(74, baseTimeWidth - 24)
                                                         : (tight ? std::max(82, baseTimeWidth - 14)
                                                                  : (narrow ? std::max(90, baseTimeWidth - 8) : baseTimeWidth))));
    }
    if (speedButton_ != nullptr) {
        speedButton_->setVisible(showSpeedButton_ && !condensed);
        const int baseSpeedWidth = metrics.speedButtonWidth;
        speedButton_->setMinimumWidth(condensed ? std::max(54, baseSpeedWidth - 12)
                                                : (tight ? std::max(60, baseSpeedWidth - 8)
                                                         : (narrow ? std::max(64, baseSpeedWidth - 4) : baseSpeedWidth)));
    }
    if (qualityButton_ != nullptr) {
        qualityButton_->setVisible(qualityButtonVisible);
    }
    if (subtitleButton_ != nullptr) {
        subtitleButton_->setVisible(subtitleButtonVisible);
    }
    if (volumeIconLabel_ != nullptr) {
        volumeIconLabel_->setVisible(volumeIconVisible);
    }
    if (volumeSlider_ != nullptr) {
        volumeSlider_->setVisible(volumeSliderVisible);
        const int baseVolumeWidth = volumeSliderPreferredWidth_ > 0 ? volumeSliderPreferredWidth_ : metrics.defaultVolumeSliderWidth;
        int minimumVolumeWidth = 0;
        int maximumVolumeWidth = 0;
        if (volumeSliderVisible) {
            if (narrow) {
                minimumVolumeWidth = std::clamp(baseVolumeWidth - 10, 56, baseVolumeWidth);
                maximumVolumeWidth = std::clamp(baseVolumeWidth - 4, minimumVolumeWidth, baseVolumeWidth);
            } else if (compact) {
                minimumVolumeWidth = std::clamp(baseVolumeWidth - 4, 64, baseVolumeWidth);
                maximumVolumeWidth = baseVolumeWidth;
            } else {
                minimumVolumeWidth = baseVolumeWidth;
                maximumVolumeWidth = baseVolumeWidth;
            }
        }
        volumeSlider_->setMinimumWidth(minimumVolumeWidth);
        volumeSlider_->setMaximumWidth(maximumVolumeWidth);
    }
    if (volumeValueLabel_ != nullptr) {
        volumeValueLabel_->setVisible(volumeValueVisible);
    }
    if (volumeGroup_ != nullptr) {
        const bool anyVolumeControlVisible = qualityButtonVisible
            || subtitleButtonVisible
            || volumeIconVisible
            || volumeSliderVisible
            || volumeValueVisible;
        volumeGroup_->setVisible(anyVolumeControlVisible);
    }

    if (QLayout *rootLayout = layout(); rootLayout != nullptr) {
        rootLayout->invalidate();
    }
    updateGeometry();
}

void ControlBar::updateButtonIcons()
{
    const QColor baseColor = openButton_ != nullptr
        ? openButton_->palette().color(QPalette::ButtonText)
        : QColor(QStringLiteral("#edf0f4"));
    const QColor accentColor = openButton_ != nullptr
        ? openButton_->palette().color(QPalette::Highlight)
        : QColor(QStringLiteral("#7ba5ff"));
    const QSize regularIconSize(18, 18);
    const QSize primaryIconSize(20, 20);
    const QSize detailsIconSize(20, 20);

    if (openButton_ != nullptr) {
        openButton_->setIcon(drawGlyphIcon(ControlGlyph::Open, baseColor, regularIconSize));
        openButton_->setIconSize(regularIconSize);
    }
    if (previousButton_ != nullptr) {
        previousButton_->setIcon(drawGlyphIcon(ControlGlyph::Previous, baseColor, regularIconSize));
        previousButton_->setIconSize(regularIconSize);
    }
    if (playPauseButton_ != nullptr) {
        playPauseButton_->setIcon(drawGlyphIcon(paused_ ? ControlGlyph::Play : ControlGlyph::Pause, baseColor, primaryIconSize));
        playPauseButton_->setIconSize(primaryIconSize);
    }
    if (nextButton_ != nullptr) {
        nextButton_->setIcon(drawGlyphIcon(ControlGlyph::Next, baseColor, regularIconSize));
        nextButton_->setIconSize(regularIconSize);
    }
    if (stopButton_ != nullptr) {
        stopButton_->setIcon(drawGlyphIcon(ControlGlyph::Stop, baseColor, regularIconSize));
        stopButton_->setIconSize(regularIconSize);
    }
    if (fullscreenButton_ != nullptr) {
        fullscreenButton_->setIcon(drawGlyphIcon(ControlGlyph::Fullscreen, baseColor, regularIconSize));
        fullscreenButton_->setIconSize(regularIconSize);
    }
    if (playlistButton_ != nullptr) {
        playlistButton_->setIcon(drawGlyphIcon(ControlGlyph::Playlist, baseColor, regularIconSize));
        playlistButton_->setIconSize(regularIconSize);
    }
    if (detailsButton_ != nullptr) {
        detailsButton_->setIcon(drawGlyphIcon(ControlGlyph::Details, baseColor, detailsIconSize));
        detailsButton_->setIconSize(detailsIconSize);
    }
    if (volumeIconLabel_ != nullptr) {
        volumeIconLabel_->setPixmap(drawGlyphIcon(ControlGlyph::Volume, baseColor, QSize(18, 18)).pixmap(18, 18));
    }
    if (repeatButton_ != nullptr) {
        const bool repeatActive = repeatMode_ != QStringLiteral("off");
        repeatButton_->setIcon(drawGlyphIcon(repeatGlyph(repeatMode_), repeatActive ? accentColor : baseColor, regularIconSize));
        repeatButton_->setIconSize(regularIconSize);
    }
    if (loopButton_ != nullptr) {
        const bool loopActive = loopStartSeconds_ >= 0.0 || loopEndSeconds_ >= 0.0;
        loopButton_->setIcon(drawGlyphIcon(loopGlyph(loopStartSeconds_, loopEndSeconds_), loopActive ? accentColor : baseColor, regularIconSize));
        loopButton_->setIconSize(regularIconSize);
    }
    if (qualityButton_ != nullptr) {
        qualityButton_->setIcon(drawGlyphIcon(ControlGlyph::Quality, baseColor, regularIconSize));
        qualityButton_->setIconSize(regularIconSize);
    }
    if (subtitleButton_ != nullptr) {
        subtitleButton_->setIcon(drawGlyphIcon(ControlGlyph::Subtitle, baseColor, regularIconSize));
        subtitleButton_->setIconSize(regularIconSize);
    }
}

}  // namespace revaplayer::ui
