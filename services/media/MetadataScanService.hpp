#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <memory>

struct mpv_event;
struct mpv_handle;

namespace revaplayer::infrastructure::mpv {
class MpvEventBridge;
}

namespace revaplayer::services::media {

struct MediaScanResult final {
    QString source;
    QString mediaTitle;
    QString artist;
    QString album;
    QString fileFormat;
    double durationSeconds {0.0};
    int width {0};
    int height {0};
    bool hasVideoTrack {false};
    bool hasAudioTrack {false};
    int subtitleTrackCount {0};
};

class MetadataScanService final : public QObject {
    Q_OBJECT

public:
    explicit MetadataScanService(QObject *parent = nullptr);
    ~MetadataScanService() override;

    bool initialize();
    [[nodiscard]] bool isInitialized() const;
    [[nodiscard]] bool isBusy() const;

    void enqueueSource(const QString &source);
    void enqueueSources(const QStringList &sources);
    void cancel();

signals:
    void metadataReady(const revaplayer::services::media::MediaScanResult &result);
    void metadataUnavailable(const QString &source, const QString &reason);
    void busyChanged(bool busy);

private:
    [[nodiscard]] static QString normalizeSource(const QString &source);
    [[nodiscard]] bool sendCommand(const QStringList &arguments);
    [[nodiscard]] MediaScanResult collectCurrentResult() const;
    void scheduleActiveResultCollection(int delayMs = 0);
    void finalizeActiveResultCollection(bool force = false);
    void setBusy(bool busy);
    void startNextScan();
    void processPendingEvents();
    void handleEvent(mpv_event *event);

    mpv_handle *handle_ {nullptr};
    std::unique_ptr<revaplayer::infrastructure::mpv::MpvEventBridge> eventBridge_;
    QStringList queue_;
    QSet<QString> queuedSources_;
    QString activeSource_;
    QElapsedTimer activeScanElapsed_;
    bool activeResultCollectionScheduled_ {false};
    bool initialized_ {false};
    bool busy_ {false};
};

}  // namespace revaplayer::services::media
