#pragma once

#include <QDialog>

class QLineEdit;
class QTextEdit;
class QComboBox;

namespace revaplayer::ui {

class BookmarkDialog final : public QDialog {
    Q_OBJECT

public:
    explicit BookmarkDialog(const QString &timeLabel,
                            const QString &suggestedTitle,
                            const QStringList &suggestedCategories = {},
                            QWidget *parent = nullptr);

    [[nodiscard]] QString bookmarkTitle() const;
    [[nodiscard]] QString bookmarkCategory() const;
    [[nodiscard]] QString bookmarkNote() const;

private:
    QLineEdit *titleEdit_ {nullptr};
    QComboBox *categoryComboBox_ {nullptr};
    QTextEdit *noteEdit_ {nullptr};
};

}  // namespace revaplayer::ui
