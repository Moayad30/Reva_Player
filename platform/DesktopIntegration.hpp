#pragma once

#include <QString>

namespace revaplayer::platform {

class DesktopIntegration final {
public:
    [[nodiscard]] QString platformName() const;
};

}  // namespace revaplayer::platform

