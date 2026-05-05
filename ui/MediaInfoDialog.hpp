#pragma once

#include <QDialog>

class QPlainTextEdit;

namespace revaplayer::ui {

class MediaInfoDialog final : public QDialog {
    Q_OBJECT

public:
    explicit MediaInfoDialog(QWidget *parent = nullptr);

    void setReport(const QString &report);

private:
    QPlainTextEdit *reportView_ {nullptr};
};

}  // namespace revaplayer::ui
