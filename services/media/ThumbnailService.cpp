#include "services/media/ThumbnailService.hpp"

#include "services/media/ThumbnailPolicy.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <utility>

namespace revaplayer::services::media {
namespace {

constexpr qint64 kPendingRequestTimeoutMs = 900;
constexpr qint64 kActiveRequestTimeoutMs = 1400;

QString normalizeSource(const QString &source)
{
    const QString trimmed = source.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QFileInfo fileInfo(trimmed);
    if (fileInfo.exists() && fileInfo.isFile()) {
        const QString canonical = fileInfo.canonicalFilePath();
        return canonical.isEmpty() ? fileInfo.absoluteFilePath() : canonical;
    }

    return trimmed;
}

QString unavailableReasonForPreview()
{
    return QStringLiteral("Preview is available for the currently loaded media only.");
}

QString previewRequestTimedOutReason()
{
    return QStringLiteral("Thumbnail preview timed out.");
}

}  // namespace

ThumbnailService::ThumbnailService(QObject *parent)
    : QObject(parent)
    , pollTimer_(new QTimer(this))
{
    pollTimer_->setInterval(thumbnailPolicyForProfile(profile_).pollIntervalMs);
    pollTimer_->setSingleShot(false);
    connect(pollTimer_, &QTimer::timeout, this, &ThumbnailService::pollThumbfastOutput);

    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (!cacheRoot.trimmed().isEmpty()) {
        cacheDirectoryPath_ = QDir(cacheRoot).filePath(QStringLiteral("thumbnails"));
        QDir().mkpath(cacheDirectoryPath_);
    }
}

ThumbnailService::~ThumbnailService()
{
    cancel();
}

qint64 ThumbnailService::bucketMillisecondsFor(const double timeSeconds)
{
    return static_cast<qint64>(std::llround(std::max(0.0, timeSeconds))) * 1000;
}

void ThumbnailService::setProfile(const revaplayer::domain::PlayerProfile profile)
{
    if (profile_ == profile) {
        return;
    }

    profile_ = profile;
    if (pollTimer_ != nullptr) {
        pollTimer_->setInterval(thumbnailPolicyForProfile(profile_).pollIntervalMs);
    }
    cache_.clear();
    cacheOrder_.clear();
    cancel();
}

revaplayer::domain::PlayerProfile ThumbnailService::profile() const
{
    return profile_;
}

int ThumbnailService::recommendedDebounceIntervalMs() const
{
    return thumbnailPolicyForProfile(profile_).debounceIntervalMs;
}

void ThumbnailService::setPreviewEnabled(const bool enabled)
{
    if (previewEnabled_ == enabled) {
        return;
    }

    previewEnabled_ = enabled;
    cache_.clear();
    cacheOrder_.clear();
    cancel();
}

bool ThumbnailService::previewEnabled() const
{
    return previewEnabled_;
}

void ThumbnailService::setPreviewWidthOverride(const int width)
{
    const int clampedWidth = std::clamp(width, 0, 640);
    if (previewWidthOverride_ == clampedWidth) {
        return;
    }

    previewWidthOverride_ = clampedWidth;
    cache_.clear();
    cacheOrder_.clear();
    cancel();
}

int ThumbnailService::previewWidthOverride() const
{
    return previewWidthOverride_;
}

void ThumbnailService::setCommandDispatcher(std::function<bool(const QStringList &arguments)> dispatcher)
{
    commandDispatcher_ = std::move(dispatcher);
    if (!activeRequest_.has_value()) {
        startPendingRequestIfAny();
    }
}

void ThumbnailService::setCurrentSource(const QString &source)
{
    const QString normalizedSource = normalizeSource(source);
    if (currentSource_ == normalizedSource) {
        return;
    }

    cancel();
    currentSource_ = normalizedSource;
    // Keep thumbfast runtime state when switching between non-empty media
    // sources so the first hover preview can work immediately after opening.
    // Fully reset only when clearing the active source (idle/no media).
    if (currentSource_.isEmpty()) {
        thumbfastAvailable_ = false;
        thumbfastDisabled_ = true;
        thumbfastInfoReceived_ = false;
        thumbfastThumbnailBasePath_.clear();
        thumbfastFrameSize_ = {};
    }
    thumbfastOutputTimestamp_ = {};
    thumbfastOutputSize_ = -1;
}

void ThumbnailService::handleMpvClientMessage(const QStringList &arguments)
{
    if (arguments.isEmpty() || arguments.first() != QStringLiteral("thumbfast-info") || arguments.size() < 2) {
        return;
    }

    const QJsonDocument payload = QJsonDocument::fromJson(arguments.at(1).toUtf8());
    if (!payload.isObject()) {
        return;
    }

    const QJsonObject object = payload.object();
    thumbfastInfoReceived_ = true;
    thumbfastAvailable_ = object.value(QStringLiteral("available")).toBool(false);
    thumbfastDisabled_ = object.value(QStringLiteral("disabled")).toBool(true);
    thumbfastThumbnailBasePath_ = object.value(QStringLiteral("thumbnail")).toString().trimmed();
    thumbfastFrameSize_ = QSize(
        object.value(QStringLiteral("width")).toInt(),
        object.value(QStringLiteral("height")).toInt());

    if (!thumbfastAvailable_ || thumbfastThumbnailBasePath_.isEmpty()) {
        thumbfastOutputTimestamp_ = {};
        thumbfastOutputSize_ = -1;
    }

    if (thumbfastDisabled_ && activeRequest_.has_value()) {
        const ThumbnailRequest failedRequest = *activeRequest_;
        activeRequest_.reset();
        if (pollTimer_ != nullptr) {
            pollTimer_->stop();
        }
        emit thumbnailUnavailable(
            failedRequest.source,
            failedRequest.bucketMilliseconds,
            QStringLiteral("Thumbfast preview is unavailable for this media."));
    }

    if (!activeRequest_.has_value()) {
        startPendingRequestIfAny();
    }
}

void ThumbnailService::requestThumbnail(const QString &source, const double timeSeconds)
{
    const qint64 bucketMilliseconds = bucketMillisecondsFor(timeSeconds);
    if (!previewEnabled_) {
        emit thumbnailUnavailable(
            source,
            bucketMilliseconds,
            QStringLiteral("Preview thumbnails are disabled in Preferences."));
        return;
    }

    const QString normalizedSource = normalizeSource(source);
    if (normalizedSource.isEmpty()) {
        emit thumbnailUnavailable(source, bucketMilliseconds, unavailableReasonForPreview());
        return;
    }

    const ThumbnailRequest request {
        normalizedSource,
        std::max(0.0, timeSeconds),
        bucketMilliseconds,
    };

    const QString key = cacheKey(request.source, request.bucketMilliseconds);
    if (cache_.contains(key)) {
        emit thumbnailReady(request.source, request.bucketMilliseconds, cache_.value(key));
        return;
    }

    QImage diskCachedImage;
    if (loadFromDiskCache(request, &diskCachedImage)) {
        storeCacheEntry(request, diskCachedImage);
        emit thumbnailReady(request.source, request.bucketMilliseconds, diskCachedImage);
        return;
    }

    if (activeRequest_.has_value()
        && activeRequest_->source == request.source
        && activeRequest_->bucketMilliseconds == request.bucketMilliseconds) {
        return;
    }

    if (pendingRequest_.has_value()
        && pendingRequest_->source == request.source
        && pendingRequest_->bucketMilliseconds == request.bucketMilliseconds) {
        return;
    }

    pendingRequest_ = request;
    startPendingRequestIfAny();
}

bool ThumbnailService::loadCachedThumbnail(const QString &source, const double timeSeconds, QImage *image) const
{
    if (image == nullptr) {
        return false;
    }

    const QString normalizedSource = normalizeSource(source);
    if (normalizedSource.isEmpty()) {
        return false;
    }

    const ThumbnailRequest request {
        normalizedSource,
        std::max(0.0, timeSeconds),
        bucketMillisecondsFor(timeSeconds),
    };

    const QString key = cacheKey(request.source, request.bucketMilliseconds);
    if (cache_.contains(key)) {
        *image = cache_.value(key);
        return !image->isNull();
    }

    return loadFromDiskCache(request, image);
}

void ThumbnailService::clearCache()
{
    cancel();
    cache_.clear();
    cacheOrder_.clear();
    if (!cacheDirectoryPath_.trimmed().isEmpty()) {
        QDir(cacheDirectoryPath_).removeRecursively();
        QDir().mkpath(cacheDirectoryPath_);
    }
}

void ThumbnailService::cancel()
{
    pendingRequest_.reset();
    activeRequest_.reset();
    pendingRequestWaiting_ = false;
    if (pollTimer_ != nullptr) {
        pollTimer_->stop();
    }
    static_cast<void>(dispatchThumbfastCommand({QStringLiteral("script-message"), QStringLiteral("clear")}));
}

void ThumbnailService::pollThumbfastOutput()
{
    if (activeRequest_.has_value()) {
        if (activeRequestElapsed_.isValid() && activeRequestElapsed_.elapsed() > kActiveRequestTimeoutMs) {
            failActiveRequest(previewRequestTimedOutReason());
            startPendingRequestIfAny();
            return;
        }

        QImage image;
        if (!loadThumbfastImage(&image)) {
            return;
        }

        const ThumbnailRequest completedRequest = *activeRequest_;
        activeRequest_.reset();
        if (pollTimer_ != nullptr) {
            pollTimer_->stop();
        }

        const QFileInfo outputInfo(thumbfastThumbnailBasePath_ + QStringLiteral(".bgra"));
        thumbfastOutputTimestamp_ = outputInfo.lastModified();
        thumbfastOutputSize_ = outputInfo.exists() ? outputInfo.size() : -1;

        storeCacheEntry(completedRequest, image);
        emit thumbnailReady(completedRequest.source, completedRequest.bucketMilliseconds, image);
        startPendingRequestIfAny();
        return;
    }

    if (pendingRequest_.has_value()) {
        startPendingRequestIfAny();
        return;
    }

    if (pollTimer_ != nullptr) {
        pollTimer_->stop();
    }
}

int ThumbnailService::effectivePreviewWidth() const
{
    return previewWidthOverride_ > 0
        ? previewWidthOverride_
        : thumbnailPolicyForProfile(profile_).previewWidth;
}

QString ThumbnailService::cacheKey(const QString &source, const qint64 bucketMilliseconds) const
{
    return QStringLiteral("%1|%2|%3")
        .arg(source)
        .arg(bucketMilliseconds)
        .arg(effectivePreviewWidth());
}

QString ThumbnailService::diskCachePathFor(const ThumbnailRequest &request) const
{
    if (cacheDirectoryPath_.trimmed().isEmpty()) {
        return {};
    }

    const QByteArray digest = QCryptographicHash::hash(
        QStringLiteral("%1|%2|%3")
            .arg(request.source)
            .arg(request.bucketMilliseconds)
            .arg(effectivePreviewWidth())
            .toUtf8(),
        QCryptographicHash::Sha1).toHex();
    return QDir(cacheDirectoryPath_).filePath(QString::fromLatin1(digest) + QStringLiteral(".jpg"));
}

bool ThumbnailService::loadFromDiskCache(const ThumbnailRequest &request, QImage *image) const
{
    if (image == nullptr) {
        return false;
    }

    const QString cachePath = diskCachePathFor(request);
    if (cachePath.isEmpty()) {
        return false;
    }

    const QFileInfo fileInfo(cachePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    return image->load(cachePath);
}

bool ThumbnailService::dispatchThumbfastCommand(const QStringList &arguments) const
{
    return commandDispatcher_ != nullptr && commandDispatcher_(arguments);
}

bool ThumbnailService::thumbfastReadyFor(const ThumbnailRequest &request, QString *reason) const
{
    const auto setReason = [reason](const QString &message) {
        if (reason != nullptr) {
            *reason = message;
        }
    };

    if (commandDispatcher_ == nullptr) {
        setReason(QStringLiteral("The mpv thumbnail bridge is not configured."));
        return false;
    }

    if (currentSource_.trimmed().isEmpty() || currentSource_ != request.source) {
        setReason(unavailableReasonForPreview());
        return false;
    }

    if (!thumbfastAvailable_) {
        setReason(QStringLiteral("Thumbfast is still preparing the preview worker."));
        return false;
    }

    if (thumbfastDisabled_) {
        setReason(QStringLiteral("Thumbfast preview is unavailable for this media."));
        return false;
    }

    if (thumbfastThumbnailBasePath_.trimmed().isEmpty() || thumbfastFrameSize_.isEmpty()) {
        setReason(QStringLiteral("Thumbfast did not publish a preview target yet."));
        return false;
    }

    setReason(QString {});
    return true;
}

bool ThumbnailService::loadThumbfastImage(QImage *image) const
{
    if (image == nullptr
        || !thumbfastAvailable_
        || thumbfastThumbnailBasePath_.trimmed().isEmpty()
        || thumbfastFrameSize_.isEmpty()) {
        return false;
    }

    const QString outputPath = thumbfastThumbnailBasePath_ + QStringLiteral(".bgra");
    const QFileInfo fileInfo(outputPath);
    if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.lastModified().isValid()) {
        return false;
    }

    if (fileInfo.lastModified() == thumbfastOutputTimestamp_ && fileInfo.size() == thumbfastOutputSize_) {
        return false;
    }

    const qint64 expectedBytes = static_cast<qint64>(thumbfastFrameSize_.width())
        * static_cast<qint64>(thumbfastFrameSize_.height()) * 4;
    if (expectedBytes <= 0 || fileInfo.size() != expectedBytes) {
        return false;
    }

    QFile file(outputPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray rawBytes = file.readAll();
    if (rawBytes.size() != expectedBytes) {
        return false;
    }

    QImage frame(
        reinterpret_cast<const uchar *>(rawBytes.constData()),
        thumbfastFrameSize_.width(),
        thumbfastFrameSize_.height(),
        thumbfastFrameSize_.width() * 4,
        QImage::Format_ARGB32);
    if (frame.isNull()) {
        return false;
    }

    QImage detached = frame.copy();
    const int previewWidth = effectivePreviewWidth();
    if (previewWidth > 0 && detached.width() > previewWidth) {
        detached = detached.scaledToWidth(previewWidth, Qt::FastTransformation);
    }

    if (detached.isNull()) {
        return false;
    }

    *image = detached;
    return true;
}

void ThumbnailService::startRequest(const ThumbnailRequest &request)
{
    activeRequest_ = request;
    activeRequestElapsed_.restart();
    pendingRequestWaiting_ = false;

    const QFileInfo outputInfo(thumbfastThumbnailBasePath_ + QStringLiteral(".bgra"));
    thumbfastOutputTimestamp_ = outputInfo.lastModified();
    thumbfastOutputSize_ = outputInfo.exists() ? outputInfo.size() : -1;

    const QString timeArgument = QString::number(request.bucketMilliseconds / 1000.0, 'f', 3);
    if (!dispatchThumbfastCommand({
            QStringLiteral("script-message"),
            QStringLiteral("thumb"),
            timeArgument,
            QStringLiteral(""),
            QStringLiteral(""),
        })) {
        failActiveRequest(QStringLiteral("The thumbfast request could not be sent to mpv."));
        startPendingRequestIfAny();
        return;
    }

    if (pollTimer_ != nullptr && !pollTimer_->isActive()) {
        pollTimer_->start();
    }
}

void ThumbnailService::storeCacheEntry(const ThumbnailRequest &request, const QImage &image)
{
    const QString key = cacheKey(request.source, request.bucketMilliseconds);
    cache_.insert(key, image);
    cacheOrder_.removeAll(key);
    cacheOrder_.push_back(key);

    const int memoryCacheEntries = std::max(8, thumbnailPolicyForProfile(profile_).memoryCacheEntries);
    while (cacheOrder_.size() > memoryCacheEntries) {
        const QString evictedKey = cacheOrder_.takeFirst();
        cache_.remove(evictedKey);
    }

    const QString cachePath = diskCachePathFor(request);
    if (!cachePath.isEmpty()) {
        QSaveFile file(cachePath);
        if (file.open(QIODevice::WriteOnly)) {
            image.save(&file, "JPEG", 82);
            file.commit();
        }
    }
}

void ThumbnailService::startPendingRequestIfAny()
{
    if (activeRequest_.has_value() || !pendingRequest_.has_value()) {
        return;
    }

    QString reason;
    if (thumbfastReadyFor(*pendingRequest_, &reason)) {
        const ThumbnailRequest request = *pendingRequest_;
        pendingRequest_.reset();
        pendingRequestWaiting_ = false;
        startRequest(request);
        return;
    }

    if (!shouldKeepWaitingForPendingRequest()) {
        failPendingRequest(reason);
        return;
    }

    if (!pendingRequestWaiting_) {
        pendingRequestElapsed_.restart();
        pendingRequestWaiting_ = true;
    } else if (pendingRequestElapsed_.isValid() && pendingRequestElapsed_.elapsed() > kPendingRequestTimeoutMs) {
        failPendingRequest(previewRequestTimedOutReason());
        return;
    }

    if (pollTimer_ != nullptr && !pollTimer_->isActive()) {
        pollTimer_->start();
    }
}

bool ThumbnailService::shouldKeepWaitingForPendingRequest() const
{
    return commandDispatcher_ != nullptr
        && (!thumbfastInfoReceived_ || !thumbfastDisabled_)
        && !currentSource_.trimmed().isEmpty()
        && pendingRequest_.has_value()
        && currentSource_ == pendingRequest_->source;
}

void ThumbnailService::failActiveRequest(const QString &reason)
{
    if (!activeRequest_.has_value()) {
        return;
    }

    const ThumbnailRequest failedRequest = *activeRequest_;
    activeRequest_.reset();
    if (pollTimer_ != nullptr) {
        pollTimer_->stop();
    }
    static_cast<void>(dispatchThumbfastCommand({QStringLiteral("script-message"), QStringLiteral("clear")}));
    emit thumbnailUnavailable(failedRequest.source, failedRequest.bucketMilliseconds, reason);
}

void ThumbnailService::failPendingRequest(const QString &reason)
{
    if (!pendingRequest_.has_value()) {
        return;
    }

    const ThumbnailRequest failedRequest = *pendingRequest_;
    pendingRequest_.reset();
    pendingRequestWaiting_ = false;
    if (!activeRequest_.has_value() && pollTimer_ != nullptr) {
        pollTimer_->stop();
    }
    emit thumbnailUnavailable(failedRequest.source, failedRequest.bucketMilliseconds, reason);
}

}  // namespace revaplayer::services::media
