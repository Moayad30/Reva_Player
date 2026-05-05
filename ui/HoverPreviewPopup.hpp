#pragma once

#include <QFrame>
#include <QImage>

class QLabel;

namespace revaplayer::ui {

class HoverPreviewPopup final : public QFrame {
    Q_OBJECT

public:
    explicit HoverPreviewPopup(QWidget *parent = nullptr);

    void setPreviewWidth(int width);
    void setVerticalOffset(int pixels);
    void setScreenPadding(int pixels);
    void showPreview(const QImage &image, const QString &timeText, const QPoint &globalAnchor);
    void showStatus(const QString &statusText, const QString &timeText, const QPoint &globalAnchor);

private:
    void moveNear(const QPoint &globalAnchor);

    QLabel *imageLabel_ {nullptr};
    QLabel *timeLabel_ {nullptr};
    QLabel *statusLabel_ {nullptr};
    int previewWidth_ {280};
    int verticalOffset_ {18};
    int screenPadding_ {8};
};

}  // namespace revaplayer::ui
