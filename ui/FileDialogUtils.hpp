#pragma once

#include <QFileDialog>
#include <QString>
#include <QStringList>

class QWidget;

namespace revaplayer::ui::filedialog {

QString getOpenFileName(
    QWidget *parent,
    const QString &caption,
    const QString &directory,
    const QString &filter = QString {},
    QFileDialog::Options options = {});

QStringList getOpenFileNames(
    QWidget *parent,
    const QString &caption,
    const QString &directory,
    const QString &filter = QString {},
    QFileDialog::Options options = {});

QString getSaveFileName(
    QWidget *parent,
    const QString &caption,
    const QString &directory,
    const QString &filter = QString {},
    QFileDialog::Options options = {});

QString getExistingDirectory(
    QWidget *parent,
    const QString &caption,
    const QString &directory,
    QFileDialog::Options options = {});

} // namespace revaplayer::ui::filedialog
