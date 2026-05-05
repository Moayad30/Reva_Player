#pragma once

#include <QString>

namespace revaplayer::domain {

enum class TrackType {
    Video,
    Audio,
    Subtitle,
    Unknown,
};

struct TrackInfo {
    int id {-1};
    TrackType type {TrackType::Unknown};
    QString title;
    QString language;
    QString codec;
    int width {0};
    int height {0};
    double fps {0.0};
    bool selected {false};
    bool external {false};
};

inline QString trackTypeLabel(const TrackType type)
{
    switch (type) {
    case TrackType::Video:
        return QStringLiteral("Video");
    case TrackType::Audio:
        return QStringLiteral("Audio");
    case TrackType::Subtitle:
        return QStringLiteral("Subtitle");
    case TrackType::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

}  // namespace revaplayer::domain
