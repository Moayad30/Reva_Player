#include "infrastructure/mpv/MpvEventBridge.hpp"

#include <QMetaObject>

#include <mpv/client.h>

namespace revaplayer::infrastructure::mpv {

MpvEventBridge::MpvEventBridge(mpv_handle *handle, QObject *parent)
    : QObject(parent)
    , handle_(handle)
{
    if (handle_ != nullptr) {
        mpv_set_wakeup_callback(handle_, &MpvEventBridge::onWakeup, this);
    }
}

MpvEventBridge::~MpvEventBridge()
{
    if (handle_ != nullptr) {
        mpv_set_wakeup_callback(handle_, nullptr, nullptr);
    }
}

void MpvEventBridge::dispatchWakeup()
{
    wakeupPending_.store(false);
    emit wakeup();
}

void MpvEventBridge::onWakeup(void *context)
{
    auto *bridge = static_cast<MpvEventBridge *>(context);
    if (bridge == nullptr) {
        return;
    }

    if (bridge->wakeupPending_.exchange(true)) {
        return;
    }

    QMetaObject::invokeMethod(bridge, &MpvEventBridge::dispatchWakeup, Qt::QueuedConnection);
}

}  // namespace revaplayer::infrastructure::mpv

