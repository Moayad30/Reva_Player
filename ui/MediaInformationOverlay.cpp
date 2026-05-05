#include "ui/MediaInformationOverlay.hpp"

#include <QEvent>
#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

#include <algorithm>

namespace revaplayer::ui {

MediaInformationOverlay::MediaInformationOverlay(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("mediaInformationOverlay"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(0);

    label_ = new QLabel(this);
    label_->setObjectName(QStringLiteral("mediaInformationOverlayLabel"));
    label_->setWordWrap(true);
    label_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    QFont labelFont = label_->font();
    labelFont.setFamily(QStringLiteral("monospace"));
    labelFont.setStyleHint(QFont::Monospace);
    label_->setFont(labelFont);
    layout->addWidget(label_);

    if (parent != nullptr) {
        parent->installEventFilter(this);
    }

    hide();
}

void MediaInformationOverlay::setInformationText(const QString &text)
{
    label_->setText(text);
    if (QWidget *parent = parentWidget()) {
        const int overlayWidth = std::clamp(parent->width() - 40, 340, 560);
        label_->setFixedWidth(overlayWidth - 28);
    }
    layout()->activate();
    adjustSize();
    updatePlacement();
}

bool MediaInformationOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event != nullptr && event->type() == QEvent::Resize) {
        updatePlacement();
    }

    return QWidget::eventFilter(watched, event);
}

void MediaInformationOverlay::updatePlacement()
{
    QWidget *parent = parentWidget();
    if (parent == nullptr) {
        return;
    }

    const int overlayWidth = std::clamp(parent->width() - 40, 340, 560);
    label_->setFixedWidth(overlayWidth - 28);
    layout()->activate();
    adjustSize();
    move(std::max(16, parent->width() / 48), std::max(14, parent->height() / 42));
    raise();
}

}  // namespace revaplayer::ui
