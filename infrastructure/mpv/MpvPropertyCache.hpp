#pragma once

#include "domain/ChapterInfo.hpp"
#include "domain/MediaItem.hpp"
#include "domain/PlaybackDiagnostics.hpp"
#include "domain/TrackInfo.hpp"

#include <QVector>

namespace revaplayer::infrastructure::mpv {

class MpvPropertyCache final {
public:
    [[nodiscard]] bool idleActive() const;
    void setIdleActive(bool idleActive);

    [[nodiscard]] bool paused() const;
    void setPaused(bool paused);

    [[nodiscard]] double timeSeconds() const;
    void setTimeSeconds(double timeSeconds);

    [[nodiscard]] double durationSeconds() const;
    void setDurationSeconds(double durationSeconds);

    [[nodiscard]] int volume() const;
    void setVolume(int volume);
    [[nodiscard]] bool muted() const;
    void setMuted(bool muted);

    [[nodiscard]] double speed() const;
    void setSpeed(double speed);

    [[nodiscard]] double subtitleDelay() const;
    void setSubtitleDelay(double subtitleDelay);
    [[nodiscard]] bool subtitleVisible() const;
    void setSubtitleVisible(bool subtitleVisible);
    [[nodiscard]] double subtitleScale() const;
    void setSubtitleScale(double subtitleScale);
    [[nodiscard]] int subtitlePosition() const;
    void setSubtitlePosition(int subtitlePosition);
    [[nodiscard]] const QString &subtitleFontFamily() const;
    void setSubtitleFontFamily(QString subtitleFontFamily);
    [[nodiscard]] int subtitleFontSize() const;
    void setSubtitleFontSize(int subtitleFontSize);
    [[nodiscard]] const QString &subtitleAssOverride() const;
    void setSubtitleAssOverride(QString subtitleAssOverride);

    [[nodiscard]] double audioDelay() const;
    void setAudioDelay(double audioDelay);

    [[nodiscard]] const QString &mediaTitle() const;
    void setMediaTitle(QString mediaTitle);

    [[nodiscard]] const QVector<revaplayer::domain::PlaylistEntry> &playlist() const;
    void setPlaylist(QVector<revaplayer::domain::PlaylistEntry> playlist);

    [[nodiscard]] int currentPlaylistIndex() const;
    void setCurrentPlaylistIndex(int currentPlaylistIndex);

    [[nodiscard]] const QVector<revaplayer::domain::ChapterInfo> &chapters() const;
    void setChapters(QVector<revaplayer::domain::ChapterInfo> chapters);

    [[nodiscard]] int currentChapterIndex() const;
    void setCurrentChapterIndex(int currentChapterIndex);

    [[nodiscard]] const QVector<revaplayer::domain::TrackInfo> &tracks() const;
    void setTracks(QVector<revaplayer::domain::TrackInfo> tracks);

    [[nodiscard]] const revaplayer::domain::PlaybackDiagnostics &diagnostics() const;
    void setDiagnostics(revaplayer::domain::PlaybackDiagnostics diagnostics);

private:
    bool idleActive_ {true};
    bool paused_ {true};
    double timeSeconds_ {0.0};
    double durationSeconds_ {0.0};
    int volume_ {100};
    bool muted_ {false};
    double speed_ {1.0};
    double subtitleDelay_ {0.0};
    bool subtitleVisible_ {true};
    double subtitleScale_ {1.0};
    int subtitlePosition_ {100};
    QString subtitleFontFamily_ {QStringLiteral("sans-serif")};
    int subtitleFontSize_ {38};
    QString subtitleAssOverride_ {QStringLiteral("scale")};
    double audioDelay_ {0.0};
    QString mediaTitle_;
    QVector<revaplayer::domain::PlaylistEntry> playlist_;
    int currentPlaylistIndex_ {-1};
    QVector<revaplayer::domain::ChapterInfo> chapters_;
    int currentChapterIndex_ {-1};
    QVector<revaplayer::domain::TrackInfo> tracks_;
    revaplayer::domain::PlaybackDiagnostics diagnostics_;
};

}  // namespace revaplayer::infrastructure::mpv
