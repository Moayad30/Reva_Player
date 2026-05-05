#include "services/media/ThumbnailPolicy.hpp"

namespace revaplayer::services::media {

ThumbnailPolicy thumbnailPolicyForProfile(const revaplayer::domain::PlayerProfile profile)
{
    switch (profile) {
    case revaplayer::domain::PlayerProfile::Battery:
        return ThumbnailPolicy {
            .debounceIntervalMs = 180,
            .pollIntervalMs = 100,
            .previewWidth = 224,
            .memoryCacheEntries = 32,
        };
    case revaplayer::domain::PlayerProfile::Balanced:
        return ThumbnailPolicy {
            .debounceIntervalMs = 120,
            .pollIntervalMs = 75,
            .previewWidth = 352,
            .memoryCacheEntries = 160,
        };
    case revaplayer::domain::PlayerProfile::Quality:
        return ThumbnailPolicy {
            .debounceIntervalMs = 75,
            .pollIntervalMs = 50,
            .previewWidth = 512,
            .memoryCacheEntries = 320,
        };
    }

    return ThumbnailPolicy {};
}

}  // namespace revaplayer::services::media
