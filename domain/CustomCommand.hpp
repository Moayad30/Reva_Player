#pragma once

#include <QString>

namespace revaplayer::domain {

struct CustomCommand final {
    qint64 id {-1};
    QString name;
    QString script;
};

}  // namespace revaplayer::domain
