#include "ui/FileDialogUtils.hpp"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

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

HostDialogResult hostExistingDirectory(
    const QString &caption,
    const QString &directory)
{
    switch (hostDialogBackend()) {
    case HostDialogBackend::KDialog: {
        QStringList arguments {
            QStringLiteral("--getexistingdirectory"),
            normalizedDialogPath(directory, true),
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
            QStringLiteral("--directory"),
            QStringLiteral("--filename=%1").arg(normalizedDialogPath(directory, true)),
        };
        if (!caption.trimmed().isEmpty()) {
            arguments << QStringLiteral("--title=%1").arg(caption);
        }
        return runDialogProcess(tool, arguments, false);
    }
    case HostDialogBackend::None:
        return {};
    }

    return {};
}

} // namespace

QString getOpenFileName(
    QWidget *parent,
    const QString &caption,
    const QString &directory,
    const QString &filter,
    const QFileDialog::Options options)
{
    const HostDialogResult hostResult = hostOpenFileName(caption, directory, filter);
    if (hostResult.handled) {
        return hostResult.values.value(0);
    }
    return QFileDialog::getOpenFileName(parent, caption, directory, filter, nullptr, options);
}

QStringList getOpenFileNames(
    QWidget *parent,
    const QString &caption,
    const QString &directory,
    const QString &filter,
    const QFileDialog::Options options)
{
    const HostDialogResult hostResult = hostOpenFileNames(caption, directory, filter);
    if (hostResult.handled) {
        return hostResult.values;
    }
    return QFileDialog::getOpenFileNames(parent, caption, directory, filter, nullptr, options);
}

QString getSaveFileName(
    QWidget *parent,
    const QString &caption,
    const QString &directory,
    const QString &filter,
    const QFileDialog::Options options)
{
    const HostDialogResult hostResult = hostSaveFileName(caption, directory, filter);
    if (hostResult.handled) {
        return hostResult.values.value(0);
    }
    return QFileDialog::getSaveFileName(parent, caption, directory, filter, nullptr, options);
}

QString getExistingDirectory(
    QWidget *parent,
    const QString &caption,
    const QString &directory,
    const QFileDialog::Options options)
{
    const HostDialogResult hostResult = hostExistingDirectory(caption, directory);
    if (hostResult.handled) {
        return hostResult.values.value(0);
    }
    return QFileDialog::getExistingDirectory(parent, caption, directory, options);
}

} // namespace revaplayer::ui::filedialog
