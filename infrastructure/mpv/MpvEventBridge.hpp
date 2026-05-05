#pragma once

#include <QObject>

#include <atomic>

struct mpv_handle;

namespace revaplayer::infrastructure::mpv {

class MpvEventBridge final : public QObject {
    Q_OBJECT

public:
    explicit MpvEventBridge(mpv_handle *handle, QObject *parent = nullptr);
    ~MpvEventBridge() override;

signals:
    void wakeup();

private slots:
    void dispatchWakeup();

private:
    static void onWakeup(void *context);

    mpv_handle *handle_ {nullptr};
    std::atomic_bool wakeupPending_ {false};
};

}  // namespace revaplayer::infrastructure::mpv

