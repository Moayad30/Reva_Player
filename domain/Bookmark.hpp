#pragma once

#include <QtGlobal>
#include <QString>

namespace revaplayer::domain {

struct Bookmark {
    qint64 id {-1};
    QString source;
    QString title;
    QString category;
    QString note;
    double positionSeconds {0.0};
    QString createdAt;
    QString updatedAt;
};

}  // namespace revaplayer::domain
