#pragma once

#include <QString>

namespace revaplayer::domain {

enum class PlayerProfile {
    Battery,
    Balanced,
    Quality,
};

[[nodiscard]] inline QString playerProfileId(const PlayerProfile profile)
{
    switch (profile) {
    case PlayerProfile::Battery:
        return QStringLiteral("battery");
    case PlayerProfile::Balanced:
        return QStringLiteral("balanced");
    case PlayerProfile::Quality:
        return QStringLiteral("quality");
    }

    return QStringLiteral("balanced");
}

[[nodiscard]] inline QString playerProfileLabel(const PlayerProfile profile)
{
    switch (profile) {
    case PlayerProfile::Battery:
        return QStringLiteral("Battery");
    case PlayerProfile::Balanced:
        return QStringLiteral("Balanced");
    case PlayerProfile::Quality:
        return QStringLiteral("Quality");
    }

    return QStringLiteral("Balanced");
}

[[nodiscard]] inline PlayerProfile playerProfileFromId(const QString &rawId)
{
    const QString id = rawId.trimmed().toLower();
    if (id == QStringLiteral("battery")) {
        return PlayerProfile::Battery;
    }
    if (id == QStringLiteral("quality")) {
        return PlayerProfile::Quality;
    }

    return PlayerProfile::Balanced;
}

}  // namespace revaplayer::domain
