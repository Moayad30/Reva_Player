#include "ui/BookmarkDialog.hpp"

#include "application/UiLanguage.hpp"

#include <QDialogButtonBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace revaplayer::ui {
namespace {

QString uiText(const char *sourceText)
{
    return revaplayer::application::translateUiText(QString::fromUtf8(sourceText));
}

}  // namespace

BookmarkDialog::BookmarkDialog(const QString &timeLabel,
                               const QString &suggestedTitle,
                               const QStringList &suggestedCategories,
                               QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(uiText("Add Bookmark"));
    setModal(true);
    resize(420, 240);

    auto *timeLabelWidget = new QLabel(uiText("Create a bookmark at %1").arg(timeLabel), this);
    timeLabelWidget->setWordWrap(true);

    titleEdit_ = new QLineEdit(this);
    titleEdit_->setText(suggestedTitle.trimmed());
    titleEdit_->selectAll();

    categoryComboBox_ = new QComboBox(this);
    categoryComboBox_->setEditable(true);
    categoryComboBox_->setInsertPolicy(QComboBox::NoInsert);
    categoryComboBox_->addItem(QStringLiteral(""));
    for (const QString &category : suggestedCategories) {
        const QString trimmed = category.trimmed();
        if (trimmed.isEmpty() || categoryComboBox_->findText(trimmed, Qt::MatchFixedString) >= 0) {
            continue;
        }
        categoryComboBox_->addItem(trimmed);
    }
    categoryComboBox_->setCurrentIndex(0);
    categoryComboBox_->lineEdit()->setPlaceholderText(uiText("Optional category"));

    noteEdit_ = new QTextEdit(this);
    noteEdit_->setPlaceholderText(uiText("Optional note"));
    noteEdit_->setAcceptRichText(false);

    auto *formLayout = new QFormLayout();
    formLayout->addRow(uiText("Title"), titleEdit_);
    formLayout->addRow(uiText("Category"), categoryComboBox_);
    formLayout->addRow(uiText("Note"), noteEdit_);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (auto *okButton = buttonBox->button(QDialogButtonBox::Ok); okButton != nullptr) {
        okButton->setText(uiText("OK"));
    }
    if (auto *cancelButton = buttonBox->button(QDialogButtonBox::Cancel); cancelButton != nullptr) {
        cancelButton->setText(uiText("Cancel"));
    }
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);
    layout->addWidget(timeLabelWidget);
    layout->addLayout(formLayout);
    layout->addWidget(buttonBox);
}

QString BookmarkDialog::bookmarkTitle() const
{
    return titleEdit_ != nullptr ? titleEdit_->text().trimmed() : QString {};
}

QString BookmarkDialog::bookmarkCategory() const
{
    return categoryComboBox_ != nullptr ? categoryComboBox_->currentText().trimmed() : QString {};
}

QString BookmarkDialog::bookmarkNote() const
{
    return noteEdit_ != nullptr ? noteEdit_->toPlainText().trimmed() : QString {};
}

}  // namespace revaplayer::ui
