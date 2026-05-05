#pragma once

#include "domain/PlaybackDiagnostics.hpp"
#include "domain/TrackInfo.hpp"

#include <QString>
#include <QVector>

namespace revaplayer::application {

[[nodiscard]] QString buildMediaInformationOverlayText(const revaplayer::domain::PlaybackDiagnostics &diagnostics,
                                            const QVector<revaplayer::domain::TrackInfo> &tracks,
                                            const QString &title,
                                            double positionSeconds,
                                            double durationSeconds,
                                            double speed);

[[nodiscard]] QString buildMediaInfoReport(const QString &title,
                                           const revaplayer::domain::PlaybackDiagnostics &diagnostics,
                                           const QVector<revaplayer::domain::TrackInfo> &tracks,
                                           double durationSeconds);

}  // namespace revaplayer::application
