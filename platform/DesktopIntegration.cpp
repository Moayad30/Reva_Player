#include "platform/DesktopIntegration.hpp"

#include <QGuiApplication>

namespace revaplayer::platform {

QString DesktopIntegration::platformName() const
{
    return QGuiApplication::platformName();
}

}  // namespace revaplayer::platform

