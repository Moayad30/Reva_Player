#pragma once

#include "domain/PlayerProfile.hpp"

namespace revaplayer::services::media {

struct ThumbnailPolicy final {
    int debounceIntervalMs {140};
    int pollIntervalMs {75};
    int previewWidth {224};
    int memoryCacheEntries {72};
};

[[nodiscard]] ThumbnailPolicy thumbnailPolicyForProfile(revaplayer::domain::PlayerProfile profile);

}  // namespace revaplayer::services::media
