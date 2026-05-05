#include "application/PlaybackTuning.hpp"
#include "infrastructure/mpv/MpvCore.hpp"

#include "application/SubtitleStyleOptions.hpp"
#include "infrastructure/mpv/MpvEventBridge.hpp"
#include "infrastructure/mpv/MpvRenderHost.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>
#include <utility>

#include <mpv/client.h>

namespace revaplayer::infrastructure::mpv {
namespace {

constexpr auto kPauseProperty = "pause";
constexpr auto kIdleActiveProperty = "idle-active";
constexpr auto kTimePosProperty = "time-pos";
constexpr auto kDurationProperty = "duration";
constexpr auto kVolumeProperty = "volume";
constexpr auto kMuteProperty = "mute";
constexpr auto kSpeedProperty = "speed";
constexpr auto kSubtitleDelayProperty = "sub-delay";
constexpr auto kSubtitleVisibilityProperty = "sub-visibility";
constexpr auto kSubtitleScaleProperty = "sub-scale";
constexpr auto kSubtitlePositionProperty = "sub-pos";
constexpr auto kSubtitleFontProperty = "sub-font";
constexpr auto kSubtitleFontSizeProperty = "sub-font-size";
constexpr auto kSubtitleAssOverrideProperty = "sub-ass-override";
constexpr auto kAudioDelayProperty = "audio-delay";
constexpr auto kPathProperty = "path";
constexpr auto kFileFormatProperty = "file-format";
constexpr auto kCurrentVoProperty = "current-vo";
constexpr auto kCurrentAoProperty = "current-ao";
constexpr auto kHwdecCurrentProperty = "hwdec-current";
constexpr auto kVideoCodecProperty = "video-codec";
constexpr auto kAudioCodecProperty = "audio-codec-name";
constexpr auto kWidthProperty = "width";
constexpr auto kHeightProperty = "height";
constexpr auto kDisplayWidthProperty = "dwidth";
constexpr auto kDisplayHeightProperty = "dheight";
constexpr auto kContainerFpsProperty = "container-fps";
constexpr auto kEstimatedVfFpsProperty = "estimated-vf-fps";
constexpr auto kVideoBitrateProperty = "video-bitrate";
constexpr auto kAudioBitrateProperty = "audio-bitrate";
constexpr auto kCacheSpeedProperty = "cache-speed";
constexpr auto kDemuxerCacheDurationProperty = "demuxer-cache-duration";
constexpr auto kPausedForCacheProperty = "paused-for-cache";
constexpr auto kMediaTitleProperty = "media-title";
constexpr auto kPlaylistProperty = "playlist";
constexpr auto kPlaylistPositionProperty = "playlist-pos";
constexpr auto kChapterListProperty = "chapter-list";
constexpr auto kChapterProperty = "chapter";
constexpr auto kTrackListProperty = "track-list";
constexpr int kDiagnosticsRefreshIntervalMs = 1000;

QString extractBundledThumbfastScript()
{
    QFile resourceFile(QStringLiteral(":/mpv/thumbfast.lua"));
    if (!resourceFile.exists()) {
        return {};
    }

    const QString appDataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataDirectory.trimmed().isEmpty()) {
        return {};
    }

    const QString scriptsDirectory = QDir(appDataDirectory).filePath(QStringLiteral("mpv/scripts"));
    QDir().mkpath(scriptsDirectory);
    const QString targetPath = QDir(scriptsDirectory).filePath(QStringLiteral("thumbfast.lua"));

    if (!resourceFile.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray resourcePayload = resourceFile.readAll();
    resourceFile.close();

    bool needsWrite = true;
    QFile existingFile(targetPath);
    if (existingFile.open(QIODevice::ReadOnly)) {
        needsWrite = existingFile.readAll() != resourcePayload;
        existingFile.close();
    }

    if (needsWrite) {
        QSaveFile targetFile(targetPath);
        if (!targetFile.open(QIODevice::WriteOnly)) {
            return {};
        }
        targetFile.write(resourcePayload);
        if (!targetFile.commit()) {
            return {};
        }
    }

    return targetPath;
}

QString findBundledThumbfastScriptPath()
{
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    const QStringList candidates {
        QDir(applicationDirectory).filePath(QStringLiteral("scripts/thumbfast.lua")),
        QDir(applicationDirectory).filePath(QStringLiteral("../share/revaplayer/mpv/scripts/thumbfast.lua")),
        QDir(applicationDirectory).filePath(QStringLiteral("../share/mpv/scripts/thumbfast.lua")),
        QDir(applicationDirectory).filePath(QStringLiteral("../resources/mpv/thumbfast.lua")),
        extractBundledThumbfastScript(),
        QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("mpv/scripts/thumbfast.lua")),
    };

    for (const QString &candidate : candidates) {
        const QFileInfo fileInfo(candidate);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            continue;
        }

        const QString canonical = fileInfo.canonicalFilePath();
        return canonical.isEmpty() ? fileInfo.absoluteFilePath() : canonical;
    }

    return {};
}

QString thumbfastRuntimeBasePath()
{
    QString runtimeRoot = qEnvironmentVariable("XDG_RUNTIME_DIR").trimmed();
    if (runtimeRoot.isEmpty()) {
        runtimeRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    if (runtimeRoot.trimmed().isEmpty()) {
        runtimeRoot = QDir::tempPath();
    }

    const QString runtimeDirectory = QDir(runtimeRoot).filePath(QStringLiteral("revaplayer"));
    QDir().mkpath(runtimeDirectory);

    const QString unique = QStringLiteral("thumbfast-%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    return QDir(runtimeDirectory).filePath(unique);
}

QByteArray thumbfastScriptOptions(const QString &runtimeBasePath)
{
    const QString normalizedRuntimeBasePath = QDir::cleanPath(runtimeBasePath);
    const QStringList options {
        QStringLiteral("thumbfast-max_width=420"),
        QStringLiteral("thumbfast-max_height=420"),
        QStringLiteral("thumbfast-scale_factor=1"),
        QStringLiteral("thumbfast-spawn_first=no"),
        QStringLiteral("thumbfast-network=no"),
        QStringLiteral("thumbfast-audio=no"),
        QStringLiteral("thumbfast-socket=%1.sock").arg(normalizedRuntimeBasePath),
        QStringLiteral("thumbfast-thumbnail=%1.out").arg(normalizedRuntimeBasePath),
        QStringLiteral("ytdl_hook-all_formats=yes"),
    };
    return options.join(QChar(',')).toUtf8();
}

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

std::optional<int> nodeToInt(const mpv_node *node)
{
    if (node == nullptr) {
        return std::nullopt;
    }

    switch (node->format) {
    case MPV_FORMAT_INT64:
        return static_cast<int>(node->u.int64);
    case MPV_FORMAT_DOUBLE:
        return static_cast<int>(std::lround(node->u.double_));
    case MPV_FORMAT_FLAG:
        return node->u.flag != 0 ? 1 : 0;
    default:
        return std::nullopt;
    }
}

std::optional<double> nodeToDouble(const mpv_node *node)
{
    if (node == nullptr) {
        return std::nullopt;
    }

    switch (node->format) {
    case MPV_FORMAT_DOUBLE:
        return node->u.double_;
    case MPV_FORMAT_INT64:
        return static_cast<double>(node->u.int64);
    case MPV_FORMAT_FLAG:
        return node->u.flag != 0 ? 1.0 : 0.0;
    default:
        return std::nullopt;
    }
}

bool nodeToBool(const mpv_node *node)
{
    return nodeToInt(node).value_or(0) != 0;
}

revaplayer::domain::TrackType trackTypeFromNode(const mpv_node *node)
{
    const QString raw = nodeToQString(node).trimmed().toLower();
    if (raw == QStringLiteral("video")) {
        return revaplayer::domain::TrackType::Video;
    }
    if (raw == QStringLiteral("audio")) {
        return revaplayer::domain::TrackType::Audio;
    }
    if (raw == QStringLiteral("sub")) {
        return revaplayer::domain::TrackType::Subtitle;
    }
    return revaplayer::domain::TrackType::Unknown;
}

QVector<revaplayer::domain::PlaylistEntry> parsePlaylistNode(const mpv_node &node)
{
    QVector<revaplayer::domain::PlaylistEntry> entries;
    if (node.format != MPV_FORMAT_NODE_ARRAY || node.u.list == nullptr) {
        return entries;
    }

    const auto *list = node.u.list;
    entries.reserve(list->num);

    for (int index = 0; index < list->num; ++index) {
        const mpv_node &entryNode = list->values[index];
        revaplayer::domain::PlaylistEntry entry;
        entry.index = index;
        entry.source = nodeToQString(findMapValue(entryNode, "filename"));
        const QUrl sourceUrl(entry.source);
        const QString localPath = sourceUrl.isLocalFile() ? sourceUrl.toLocalFile() : entry.source;
        const bool localSource = !localPath.trimmed().isEmpty()
            && (!sourceUrl.isValid() || sourceUrl.scheme().isEmpty() || sourceUrl.isLocalFile());
        const QString fileName = localSource ? QFileInfo(localPath).fileName() : QString {};
        entry.title = fileName.isEmpty() ? nodeToQString(findMapValue(entryNode, "title")) : fileName;
        entry.isCurrent = nodeToBool(findMapValue(entryNode, "current"));

        if (entry.title.isEmpty()) {
            const QFileInfo fileInfo(entry.source);
            entry.title = fileInfo.fileName().isEmpty() ? entry.source : fileInfo.fileName();
        }

        entries.push_back(std::move(entry));
    }

    return entries;
}

QVector<revaplayer::domain::ChapterInfo> parseChapterNode(const mpv_node &node)
{
    QVector<revaplayer::domain::ChapterInfo> chapters;
    if (node.format != MPV_FORMAT_NODE_ARRAY || node.u.list == nullptr) {
        return chapters;
    }

    const auto *list = node.u.list;
    chapters.reserve(list->num);

    for (int index = 0; index < list->num; ++index) {
        const mpv_node &chapterNode = list->values[index];
        revaplayer::domain::ChapterInfo chapter;
        chapter.index = index;
        chapter.title = nodeToQString(findMapValue(chapterNode, "title"));
        chapter.timeSeconds = nodeToDouble(findMapValue(chapterNode, "time")).value_or(0.0);

        if (chapter.title.isEmpty()) {
            chapter.title = QStringLiteral("Chapter %1").arg(index + 1);
        }

        chapters.push_back(std::move(chapter));
    }

    return chapters;
}

QVector<revaplayer::domain::TrackInfo> parseTrackNode(const mpv_node &node)
{
    QVector<revaplayer::domain::TrackInfo> tracks;
    if (node.format != MPV_FORMAT_NODE_ARRAY || node.u.list == nullptr) {
        return tracks;
    }

    const auto *list = node.u.list;
    tracks.reserve(list->num);

    for (int index = 0; index < list->num; ++index) {
        const mpv_node &trackNode = list->values[index];
        revaplayer::domain::TrackInfo track;
        track.id = nodeToInt(findMapValue(trackNode, "id")).value_or(-1);
        track.type = trackTypeFromNode(findMapValue(trackNode, "type"));
        track.title = nodeToQString(findMapValue(trackNode, "title"));
        track.language = nodeToQString(findMapValue(trackNode, "lang"));
        track.codec = nodeToQString(findMapValue(trackNode, "codec"));
        track.width = nodeToInt(findMapValue(trackNode, "demux-w"))
                          .value_or(nodeToInt(findMapValue(trackNode, "w")).value_or(0));
        track.height = nodeToInt(findMapValue(trackNode, "demux-h"))
                           .value_or(nodeToInt(findMapValue(trackNode, "h")).value_or(0));
        track.fps = nodeToDouble(findMapValue(trackNode, "demux-fps"))
                        .value_or(nodeToDouble(findMapValue(trackNode, "fps")).value_or(0.0));
        track.selected = nodeToBool(findMapValue(trackNode, "selected"));
        track.external = nodeToBool(findMapValue(trackNode, "external"));
        tracks.push_back(std::move(track));
    }

    return tracks;
}

QString propertyStringValue(const mpv_event_property &property)
{
    if (property.data == nullptr || property.format != MPV_FORMAT_STRING) {
        return {};
    }

    const auto *value = static_cast<char **>(property.data);
    return (value != nullptr && *value != nullptr) ? QString::fromUtf8(*value) : QString {};
}

QString displayTargetForSource(const QString &source)
{
    const QUrl url(source);
    if (url.isValid() && !url.scheme().isEmpty() && !url.isLocalFile()) {
        return url.toDisplayString(QUrl::RemovePassword);
    }

    const QFileInfo fileInfo(source);
    return fileInfo.fileName().isEmpty() ? source : fileInfo.fileName();
}

struct EndFileDescription final {
    revaplayer::domain::PlaybackEndReason reason {revaplayer::domain::PlaybackEndReason::Unknown};
    QString message {QStringLiteral("Playback ended.")};
};

EndFileDescription describeEndFileEvent(const mpv_event_end_file &endFile)
{
    switch (endFile.reason) {
    case MPV_END_FILE_REASON_EOF:
        return {revaplayer::domain::PlaybackEndReason::ReachedEndOfFile,
                QStringLiteral("Reached the end of the current media.")};
    case MPV_END_FILE_REASON_STOP:
        return {revaplayer::domain::PlaybackEndReason::Stopped,
                QStringLiteral("Playback stopped.")};
    case MPV_END_FILE_REASON_QUIT:
        return {revaplayer::domain::PlaybackEndReason::BackendQuit,
                QStringLiteral("Playback backend requested shutdown.")};
    case MPV_END_FILE_REASON_REDIRECT:
        return {revaplayer::domain::PlaybackEndReason::Redirected,
                QStringLiteral("Media source redirected.")};
    case MPV_END_FILE_REASON_ERROR:
        return {revaplayer::domain::PlaybackEndReason::Error,
                QStringLiteral("Playback failed: %1")
                    .arg(QString::fromUtf8(mpv_error_string(endFile.error)))};
    default:
        return {revaplayer::domain::PlaybackEndReason::Unknown,
                QStringLiteral("Playback ended.")};
    }
}

QStringList clientMessageArguments(const mpv_event_client_message *message)
{
    QStringList arguments;
    if (message == nullptr || message->num_args <= 0 || message->args == nullptr) {
        return arguments;
    }

    arguments.reserve(message->num_args);
    for (int index = 0; index < message->num_args; ++index) {
        arguments.push_back(
            message->args[index] != nullptr ? QString::fromUtf8(message->args[index]) : QString {});
    }

    return arguments;
}

}  // namespace

MpvCore::MpvCore(QObject *parent)
    : QObject(parent)
{
}

MpvCore::~MpvCore()
{
    eventBridge_.reset();
    if (handle_ != nullptr) {
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
    }
}

void MpvCore::setUseExternalMpvConfig(const bool enabled)
{
    if (initialized_) {
        return;
    }

    useExternalMpvConfig_ = enabled;
}

bool MpvCore::initialize(revaplayer::infrastructure::mpv::MpvRenderHost *renderHost)
{
    if (initialized_) {
        return true;
    }

    createHandle();

    if (handle_ == nullptr) {
        emit errorOccurred(QStringLiteral("Failed to create mpv handle."));
        return false;
    }

    setStringOption("terminal", "no");
    setStringOption("config", useExternalMpvConfig_ ? "yes" : "no");
    setStringOption("load-scripts", useExternalMpvConfig_ ? "yes" : "no");
    setStringOption("osc", "no");
    setStringOption("input-default-bindings", "no");
    setStringOption("input-terminal", "no");
    setStringOption("input-vo-keyboard", "no");
    setStringOption("input-media-keys", "no");
    setStringOption("media-controls", "no");
    setStringOption("keep-open", "yes");
    setStringOption("idle", "yes");
    setStringOption("vo", "libmpv");
    setStringOption("hwdec", "auto-safe");
    // Keep mpv from spending CPU on very large background cache fill for long
    // local files while preserving normal forward/backward buffering.
    setStringOption("cache-secs", "60");
    setStringOption("demuxer-max-bytes", "64MiB");
    setStringOption("demuxer-max-back-bytes", "16MiB");
    setStringOption("prefetch-playlist", "no");
    // Direct-render decode paths can produce corrupted scanlines when the same
    // frame is re-composited for subtitle/OSD updates on some drivers.
    setStringOption("vd-lavc-dr", "no");
    const QString thumbfastScriptPath = useExternalMpvConfig_ ? QString {} : findBundledThumbfastScriptPath();
    if (!useExternalMpvConfig_ && !thumbfastScriptPath.trimmed().isEmpty()) {
        setStringOption("scripts", thumbfastScriptPath.toUtf8());
        setStringOption("script-opts", thumbfastScriptOptions(thumbfastRuntimeBasePath()));
    }

    const int initializeResult = mpv_initialize(handle_);
    if (initializeResult < 0) {
        emit errorOccurred(QStringLiteral("mpv initialization failed: %1")
                               .arg(QString::fromUtf8(mpv_error_string(initializeResult))));
        return false;
    }

    eventBridge_ = std::make_unique<MpvEventBridge>(handle_, this);
    connect(eventBridge_.get(), &MpvEventBridge::wakeup, this, &MpvCore::processPendingEvents);

    if (renderHost != nullptr) {
        connect(renderHost, &revaplayer::infrastructure::mpv::MpvRenderHost::rendererError,
                this, &MpvCore::errorOccurred);
        renderHost->setMpvHandle(handle_);
    }

    registerObservers();
    initialized_ = true;
    emit initializedChanged(true);
    emitStateSnapshot();
    return true;
}

bool MpvCore::isInitialized() const
{
    return initialized_;
}

bool MpvCore::idleActive() const
{
    return propertyCache_.idleActive();
}

bool MpvCore::paused() const
{
    return propertyCache_.paused();
}

double MpvCore::durationSeconds() const
{
    return propertyCache_.durationSeconds();
}

int MpvCore::volume() const
{
    return propertyCache_.volume();
}

bool MpvCore::muted() const
{
    return propertyCache_.muted();
}

void MpvCore::loadFile(const QString &filePath, const bool append)
{
    if (!ensureReady()) {
        return;
    }

    sendLoadCommand(filePath, append, !append);
}

void MpvCore::loadFiles(const QStringList &filePaths)
{
    if (filePaths.isEmpty()) {
        return;
    }

    loadFile(filePaths.first(), false);
    for (int index = 1; index < filePaths.size(); ++index) {
        loadFile(filePaths.at(index), true);
    }
}

void MpvCore::loadUrl(const QString &url, const bool append)
{
    if (!ensureReady()) {
        return;
    }

    sendLoadCommand(url, append, !append);
}

void MpvCore::togglePause()
{
    if (!ensureReady()) {
        return;
    }

    sendCommand({QStringLiteral("cycle"), QStringLiteral("pause")});
}

void MpvCore::setPaused(const bool paused)
{
    if (!ensureReady()) {
        return;
    }

    int pauseFlag = paused ? 1 : 0;
    setProperty(kPauseProperty, MPV_FORMAT_FLAG, &pauseFlag);
}

void MpvCore::stop()
{
    if (!ensureReady()) {
        return;
    }

    sendCommand({QStringLiteral("stop")});
}

bool MpvCore::executeCommand(const QStringList &arguments)
{
    if (!ensureReady() || arguments.isEmpty()) {
        return false;
    }

    return sendCommand(arguments);
}

void MpvCore::loadSubtitleFile(const QString &filePath)
{
    if (!ensureReady() || filePath.trimmed().isEmpty()) {
        return;
    }

    sendCommand({
        QStringLiteral("sub-add"),
        filePath,
        QStringLiteral("select"),
    });
}

bool MpvCore::captureScreenshot(const QString &filePath)
{
    if (!ensureReady() || filePath.trimmed().isEmpty()) {
        return false;
    }

    return sendCommand({
        QStringLiteral("screenshot-to-file"),
        filePath,
        QStringLiteral("video"),
    });
}

void MpvCore::seekToSeconds(const double seconds)
{
    if (!ensureReady()) {
        return;
    }

    double targetSeconds = std::max(0.0, seconds);
    setProperty(kTimePosProperty, MPV_FORMAT_DOUBLE, &targetSeconds);
}

void MpvCore::seekToFraction(double fraction)
{
    if (!ensureReady()) {
        return;
    }

    fraction = std::clamp(fraction, 0.0, 1.0);
    sendCommand({
        QStringLiteral("seek"),
        QString::number(fraction * 100.0, 'f', 3),
        QStringLiteral("absolute-percent"),
    });
}

void MpvCore::seekBySeconds(const int seconds)
{
    if (!ensureReady() || seconds == 0) {
        return;
    }

    sendCommand({
        QStringLiteral("seek"),
        QString::number(seconds),
        QStringLiteral("relative+exact"),
    });
}

void MpvCore::setVolume(const int volume)
{
    if (!ensureReady()) {
        return;
    }

    double clampedVolume = static_cast<double>(revaplayer::application::clampPlaybackVolume(volume));
    setProperty(kVolumeProperty, MPV_FORMAT_DOUBLE, &clampedVolume);

    if (clampedVolume > 0.0 && propertyCache_.muted()) {
        int muteFlag = 0;
        setProperty(kMuteProperty, MPV_FORMAT_FLAG, &muteFlag);
    }
}

void MpvCore::adjustVolume(const int delta)
{
    if (!ensureReady() || delta == 0) {
        return;
    }

    setVolume(propertyCache_.volume() + delta);
}

void MpvCore::setMuted(const bool muted)
{
    if (!ensureReady()) {
        return;
    }

    int muteFlag = muted ? 1 : 0;
    setProperty(kMuteProperty, MPV_FORMAT_FLAG, &muteFlag);
}

void MpvCore::toggleMuted()
{
    setMuted(!propertyCache_.muted());
}

void MpvCore::setSpeed(double speed)
{
    if (!ensureReady()) {
        return;
    }

    if (!std::isfinite(speed)) {
        speed = 1.0;
    }

    double effectiveSpeed = std::clamp(speed, 0.25, 4.0);
    setProperty(kSpeedProperty, MPV_FORMAT_DOUBLE, &effectiveSpeed);
}

void MpvCore::adjustSpeed(const double delta)
{
    if (!ensureReady() || delta == 0.0) {
        return;
    }

    setSpeed(propertyCache_.speed() + delta);
}

void MpvCore::resetSpeed()
{
    setSpeed(1.0);
}

void MpvCore::setSubtitleDelay(double seconds)
{
    if (!ensureReady()) {
        return;
    }

    if (!std::isfinite(seconds)) {
        seconds = 0.0;
    }

    setProperty(kSubtitleDelayProperty, MPV_FORMAT_DOUBLE, &seconds);
}

void MpvCore::adjustSubtitleDelay(const double deltaSeconds)
{
    if (!ensureReady() || deltaSeconds == 0.0) {
        return;
    }

    setSubtitleDelay(propertyCache_.subtitleDelay() + deltaSeconds);
}

void MpvCore::resetSubtitleDelay()
{
    setSubtitleDelay(0.0);
}

void MpvCore::setSubtitleVisible(const bool visible)
{
    if (!ensureReady()) {
        return;
    }

    int visibleFlag = visible ? 1 : 0;
    setProperty(kSubtitleVisibilityProperty, MPV_FORMAT_FLAG, &visibleFlag);
}

void MpvCore::toggleSubtitleVisible()
{
    setSubtitleVisible(!propertyCache_.subtitleVisible());
}

void MpvCore::setSubtitleScale(double scale)
{
    if (!ensureReady()) {
        return;
    }

    double effectiveScale = revaplayer::application::clampSubtitleScale(scale);
    setProperty(kSubtitleScaleProperty, MPV_FORMAT_DOUBLE, &effectiveScale);
}

void MpvCore::adjustSubtitleScale(const double delta)
{
    if (!ensureReady() || delta == 0.0) {
        return;
    }

    setSubtitleScale(propertyCache_.subtitleScale() + delta);
}

void MpvCore::resetSubtitleScale()
{
    setSubtitleScale(1.0);
}

void MpvCore::setSubtitlePosition(const int position)
{
    if (!ensureReady()) {
        return;
    }

    double effectivePosition = static_cast<double>(revaplayer::application::clampSubtitlePosition(position));
    setProperty(kSubtitlePositionProperty, MPV_FORMAT_DOUBLE, &effectivePosition);
}

void MpvCore::adjustSubtitlePosition(const int delta)
{
    if (!ensureReady() || delta == 0) {
        return;
    }

    setSubtitlePosition(propertyCache_.subtitlePosition() + delta);
}

void MpvCore::resetSubtitlePosition()
{
    setSubtitlePosition(100);
}

void MpvCore::setSubtitleFontFamily(const QString &fontFamily)
{
    if (!ensureReady()) {
        return;
    }

    const QString effectiveFamily = fontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : fontFamily.trimmed();
    QByteArray utf8Family = effectiveFamily.toUtf8();
    const char *rawFamily = utf8Family.constData();
    setProperty(kSubtitleFontProperty, MPV_FORMAT_STRING, &rawFamily);
}

void MpvCore::setSubtitleFontSize(const int fontSize)
{
    if (!ensureReady()) {
        return;
    }

    double effectiveSize = static_cast<double>(revaplayer::application::clampSubtitleFontSize(fontSize));
    setProperty(kSubtitleFontSizeProperty, MPV_FORMAT_DOUBLE, &effectiveSize);
}

void MpvCore::setSubtitleAssOverride(const QString &mode)
{
    if (!ensureReady()) {
        return;
    }

    const QString effectiveMode = revaplayer::application::normalizeSubtitleAssOverride(mode);
    QByteArray utf8Mode = effectiveMode.toUtf8();
    const char *rawMode = utf8Mode.constData();
    setProperty(kSubtitleAssOverrideProperty, MPV_FORMAT_STRING, &rawMode);
}

void MpvCore::cycleSubtitleAssOverride()
{
    setSubtitleAssOverride(revaplayer::application::nextSubtitleAssOverride(propertyCache_.subtitleAssOverride()));
}

void MpvCore::setAudioDelay(double seconds)
{
    if (!ensureReady()) {
        return;
    }

    if (!std::isfinite(seconds)) {
        seconds = 0.0;
    }

    setProperty(kAudioDelayProperty, MPV_FORMAT_DOUBLE, &seconds);
}

void MpvCore::adjustAudioDelay(const double deltaSeconds)
{
    if (!ensureReady() || deltaSeconds == 0.0) {
        return;
    }

    setAudioDelay(propertyCache_.audioDelay() + deltaSeconds);
}

void MpvCore::resetAudioDelay()
{
    setAudioDelay(0.0);
}

void MpvCore::setLoopStart(double seconds)
{
    if (!ensureReady()) {
        return;
    }

    if (!std::isfinite(seconds)) {
        seconds = 0.0;
    }

    setCommandProperty(QStringLiteral("ab-loop-a"), QString::number(std::max(0.0, seconds), 'f', 3));
}

void MpvCore::setLoopEnd(double seconds)
{
    if (!ensureReady()) {
        return;
    }

    if (!std::isfinite(seconds)) {
        seconds = 0.0;
    }

    setCommandProperty(QStringLiteral("ab-loop-b"), QString::number(std::max(0.0, seconds), 'f', 3));
}

void MpvCore::clearLoop()
{
    if (!ensureReady()) {
        return;
    }

    sendCommand({QStringLiteral("set"), QStringLiteral("ab-loop-a"), QStringLiteral("no")});
    sendCommand({QStringLiteral("set"), QStringLiteral("ab-loop-b"), QStringLiteral("no")});
}

void MpvCore::frameStepForward()
{
    if (!ensureReady()) {
        return;
    }

    sendCommand({QStringLiteral("frame-step")});
}

void MpvCore::frameStepBackward()
{
    if (!ensureReady()) {
        return;
    }

    sendCommand({QStringLiteral("frame-back-step")});
}

void MpvCore::setRepeatMode(const QString &mode)
{
    if (!ensureReady()) {
        return;
    }

    const QString normalized = mode.trimmed().toLower();
    const bool repeatFile = normalized == QStringLiteral("file");
    const bool repeatPlaylist = normalized == QStringLiteral("playlist");

    setCommandProperty(QStringLiteral("loop-file"), repeatFile ? QStringLiteral("inf") : QStringLiteral("no"));
    setCommandProperty(QStringLiteral("loop-playlist"), repeatPlaylist ? QStringLiteral("inf") : QStringLiteral("no"));
}

void MpvCore::setVideoAspectOverride(const QString &aspectOverride)
{
    if (!ensureReady()) {
        return;
    }

    setCommandProperty(
        QStringLiteral("video-aspect-override"),
        aspectOverride.trimmed().isEmpty() ? QStringLiteral("no") : aspectOverride.trimmed());
}

void MpvCore::resetVideoAspectOverride()
{
    setVideoAspectOverride(QStringLiteral("no"));
}

void MpvCore::setVideoCrop(const QString &crop)
{
    if (!ensureReady()) {
        return;
    }

    setCommandProperty(QStringLiteral("video-crop"), crop.trimmed());
}

void MpvCore::clearVideoCrop()
{
    setVideoCrop(QString {});
}

void MpvCore::setVideoZoomFactor(const double factor)
{
    if (!ensureReady()) {
        return;
    }

    setCommandProperty(
        QStringLiteral("video-zoom"),
        QString::number(revaplayer::application::videoZoomFactorToLog2(factor), 'f', 4));
}

void MpvCore::resetVideoZoom()
{
    setVideoZoomFactor(1.0);
}

void MpvCore::setVideoAlignment(const double horizontal, const double vertical)
{
    if (!ensureReady()) {
        return;
    }

    setCommandProperty(
        QStringLiteral("video-align-x"),
        QString::number(revaplayer::application::clampVideoViewportAlignment(horizontal), 'f', 4));
    setCommandProperty(
        QStringLiteral("video-align-y"),
        QString::number(revaplayer::application::clampVideoViewportAlignment(vertical), 'f', 4));
}

void MpvCore::resetVideoAlignment()
{
    setVideoAlignment(0.0, 0.0);
}

void MpvCore::setVideoRotation(int degrees)
{
    if (!ensureReady()) {
        return;
    }

    degrees = ((degrees % 360) + 360) % 360;
    setCommandProperty(QStringLiteral("video-rotate"), QString::number(degrees));
}

void MpvCore::setDeinterlace(const bool enabled)
{
    if (!ensureReady()) {
        return;
    }

    setCommandProperty(QStringLiteral("deinterlace"), enabled ? QStringLiteral("yes") : QStringLiteral("no"));
}

void MpvCore::nextPlaylistItem()
{
    if (!ensureReady()) {
        return;
    }

    const int currentIndex = propertyCache_.currentPlaylistIndex();
    const int nextIndex = currentIndex + 1;
    if (currentIndex < 0 || nextIndex >= propertyCache_.playlist().size()) {
        return;
    }

    selectPlaylistIndex(nextIndex);
}

void MpvCore::previousPlaylistItem()
{
    if (!ensureReady()) {
        return;
    }

    const int currentIndex = propertyCache_.currentPlaylistIndex();
    if (currentIndex <= 0) {
        return;
    }

    selectPlaylistIndex(currentIndex - 1);
}

void MpvCore::selectPlaylistIndex(const int index)
{
    if (!ensureReady()) {
        return;
    }

    int64_t playlistIndex = index;
    setProperty(kPlaylistPositionProperty, MPV_FORMAT_INT64, &playlistIndex);
}

void MpvCore::nextChapter()
{
    if (!ensureReady()) {
        return;
    }

    const int nextChapterIndex = propertyCache_.currentChapterIndex() + 1;
    if (propertyCache_.chapters().isEmpty() || nextChapterIndex >= propertyCache_.chapters().size()) {
        return;
    }

    selectChapter(nextChapterIndex);
}

void MpvCore::previousChapter()
{
    if (!ensureReady()) {
        return;
    }

    const int currentChapterIndex = propertyCache_.currentChapterIndex();
    if (propertyCache_.chapters().isEmpty() || currentChapterIndex <= 0) {
        return;
    }

    selectChapter(currentChapterIndex - 1);
}

void MpvCore::selectChapter(const int index)
{
    if (!ensureReady()) {
        return;
    }

    int64_t chapterIndex = index;
    setProperty(kChapterProperty, MPV_FORMAT_INT64, &chapterIndex);
}

void MpvCore::selectTrack(const revaplayer::domain::TrackType type, const int trackId)
{
    if (!ensureReady() || trackId < 0) {
        return;
    }

    const char *propertyName = nullptr;
    switch (type) {
    case revaplayer::domain::TrackType::Video:
        propertyName = "vid";
        break;
    case revaplayer::domain::TrackType::Audio:
        propertyName = "aid";
        break;
    case revaplayer::domain::TrackType::Subtitle:
        propertyName = "sid";
        break;
    case revaplayer::domain::TrackType::Unknown:
    default:
        return;
    }

    int64_t selectedTrack = trackId;
    setProperty(propertyName, MPV_FORMAT_INT64, &selectedTrack);
}

bool MpvCore::sendLoadCommand(const QString &target, const bool append, const bool announce)
{
    const QString mode = append ? QStringLiteral("append-play") : QStringLiteral("replace");
    if (announce) {
        emit loadStarted(displayTargetForSource(target));
    }

    return sendCommand({QStringLiteral("loadfile"), target, mode});
}

void MpvCore::createHandle()
{
    if (handle_ == nullptr) {
        handle_ = mpv_create();
    }
}

void MpvCore::registerObservers() const
{
    mpv_observe_property(handle_, 0, kIdleActiveProperty, MPV_FORMAT_FLAG);
    mpv_observe_property(handle_, 0, kPauseProperty, MPV_FORMAT_FLAG);
    mpv_observe_property(handle_, 0, kTimePosProperty, MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 0, kDurationProperty, MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 0, kVolumeProperty, MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 0, kMuteProperty, MPV_FORMAT_FLAG);
    mpv_observe_property(handle_, 0, kSpeedProperty, MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 0, kSubtitleDelayProperty, MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 0, kSubtitleVisibilityProperty, MPV_FORMAT_FLAG);
    mpv_observe_property(handle_, 0, kSubtitleScaleProperty, MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 0, kSubtitlePositionProperty, MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 0, kSubtitleFontProperty, MPV_FORMAT_STRING);
    mpv_observe_property(handle_, 0, kSubtitleFontSizeProperty, MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 0, kSubtitleAssOverrideProperty, MPV_FORMAT_STRING);
    mpv_observe_property(handle_, 0, kAudioDelayProperty, MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 0, kPathProperty, MPV_FORMAT_STRING);
    mpv_observe_property(handle_, 0, kFileFormatProperty, MPV_FORMAT_STRING);
    mpv_observe_property(handle_, 0, kCurrentVoProperty, MPV_FORMAT_STRING);
    mpv_observe_property(handle_, 0, kCurrentAoProperty, MPV_FORMAT_STRING);
    mpv_observe_property(handle_, 0, kHwdecCurrentProperty, MPV_FORMAT_STRING);
    mpv_observe_property(handle_, 0, kVideoCodecProperty, MPV_FORMAT_STRING);
    mpv_observe_property(handle_, 0, kAudioCodecProperty, MPV_FORMAT_STRING);
    mpv_observe_property(handle_, 0, kWidthProperty, MPV_FORMAT_INT64);
    mpv_observe_property(handle_, 0, kHeightProperty, MPV_FORMAT_INT64);
    mpv_observe_property(handle_, 0, kDisplayWidthProperty, MPV_FORMAT_INT64);
    mpv_observe_property(handle_, 0, kDisplayHeightProperty, MPV_FORMAT_INT64);
    mpv_observe_property(handle_, 0, kContainerFpsProperty, MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 0, kEstimatedVfFpsProperty, MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 0, kVideoBitrateProperty, MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 0, kAudioBitrateProperty, MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 0, kCacheSpeedProperty, MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 0, kDemuxerCacheDurationProperty, MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle_, 0, kPausedForCacheProperty, MPV_FORMAT_FLAG);
    mpv_observe_property(handle_, 0, kMediaTitleProperty, MPV_FORMAT_STRING);
    mpv_observe_property(handle_, 0, kPlaylistProperty, MPV_FORMAT_NODE);
    mpv_observe_property(handle_, 0, kPlaylistPositionProperty, MPV_FORMAT_INT64);
    mpv_observe_property(handle_, 0, kChapterListProperty, MPV_FORMAT_NODE);
    mpv_observe_property(handle_, 0, kChapterProperty, MPV_FORMAT_INT64);
    mpv_observe_property(handle_, 0, kTrackListProperty, MPV_FORMAT_NODE);
}

void MpvCore::processPendingEvents()
{
    if (handle_ == nullptr) {
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

void MpvCore::handleEvent(mpv_event *event)
{
    switch (event->event_id) {
    case MPV_EVENT_PROPERTY_CHANGE:
        handlePropertyChange(*static_cast<mpv_event_property *>(event->data));
        break;
    case MPV_EVENT_FILE_LOADED:
        emit fileLoaded();
        emitStateSnapshot();
        break;
    case MPV_EVENT_CLIENT_MESSAGE:
        emit clientMessageReceived(clientMessageArguments(static_cast<mpv_event_client_message *>(event->data)));
        break;
    case MPV_EVENT_END_FILE: {
        const auto *endFile = static_cast<mpv_event_end_file *>(event->data);
        const EndFileDescription description = endFile != nullptr
            ? describeEndFileEvent(*endFile)
            : EndFileDescription {};
        emit fileEnded(description.reason, description.message);
        break;
    }
    case MPV_EVENT_SHUTDOWN:
        initialized_ = false;
        emit initializedChanged(false);
        break;
    default:
        break;
    }
}

void MpvCore::handlePropertyChange(const mpv_event_property &property)
{
    const QByteArray propertyName = property.name != nullptr ? QByteArray(property.name) : QByteArray {};
    auto updateDiagnostics = [this](const auto &updater, const bool throttle = false) {
        auto diagnostics = propertyCache_.diagnostics();
        updater(diagnostics);
        propertyCache_.setDiagnostics(std::move(diagnostics));
        emitDiagnosticsChanged(!throttle);
    };

    if (propertyName == kIdleActiveProperty) {
        const bool idleActive = property.data != nullptr
            ? (*static_cast<int *>(property.data) != 0)
            : true;
        propertyCache_.setIdleActive(idleActive);
        if (idleActive) {
            propertyCache_.setDiagnostics({});
            emitDiagnosticsChanged(true);
        }
        emit idleChanged(idleActive);
        return;
    }

    if (propertyName == kPauseProperty) {
        const bool pausedValue = property.data != nullptr
            ? (*static_cast<int *>(property.data) != 0)
            : true;
        propertyCache_.setPaused(pausedValue);
        emit pausedChanged(pausedValue);
        return;
    }

    if (propertyName == kTimePosProperty) {
        const double timeSeconds = property.data != nullptr
            ? *static_cast<double *>(property.data)
            : 0.0;
        propertyCache_.setTimeSeconds(timeSeconds);
        emit playbackPositionChanged(timeSeconds, propertyCache_.durationSeconds());
        return;
    }

    if (propertyName == kDurationProperty) {
        const double durationSeconds = property.data != nullptr
            ? *static_cast<double *>(property.data)
            : 0.0;
        propertyCache_.setDurationSeconds(durationSeconds);
        emit playbackPositionChanged(propertyCache_.timeSeconds(), durationSeconds);
        return;
    }

    if (propertyName == kVolumeProperty) {
        const int volumeValue = property.data != nullptr
            ? static_cast<int>(std::lround(*static_cast<double *>(property.data)))
            : 100;
        propertyCache_.setVolume(volumeValue);
        emit volumeChanged(volumeValue);
        return;
    }

    if (propertyName == kSpeedProperty) {
        const double speedValue = property.data != nullptr
            ? *static_cast<double *>(property.data)
            : 1.0;
        propertyCache_.setSpeed(speedValue);
        emit speedChanged(speedValue);
        return;
    }

    if (propertyName == kMuteProperty) {
        const bool muted = property.data != nullptr
            ? (*static_cast<int *>(property.data) != 0)
            : false;
        propertyCache_.setMuted(muted);
        emit mutedChanged(muted);
        return;
    }

    if (propertyName == kSubtitleDelayProperty) {
        const double delaySeconds = property.data != nullptr
            ? *static_cast<double *>(property.data)
            : 0.0;
        propertyCache_.setSubtitleDelay(delaySeconds);
        emit subtitleDelayChanged(delaySeconds);
        return;
    }

    if (propertyName == kSubtitleVisibilityProperty) {
        const bool visible = property.data != nullptr
            ? (*static_cast<int *>(property.data) != 0)
            : true;
        propertyCache_.setSubtitleVisible(visible);
        emit subtitleVisibilityChanged(visible);
        return;
    }

    if (propertyName == kSubtitleScaleProperty) {
        const double scale = property.data != nullptr
            ? *static_cast<double *>(property.data)
            : 1.0;
        propertyCache_.setSubtitleScale(revaplayer::application::clampSubtitleScale(scale));
        emit subtitleScaleChanged(propertyCache_.subtitleScale());
        return;
    }

    if (propertyName == kSubtitlePositionProperty) {
        const double position = property.data != nullptr
            ? *static_cast<double *>(property.data)
            : 100.0;
        propertyCache_.setSubtitlePosition(
            revaplayer::application::clampSubtitlePosition(static_cast<int>(std::lround(position))));
        emit subtitlePositionChanged(propertyCache_.subtitlePosition());
        return;
    }

    if (propertyName == kSubtitleFontProperty) {
        const QString fontFamily = propertyStringValue(property).trimmed();
        propertyCache_.setSubtitleFontFamily(fontFamily.isEmpty() ? QStringLiteral("sans-serif") : fontFamily);
        emit subtitleFontFamilyChanged(propertyCache_.subtitleFontFamily());
        return;
    }

    if (propertyName == kSubtitleFontSizeProperty) {
        const double fontSize = property.data != nullptr
            ? *static_cast<double *>(property.data)
            : 38.0;
        propertyCache_.setSubtitleFontSize(
            revaplayer::application::clampSubtitleFontSize(static_cast<int>(std::lround(fontSize))));
        emit subtitleFontSizeChanged(propertyCache_.subtitleFontSize());
        return;
    }

    if (propertyName == kSubtitleAssOverrideProperty) {
        propertyCache_.setSubtitleAssOverride(
            revaplayer::application::normalizeSubtitleAssOverride(propertyStringValue(property)));
        emit subtitleAssOverrideChanged(propertyCache_.subtitleAssOverride());
        return;
    }

    if (propertyName == kAudioDelayProperty) {
        const double delaySeconds = property.data != nullptr
            ? *static_cast<double *>(property.data)
            : 0.0;
        propertyCache_.setAudioDelay(delaySeconds);
        emit audioDelayChanged(delaySeconds);
        return;
    }

    if (propertyName == kMediaTitleProperty) {
        const QString title = propertyStringValue(property);
        propertyCache_.setMediaTitle(title);
        emit titleChanged(title);
        return;
    }

    if (propertyName == kPathProperty) {
        updateDiagnostics([&property](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.source = propertyStringValue(property);
        });
        return;
    }

    if (propertyName == kFileFormatProperty) {
        updateDiagnostics([&property](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.fileFormat = propertyStringValue(property);
        });
        return;
    }

    if (propertyName == kCurrentVoProperty) {
        updateDiagnostics([&property](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.currentVideoOutput = propertyStringValue(property);
        });
        return;
    }

    if (propertyName == kCurrentAoProperty) {
        updateDiagnostics([&property](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.currentAudioOutput = propertyStringValue(property);
        });
        return;
    }

    if (propertyName == kHwdecCurrentProperty) {
        updateDiagnostics([&property](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.hardwareDecoding = propertyStringValue(property);
        });
        return;
    }

    if (propertyName == kVideoCodecProperty) {
        updateDiagnostics([&property](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.videoCodec = propertyStringValue(property);
        });
        return;
    }

    if (propertyName == kAudioCodecProperty) {
        updateDiagnostics([&property](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.audioCodec = propertyStringValue(property);
        });
        return;
    }

    if (propertyName == kWidthProperty) {
        const int encodedWidth = property.data != nullptr ? static_cast<int>(*static_cast<int64_t *>(property.data)) : -1;
        updateDiagnostics([encodedWidth](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.encodedWidth = encodedWidth;
        });
        return;
    }

    if (propertyName == kHeightProperty) {
        const int encodedHeight = property.data != nullptr ? static_cast<int>(*static_cast<int64_t *>(property.data)) : -1;
        updateDiagnostics([encodedHeight](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.encodedHeight = encodedHeight;
        });
        return;
    }

    if (propertyName == kDisplayWidthProperty) {
        const int displayWidth = property.data != nullptr ? static_cast<int>(*static_cast<int64_t *>(property.data)) : -1;
        updateDiagnostics([displayWidth](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.displayWidth = displayWidth;
        });
        return;
    }

    if (propertyName == kDisplayHeightProperty) {
        const int displayHeight = property.data != nullptr ? static_cast<int>(*static_cast<int64_t *>(property.data)) : -1;
        updateDiagnostics([displayHeight](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.displayHeight = displayHeight;
        });
        return;
    }

    if (propertyName == kContainerFpsProperty) {
        const double containerFps = property.data != nullptr ? *static_cast<double *>(property.data) : 0.0;
        updateDiagnostics([containerFps](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.containerFps = containerFps;
        });
        return;
    }

    if (propertyName == kEstimatedVfFpsProperty) {
        const double estimatedVfFps = property.data != nullptr ? *static_cast<double *>(property.data) : 0.0;
        updateDiagnostics([estimatedVfFps](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.estimatedVideoFps = estimatedVfFps;
        });
        return;
    }

    if (propertyName == kVideoBitrateProperty) {
        const double videoBitrate = property.data != nullptr ? *static_cast<double *>(property.data) : 0.0;
        updateDiagnostics([videoBitrate](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.videoBitrate = videoBitrate;
        });
        return;
    }

    if (propertyName == kAudioBitrateProperty) {
        const double audioBitrate = property.data != nullptr ? *static_cast<double *>(property.data) : 0.0;
        updateDiagnostics([audioBitrate](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.audioBitrate = audioBitrate;
        });
        return;
    }

    if (propertyName == kCacheSpeedProperty) {
        const double cacheSpeed = property.data != nullptr ? *static_cast<double *>(property.data) : 0.0;
        updateDiagnostics([cacheSpeed](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.cacheSpeed = cacheSpeed;
        }, true);
        return;
    }

    if (propertyName == kDemuxerCacheDurationProperty) {
        const double cacheDurationSeconds = property.data != nullptr ? *static_cast<double *>(property.data) : 0.0;
        updateDiagnostics([cacheDurationSeconds](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.cacheDurationSeconds = cacheDurationSeconds;
        }, true);
        return;
    }

    if (propertyName == kPausedForCacheProperty) {
        const bool pausedForCache = property.data != nullptr
            ? (*static_cast<int *>(property.data) != 0)
            : false;
        updateDiagnostics([pausedForCache](revaplayer::domain::PlaybackDiagnostics &diagnostics) {
            diagnostics.pausedForCache = pausedForCache;
        }, true);
        return;
    }

    if (propertyName == kPlaylistProperty) {
        QVector<revaplayer::domain::PlaylistEntry> playlist;
        if (property.data != nullptr && property.format == MPV_FORMAT_NODE) {
            playlist = parsePlaylistNode(*static_cast<mpv_node *>(property.data));
        }
        propertyCache_.setPlaylist(std::move(playlist));
        emit playlistChanged(propertyCache_.playlist(), propertyCache_.currentPlaylistIndex());
        return;
    }

    if (propertyName == kPlaylistPositionProperty) {
        const int playlistIndex = property.data != nullptr
            ? static_cast<int>(*static_cast<int64_t *>(property.data))
            : -1;
        propertyCache_.setCurrentPlaylistIndex(playlistIndex);
        emit playlistChanged(propertyCache_.playlist(), playlistIndex);
        return;
    }

    if (propertyName == kChapterListProperty) {
        QVector<revaplayer::domain::ChapterInfo> chapters;
        if (property.data != nullptr && property.format == MPV_FORMAT_NODE) {
            chapters = parseChapterNode(*static_cast<mpv_node *>(property.data));
        }
        propertyCache_.setChapters(std::move(chapters));
        emit chaptersChanged(propertyCache_.chapters(), propertyCache_.currentChapterIndex());
        return;
    }

    if (propertyName == kChapterProperty) {
        const int chapterIndex = property.data != nullptr
            ? static_cast<int>(*static_cast<int64_t *>(property.data))
            : -1;
        propertyCache_.setCurrentChapterIndex(chapterIndex);
        emit chaptersChanged(propertyCache_.chapters(), chapterIndex);
        return;
    }

    if (propertyName == kTrackListProperty) {
        QVector<revaplayer::domain::TrackInfo> tracks;
        if (property.data != nullptr && property.format == MPV_FORMAT_NODE) {
            tracks = parseTrackNode(*static_cast<mpv_node *>(property.data));
        }
        propertyCache_.setTracks(std::move(tracks));
        emit tracksChanged(propertyCache_.tracks());
        return;
    }

}

void MpvCore::emitDiagnosticsChanged(const bool force)
{
    if (!force
        && diagnosticsEmitElapsed_.isValid()
        && diagnosticsEmitElapsed_.elapsed() < kDiagnosticsRefreshIntervalMs) {
        return;
    }

    diagnosticsEmitElapsed_.restart();
    emit diagnosticsChanged(propertyCache_.diagnostics());
}

void MpvCore::emitStateSnapshot()
{
    emit idleChanged(propertyCache_.idleActive());
    emit pausedChanged(propertyCache_.paused());
    emit playbackPositionChanged(propertyCache_.timeSeconds(), propertyCache_.durationSeconds());
    emit volumeChanged(propertyCache_.volume());
    emit mutedChanged(propertyCache_.muted());
    emit speedChanged(propertyCache_.speed());
    emit subtitleDelayChanged(propertyCache_.subtitleDelay());
    emit subtitleVisibilityChanged(propertyCache_.subtitleVisible());
    emit subtitleScaleChanged(propertyCache_.subtitleScale());
    emit subtitlePositionChanged(propertyCache_.subtitlePosition());
    emit subtitleFontFamilyChanged(propertyCache_.subtitleFontFamily());
    emit subtitleFontSizeChanged(propertyCache_.subtitleFontSize());
    emit subtitleAssOverrideChanged(propertyCache_.subtitleAssOverride());
    emit audioDelayChanged(propertyCache_.audioDelay());
    emit titleChanged(propertyCache_.mediaTitle());
    emitDiagnosticsChanged(true);
    emit playlistChanged(propertyCache_.playlist(), propertyCache_.currentPlaylistIndex());
    emit chaptersChanged(propertyCache_.chapters(), propertyCache_.currentChapterIndex());
    emit tracksChanged(propertyCache_.tracks());
}

bool MpvCore::setProperty(const char *name, const mpv_format format, void *value)
{
    const int result = mpv_set_property(handle_, name, format, value);
    if (result < 0) {
        emitMpvError(QStringLiteral("mpv property update failed for '%1'").arg(QString::fromUtf8(name)), result);
        return false;
    }

    return true;
}

bool MpvCore::setCommandProperty(const QString &name, const QString &value)
{
    if (name.trimmed().isEmpty()) {
        return false;
    }

    return sendCommand({QStringLiteral("set"), name.trimmed(), value});
}

void MpvCore::emitMpvError(const QString &context, const int errorCode)
{
    emit errorOccurred(QStringLiteral("%1: %2")
                           .arg(context, QString::fromUtf8(mpv_error_string(errorCode))));
}

void MpvCore::setStringOption(const char *name, const char *value) const
{
    if (handle_ != nullptr && name != nullptr && value != nullptr) {
        const int result = mpv_set_option_string(handle_, name, value);
        if (result < 0) {
            const_cast<MpvCore *>(this)->emitMpvError(
                QStringLiteral("mpv option setup failed for '%1'").arg(QString::fromUtf8(name)),
                result);
        }
    }
}

void MpvCore::setStringOption(const char *name, const QByteArray &value) const
{
    setStringOption(name, value.constData());
}

bool MpvCore::sendCommand(const QStringList &arguments)
{
    if (handle_ == nullptr || arguments.isEmpty()) {
        return false;
    }

    QList<QByteArray> utf8Arguments;
    utf8Arguments.reserve(arguments.size());

    QVector<const char *> rawArguments;
    rawArguments.reserve(arguments.size() + 1);

    for (const QString &argument : arguments) {
        utf8Arguments.push_back(argument.toUtf8());
    }

    for (const QByteArray &argument : utf8Arguments) {
        rawArguments.push_back(argument.constData());
    }
    rawArguments.push_back(nullptr);

    const int commandResult = mpv_command_async(handle_, 0, rawArguments.data());
    if (commandResult < 0) {
        emit errorOccurred(QStringLiteral("mpv command failed: %1")
                               .arg(QString::fromUtf8(mpv_error_string(commandResult))));
        return false;
    }

    return true;
}

bool MpvCore::ensureReady()
{
    if (handle_ != nullptr && initialized_) {
        return true;
    }

    emit errorOccurred(QStringLiteral("Playback backend is not initialized."));
    return false;
}

}  // namespace revaplayer::infrastructure::mpv
