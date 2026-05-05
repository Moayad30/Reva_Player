#pragma once

#include "domain/PlayerProfile.hpp"

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

class QTimer;

namespace revaplayer::services::media {

struct ThumbnailRequest final {
    QString source;
    double timeSeconds {0.0};
    qint64 bucketMilliseconds {0};
};

class ThumbnailService final : public QObject {
    Q_OBJECT

public:
    explicit ThumbnailService(QObject *parent = nullptr);
    ~ThumbnailService() override;

    static qint64 bucketMillisecondsFor(double timeSeconds);
    void setProfile(revaplayer::domain::PlayerProfile profile);
    [[nodiscard]] revaplayer::domain::PlayerProfile profile() const;
    [[nodiscard]] int recommendedDebounceIntervalMs() const;
    void setPreviewEnabled(bool enabled);
    [[nodiscard]] bool previewEnabled() const;
    void setPreviewWidthOverride(int width);
    [[nodiscard]] int previewWidthOverride() const;
    void setCommandDispatcher(std::function<bool(const QStringList &arguments)> dispatcher);
    void setCurrentSource(const QString &source);
    void handleMpvClientMessage(const QStringList &arguments);
    void requestThumbnail(const QString &source, double timeSeconds);
    [[nodiscard]] bool loadCachedThumbnail(const QString &source, double timeSeconds, QImage *image) const;
    void clearCache();
    void cancel();

signals:
    void thumbnailReady(const QString &source, qint64 bucketMilliseconds, const QImage &image);
    void thumbnailUnavailable(const QString &source, qint64 bucketMilliseconds, const QString &reason);

private:
    void pollThumbfastOutput();
    [[nodiscard]] int effectivePreviewWidth() const;
    [[nodiscard]] QString cacheKey(const QString &source, qint64 bucketMilliseconds) const;
    [[nodiscard]] QString diskCachePathFor(const ThumbnailRequest &request) const;
    [[nodiscard]] bool loadFromDiskCache(const ThumbnailRequest &request, QImage *image) const;
    [[nodiscard]] bool dispatchThumbfastCommand(const QStringList &arguments) const;
    [[nodiscard]] bool thumbfastReadyFor(const ThumbnailRequest &request, QString *reason = nullptr) const;
    [[nodiscard]] bool loadThumbfastImage(QImage *image) const;
    void startRequest(const ThumbnailRequest &request);
    void storeCacheEntry(const ThumbnailRequest &request, const QImage &image);
    void startPendingRequestIfAny();
    [[nodiscard]] bool shouldKeepWaitingForPendingRequest() const;
    void failActiveRequest(const QString &reason);
    void failPendingRequest(const QString &reason);

    QTimer *pollTimer_ {nullptr};
    std::optional<ThumbnailRequest> activeRequest_;
    std::optional<ThumbnailRequest> pendingRequest_;
    QHash<QString, QImage> cache_;
    QStringList cacheOrder_;
    QString cacheDirectoryPath_;
    QString currentSource_;
    QString thumbfastThumbnailBasePath_;
    QSize thumbfastFrameSize_;
    QDateTime thumbfastOutputTimestamp_;
    qint64 thumbfastOutputSize_ {-1};
    std::function<bool(const QStringList &arguments)> commandDispatcher_;
    revaplayer::domain::PlayerProfile profile_ {revaplayer::domain::PlayerProfile::Balanced};
    bool previewEnabled_ {true};
    bool thumbfastAvailable_ {false};
    bool thumbfastDisabled_ {true};
    bool thumbfastInfoReceived_ {false};
    int previewWidthOverride_ {0};
    QElapsedTimer activeRequestElapsed_;
    QElapsedTimer pendingRequestElapsed_;
    bool pendingRequestWaiting_ {false};
};

}  // namespace revaplayer::services::media
