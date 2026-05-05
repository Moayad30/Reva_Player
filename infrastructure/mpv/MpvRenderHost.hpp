#pragma once

#include <QOpenGLWidget>

#include <atomic>

struct mpv_handle;
struct mpv_render_context;

class QMouseEvent;

namespace revaplayer::infrastructure::mpv {

class MpvRenderHost final : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit MpvRenderHost(QWidget *parent = nullptr);
    ~MpvRenderHost() override;

    void setMpvHandle(mpv_handle *handle);
    [[nodiscard]] bool isRendererReady() const;

signals:
    void doubleClicked(Qt::MouseButton button);
    void rendererReady();
    void rendererError(const QString &message);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void requestFrameUpdate();
    void onContextAboutToBeDestroyed();

private:
    static void *getProcAddress(void *ctx, const char *name);
    static void onRenderUpdate(void *ctx);
    void initializeRenderer();
    void cleanupRenderer();

    mpv_handle *mpvHandle_ {nullptr};
    mpv_render_context *renderContext_ {nullptr};
    bool rendererReady_ {false};
    bool glInitialized_ {false};
    std::atomic_bool frameUpdateQueued_ {false};
};

}  // namespace revaplayer::infrastructure::mpv
