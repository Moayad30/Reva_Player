#pragma once

#include <QWidget>

class QLabel;

namespace revaplayer::ui {

class MediaInformationOverlay final : public QWidget {
    Q_OBJECT

public:
    explicit MediaInformationOverlay(QWidget *parent = nullptr);

    void setInformationText(const QString &text);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updatePlacement();

    QLabel *label_ {nullptr};
};

}  // namespace revaplayer::ui
