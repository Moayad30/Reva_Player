#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QFormLayout;
class QLabel;

namespace revaplayer::application {
class SettingsController;
}

namespace revaplayer::ui {

class FirstRunDialog final : public QDialog {
    Q_OBJECT

public:
    explicit FirstRunDialog(revaplayer::application::SettingsController *settingsController,
                            QWidget *parent = nullptr);

    void applySelections();
    [[nodiscard]] QString selectedLanguageId() const;

private:
    void applyRightAlignedPresentation();
    void rebuildLanguageOptions();
    void rebuildThemeOptions();
    void refreshTexts();
    void previewLanguage(const QString &languageId);

    revaplayer::application::SettingsController *settingsController_ {nullptr};
    QLabel *introLabel_ {nullptr};
    QLabel *languageLabel_ {nullptr};
    QLabel *themeLabel_ {nullptr};
    QComboBox *languageComboBox_ {nullptr};
    QComboBox *themeComboBox_ {nullptr};
    QCheckBox *dashboardCheckBox_ {nullptr};
    QCheckBox *progressCheckBox_ {nullptr};
    QDialogButtonBox *buttonBox_ {nullptr};
    QFormLayout *formLayout_ {nullptr};
};

}  // namespace revaplayer::ui
