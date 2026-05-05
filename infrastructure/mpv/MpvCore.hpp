#pragma once

#include "domain/ChapterInfo.hpp"
#include "domain/MediaItem.hpp"
#include "domain/PlaybackEndReason.hpp"
#include "domain/PlaybackDiagnostics.hpp"
#include "domain/TrackInfo.hpp"
#include "infrastructure/mpv/MpvPropertyCache.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>

#include <memory>

#include <mpv/client.h>

struct mpv_event;
struct mpv_event_property;
struct mpv_handle;

namespace revaplayer::infrastructure::mpv {

class MpvEventBridge;
class MpvRenderHost;

class MpvCore final : public QObject {
    Q_OBJECT

public:
    explicit MpvCore(QObject *parent = nullptr);
    ~MpvCore() override;

    void setUseExternalMpvConfig(bool enabled);
    bool initialize(revaplayer::infrastructure::mpv::MpvRenderHost *renderHost);
    [[nodiscard]] bool isInitialized() const;
    [[nodiscard]] bool idleActive() const;
    [[nodiscard]] bool paused() const;
    [[nodiscard]] double durationSeconds() const;
    [[nodiscard]] int volume() const;
    [[nodiscard]] bool muted() const;

    void loadFile(const QString &filePath, bool append = false);
    void loadFiles(const QStringList &filePaths);
    void loadUrl(const QString &url, bool append = false);
    void togglePause();
    void setPaused(bool paused);
    void stop();
    bool executeCommand(const QStringList &arguments);
    void loadSubtitleFile(const QString &filePath);
    bool captureScreenshot(const QString &filePath);
    void seekToSeconds(double seconds);
    void seekToFraction(double fraction);
    void seekBySeconds(int seconds);
    void setVolume(int volume);
    void adjustVolume(int delta);
    void setMuted(bool muted);
    void toggleMuted();
    void setSpeed(double speed);
    void adjustSpeed(double delta);
    void resetSpeed();
    void setSubtitleDelay(double seconds);
    void adjustSubtitleDelay(double deltaSeconds);
    void resetSubtitleDelay();
    void setSubtitleVisible(bool visible);
    void toggleSubtitleVisible();
    void setSubtitleScale(double scale);
    void adjustSubtitleScale(double delta);
    void resetSubtitleScale();
    void setSubtitlePosition(int position);
    void adjustSubtitlePosition(int delta);
    void resetSubtitlePosition();
    void setSubtitleFontFamily(const QString &fontFamily);
    void setSubtitleFontSize(int fontSize);
    void setSubtitleAssOverride(const QString &mode);
    void cycleSubtitleAssOverride();
    void setAudioDelay(double seconds);
    void adjustAudioDelay(double deltaSeconds);
    void resetAudioDelay();
    void setLoopStart(double seconds);
    void setLoopEnd(double seconds);
    void clearLoop();
    void frameStepForward();
    void frameStepBackward();
    void setRepeatMode(const QString &mode);
    void setVideoAspectOverride(const QString &aspectOverride);
    void resetVideoAspectOverride();
    void setVideoCrop(const QString &crop);
    void clearVideoCrop();
    void setVideoZoomFactor(double factor);
    void resetVideoZoom();
    void setVideoAlignment(double horizontal, double vertical);
    void resetVideoAlignment();
    void setVideoRotation(int degrees);
    void setDeinterlace(bool enabled);
    void nextPlaylistItem();
    void previousPlaylistItem();
    void selectPlaylistIndex(int index);
    void nextChapter();
    void previousChapter();
    void selectChapter(int index);
    void selectTrack(revaplayer::domain::TrackType type, int trackId);

signals:
    void initializedChanged(bool initialized);
    void loadStarted(const QString &displayTarget);
    void clientMessageReceived(const QStringList &arguments);
    void idleChanged(bool idleActive);
    void pausedChanged(bool paused);
    void playbackPositionChanged(double positionSeconds, double durationSeconds);
    void volumeChanged(int volume);
    void mutedChanged(bool muted);
    void speedChanged(double speed);
    void subtitleDelayChanged(double delaySeconds);
    void subtitleVisibilityChanged(bool visible);
    void subtitleScaleChanged(double scale);
    void subtitlePositionChanged(int position);
    void subtitleFontFamilyChanged(const QString &fontFamily);
    void subtitleFontSizeChanged(int fontSize);
    void subtitleAssOverrideChanged(const QString &mode);
    void audioDelayChanged(double delaySeconds);
    void titleChanged(const QString &title);
    void diagnosticsChanged(const revaplayer::domain::PlaybackDiagnostics &diagnostics);
    void playlistChanged(const QVector<revaplayer::domain::PlaylistEntry> &entries, int currentIndex);
    void chaptersChanged(const QVector<revaplayer::domain::ChapterInfo> &chapters, int currentIndex);
    void tracksChanged(const QVector<revaplayer::domain::TrackInfo> &tracks);
    void fileLoaded();
    void fileEnded(revaplayer::domain::PlaybackEndReason reason, const QString &message);
    void errorOccurred(const QString &message);

private:
    bool sendLoadCommand(const QString &target, bool append, bool announce);
    void createHandle();
    void registerObservers() const;
    void processPendingEvents();
    void handleEvent(mpv_event *event);
    void handlePropertyChange(const mpv_event_property &property);
    void emitDiagnosticsChanged(bool force = false);
    void emitStateSnapshot();
    bool setProperty(const char *name, mpv_format format, void *value);
    bool setCommandProperty(const QString &name, const QString &value);
    void emitMpvError(const QString &context, int errorCode);
    void setStringOption(const char *name, const char *value) const;
    void setStringOption(const char *name, const QByteArray &value) const;
    bool sendCommand(const QStringList &arguments);
    bool ensureReady();

    mpv_handle *handle_ {nullptr};
    bool initialized_ {false};
    bool useExternalMpvConfig_ {false};
    MpvPropertyCache propertyCache_;
    std::unique_ptr<MpvEventBridge> eventBridge_;
    QElapsedTimer diagnosticsEmitElapsed_;
};

}  // namespace revaplayer::infrastructure::mpv
