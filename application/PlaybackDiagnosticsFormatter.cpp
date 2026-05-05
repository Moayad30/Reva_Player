#include "application/PlaybackDiagnosticsFormatter.hpp"

#include "application/PlaybackTuning.hpp"
#include "application/UiLanguage.hpp"

#include <QStringList>
#include <QUrl>
#include <QtGlobal>

#include <algorithm>

namespace revaplayer::application {
namespace {

QString uiText(const char *sourceText)
{
    return revaplayer::application::translateUiText(QString::fromUtf8(sourceText));
}

QString fallbackText(const QString &value, const QString &fallback = uiText("Unknown"))
{
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() ? fallback : trimmed;
}

QString formatResolution(const int width, const int height)
{
    if (width <= 0 || height <= 0) {
        return uiText("Unknown");
    }

    return QStringLiteral("%1x%2").arg(width).arg(height);
}

QString formatDuration(const double durationSeconds)
{
    if (durationSeconds <= 0.0) {
        return uiText("Unknown");
    }

    const int totalSeconds = qMax(0, static_cast<int>(qRound64(durationSeconds)));
    const int hours = totalSeconds / 3600;
    const int minutes = (totalSeconds % 3600) / 60;
    const int seconds = totalSeconds % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }

    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString formatBitrate(const double bitrate)
{
    if (bitrate <= 0.0) {
        return uiText("Unknown");
    }

    if (bitrate >= 1000000.0) {
        return QStringLiteral("%1 Mbps").arg(QString::number(bitrate / 1000000.0, 'f', 2));
    }

    return QStringLiteral("%1 kbps").arg(QString::number(bitrate / 1000.0, 'f', 0));
}

QString formatFps(const double fps)
{
    if (fps <= 0.0) {
        return uiText("Unknown");
    }

    return uiText("%1 fps").arg(QString::number(fps, 'f', 2));
}

QString trackSummaryLine(const revaplayer::domain::TrackInfo &track)
{
    QStringList parts;
    parts << QStringLiteral("%1 #%2").arg(revaplayer::domain::trackTypeLabel(track.type)).arg(track.id);

    if (!track.title.trimmed().isEmpty()) {
        parts << track.title.trimmed();
    }
    if (!track.language.trimmed().isEmpty()) {
        parts << track.language.trimmed().toUpper();
    }
    if (track.external) {
        parts << uiText("External");
    }
    if (track.selected) {
        parts << uiText("Selected");
    }

    return QStringLiteral("- %1").arg(parts.join(QStringLiteral(" | ")));
}

QString selectedTrackSummary(const QVector<revaplayer::domain::TrackInfo> &tracks,
                             const revaplayer::domain::TrackType type)
{
    const auto selectedIt = std::find_if(tracks.cbegin(), tracks.cend(), [type](const auto &track) {
        return track.type == type && track.selected;
    });
    if (selectedIt == tracks.cend()) {
        return type == revaplayer::domain::TrackType::Subtitle ? uiText("None") : uiText("Unknown");
    }

    QStringList parts;
    if (!selectedIt->title.trimmed().isEmpty()) {
        parts << selectedIt->title.trimmed();
    }
    if (!selectedIt->language.trimmed().isEmpty()) {
        parts << selectedIt->language.trimmed().toUpper();
    }
    if (!selectedIt->codec.trimmed().isEmpty()) {
        parts << selectedIt->codec.trimmed();
    }
    if (selectedIt->external) {
        parts << uiText("External");
    }
    if (parts.isEmpty()) {
        parts << QStringLiteral("#%1").arg(selectedIt->id);
    }
    return parts.join(QStringLiteral(" | "));
}

QString displaySourceName(const QString &title, const QString &source)
{
    const QString trimmedTitle = title.trimmed();
    if (!trimmedTitle.isEmpty()) {
        return trimmedTitle;
    }

    const QString trimmedSource = source.trimmed();
    QString pathLike = trimmedSource;
    const QUrl url(trimmedSource);
    if (url.isValid() && !url.scheme().isEmpty()) {
        pathLike = url.isLocalFile() ? url.toLocalFile() : url.path();
    }

    pathLike.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (pathLike.size() > 1 && pathLike.endsWith(QLatin1Char('/'))) {
        pathLike.chop(1);
    }

    const qsizetype slashIndex = pathLike.lastIndexOf(QLatin1Char('/'));
    const QString fileName = slashIndex >= 0 ? pathLike.mid(slashIndex + 1).trimmed() : pathLike.trimmed();
    if (!fileName.isEmpty()) {
        return fileName;
    }

    return fallbackText(source);
}

}  // namespace

QString buildMediaInformationOverlayText(const revaplayer::domain::PlaybackDiagnostics &diagnostics,
                              const QVector<revaplayer::domain::TrackInfo> &tracks,
                              const QString &title,
                              const double positionSeconds,
                              const double durationSeconds,
                              const double speed)
{
    QStringList lines;
    lines << uiText("Media Information");
    lines << QStringLiteral("%1: %2").arg(uiText("File"), displaySourceName(title, diagnostics.source));
    lines << QStringLiteral("%1: %2 / %3")
                 .arg(uiText("Time"), formatDuration(positionSeconds), formatDuration(durationSeconds));
    lines << QStringLiteral("%1: %2").arg(uiText("Speed"), formatPlaybackRate(speed));
    lines << QStringLiteral("%1: %2").arg(uiText("Container"), fallbackText(diagnostics.fileFormat));
    lines << QStringLiteral("%1: %2").arg(uiText("Resolution"), formatResolution(diagnostics.displayWidth, diagnostics.displayHeight));
    lines << QStringLiteral("%1: %2").arg(uiText("Video Codec"), fallbackText(diagnostics.videoCodec));
    lines << QStringLiteral("%1: %2").arg(uiText("Audio Codec"), fallbackText(diagnostics.audioCodec));
    lines << QStringLiteral("%1: %2 / %3")
                 .arg(uiText("FPS"), formatFps(diagnostics.containerFps), formatFps(diagnostics.estimatedVideoFps));
    lines << QStringLiteral("%1: %2 / %3")
                 .arg(uiText("Bitrate"), formatBitrate(diagnostics.videoBitrate), formatBitrate(diagnostics.audioBitrate));
    lines << QStringLiteral("%1: %2").arg(uiText("Subtitle"), selectedTrackSummary(tracks, revaplayer::domain::TrackType::Subtitle));

    if (!diagnostics.hardwareDecoding.trimmed().isEmpty()) {
        lines << QStringLiteral("%1: %2").arg(uiText("Hardware Decode"), diagnostics.hardwareDecoding.trimmed());
    }
    if (!diagnostics.currentVideoOutput.trimmed().isEmpty() || !diagnostics.currentAudioOutput.trimmed().isEmpty()) {
        lines << QStringLiteral("%1: %2 / %3")
                     .arg(uiText("Output"), fallbackText(diagnostics.currentVideoOutput), fallbackText(diagnostics.currentAudioOutput));
    }
    if (diagnostics.cacheDurationSeconds > 0.0) {
        lines << QStringLiteral("%1: %2 s")
                     .arg(uiText("Cache"), QString::number(diagnostics.cacheDurationSeconds, 'f', 1));
    }
    if (diagnostics.pausedForCache) {
        lines << uiText("Buffering");
    }

    return lines.join(QChar::fromLatin1('\n'));
}

QString buildMediaInfoReport(const QString &title,
                             const revaplayer::domain::PlaybackDiagnostics &diagnostics,
                             const QVector<revaplayer::domain::TrackInfo> &tracks,
                             const double durationSeconds)
{
    QStringList report;
    report << uiText("Title: %1").arg(fallbackText(title));
    report << uiText("Source: %1").arg(fallbackText(diagnostics.source));
    report << uiText("Format: %1").arg(fallbackText(diagnostics.fileFormat));
    report << uiText("Duration: %1").arg(formatDuration(durationSeconds));
    report << uiText("Encoded Resolution: %1").arg(formatResolution(diagnostics.encodedWidth, diagnostics.encodedHeight));
    report << uiText("Display Resolution: %1").arg(formatResolution(diagnostics.displayWidth, diagnostics.displayHeight));
    report << uiText("Video Codec: %1").arg(fallbackText(diagnostics.videoCodec));
    report << uiText("Audio Codec: %1").arg(fallbackText(diagnostics.audioCodec));
    report << uiText("Container FPS: %1").arg(formatFps(diagnostics.containerFps));
    report << uiText("Playback FPS: %1").arg(formatFps(diagnostics.estimatedVideoFps));
    report << uiText("Video Bitrate: %1").arg(formatBitrate(diagnostics.videoBitrate));
    report << uiText("Audio Bitrate: %1").arg(formatBitrate(diagnostics.audioBitrate));
    report << uiText("Video Output: %1").arg(fallbackText(diagnostics.currentVideoOutput));
    report << uiText("Audio Output: %1").arg(fallbackText(diagnostics.currentAudioOutput));
    report << uiText("Hardware Decode: %1").arg(fallbackText(diagnostics.hardwareDecoding, uiText("Disabled / Unknown")));
    report << uiText("Cache Duration: %1 s")
                 .arg(diagnostics.cacheDurationSeconds > 0.0
                          ? QString::number(diagnostics.cacheDurationSeconds, 'f', 2)
                          : QStringLiteral("0.00"));
    report << uiText("Cache Speed: %1x")
                 .arg(diagnostics.cacheSpeed > 0.0
                          ? QString::number(diagnostics.cacheSpeed, 'f', 2)
                          : QStringLiteral("0.00"));
    report << uiText("Paused For Cache: %1").arg(diagnostics.pausedForCache ? uiText("Yes") : uiText("No"));

    report << QString {};
    report << uiText("Tracks:");

    if (tracks.isEmpty()) {
        report << uiText("- No track information available");
    } else {
        for (const auto &track : tracks) {
            report << trackSummaryLine(track);
        }
    }

    return report.join(QChar::fromLatin1('\n'));
}

}  // namespace revaplayer::application
