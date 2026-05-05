#include "ui/FirstRunDialog.hpp"

#include "application/SettingsController.hpp"
#include "application/ThemeStyle.hpp"
#include "application/UiLanguage.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace revaplayer::ui {
namespace {

QString uiText(const char *sourceText)
{
    return revaplayer::application::translateUiText(QString::fromUtf8(sourceText));
}

}  // namespace

FirstRunDialog::FirstRunDialog(revaplayer::application::SettingsController *settingsController, QWidget *parent)
    : QDialog(parent)
    , settingsController_(settingsController)
{
    setModal(true);
    setLayoutDirection(Qt::LeftToRight);

    auto *layout = new QVBoxLayout(this);
    introLabel_ = new QLabel(this);
    introLabel_->setWordWrap(true);
    layout->addWidget(introLabel_);

    formLayout_ = new QFormLayout();
    formLayout_->setObjectName(QStringLiteral("firstRunFormLayout"));
    languageLabel_ = new QLabel(this);
    themeLabel_ = new QLabel(this);

    languageComboBox_ = new QComboBox(this);
    languageComboBox_->setObjectName(QStringLiteral("firstRunLanguageComboBox"));
    themeComboBox_ = new QComboBox(this);
    themeComboBox_->setObjectName(QStringLiteral("firstRunThemeComboBox"));

    formLayout_->addRow(languageLabel_, languageComboBox_);
    formLayout_->addRow(themeLabel_, themeComboBox_);
    layout->addLayout(formLayout_);

    dashboardCheckBox_ = new QCheckBox(this);
    dashboardCheckBox_->setObjectName(QStringLiteral("firstRunDashboardCheckBox"));
    progressCheckBox_ = new QCheckBox(this);
    progressCheckBox_->setObjectName(QStringLiteral("firstRunProgressCheckBox"));
    layout->addWidget(dashboardCheckBox_);
    layout->addWidget(progressCheckBox_);

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox_);

    applyRightAlignedPresentation();

    const QString initialLanguage = settingsController_ != nullptr
        ? settingsController_->interfaceLanguage()
        : revaplayer::application::defaultUiLanguageId();
    previewLanguage(initialLanguage);
    rebuildLanguageOptions();
    rebuildThemeOptions();

    const int languageIndex = languageComboBox_->findData(initialLanguage);
    languageComboBox_->setCurrentIndex(languageIndex >= 0 ? languageIndex : 0);

    const QString themeId = settingsController_ != nullptr
        ? settingsController_->uiTheme()
        : QStringLiteral("gray");
    const int themeIndex = themeComboBox_->findData(themeId);
    themeComboBox_->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);

    dashboardCheckBox_->setChecked(
        settingsController_ == nullptr
        || settingsController_->customValue(QStringLiteral("ui/dashboard_show_on_idle"), QStringLiteral("1")) != QStringLiteral("0"));
    progressCheckBox_->setChecked(
        settingsController_ == nullptr
        || settingsController_->customValue(QStringLiteral("playlist/progress_mode_enabled"), QStringLiteral("1")) != QStringLiteral("0"));

    connect(languageComboBox_, &QComboBox::currentIndexChanged, this, [this]() {
        previewLanguage(selectedLanguageId());
        rebuildLanguageOptions();
        rebuildThemeOptions();
        refreshTexts();
    });

    refreshTexts();
}

void FirstRunDialog::applySelections()
{
    if (settingsController_ == nullptr) {
        return;
    }

    settingsController_->setInterfaceLanguage(selectedLanguageId());
    settingsController_->setUiTheme(themeComboBox_->currentData().toString());
    settingsController_->setCustomValue(QStringLiteral("ui/mode"), QStringLiteral("simple"));
    settingsController_->setCustomValue(
        QStringLiteral("ui/dashboard_enabled"),
        dashboardCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QStringLiteral("ui/dashboard_show_on_idle"),
        dashboardCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    settingsController_->setCustomValue(
        QStringLiteral("playlist/progress_mode_enabled"),
        progressCheckBox_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
}

void FirstRunDialog::applyRightAlignedPresentation()
{
    if (formLayout_ != nullptr) {
        formLayout_->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        formLayout_->setFormAlignment(Qt::AlignRight | Qt::AlignTop);
    }

    if (introLabel_ != nullptr) {
        introLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    for (QLabel *label : {languageLabel_, themeLabel_}) {
        if (label != nullptr) {
            label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    for (QCheckBox *checkBox : {dashboardCheckBox_, progressCheckBox_}) {
        if (checkBox != nullptr) {
            checkBox->setLayoutDirection(Qt::RightToLeft);
        }
    }
}

QString FirstRunDialog::selectedLanguageId() const
{
    return languageComboBox_ != nullptr
        ? languageComboBox_->currentData().toString()
        : revaplayer::application::defaultUiLanguageId();
}

void FirstRunDialog::rebuildLanguageOptions()
{
    if (languageComboBox_ == nullptr) {
        return;
    }

    const QString selectedId = selectedLanguageId();
    languageComboBox_->blockSignals(true);
    languageComboBox_->clear();
    const auto languages = revaplayer::application::availableUiLanguages();
    for (const auto &language : languages) {
        languageComboBox_->addItem(language.label, language.id);
    }
    const int index = languageComboBox_->findData(selectedId);
    languageComboBox_->setCurrentIndex(index >= 0 ? index : 0);
    languageComboBox_->blockSignals(false);
}

void FirstRunDialog::rebuildThemeOptions()
{
    if (themeComboBox_ == nullptr) {
        return;
    }

    const QString selectedId = themeComboBox_->currentData().toString();
    themeComboBox_->clear();
    for (const auto &theme : revaplayer::application::availableThemes()) {
        themeComboBox_->addItem(revaplayer::application::translateUiText(theme.label), theme.id);
    }
    const int index = themeComboBox_->findData(selectedId);
    themeComboBox_->setCurrentIndex(index >= 0 ? index : 0);
}

void FirstRunDialog::refreshTexts()
{
    setWindowTitle(uiText("Welcome"));
    introLabel_->setText(uiText("Choose a starting setup. You can change everything later in Preferences."));
    languageLabel_->setText(uiText("Interface language"));
    themeLabel_->setText(uiText("Theme"));
    dashboardCheckBox_->setText(uiText("Show the Home Dashboard when the player is idle"));
    progressCheckBox_->setText(uiText("Enable saved-list progress tracking in the playlist"));

    if (buttonBox_ != nullptr) {
        if (QPushButton *okButton = buttonBox_->button(QDialogButtonBox::Ok); okButton != nullptr) {
            okButton->setText(uiText("OK"));
        }
        if (QPushButton *cancelButton = buttonBox_->button(QDialogButtonBox::Cancel); cancelButton != nullptr) {
            cancelButton->setText(uiText("Cancel"));
        }
    }
}

void FirstRunDialog::previewLanguage(const QString &languageId)
{
    revaplayer::application::setCurrentUiLanguage(languageId);
}

}  // namespace revaplayer::ui
