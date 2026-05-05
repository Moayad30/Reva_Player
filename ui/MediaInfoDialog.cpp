#include "ui/MediaInfoDialog.hpp"

#include "application/UiLanguage.hpp"

#include <QDialogButtonBox>
#include <QFont>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace revaplayer::ui {
namespace {

QString uiText(const char *sourceText)
{
    return revaplayer::application::translateUiText(QString::fromUtf8(sourceText));
}

}  // namespace

MediaInfoDialog::MediaInfoDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(uiText("Media Info"));
    resize(640, 520);
    setModal(false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    reportView_ = new QPlainTextEdit(this);
    reportView_->setReadOnly(true);
    QFont font = reportView_->font();
    font.setStyleHint(QFont::Monospace);
    reportView_->setFont(font);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);

    layout->addWidget(reportView_, 1);
    layout->addWidget(buttons, 0);
}

void MediaInfoDialog::setReport(const QString &report)
{
    if (reportView_->toPlainText() == report) {
        return;
    }

    reportView_->setPlainText(report);
}

}  // namespace revaplayer::ui
