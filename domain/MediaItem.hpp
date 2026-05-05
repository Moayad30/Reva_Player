#pragma once

#include <QString>

namespace revaplayer::domain {

struct MediaItem {
    QString source;
    QString displayTitle;
    bool isUrl {false};
};

struct PlaylistEntry {
    int index {-1};
    QString title;
    QString source;
    bool isCurrent {false};
};

}  // namespace revaplayer::domain

