#pragma once

#include <QMetaType>

namespace revaplayer::domain {

enum class PlaybackEndReason {
    Unknown = 0,
    ReachedEndOfFile,
    Stopped,
    BackendQuit,
    Redirected,
    Error,
};

[[nodiscard]] constexpr bool playbackEndReasonRepresentsCompletion(const PlaybackEndReason reason)
{
    return reason == PlaybackEndReason::ReachedEndOfFile;
}

[[nodiscard]] constexpr bool playbackEndReasonIsFailure(const PlaybackEndReason reason)
{
    return reason == PlaybackEndReason::Error;
}

}  // namespace revaplayer::domain

Q_DECLARE_METATYPE(revaplayer::domain::PlaybackEndReason)
