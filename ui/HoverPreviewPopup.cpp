#include "ui/HoverPreviewPopup.hpp"

#include "application/UiLanguage.hpp"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QVBoxLayout>
#include <QPixmap>

#include <algorithm>
#include <cmath>

namespace revaplayer::ui {
namespace {

QString uiText(const char *sourceText)
{
    return revaplayer::application::translateUiText(QString::fromUtf8(sourceText));
}

}  // namespace

HoverPreviewPopup::HoverPreviewPopup(QWidget *parent)
    : QFrame(parent, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setObjectName(QStringLiteral("hoverPreviewPopup"));
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFrameShape(QFrame::StyledPanel);
    setLineWidth(1);

    imageLabel_ = new QLabel(this);
    imageLabel_->setFixedSize(280, 158);
    imageLabel_->setAlignment(Qt::AlignCenter);

    timeLabel_ = new QLabel(this);
    timeLabel_->setAlignment(Qt::AlignCenter);

    statusLabel_ = new QLabel(this);
    statusLabel_->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);
    layout->addWidget(imageLabel_, 0, Qt::AlignCenter);
    layout->addWidget(timeLabel_, 0, Qt::AlignCenter);
    layout->addWidget(statusLabel_, 0, Qt::AlignCenter);

    setPreviewWidth(previewWidth_);
}

void HoverPreviewPopup::setPreviewWidth(const int width)
{
    previewWidth_ = std::clamp(width, 224, 480);
    imageLabel_->setFixedSize(previewWidth_, std::max(90, static_cast<int>(std::lround(previewWidth_ * 9.0 / 16.0))));
    adjustSize();
}

void HoverPreviewPopup::setVerticalOffset(const int pixels)
{
    verticalOffset_ = std::clamp(pixels, 8, 56);
}

void HoverPreviewPopup::setScreenPadding(const int pixels)
{
    screenPadding_ = std::clamp(pixels, 0, 32);
}

void HoverPreviewPopup::showPreview(const QImage &image, const QString &timeText, const QPoint &globalAnchor)
{
    imageLabel_->setPixmap(QPixmap::fromImage(image).scaled(
        imageLabel_->size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation));
    timeLabel_->setText(timeText);
    statusLabel_->clear();
    adjustSize();
    moveNear(globalAnchor);
    show();
}

void HoverPreviewPopup::showStatus(const QString &statusText, const QString &timeText, const QPoint &globalAnchor)
{
    imageLabel_->setPixmap(QPixmap {});
    imageLabel_->setText(uiText("Preview"));
    timeLabel_->setText(timeText);
    statusLabel_->setText(statusText);
    adjustSize();
    moveNear(globalAnchor);
    show();
}

void HoverPreviewPopup::moveNear(const QPoint &globalAnchor)
{
    QPoint target = globalAnchor - QPoint(width() / 2, height() + verticalOffset_);

    if (const QScreen *screen = QGuiApplication::screenAt(globalAnchor)) {
        const QRect available = screen->availableGeometry();
        target.setX(std::clamp(target.x(), available.left() + screenPadding_, available.right() - width() - screenPadding_));
        target.setY(std::clamp(target.y(), available.top() + screenPadding_, available.bottom() - height() - screenPadding_));
    }

    move(target);
}

}  // namespace revaplayer::ui
