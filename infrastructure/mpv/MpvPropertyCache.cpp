#include "infrastructure/mpv/MpvPropertyCache.hpp"

#include <utility>

namespace revaplayer::infrastructure::mpv {

bool MpvPropertyCache::idleActive() const
{
    return idleActive_;
}

void MpvPropertyCache::setIdleActive(const bool idleActive)
{
    idleActive_ = idleActive;
}

bool MpvPropertyCache::paused() const
{
    return paused_;
}

void MpvPropertyCache::setPaused(const bool paused)
{
    paused_ = paused;
}

double MpvPropertyCache::timeSeconds() const
{
    return timeSeconds_;
}

void MpvPropertyCache::setTimeSeconds(const double timeSeconds)
{
    timeSeconds_ = timeSeconds;
}

double MpvPropertyCache::durationSeconds() const
{
    return durationSeconds_;
}

void MpvPropertyCache::setDurationSeconds(const double durationSeconds)
{
    durationSeconds_ = durationSeconds;
}

int MpvPropertyCache::volume() const
{
    return volume_;
}

void MpvPropertyCache::setVolume(const int volume)
{
    volume_ = volume;
}

bool MpvPropertyCache::muted() const
{
    return muted_;
}

void MpvPropertyCache::setMuted(const bool muted)
{
    muted_ = muted;
}

double MpvPropertyCache::speed() const
{
    return speed_;
}

void MpvPropertyCache::setSpeed(const double speed)
{
    speed_ = speed;
}

double MpvPropertyCache::subtitleDelay() const
{
    return subtitleDelay_;
}

void MpvPropertyCache::setSubtitleDelay(const double subtitleDelay)
{
    subtitleDelay_ = subtitleDelay;
}

bool MpvPropertyCache::subtitleVisible() const
{
    return subtitleVisible_;
}

void MpvPropertyCache::setSubtitleVisible(const bool subtitleVisible)
{
    subtitleVisible_ = subtitleVisible;
}

double MpvPropertyCache::subtitleScale() const
{
    return subtitleScale_;
}

void MpvPropertyCache::setSubtitleScale(const double subtitleScale)
{
    subtitleScale_ = subtitleScale;
}

int MpvPropertyCache::subtitlePosition() const
{
    return subtitlePosition_;
}

void MpvPropertyCache::setSubtitlePosition(const int subtitlePosition)
{
    subtitlePosition_ = subtitlePosition;
}

const QString &MpvPropertyCache::subtitleFontFamily() const
{
    return subtitleFontFamily_;
}

void MpvPropertyCache::setSubtitleFontFamily(QString subtitleFontFamily)
{
    subtitleFontFamily_ = std::move(subtitleFontFamily);
}

int MpvPropertyCache::subtitleFontSize() const
{
    return subtitleFontSize_;
}

void MpvPropertyCache::setSubtitleFontSize(const int subtitleFontSize)
{
    subtitleFontSize_ = subtitleFontSize;
}

const QString &MpvPropertyCache::subtitleAssOverride() const
{
    return subtitleAssOverride_;
}

void MpvPropertyCache::setSubtitleAssOverride(QString subtitleAssOverride)
{
    subtitleAssOverride_ = std::move(subtitleAssOverride);
}

double MpvPropertyCache::audioDelay() const
{
    return audioDelay_;
}

void MpvPropertyCache::setAudioDelay(const double audioDelay)
{
    audioDelay_ = audioDelay;
}

const QString &MpvPropertyCache::mediaTitle() const
{
    return mediaTitle_;
}

void MpvPropertyCache::setMediaTitle(QString mediaTitle)
{
    mediaTitle_ = std::move(mediaTitle);
}

const QVector<revaplayer::domain::PlaylistEntry> &MpvPropertyCache::playlist() const
{
    return playlist_;
}

void MpvPropertyCache::setPlaylist(QVector<revaplayer::domain::PlaylistEntry> playlist)
{
    playlist_ = std::move(playlist);
}

int MpvPropertyCache::currentPlaylistIndex() const
{
    return currentPlaylistIndex_;
}

void MpvPropertyCache::setCurrentPlaylistIndex(const int currentPlaylistIndex)
{
    currentPlaylistIndex_ = currentPlaylistIndex;
}

const QVector<revaplayer::domain::ChapterInfo> &MpvPropertyCache::chapters() const
{
    return chapters_;
}

void MpvPropertyCache::setChapters(QVector<revaplayer::domain::ChapterInfo> chapters)
{
    chapters_ = std::move(chapters);
}

int MpvPropertyCache::currentChapterIndex() const
{
    return currentChapterIndex_;
}

void MpvPropertyCache::setCurrentChapterIndex(const int currentChapterIndex)
{
    currentChapterIndex_ = currentChapterIndex;
}

const QVector<revaplayer::domain::TrackInfo> &MpvPropertyCache::tracks() const
{
    return tracks_;
}

void MpvPropertyCache::setTracks(QVector<revaplayer::domain::TrackInfo> tracks)
{
    tracks_ = std::move(tracks);
}

const revaplayer::domain::PlaybackDiagnostics &MpvPropertyCache::diagnostics() const
{
    return diagnostics_;
}

void MpvPropertyCache::setDiagnostics(revaplayer::domain::PlaybackDiagnostics diagnostics)
{
    diagnostics_ = std::move(diagnostics);
}

}  // namespace revaplayer::infrastructure::mpv
