#pragma once

#include <QString>

#include <QMetaType>
#include <QtGlobal>

namespace revaplayer::domain {

struct PlaybackDiagnostics {
    QString source;
    QString fileFormat;
    QString currentVideoOutput;
    QString currentAudioOutput;
    QString hardwareDecoding;
    QString videoCodec;
    QString audioCodec;
    int encodedWidth {-1};
    int encodedHeight {-1};
    int displayWidth {-1};
    int displayHeight {-1};
    double containerFps {0.0};
    double estimatedVideoFps {0.0};
    double videoBitrate {0.0};
    double audioBitrate {0.0};
    double cacheSpeed {0.0};
    double cacheDurationSeconds {0.0};
    bool pausedForCache {false};
};

}  // namespace revaplayer::domain

Q_DECLARE_METATYPE(revaplayer::domain::PlaybackDiagnostics)
