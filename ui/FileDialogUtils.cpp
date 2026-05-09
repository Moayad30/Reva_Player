#include "ui/FileDialogUtils.hpp"

#include "application/UiLanguage.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBoxLayout>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QGridLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QPixmap>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSizePolicy>
#include <QSplitter>
#include <QStandardPaths>
#include <QTreeView>

#include <algorithm>
#include <array>
#include <functional>
#include <initializer_list>

namespace revaplayer::ui::filedialog {
namespace {

enum class HostDialogBackend {
    None,
    KDialog,
    Zenity,
};

struct HostDialogResult {
    bool handled {false};
    QStringList values;
};

struct ParsedFilter {
    QString label;
    QString patterns;
};

struct MediaPreviewState {
    QProcess *process {nullptr};
    QString ffmpegPath;
    QString currentPath;
    QString outputPath;
    int iconSize {44};
};

QString dialogText(const char *sourceText)
{
    return revaplayer::application::translateUiText(QString::fromUtf8(sourceText));
}

class FileDialogIconShortcutFilter : public QObject {
public:
    FileDialogIconShortcutFilter(QFileDialog *dialog, std::function<void(int)> changeIconSize, QObject *parent)
        : QObject(parent)
        , dialog_(dialog)
        , changeIconSize_(std::move(changeIconSize))
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (dialog_.isNull()
            || event->type() != QEvent::KeyPress
            || !dialog_->isVisible()) {
            return QObject::eventFilter(watched, event);
        }

        if (!belongsToDialog(watched)) {
            return QObject::eventFilter(watched, event);
        }

        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
        if (!(modifiers & Qt::ControlModifier)
            || (modifiers & Qt::AltModifier)
            || (modifiers & Qt::MetaModifier)) {
            return QObject::eventFilter(watched, event);
        }

        switch (keyEvent->key()) {
        case Qt::Key_Equal:
        case Qt::Key_Plus:
            changeIconSize_(8);
            keyEvent->accept();
            return true;
        case Qt::Key_Minus:
        case Qt::Key_Underscore:
            changeIconSize_(-8);
            keyEvent->accept();
            return true;
        default:
            return QObject::eventFilter(watched, event);
        }
    }

private:
    bool belongsToDialog(QObject *object) const
    {
        for (QObject *current = object; current != nullptr; current = current->parent()) {
            if (current == dialog_) {
                return true;
            }
        }

        auto *widget = qobject_cast<QWidget *>(object);
        for (QWidget *current = widget; current != nullptr; current = current->parentWidget()) {
            if (current == dialog_ || dialog_->isAncestorOf(current)) {
                return true;
            }
        }

        return false;
    }

    QPointer<QFileDialog> dialog_;
    std::function<void(int)> changeIconSize_;
};

bool containsAnyToken(const QString &value, const std::initializer_list<const char *> tokens)
{
    for (const char *token : tokens) {
        if (value.contains(QString::fromUtf8(token), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool isAppImageRuntime()
{
    return qEnvironmentVariableIsSet("APPIMAGE") || qEnvironmentVariableIsSet("APPDIR");
}

bool hostDialogsForced()
{
    return qEnvironmentVariableIntValue("REVA_USE_HOST_FILE_DIALOGS") != 0;
}

bool hostDialogsDisabled()
{
    return qEnvironmentVariableIntValue("REVA_DISABLE_HOST_FILE_DIALOGS") != 0;
}

bool isKdeFamilyDesktop()
{
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    const QString session = qEnvironmentVariable("DESKTOP_SESSION");
    return containsAnyToken(desktop, {"KDE", "Plasma", "LXQt"})
        || containsAnyToken(session, {"kde", "plasma", "lxqt"});
}

bool isGtkFamilyDesktop()
{
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    const QString session = qEnvironmentVariable("DESKTOP_SESSION");
    return containsAnyToken(desktop, {"GNOME", "XFCE", "MATE", "Cinnamon", "Budgie", "Pantheon"})
        || containsAnyToken(session, {"gnome", "xfce", "mate", "cinnamon", "budgie", "pantheon"});
}

QString findHostDialogTool(const QString &tool)
{
    return QStandardPaths::findExecutable(tool);
}

HostDialogBackend hostDialogBackend()
{
    if ((!isAppImageRuntime() && !hostDialogsForced()) || hostDialogsDisabled()) {
        return HostDialogBackend::None;
    }

    if (isKdeFamilyDesktop() && !findHostDialogTool(QStringLiteral("kdialog")).isEmpty()) {
        return HostDialogBackend::KDialog;
    }

    if (isGtkFamilyDesktop()
        && (!findHostDialogTool(QStringLiteral("zenity")).isEmpty()
            || !findHostDialogTool(QStringLiteral("qarma")).isEmpty())) {
        return HostDialogBackend::Zenity;
    }

    return HostDialogBackend::None;
}

QList<ParsedFilter> parseQtFilters(const QString &qtFilter)
{
    QList<ParsedFilter> filters;
    const QStringList sections = qtFilter.split(QStringLiteral(";;"), Qt::SkipEmptyParts);
    const QRegularExpression pattern(QStringLiteral(R"(^\s*(.*?)\s*\(([^)]+)\)\s*$)"));

    for (const QString &section : sections) {
        const QString trimmed = section.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        ParsedFilter filter;
        const QRegularExpressionMatch match = pattern.match(trimmed);
        if (match.hasMatch()) {
            filter.label = match.captured(1).trimmed();
            filter.patterns = match.captured(2).trimmed();
        } else {
            filter.label = trimmed;
            filter.patterns = QStringLiteral("*");
        }

        filter.patterns.replace(QStringLiteral("*.*"), QStringLiteral("*"));
        if (filter.patterns.trimmed().isEmpty()) {
            filter.patterns = QStringLiteral("*");
        }
        if (filter.label.trimmed().isEmpty()) {
            filter.label = filter.patterns;
        }

        filters.push_back(filter);
    }

    return filters;
}

QString toKDialogFilter(const QString &qtFilter)
{
    const QList<ParsedFilter> filters = parseQtFilters(qtFilter);
    QStringList converted;
    converted.reserve(filters.size());
    for (const ParsedFilter &filter : filters) {
        converted.push_back(QStringLiteral("%1|%2").arg(filter.patterns, filter.label));
    }
    return converted.join(QChar('\n'));
}

QStringList toZenityFilterArguments(const QString &qtFilter)
{
    QStringList arguments;
    for (const ParsedFilter &filter : parseQtFilters(qtFilter)) {
        arguments.push_back(QStringLiteral("--file-filter=%1 | %2").arg(filter.label, filter.patterns));
    }
    return arguments;
}

QString normalizedDialogPath(QString path, const bool directoryPreferred)
{
    if (path.isEmpty()) {
        return QDir::homePath();
    }

    const QFileInfo info(path);
    if (directoryPreferred && info.exists() && info.isDir()) {
        return QDir(path).absolutePath();
    }

    if (info.exists() && info.isDir()) {
        return QDir(path).absolutePath() + QDir::separator();
    }

    return info.absoluteFilePath();
}

QString formatByteSize(const qint64 bytes)
{
    if (bytes < 0) {
        return {};
    }

    static constexpr std::array<const char *, 5> kUnits {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    qsizetype unitIndex = 0;
    while (value >= 1024.0 && unitIndex < static_cast<qsizetype>(kUnits.size()) - 1) {
        value /= 1024.0;
        ++unitIndex;
    }

    return QStringLiteral("%1 %2")
        .arg(value, 0, unitIndex == 0 ? 'f' : 'f', unitIndex == 0 ? 0 : 1)
        .arg(QString::fromLatin1(kUnits.at(unitIndex)));
}

bool isVideoPreviewCandidate(const QFileInfo &fileInfo)
{
    static const QSet<QString> kVideoExtensions {
        QStringLiteral("3gp"),
        QStringLiteral("avi"),
        QStringLiteral("m4v"),
        QStringLiteral("mkv"),
        QStringLiteral("mov"),
        QStringLiteral("mp4"),
        QStringLiteral("mpeg"),
        QStringLiteral("mpg"),
        QStringLiteral("webm"),
        QStringLiteral("wmv"),
    };
    return fileInfo.isFile() && kVideoExtensions.contains(fileInfo.suffix().trimmed().toLower());
}

QString previewOutputPathFor(const QString &path)
{
    QString root = QStandardPaths::writableLocation(QStandardPaths::TempLocation).trimmed();
    if (root.isEmpty()) {
        root = QDir::tempPath();
    }

    const QString previewRoot = QDir(root).filePath(QStringLiteral("revaplayer-file-dialog-previews"));
    QDir().mkpath(previewRoot);
    const QByteArray digest = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QDir(previewRoot).filePath(QStringLiteral("%1.jpg").arg(QString::fromLatin1(digest)));
}

HostDialogResult runDialogProcess(
    const QString &program,
    const QStringList &arguments,
    const bool splitLines)
{
    HostDialogResult result;
    if (program.trimmed().isEmpty()) {
        return result;
    }

    QProcess process;
    process.start(program, arguments, QIODevice::ReadOnly);
    if (!process.waitForStarted(3000)) {
        return result;
    }

    process.closeWriteChannel();
    process.waitForFinished(-1);

    if (process.exitStatus() != QProcess::NormalExit) {
        return result;
    }

    if (process.exitCode() == 1) {
        result.handled = true;
        return result;
    }

    if (process.exitCode() != 0) {
        return result;
    }

    result.handled = true;
    QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    while (output.endsWith(QChar('\n')) || output.endsWith(QChar('\r'))) {
        output.chop(1);
    }
    if (output.isEmpty()) {
        return result;
    }

    if (splitLines) {
        result.values = output.split(QRegularExpression(QStringLiteral(R"([\r\n]+)")), Qt::SkipEmptyParts);
    } else {
        result.values = QStringList {output};
    }
    return result;
}

HostDialogResult hostOpenFileNames(
    const QString &caption,
    const QString &directory,
    const QString &filter)
{
    switch (hostDialogBackend()) {
    case HostDialogBackend::KDialog: {
        QStringList arguments {
            QStringLiteral("--getopenfilename"),
            normalizedDialogPath(directory, true),
            toKDialogFilter(filter),
            QStringLiteral("--multiple"),
            QStringLiteral("--separate-output"),
        };
        if (!caption.trimmed().isEmpty()) {
            arguments << QStringLiteral("--title") << caption;
        }
        return runDialogProcess(findHostDialogTool(QStringLiteral("kdialog")), arguments, true);
    }
    case HostDialogBackend::Zenity: {
        const QString tool = !findHostDialogTool(QStringLiteral("zenity")).isEmpty()
            ? findHostDialogTool(QStringLiteral("zenity"))
            : findHostDialogTool(QStringLiteral("qarma"));
        QStringList arguments {
            QStringLiteral("--file-selection"),
            QStringLiteral("--multiple"),
            QStringLiteral("--separator=\n"),
            QStringLiteral("--filename=%1").arg(normalizedDialogPath(directory, true)),
        };
        if (!caption.trimmed().isEmpty()) {
            arguments << QStringLiteral("--title=%1").arg(caption);
        }
        arguments.append(toZenityFilterArguments(filter));
        return runDialogProcess(tool, arguments, true);
    }
    case HostDialogBackend::None:
        return {};
    }

    return {};
}

HostDialogResult hostOpenFileName(
    const QString &caption,
    const QString &directory,
    const QString &filter)
{
    switch (hostDialogBackend()) {
    case HostDialogBackend::KDialog: {
        QStringList arguments {
            QStringLiteral("--getopenfilename"),
            normalizedDialogPath(directory, true),
            toKDialogFilter(filter),
        };
        if (!caption.trimmed().isEmpty()) {
            arguments << QStringLiteral("--title") << caption;
        }
        return runDialogProcess(findHostDialogTool(QStringLiteral("kdialog")), arguments, false);
    }
    case HostDialogBackend::Zenity: {
        const QString tool = !findHostDialogTool(QStringLiteral("zenity")).isEmpty()
            ? findHostDialogTool(QStringLiteral("zenity"))
            : findHostDialogTool(QStringLiteral("qarma"));
        QStringList arguments {
            QStringLiteral("--file-selection"),
            QStringLiteral("--filename=%1").arg(normalizedDialogPath(directory, true)),
        };
        if (!caption.trimmed().isEmpty()) {
            arguments << QStringLiteral("--title=%1").arg(caption);
        }
        arguments.append(toZenityFilterArguments(filter));
        return runDialogProcess(tool, arguments, false);
    }
    case HostDialogBackend::None:
        return {};
    }

    return {};
}

HostDialogResult hostSaveFileName(
    const QString &caption,
    const QString &path,
    const QString &filter)
{
    switch (hostDialogBackend()) {
    case HostDialogBackend::KDialog: {
        QStringList arguments {
            QStringLiteral("--getsavefilename"),
            normalizedDialogPath(path, false),
            toKDialogFilter(filter),
        };
        if (!caption.trimmed().isEmpty()) {
            arguments << QStringLiteral("--title") << caption;
        }
        return runDialogProcess(findHostDialogTool(QStringLiteral("kdialog")), arguments, false);
    }
    case HostDialogBackend::Zenity: {
        const QString tool = !findHostDialogTool(QStringLiteral("zenity")).isEmpty()
            ? findHostDialogTool(QStringLiteral("zenity"))
            : findHostDialogTool(QStringLiteral("qarma"));
        QStringList arguments {
            QStringLiteral("--file-selection"),
            QStringLiteral("--save"),
            QStringLiteral("--confirm-overwrite"),
            QStringLiteral("--filename=%1").arg(normalizedDialogPath(path, false)),
        };
        if (!caption.trimmed().isEmpty()) {
            arguments << QStringLiteral("--title=%1").arg(caption);
        }
        arguments.append(toZenityFilterArguments(filter));
        return runDialogProcess(tool, arguments, false);
    }
    case HostDialogBackend::None:
        return {};
    }

    return {};
}

void improveQtFileDialogAppearance(QFileDialog &dialog, const int itemIconSize = 40)
{
    dialog.setMinimumSize(860, 560);
    if (!dialog.property("revaFileDialogInitialSizeApplied").toBool()) {
        dialog.resize(980, 640);
        dialog.setProperty("revaFileDialogInitialSizeApplied", true);
    }
    const int normalizedIconSize = std::clamp(itemIconSize, 28, 160);
    const int rowHeight = std::clamp(normalizedIconSize + 12, 34, 112);
    dialog.setStyleSheet(QStringLiteral(
        "QFileDialog { font-size: 15px; background: #111820; }"
        "QFileDialog QSplitter::handle { background: rgba(116, 144, 189, 0.55); }"
        "QFileDialog QSplitter::handle:horizontal { width: 2px; margin: 0 6px; }"
        "QFileDialog QSplitter::handle:vertical { height: 2px; margin: 6px 0; }"
        "QTreeView::item, QListView::item { min-height: %1px; padding: 6px 8px; }"
        "QTreeView::item:selected, QListView::item:selected { background: #26344d; color: #ffffff; }"
        "QTreeView::item:hover, QListView::item:hover { background: rgba(63, 83, 121, 0.55); }"
        "QLineEdit, QComboBox { min-height: 34px; font-size: 15px; background: #17202b; border: 1px solid rgba(130, 157, 199, 0.28); border-radius: 8px; padding: 0 10px; }"
        "QLineEdit:focus, QComboBox:focus { border-color: #7aa2ff; }"
        "QPushButton { min-height: 34px; font-size: 15px; background: #1b2430; border: 1px solid rgba(130, 157, 199, 0.32); border-radius: 8px; padding: 0 12px; }"
        "QPushButton:hover { background: #233047; border-color: rgba(156, 185, 231, 0.62); }"
        "QPushButton:pressed { background: #2a3a58; }")
                             .arg(rowHeight));

    for (QSplitter *splitter : dialog.findChildren<QSplitter *>()) {
        splitter->setHandleWidth(8);
    }

    for (QListView *view : dialog.findChildren<QListView *>()) {
        view->setIconSize(QSize(normalizedIconSize, normalizedIconSize));
        view->setSpacing(6);
    }
    for (QTreeView *view : dialog.findChildren<QTreeView *>()) {
        view->setIconSize(QSize(normalizedIconSize, normalizedIconSize));
        view->setUniformRowHeights(false);
    }
}

void constrainPathComboBox(QFileDialog &dialog)
{
    for (QComboBox *comboBox : dialog.findChildren<QComboBox *>()) {
        const QString text = comboBox->currentText().trimmed();
        if (!text.startsWith(QDir::separator())) {
            continue;
        }

        comboBox->setMinimumWidth(280);
        comboBox->setMaximumWidth(590);
        comboBox->setSizePolicy(QSizePolicy::Preferred, comboBox->sizePolicy().verticalPolicy());
        return;
    }
}

QAbstractButton *findDialogButton(QFileDialog &dialog, const QString &objectName)
{
    for (QAbstractButton *button : dialog.findChildren<QAbstractButton *>()) {
        if (button->objectName() == objectName) {
            return button;
        }
    }
    return nullptr;
}

void hideNativeFileDialogOptionButtons(QFileDialog &dialog)
{
    for (const QString &objectName : {
             QStringLiteral("newFolderButton"),
             QStringLiteral("listModeButton"),
             QStringLiteral("detailModeButton"),
        }) {
        if (QAbstractButton *button = findDialogButton(dialog, objectName)) {
            button->hide();
        }
    }
}

void orientDialogActionButtonsHorizontally(QFileDialog &dialog)
{
    for (QDialogButtonBox *buttonBox : dialog.findChildren<QDialogButtonBox *>()) {
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setCenterButtons(false);
        buttonBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    }
}

bool placeOptionsControlInNativeToolbar(QFileDialog &dialog, QWidget *control)
{
    for (const QString &objectName : {
             QStringLiteral("toParentButton"),
             QStringLiteral("forwardButton"),
             QStringLiteral("backButton"),
             QStringLiteral("detailModeButton"),
             QStringLiteral("listModeButton"),
             QStringLiteral("newFolderButton"),
        }) {
        QAbstractButton *button = findDialogButton(dialog, objectName);
        if (button == nullptr || button->parentWidget() == nullptr || button->parentWidget()->layout() == nullptr) {
            continue;
        }

        QWidget *toolbar = button->parentWidget();
        control->setParent(toolbar);
        if (auto *box = qobject_cast<QBoxLayout *>(toolbar->layout())) {
            const int index = box->indexOf(button);
            if (index >= 0) {
                box->insertWidget(index + 1, control, 0);
            } else {
                box->addWidget(control, 0);
            }
            return true;
        }

        if (auto *grid = qobject_cast<QGridLayout *>(toolbar->layout())) {
            const int index = grid->indexOf(button);
            if (index < 0) {
                continue;
            }

            int row = 0;
            int column = 0;
            int rowSpan = 1;
            int columnSpan = 1;
            grid->getItemPosition(index, &row, &column, &rowSpan, &columnSpan);
            grid->addWidget(control, row, column + columnSpan, rowSpan, 1);
            return true;
        }
    }

    return false;
}

bool placeCompactNavigationControls(QFileDialog &dialog, QWidget *optionsControl)
{
    auto *grid = qobject_cast<QGridLayout *>(dialog.layout());
    if (grid == nullptr) {
        return false;
    }

    QList<QAbstractButton *> buttons;
    for (const QString &objectName : {
             QStringLiteral("backButton"),
             QStringLiteral("forwardButton"),
             QStringLiteral("toParentButton"),
        }) {
        if (QAbstractButton *button = findDialogButton(dialog, objectName)) {
            buttons.push_back(button);
        }
    }

    if (buttons.isEmpty()) {
        return false;
    }

    int row = 0;
    int firstColumn = grid->columnCount();
    QSet<int> movedColumns;
    for (QAbstractButton *button : buttons) {
        const int index = grid->indexOf(button);
        if (index < 0) {
            continue;
        }

        int buttonRow = 0;
        int buttonColumn = 0;
        int rowSpan = 1;
        int columnSpan = 1;
        grid->getItemPosition(index, &buttonRow, &buttonColumn, &rowSpan, &columnSpan);
        row = buttonRow;
        firstColumn = std::min(firstColumn, buttonColumn);
        movedColumns.insert(buttonColumn);
    }
    if (firstColumn == grid->columnCount()) {
        firstColumn = std::max(0, grid->columnCount() - 1);
    }

    auto *navigationControls = new QWidget(&dialog);
    navigationControls->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *navigationLayout = new QHBoxLayout(navigationControls);
    navigationLayout->setContentsMargins(0, 0, 0, 0);
    navigationLayout->setSpacing(6);

    for (QAbstractButton *button : buttons) {
        if (button->layout() != nullptr) {
            button->layout()->removeWidget(button);
        }
        if (QWidget *parent = button->parentWidget(); parent != nullptr && parent->layout() != nullptr) {
            parent->layout()->removeWidget(button);
        }
        grid->removeWidget(button);
        button->setParent(navigationControls);
        navigationLayout->addWidget(button, 0);
    }

    grid->removeWidget(optionsControl);
    optionsControl->setParent(navigationControls);
    navigationLayout->addWidget(optionsControl, 0);

    grid->addWidget(navigationControls, row, firstColumn, 1, 1, Qt::AlignRight | Qt::AlignVCenter);
    for (const int column : movedColumns) {
        if (column != firstColumn) {
            grid->setColumnMinimumWidth(column, 0);
            grid->setColumnStretch(column, 0);
        }
    }
    return true;
}

void addMediaOpenTools(QFileDialog &dialog, const bool previewEnabled = true)
{
    auto *state = new MediaPreviewState;
    state->process = new QProcess(&dialog);
    state->ffmpegPath = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    QObject::connect(&dialog, &QObject::destroyed, [state]() {
        delete state;
    });

    auto *topControls = new QWidget(&dialog);
    topControls->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    auto *topControlsLayout = new QHBoxLayout(topControls);
    topControlsLayout->setContentsMargins(0, 0, 0, 0);
    topControlsLayout->setSpacing(6);

    auto *optionsMenu = new QMenu(topControls);
    optionsMenu->setStyleSheet(QStringLiteral(
        "QMenu { background: #151a22; border: 1px solid rgba(137, 164, 205, 0.45); border-radius: 8px; padding: 6px; }"
        "QMenu::item { color: #f4f7fb; padding: 8px 28px 8px 12px; border-radius: 6px; }"
        "QMenu::item:selected { background: #26344d; }"
        "QMenu::separator { height: 1px; background: rgba(137, 164, 205, 0.25); margin: 6px 8px; }"));

    auto *newFolderAction = optionsMenu->addAction(dialogText("New Folder"));
    optionsMenu->addSeparator();

    auto *viewModeGroup = new QActionGroup(optionsMenu);
    viewModeGroup->setExclusive(true);
    auto *iconsAction = optionsMenu->addAction(dialogText("Icons"));
    iconsAction->setCheckable(true);
    iconsAction->setActionGroup(viewModeGroup);
    auto *detailsAction = optionsMenu->addAction(dialogText("Details"));
    detailsAction->setCheckable(true);
    detailsAction->setChecked(true);
    detailsAction->setActionGroup(viewModeGroup);
    optionsMenu->addSeparator();

    auto *decreaseIconSizeAction = optionsMenu->addAction(dialogText("Decrease item size"));
    decreaseIconSizeAction->setShortcuts({
        QKeySequence(Qt::CTRL | Qt::Key_Minus),
        QKeySequence(Qt::CTRL | Qt::Key_Underscore),
    });
    decreaseIconSizeAction->setShortcutContext(Qt::WindowShortcut);
    auto *increaseIconSizeAction = optionsMenu->addAction(dialogText("Increase item size"));
    increaseIconSizeAction->setShortcuts({
        QKeySequence(Qt::CTRL | Qt::Key_Equal),
        QKeySequence(Qt::CTRL | Qt::Key_Plus),
    });
    increaseIconSizeAction->setShortcutContext(Qt::WindowShortcut);

    QAction *previewAction = nullptr;
    if (previewEnabled) {
        optionsMenu->addSeparator();
        previewAction = optionsMenu->addAction(dialogText("Preview"));
        previewAction->setCheckable(true);
        previewAction->setChecked(false);
    }

    auto *optionsButton = new QPushButton(QStringLiteral("⚙"), topControls);
    optionsButton->setFixedSize(48, 42);
    optionsButton->setMenu(optionsMenu);
    optionsButton->setToolTip(dialogText("Open file dialog options"));
    optionsButton->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 21px; font-weight: 700; padding: 0; border-radius: 11px; background: #202b3a; border: 1px solid rgba(142, 172, 220, 0.46); color: #f7f9fc; }"
        "QPushButton:hover { background: #2b3a55; border-color: #93b7ff; }"
        "QPushButton:pressed { background: #334767; }"
        "QPushButton::menu-indicator { width: 0px; image: none; }"));

    topControlsLayout->addWidget(optionsButton, 0);

    auto *previewPanel = new QWidget(&dialog);
    previewPanel->setVisible(false);
    auto *previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(8, 8, 8, 8);
    previewLayout->setSpacing(8);

    auto *previewTitle = new QLabel(dialogText("Preview"), previewPanel);
    previewTitle->setObjectName(QStringLiteral("mediaDialogPreviewTitle"));
    auto *previewImage = new QLabel(dialogText("Select a media file"), previewPanel);
    previewImage->setAlignment(Qt::AlignCenter);
    previewImage->setFixedSize(260, 146);
    previewImage->setStyleSheet(QStringLiteral(
        "QLabel { background: #0b0f14; border: 1px solid rgba(134, 159, 198, 0.45); border-radius: 8px; color: #8fa3bd; }"));
    auto *previewInfo = new QLabel(previewPanel);
    previewInfo->setWordWrap(true);
    previewInfo->setMinimumWidth(260);

    previewLayout->addWidget(previewTitle, 0);
    previewLayout->addWidget(previewImage, 0);
    previewLayout->addWidget(previewInfo, 0);
    previewLayout->addStretch(1);

    if (auto *grid = qobject_cast<QGridLayout *>(dialog.layout())) {
        const int rowCount = grid->rowCount();
        const int columnCount = grid->columnCount();
        if (!placeCompactNavigationControls(dialog, topControls)
            && !placeOptionsControlInNativeToolbar(dialog, topControls)) {
            grid->addWidget(topControls, 0, std::max(0, columnCount - 1), 1, 1, Qt::AlignRight | Qt::AlignVCenter);
        }
        if (previewEnabled) {
            const int previewColumn = grid->columnCount();
            grid->addWidget(previewPanel, 0, previewColumn, rowCount, 1);
            grid->setColumnStretch(previewColumn, 0);
        }
    } else if (auto *box = qobject_cast<QBoxLayout *>(dialog.layout())) {
        if (!placeOptionsControlInNativeToolbar(dialog, topControls)) {
            box->insertWidget(0, topControls);
        }
        if (previewEnabled) {
            box->addWidget(previewPanel);
        }
    }
    constrainPathComboBox(dialog);
    hideNativeFileDialogOptionButtons(dialog);
    orientDialogActionButtonsHorizontally(dialog);

    const auto updatePreviewImage = [previewImage](const QImage &image) {
        if (image.isNull()) {
            previewImage->setPixmap(QPixmap {});
            return;
        }
        previewImage->setPixmap(QPixmap::fromImage(image).scaled(
            previewImage->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
    };

    QObject::connect(state->process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), &dialog, [state, updatePreviewImage, previewImage, previewPanel]() {
        if (!previewPanel->isVisible()) {
            return;
        }
        if (state->process == nullptr || state->process->exitStatus() != QProcess::NormalExit || state->process->exitCode() != 0) {
            previewImage->setText(dialogText("Preview unavailable"));
            return;
        }

        QImageReader reader(state->outputPath);
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        if (image.isNull()) {
            previewImage->setText(dialogText("Preview unavailable"));
            return;
        }

        previewImage->setText(QString {});
        updatePreviewImage(image);
    });

    const auto stopPreviewProcess = [state]() {
        if (state->process != nullptr && state->process->state() != QProcess::NotRunning) {
            state->process->kill();
            state->process->waitForFinished(100);
        }
    };

    const auto changeIconSize = [state, &dialog](const int delta) {
        state->iconSize = std::clamp(state->iconSize + delta, 32, 144);
        improveQtFileDialogAppearance(dialog, state->iconSize);
        constrainPathComboBox(dialog);
        hideNativeFileDialogOptionButtons(dialog);
    };

    QObject::connect(newFolderAction, &QAction::triggered, &dialog, [&dialog]() {
        if (QAbstractButton *button = findDialogButton(dialog, QStringLiteral("newFolderButton"))) {
            button->click();
        }
    });
    QObject::connect(iconsAction, &QAction::triggered, &dialog, [&dialog]() {
        dialog.setViewMode(QFileDialog::List);
        hideNativeFileDialogOptionButtons(dialog);
    });
    QObject::connect(detailsAction, &QAction::triggered, &dialog, [&dialog]() {
        dialog.setViewMode(QFileDialog::Detail);
        hideNativeFileDialogOptionButtons(dialog);
    });
    QObject::connect(decreaseIconSizeAction, &QAction::triggered, &dialog, [changeIconSize]() {
        changeIconSize(-8);
    });
    QObject::connect(increaseIconSizeAction, &QAction::triggered, &dialog, [changeIconSize]() {
        changeIconSize(8);
    });
    dialog.addAction(decreaseIconSizeAction);
    dialog.addAction(increaseIconSizeAction);

    auto *iconShortcutFilter = new FileDialogIconShortcutFilter(&dialog, changeIconSize, &dialog);
    qApp->installEventFilter(iconShortcutFilter);
    QObject::connect(&dialog, &QObject::destroyed, qApp, [iconShortcutFilter]() {
        qApp->removeEventFilter(iconShortcutFilter);
    });

    if (!previewEnabled || previewAction == nullptr) {
        return;
    }

    const auto refreshPreview = [state, previewPanel, previewInfo, previewImage, updatePreviewImage, stopPreviewProcess](const QString &path) {
        stopPreviewProcess();
        previewImage->setPixmap(QPixmap {});
        if (!previewPanel->isVisible()) {
            previewImage->setText(dialogText("Select a media file"));
            previewInfo->clear();
            return;
        }

        const QFileInfo info(path);
        if (!info.exists()) {
            previewImage->setText(dialogText("Select a media file"));
            previewInfo->clear();
            return;
        }

        const QString sizeText = info.isFile() ? formatByteSize(info.size()) : dialogText("Folder");
        previewInfo->setText(QStringLiteral("%1\n%2")
                                 .arg(info.fileName(), sizeText));

        QImageReader imageReader(info.absoluteFilePath());
        imageReader.setAutoTransform(true);
        if (imageReader.canRead()) {
            const QImage image = imageReader.read();
            if (!image.isNull()) {
                previewImage->setText(QString {});
                updatePreviewImage(image);
                return;
            }
        }

        if (!isVideoPreviewCandidate(info)) {
            previewImage->setText(dialogText("Preview unavailable"));
            return;
        }

        if (state->ffmpegPath.isEmpty()) {
            previewImage->setText(dialogText("Video preview requires ffmpeg"));
            return;
        }

        state->outputPath = previewOutputPathFor(info.absoluteFilePath());
        QFile::remove(state->outputPath);
        previewImage->setText(dialogText("Generating preview..."));
        state->process->start(
            state->ffmpegPath,
            {
                QStringLiteral("-hide_banner"),
                QStringLiteral("-loglevel"),
                QStringLiteral("error"),
                QStringLiteral("-y"),
                QStringLiteral("-ss"),
                QStringLiteral("00:00:01"),
                QStringLiteral("-i"),
                info.absoluteFilePath(),
                QStringLiteral("-frames:v"),
                QStringLiteral("1"),
                QStringLiteral("-vf"),
                QStringLiteral("scale=520:-2"),
                state->outputPath,
            });
    };

    QObject::connect(&dialog, &QFileDialog::currentChanged, &dialog, [state, refreshPreview](const QString &path) {
        state->currentPath = path;
        refreshPreview(state->currentPath);
    });

    QObject::connect(previewAction, &QAction::toggled, &dialog, [state, previewPanel, previewImage, previewInfo, refreshPreview, stopPreviewProcess](const bool visible) {
        previewPanel->setVisible(visible);
        if (visible) {
            refreshPreview(state->currentPath);
            return;
        }

        stopPreviewProcess();
        previewImage->setPixmap(QPixmap {});
        previewImage->setText(dialogText("Select a media file"));
        previewInfo->clear();
    });
}

} // namespace

QString getOpenFileName(
    QWidget *parent,
    const QString &caption,
    const QString &directory,
    const QString &filter,
    const QFileDialog::Options options)
{
    QFileDialog::Options effectiveOptions = options;
#if defined(Q_OS_LINUX)
    effectiveOptions |= QFileDialog::DontUseNativeDialog;
#endif
    const HostDialogResult hostResult = hostOpenFileName(caption, directory, filter);
    if (hostResult.handled) {
        return hostResult.values.value(0);
    }

    QFileDialog dialog(parent, caption, directory, filter);
    dialog.setOptions(effectiveOptions);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setViewMode(QFileDialog::Detail);
    improveQtFileDialogAppearance(dialog);
    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }
    const QStringList files = dialog.selectedFiles();
    return files.isEmpty() ? QString {} : files.constFirst();
}

QStringList getOpenFileNames(
    QWidget *parent,
    const QString &caption,
    const QString &directory,
    const QString &filter,
    const QFileDialog::Options options)
{
    QFileDialog::Options effectiveOptions = options;
#if defined(Q_OS_LINUX)
    effectiveOptions |= QFileDialog::DontUseNativeDialog;
#endif
    const HostDialogResult hostResult = hostOpenFileNames(caption, directory, filter);
    if (hostResult.handled) {
        return hostResult.values;
    }

    QFileDialog dialog(parent, caption, directory, filter);
    dialog.setOptions(effectiveOptions);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setViewMode(QFileDialog::Detail);
    improveQtFileDialogAppearance(dialog);
    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }
    return dialog.selectedFiles();
}

QStringList getOpenMediaFileNames(
    QWidget *parent,
    const QString &caption,
    const QString &directory,
    const QString &filter,
    const QFileDialog::Options options)
{
    QFileDialog::Options effectiveOptions = options;
#if defined(Q_OS_LINUX)
    effectiveOptions |= QFileDialog::DontUseNativeDialog;
#endif

    QFileDialog dialog(parent, caption, directory, filter);
    dialog.setOptions(effectiveOptions);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setViewMode(QFileDialog::Detail);
    improveQtFileDialogAppearance(dialog, 44);
    addMediaOpenTools(dialog);
    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }
    return dialog.selectedFiles();
}

QString getSaveFileName(
    QWidget *parent,
    const QString &caption,
    const QString &directory,
    const QString &filter,
    const QFileDialog::Options options)
{
    QFileDialog::Options effectiveOptions = options;
#if defined(Q_OS_LINUX)
    effectiveOptions |= QFileDialog::DontUseNativeDialog;
#endif
    const HostDialogResult hostResult = hostSaveFileName(caption, directory, filter);
    if (hostResult.handled) {
        return hostResult.values.value(0);
    }

    QFileDialog dialog(parent, caption, directory, filter);
    dialog.setOptions(effectiveOptions);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setViewMode(QFileDialog::Detail);
    improveQtFileDialogAppearance(dialog);
    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }
    const QStringList files = dialog.selectedFiles();
    return files.isEmpty() ? QString {} : files.constFirst();
}

QString getExistingDirectory(
    QWidget *parent,
    const QString &caption,
    const QString &directory,
    const QFileDialog::Options options)
{
    QFileDialog::Options effectiveOptions = options;
#if defined(Q_OS_LINUX)
    effectiveOptions |= QFileDialog::DontUseNativeDialog;
#endif

    QFileDialog dialog(parent, caption, directory);
    dialog.setOptions(effectiveOptions);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setViewMode(QFileDialog::Detail);
    improveQtFileDialogAppearance(dialog, 44);
    addMediaOpenTools(dialog, false);
    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }
    const QStringList files = dialog.selectedFiles();
    return files.isEmpty() ? QString {} : files.constFirst();
}

} // namespace revaplayer::ui::filedialog
