#include "application/PlaybackController.hpp"

#include "application/CustomCommandScript.hpp"
#include "infrastructure/mpv/MpvCore.hpp"

namespace revaplayer::application {

PlaybackController::PlaybackController(QObject *parent)
    : QObject(parent)
    , mpvCore_(std::make_unique<revaplayer::infrastructure::mpv::MpvCore>(this))
{
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::loadStarted, this, &PlaybackController::loadStarted);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::clientMessageReceived, this, &PlaybackController::clientMessageReceived);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::idleChanged, this, &PlaybackController::idleChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::pausedChanged, this, &PlaybackController::pausedChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::playbackPositionChanged, this, &PlaybackController::playbackPositionChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::volumeChanged, this, &PlaybackController::volumeChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::mutedChanged, this, &PlaybackController::mutedChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::speedChanged, this, &PlaybackController::speedChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::subtitleDelayChanged, this, &PlaybackController::subtitleDelayChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::subtitleVisibilityChanged, this, &PlaybackController::subtitleVisibilityChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::subtitleScaleChanged, this, &PlaybackController::subtitleScaleChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::subtitlePositionChanged, this, &PlaybackController::subtitlePositionChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::subtitleFontFamilyChanged, this, &PlaybackController::subtitleFontFamilyChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::subtitleFontSizeChanged, this, &PlaybackController::subtitleFontSizeChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::subtitleAssOverrideChanged, this, &PlaybackController::subtitleAssOverrideChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::audioDelayChanged, this, &PlaybackController::audioDelayChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::titleChanged, this, &PlaybackController::titleChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::diagnosticsChanged, this, &PlaybackController::diagnosticsChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::playlistChanged, this, &PlaybackController::playlistChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::chaptersChanged, this, &PlaybackController::chaptersChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::tracksChanged, this, &PlaybackController::tracksChanged);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::fileLoaded, this, &PlaybackController::fileLoaded);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::fileEnded, this, &PlaybackController::fileEnded);
    connect(mpvCore_.get(), &revaplayer::infrastructure::mpv::MpvCore::errorOccurred, this, &PlaybackController::errorOccurred);
}

PlaybackController::~PlaybackController() = default;

void PlaybackController::setUseExternalMpvConfig(const bool enabled)
{
    if (mpvCore_ != nullptr) {
        mpvCore_->setUseExternalMpvConfig(enabled);
    }
}

bool PlaybackController::initialize(revaplayer::infrastructure::mpv::MpvRenderHost *renderHost)
{
    return mpvCore_->initialize(renderHost);
}

bool PlaybackController::isInitialized() const
{
    return mpvCore_ != nullptr && mpvCore_->isInitialized();
}

void PlaybackController::openFiles(const QStringList &paths)
{
    mpvCore_->loadFiles(paths);
}

void PlaybackController::openUrl(const QString &url)
{
    mpvCore_->loadUrl(url);
}

void PlaybackController::togglePause()
{
    mpvCore_->togglePause();
}

void PlaybackController::stop()
{
    mpvCore_->stop();
}

bool PlaybackController::executeMpvCommand(const QStringList &arguments)
{
    return mpvCore_->executeCommand(arguments);
}

bool PlaybackController::executeCustomCommandScript(const QString &script, QString *errorMessage)
{
    const QVector<QStringList> commands = revaplayer::application::parseCustomCommandScript(script, errorMessage);
    if (commands.isEmpty()) {
        return false;
    }

    for (const QStringList &command : commands) {
        if (!mpvCore_->executeCommand(command)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("The custom command could not be sent to mpv.");
            }
            return false;
        }
    }

    return true;
}

void PlaybackController::loadSubtitleFile(const QString &filePath)
{
    mpvCore_->loadSubtitleFile(filePath);
}

bool PlaybackController::captureScreenshot(const QString &filePath)
{
    return mpvCore_->captureScreenshot(filePath);
}

void PlaybackController::seekToSeconds(const double seconds)
{
    mpvCore_->seekToSeconds(seconds);
}

void PlaybackController::seekToFraction(const double fraction)
{
    mpvCore_->seekToFraction(fraction);
}

void PlaybackController::seekBySeconds(const int seconds)
{
    mpvCore_->seekBySeconds(seconds);
}

void PlaybackController::setVolume(const int volume)
{
    mpvCore_->setVolume(volume);
}

void PlaybackController::adjustVolume(const int delta)
{
    mpvCore_->adjustVolume(delta);
}

void PlaybackController::setMuted(const bool muted)
{
    mpvCore_->setMuted(muted);
}

void PlaybackController::toggleMuted()
{
    mpvCore_->toggleMuted();
}

void PlaybackController::setSpeed(const double speed)
{
    mpvCore_->setSpeed(speed);
}

void PlaybackController::adjustSpeed(const double delta)
{
    mpvCore_->adjustSpeed(delta);
}

void PlaybackController::resetSpeed()
{
    mpvCore_->resetSpeed();
}

void PlaybackController::setSubtitleDelay(const double seconds)
{
    mpvCore_->setSubtitleDelay(seconds);
}

void PlaybackController::adjustSubtitleDelay(const double deltaSeconds)
{
    mpvCore_->adjustSubtitleDelay(deltaSeconds);
}

void PlaybackController::resetSubtitleDelay()
{
    mpvCore_->resetSubtitleDelay();
}

void PlaybackController::setSubtitleVisible(const bool visible)
{
    mpvCore_->setSubtitleVisible(visible);
}

void PlaybackController::toggleSubtitleVisible()
{
    mpvCore_->toggleSubtitleVisible();
}

void PlaybackController::setSubtitleScale(const double scale)
{
    mpvCore_->setSubtitleScale(scale);
}

void PlaybackController::adjustSubtitleScale(const double delta)
{
    mpvCore_->adjustSubtitleScale(delta);
}

void PlaybackController::resetSubtitleScale()
{
    mpvCore_->resetSubtitleScale();
}

void PlaybackController::setSubtitlePosition(const int position)
{
    mpvCore_->setSubtitlePosition(position);
}

void PlaybackController::adjustSubtitlePosition(const int delta)
{
    mpvCore_->adjustSubtitlePosition(delta);
}

void PlaybackController::resetSubtitlePosition()
{
    mpvCore_->resetSubtitlePosition();
}

void PlaybackController::setSubtitleFontFamily(const QString &fontFamily)
{
    mpvCore_->setSubtitleFontFamily(fontFamily);
}

void PlaybackController::setSubtitleFontSize(const int fontSize)
{
    mpvCore_->setSubtitleFontSize(fontSize);
}

void PlaybackController::setSubtitleAssOverride(const QString &mode)
{
    mpvCore_->setSubtitleAssOverride(mode);
}

void PlaybackController::cycleSubtitleAssOverride()
{
    mpvCore_->cycleSubtitleAssOverride();
}

void PlaybackController::setAudioDelay(const double seconds)
{
    mpvCore_->setAudioDelay(seconds);
}

void PlaybackController::adjustAudioDelay(const double deltaSeconds)
{
    mpvCore_->adjustAudioDelay(deltaSeconds);
}

void PlaybackController::resetAudioDelay()
{
    mpvCore_->resetAudioDelay();
}

void PlaybackController::setLoopStart(const double seconds)
{
    mpvCore_->setLoopStart(seconds);
}

void PlaybackController::setLoopEnd(const double seconds)
{
    mpvCore_->setLoopEnd(seconds);
}

void PlaybackController::clearLoop()
{
    mpvCore_->clearLoop();
}

void PlaybackController::frameStepForward()
{
    mpvCore_->frameStepForward();
}

void PlaybackController::frameStepBackward()
{
    mpvCore_->frameStepBackward();
}

void PlaybackController::setRepeatMode(const QString &mode)
{
    mpvCore_->setRepeatMode(mode);
}

void PlaybackController::setVideoAspectOverride(const QString &aspectOverride)
{
    mpvCore_->setVideoAspectOverride(aspectOverride);
}

void PlaybackController::resetVideoAspectOverride()
{
    mpvCore_->resetVideoAspectOverride();
}

void PlaybackController::setVideoCrop(const QString &crop)
{
    mpvCore_->setVideoCrop(crop);
}

void PlaybackController::clearVideoCrop()
{
    mpvCore_->clearVideoCrop();
}

void PlaybackController::setVideoZoomFactor(const double factor)
{
    mpvCore_->setVideoZoomFactor(factor);
}

void PlaybackController::resetVideoZoom()
{
    mpvCore_->resetVideoZoom();
}

void PlaybackController::setVideoAlignment(const double horizontal, const double vertical)
{
    mpvCore_->setVideoAlignment(horizontal, vertical);
}

void PlaybackController::resetVideoAlignment()
{
    mpvCore_->resetVideoAlignment();
}

void PlaybackController::setVideoRotation(const int degrees)
{
    mpvCore_->setVideoRotation(degrees);
}

void PlaybackController::setDeinterlace(const bool enabled)
{
    mpvCore_->setDeinterlace(enabled);
}

void PlaybackController::nextPlaylistItem()
{
    mpvCore_->nextPlaylistItem();
}

void PlaybackController::previousPlaylistItem()
{
    mpvCore_->previousPlaylistItem();
}

void PlaybackController::selectPlaylistIndex(const int index)
{
    mpvCore_->selectPlaylistIndex(index);
}

void PlaybackController::nextChapter()
{
    mpvCore_->nextChapter();
}

void PlaybackController::previousChapter()
{
    mpvCore_->previousChapter();
}

void PlaybackController::selectChapter(const int index)
{
    mpvCore_->selectChapter(index);
}

void PlaybackController::selectTrack(const revaplayer::domain::TrackType type, const int trackId)
{
    mpvCore_->selectTrack(type, trackId);
}

}  // namespace revaplayer::application
