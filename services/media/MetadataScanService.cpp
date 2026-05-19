#include "services/media/MetadataScanService.hpp"

#include "infrastructure/mpv/MpvEventBridge.hpp"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <vector>

#include <mpv/client.h>

namespace revaplayer::services::media {
namespace {

constexpr auto kDurationProperty = "duration";
constexpr auto kWidthProperty = "width";
constexpr auto kHeightProperty = "height";
constexpr auto kFileFormatProperty = "file-format";
constexpr auto kMediaTitleProperty = "media-title";
constexpr auto kTrackListProperty = "track-list";
constexpr int kMetadataCollectionRetryDelayMs = 75;
constexpr int kMetadataCollectionTimeoutMs = 5000;
constexpr int kMetadataCollectionEndGraceMs = 350;

const mpv_node *findMapValue(const mpv_node &node, const char *key)
{
    if (node.format != MPV_FORMAT_NODE_MAP || node.u.list == nullptr || key == nullptr) {
        return nullptr;
    }

    const auto *list = node.u.list;
    for (int index = 0; index < list->num; ++index) {
        if (list->keys != nullptr && list->keys[index] != nullptr && std::strcmp(list->keys[index], key) == 0) {
            return &list->values[index];
        }
    }

    return nullptr;
}

QString nodeToQString(const mpv_node *node)
{
    if (node == nullptr) {
        return {};
    }

    switch (node->format) {
    case MPV_FORMAT_STRING:
        return node->u.string != nullptr ? QString::fromUtf8(node->u.string) : QString {};
    case MPV_FORMAT_INT64:
        return QString::number(node->u.int64);
    case MPV_FORMAT_DOUBLE:
        return QString::number(node->u.double_);
    case MPV_FORMAT_FLAG:
        return node->u.flag != 0 ? QStringLiteral("true") : QStringLiteral("false");
    default:
        return {};
    }
}

QString propertyString(mpv_handle *handle, const char *name)
{
    if (handle == nullptr || name == nullptr) {
        return {};
    }

    char *value = mpv_get_property_string(handle, name);
    if (value == nullptr) {
        return {};
    }

    const QString result = QString::fromUtf8(value);
    mpv_free(value);
    return result.trimmed();
}

QString firstNonEmptyProperty(mpv_handle *handle, const std::initializer_list<const char *> names)
{
    for (const char *name : names) {
        const QString value = propertyString(handle, name);
        if (!value.trimmed().isEmpty()) {
            return value.trimmed();
        }
    }

    return {};
}

double propertyDouble(mpv_handle *handle, const char *name)
{
    if (handle == nullptr || name == nullptr) {
        return 0.0;
    }

    double value = 0.0;
    if (mpv_get_property(handle, name, MPV_FORMAT_DOUBLE, &value) < 0 || !std::isfinite(value)) {
        return 0.0;
    }

    return std::max(0.0, value);
}

int propertyInt(mpv_handle *handle, const char *name)
{
    if (handle == nullptr || name == nullptr) {
        return 0;
    }

    int64_t value = 0;
    if (mpv_get_property(handle, name, MPV_FORMAT_INT64, &value) < 0) {
        return 0;
    }

    return std::max<int64_t>(0, value);
}

QString describeEndFileError(const mpv_event_end_file *endFile)
{
    if (endFile == nullptr) {
        return QStringLiteral("Metadata scan failed.");
    }

    if (endFile->reason == MPV_END_FILE_REASON_ERROR) {
        return QStringLiteral("Metadata scan failed: %1")
            .arg(QString::fromUtf8(mpv_error_string(endFile->error)));
    }

    if (endFile->reason == MPV_END_FILE_REASON_STOP) {
        return QStringLiteral("Metadata scan stopped.");
    }

    return QStringLiteral("Metadata scan ended before file metadata became available.");
}

bool mediaScanResultHasUsefulMetadata(const MediaScanResult &result)
{
    return result.durationSeconds > 0.0
        || result.width > 0
        || result.height > 0
        || result.hasVideoTrack
        || result.hasAudioTrack
        || result.subtitleTrackCount > 0
        || !result.fileFormat.trimmed().isEmpty();
}

void setStringOption(mpv_handle *handle, const char *name, const char *value, const bool ignoreMissing = false)
{
    if (handle == nullptr || name == nullptr || value == nullptr) {
        return;
    }

    const int result = mpv_set_option_string(handle, name, value);
    if (ignoreMissing && result == MPV_ERROR_OPTION_NOT_FOUND) {
        return;
    }
    if (result < 0) {
        qWarning("mpv option %s failed: %s", name, mpv_error_string(result));
    }
}

}  // namespace

MetadataScanService::MetadataScanService(QObject *parent)
    : QObject(parent)
{
}

MetadataScanService::~MetadataScanService()
{
    cancel();
    eventBridge_.reset();
    if (handle_ != nullptr) {
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
    }
}

bool MetadataScanService::initialize()
{
    if (initialized_) {
        return true;
    }

    handle_ = mpv_create();
    if (handle_ == nullptr) {
        return false;
    }

    setStringOption(handle_, "terminal", "no");
    setStringOption(handle_, "config", "no");
    setStringOption(handle_, "load-scripts", "no");
    setStringOption(handle_, "osc", "no");
    setStringOption(handle_, "input-default-bindings", "no");
    setStringOption(handle_, "input-terminal", "no");
    setStringOption(handle_, "input-vo-keyboard", "no");
    setStringOption(handle_, "input-media-keys", "no");
    setStringOption(handle_, "media-controls", "no", true);
    setStringOption(handle_, "keep-open", "no");
    setStringOption(handle_, "idle", "yes");
    setStringOption(handle_, "pause", "yes");
    setStringOption(handle_, "cache", "no");
    setStringOption(handle_, "prefetch-playlist", "no");
    setStringOption(handle_, "vo", "null");
    setStringOption(handle_, "ao", "null");
    setStringOption(handle_, "hwdec", "no");

    const int result = mpv_initialize(handle_);
    if (result < 0) {
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
        return false;
    }

    eventBridge_ = std::make_unique<revaplayer::infrastructure::mpv::MpvEventBridge>(handle_, this);
    connect(eventBridge_.get(), &revaplayer::infrastructure::mpv::MpvEventBridge::wakeup,
            this, &MetadataScanService::processPendingEvents);

    initialized_ = true;
    return true;
}

bool MetadataScanService::isInitialized() const
{
    return initialized_;
}

bool MetadataScanService::isBusy() const
{
    return busy_;
}

void MetadataScanService::enqueueSource(const QString &source)
{
    enqueueSources(QStringList {source});
}

void MetadataScanService::enqueueSources(const QStringList &sources)
{
    if (!initialize()) {
        for (const QString &source : sources) {
            const QString normalized = normalizeSource(source);
            if (!normalized.isEmpty()) {
                emit metadataUnavailable(normalized, QStringLiteral("Metadata scanner could not initialize mpv."));
            }
        }
        return;
    }

    for (const QString &source : sources) {
        const QString normalized = normalizeSource(source);
        if (normalized.isEmpty() || queuedSources_.contains(normalized)) {
            continue;
        }

        queue_.push_back(normalized);
        queuedSources_.insert(normalized);
    }

    startNextScan();
}

void MetadataScanService::cancel()
{
    queue_.clear();
    queuedSources_.clear();
    activeSource_.clear();
    activeResultCollectionScheduled_ = false;
    activeScanElapsed_.invalidate();
    if (initialized_) {
        static_cast<void>(sendCommand({QStringLiteral("stop")}));
    }
    setBusy(false);
}

QString MetadataScanService::normalizeSource(const QString &source)
{
    const QString trimmed = source.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QUrl url(trimmed);
    if (url.isValid() && url.isLocalFile()) {
        const QFileInfo fileInfo(url.toLocalFile());
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            return {};
        }
        const QString canonical = fileInfo.canonicalFilePath();
        return canonical.isEmpty() ? fileInfo.absoluteFilePath() : canonical;
    }

    const QFileInfo fileInfo(trimmed);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return {};
    }

    const QString canonical = fileInfo.canonicalFilePath();
    return canonical.isEmpty() ? fileInfo.absoluteFilePath() : canonical;
}

bool MetadataScanService::sendCommand(const QStringList &arguments)
{
    if (!initialized_ || handle_ == nullptr || arguments.isEmpty()) {
        return false;
    }

    QList<QByteArray> utf8Arguments;
    utf8Arguments.reserve(arguments.size());
    std::vector<const char *> rawArguments;
    rawArguments.reserve(static_cast<size_t>(arguments.size()) + 1);

    for (const QString &argument : arguments) {
        utf8Arguments.push_back(argument.toUtf8());
    }
    for (const QByteArray &argument : utf8Arguments) {
        rawArguments.push_back(argument.constData());
    }
    rawArguments.push_back(nullptr);

    return mpv_command(handle_, rawArguments.data()) >= 0;
}

MediaScanResult MetadataScanService::collectCurrentResult() const
{
    MediaScanResult result;
    result.source = activeSource_;
    result.durationSeconds = propertyDouble(handle_, kDurationProperty);
    result.width = propertyInt(handle_, kWidthProperty);
    result.height = propertyInt(handle_, kHeightProperty);
    result.fileFormat = propertyString(handle_, kFileFormatProperty);
    result.mediaTitle = firstNonEmptyProperty(handle_, {
        "metadata/by-key/title",
        "metadata/by-key/TITLE",
        kMediaTitleProperty,
    });
    result.artist = firstNonEmptyProperty(handle_, {
        "metadata/by-key/artist",
        "metadata/by-key/ARTIST",
        "metadata/by-key/album_artist",
        "metadata/by-key/ALBUM_ARTIST",
        "metadata/by-key/albumartist",
        "metadata/by-key/ALBUMARTIST",
        "metadata/by-key/performer",
        "metadata/by-key/PERFORMER",
    });
    result.album = firstNonEmptyProperty(handle_, {
        "metadata/by-key/album",
        "metadata/by-key/ALBUM",
    });

    mpv_node trackNode {};
    if (handle_ != nullptr && mpv_get_property(handle_, kTrackListProperty, MPV_FORMAT_NODE, &trackNode) >= 0) {
        if (trackNode.format == MPV_FORMAT_NODE_ARRAY && trackNode.u.list != nullptr) {
            const auto *list = trackNode.u.list;
            for (int index = 0; index < list->num; ++index) {
                const mpv_node &entryNode = list->values[index];
                const QString type = nodeToQString(findMapValue(entryNode, "type")).trimmed().toLower();
                if (type == QStringLiteral("video")) {
                    result.hasVideoTrack = true;
                } else if (type == QStringLiteral("audio")) {
                    result.hasAudioTrack = true;
                } else if (type == QStringLiteral("sub")) {
                    ++result.subtitleTrackCount;
                }
            }
        }
        mpv_free_node_contents(&trackNode);
    }

    if (result.mediaTitle.trimmed().isEmpty()) {
        const QFileInfo fileInfo(result.source);
        result.mediaTitle = fileInfo.completeBaseName().trimmed().isEmpty() ? fileInfo.fileName() : fileInfo.completeBaseName();
    }

    return result;
}

void MetadataScanService::setBusy(const bool busy)
{
    if (busy_ == busy) {
        return;
    }

    busy_ = busy;
    emit busyChanged(busy_);
}

void MetadataScanService::startNextScan()
{
    if (!initialized_ || !activeSource_.isEmpty()) {
        return;
    }

    while (!queue_.isEmpty()) {
        const QString source = queue_.takeFirst();
        if (source.trimmed().isEmpty()) {
            continue;
        }

        activeSource_ = source;
        activeResultCollectionScheduled_ = false;
        activeScanElapsed_.start();
        setBusy(true);
        if (!sendCommand({QStringLiteral("loadfile"), source, QStringLiteral("replace")})) {
            const QString failedSource = activeSource_;
            activeSource_.clear();
            activeResultCollectionScheduled_ = false;
            activeScanElapsed_.invalidate();
            queuedSources_.remove(failedSource);
            setBusy(false);
            emit metadataUnavailable(failedSource, QStringLiteral("Failed to start the metadata scan for this file."));
            continue;
        }
        return;
    }

    setBusy(false);
}

void MetadataScanService::scheduleActiveResultCollection(const int delayMs)
{
    if (activeSource_.isEmpty() || activeResultCollectionScheduled_) {
        return;
    }

    activeResultCollectionScheduled_ = true;
    QTimer::singleShot(std::max(0, delayMs), this, [this]() {
        activeResultCollectionScheduled_ = false;
        finalizeActiveResultCollection(false);
    });
}

void MetadataScanService::finalizeActiveResultCollection(const bool force)
{
    if (activeSource_.isEmpty()) {
        return;
    }

    const MediaScanResult result = collectCurrentResult();
    const bool ready = mediaScanResultHasUsefulMetadata(result);
    const bool timedOut = !activeScanElapsed_.isValid()
        || activeScanElapsed_.elapsed() >= kMetadataCollectionTimeoutMs;
    const bool allowPostEndGraceRetry = force
        && activeScanElapsed_.isValid()
        && activeScanElapsed_.elapsed() < (kMetadataCollectionTimeoutMs + kMetadataCollectionEndGraceMs);
    if (!force && !ready && !timedOut) {
        scheduleActiveResultCollection(kMetadataCollectionRetryDelayMs);
        return;
    }
    if (force && !ready && allowPostEndGraceRetry) {
        scheduleActiveResultCollection(kMetadataCollectionRetryDelayMs);
        return;
    }

    if (!ready) {
        const QString failedSource = activeSource_;
        activeSource_.clear();
        activeResultCollectionScheduled_ = false;
        activeScanElapsed_.invalidate();
        queuedSources_.remove(failedSource);
        setBusy(false);
        emit metadataUnavailable(failedSource, QStringLiteral("Metadata scan ended before file metadata became available."));
        QMetaObject::invokeMethod(this, [this]() {
            startNextScan();
        }, Qt::QueuedConnection);
        return;
    }

    const QString completedSource = activeSource_;
    activeSource_.clear();
    activeResultCollectionScheduled_ = false;
    activeScanElapsed_.invalidate();
    queuedSources_.remove(completedSource);
    setBusy(false);
    emit metadataReady(result);
    static_cast<void>(sendCommand({QStringLiteral("stop")}));
    QMetaObject::invokeMethod(this, [this]() {
        startNextScan();
    }, Qt::QueuedConnection);
}

void MetadataScanService::processPendingEvents()
{
    if (!initialized_ || handle_ == nullptr) {
        return;
    }

    while (true) {
        mpv_event *event = mpv_wait_event(handle_, 0.0);
        if (event == nullptr || event->event_id == MPV_EVENT_NONE) {
            break;
        }

        handleEvent(event);
    }
}

void MetadataScanService::handleEvent(mpv_event *event)
{
    if (event == nullptr) {
        return;
    }

    switch (event->event_id) {
    case MPV_EVENT_FILE_LOADED:
        scheduleActiveResultCollection(0);
        break;
    case MPV_EVENT_END_FILE: {
        if (activeSource_.isEmpty()) {
            break;
        }

        const auto *endFile = static_cast<mpv_event_end_file *>(event->data);
        if (endFile != nullptr && endFile->reason != MPV_END_FILE_REASON_ERROR) {
            finalizeActiveResultCollection(true);
            break;
        }

        const QString failedSource = activeSource_;
        activeSource_.clear();
        activeResultCollectionScheduled_ = false;
        activeScanElapsed_.invalidate();
        queuedSources_.remove(failedSource);
        setBusy(false);
        emit metadataUnavailable(
            failedSource,
            describeEndFileError(endFile));
        QMetaObject::invokeMethod(this, [this]() {
            startNextScan();
        }, Qt::QueuedConnection);
        break;
    }
    default:
        break;
    }
}

}  // namespace revaplayer::services::media
