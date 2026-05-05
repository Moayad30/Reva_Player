#pragma once

#include <QString>

namespace revaplayer::domain {

struct ChapterInfo {
    int index {-1};
    QString title;
    double timeSeconds {0.0};
};

}  // namespace revaplayer::domain

