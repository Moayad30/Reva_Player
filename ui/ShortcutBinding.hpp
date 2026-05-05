#pragma once

#include <QKeySequence>
#include <QString>

namespace revaplayer::ui {

struct ShortcutBinding {
    QString id;
    QString category;
    QString label;
    QKeySequence defaultSequence;
    QKeySequence currentSequence;
};

}  // namespace revaplayer::ui
