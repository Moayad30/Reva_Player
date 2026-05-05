#include "application/PlaybackTuning.hpp"

#include <algorithm>
#include <cmath>

namespace revaplayer::application {

double clampPlaybackSpeed(const double speed)
{
    return std::clamp(speed, 0.25, 4.0);
}

int clampPlaybackVolume(const int volume)
{
    return std::clamp(volume, 0, kMaximumPlaybackVolume);
}

QString formatPlaybackRate(const double speed)
{
    QString text = QString::number(clampPlaybackSpeed(speed), 'f', 2);
    while (text.contains(QStringLiteral(".")) && (text.endsWith(QStringLiteral("0")) || text.endsWith(QStringLiteral(".")))) {
        text.chop(1);
    }

    return QStringLiteral("%1x").arg(text);
}

double clampVideoZoomFactor(const double factor)
{
    return std::clamp(factor, 1.0, 12.0);
}

double clampVideoViewportAlignment(const double alignment)
{
    return std::clamp(alignment, -1.0, 1.0);
}

double videoZoomFactorToLog2(const double factor)
{
    return std::log2(clampVideoZoomFactor(factor));
}

QString formatScaleFactor(const double factor)
{
    QString text = QString::number(clampVideoZoomFactor(factor), 'f', 2);
    while (text.contains(QStringLiteral(".")) && (text.endsWith(QStringLiteral("0")) || text.endsWith(QStringLiteral(".")))) {
        text.chop(1);
    }

    return QStringLiteral("%1x").arg(text);
}

QString formatSignedSeconds(const double seconds)
{
    return QStringLiteral("%1%2s")
        .arg(seconds >= 0.0 ? QStringLiteral("+") : QStringLiteral(""))
        .arg(QString::number(seconds, 'f', 2));
}

int normalizeRightAngleRotation(const int degrees)
{
    const int normalized = ((degrees % 360) + 360) % 360;
    return normalized - (normalized % 90);
}

}  // namespace revaplayer::application
