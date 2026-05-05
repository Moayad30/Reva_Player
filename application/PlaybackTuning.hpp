#pragma once

#include <QString>

namespace revaplayer::application {

inline constexpr int kDefaultPlaybackVolume = 100;
inline constexpr int kMaximumPlaybackVolume = 150;

[[nodiscard]] double clampPlaybackSpeed(double speed);
[[nodiscard]] int clampPlaybackVolume(int volume);
[[nodiscard]] QString formatPlaybackRate(double speed);
[[nodiscard]] double clampVideoZoomFactor(double factor);
[[nodiscard]] double clampVideoViewportAlignment(double alignment);
[[nodiscard]] double videoZoomFactorToLog2(double factor);
[[nodiscard]] QString formatScaleFactor(double factor);
[[nodiscard]] QString formatSignedSeconds(double seconds);
[[nodiscard]] int normalizeRightAngleRotation(int degrees);

}  // namespace revaplayer::application
