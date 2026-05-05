#include "infrastructure/mpv/MpvRenderHost.hpp"

#include <QByteArray>
#include <QMetaObject>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>

#include <algorithm>
#include <cmath>

#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>

namespace revaplayer::infrastructure::mpv {
namespace {

QString renderErrorMessage(const QString &context, const int errorCode)
{
    return QStringLiteral("%1: %2").arg(context, QString::fromUtf8(mpv_error_string(errorCode)));
}

}  // namespace

MpvRenderHost::MpvRenderHost(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setObjectName(QStringLiteral("mpvRenderHost"));
    setContextMenuPolicy(Qt::CustomContextMenu);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
    setAutoFillBackground(false);
    setMinimumSize(1, 1);
    setTextureFormat(GL_RGBA8);

    QSurfaceFormat surfaceFormat = format();
    surfaceFormat.setRenderableType(QSurfaceFormat::OpenGL);
    surfaceFormat.setProfile(QSurfaceFormat::CompatibilityProfile);
    surfaceFormat.setVersion(2, 1);
    surfaceFormat.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    surfaceFormat.setSwapInterval(1);
    surfaceFormat.setRedBufferSize(8);
    surfaceFormat.setGreenBufferSize(8);
    surfaceFormat.setBlueBufferSize(8);
    surfaceFormat.setAlphaBufferSize(8);
    surfaceFormat.setDepthBufferSize(24);
    surfaceFormat.setStencilBufferSize(8);
    surfaceFormat.setSamples(0);
    setFormat(surfaceFormat);

    connect(this, &QOpenGLWidget::frameSwapped, this, [this]() {
        if (renderContext_ != nullptr) {
            mpv_render_context_report_swap(renderContext_);
        }
    });
}

MpvRenderHost::~MpvRenderHost()
{
    if (context() != nullptr) {
        makeCurrent();
        cleanupRenderer();
        doneCurrent();
    } else {
        cleanupRenderer();
    }
}

void MpvRenderHost::setMpvHandle(mpv_handle *handle)
{
    mpvHandle_ = handle;

    if (glInitialized_ && context() != nullptr) {
        makeCurrent();
        initializeRenderer();
        doneCurrent();
        update();
    }
}

bool MpvRenderHost::isRendererReady() const
{
    return rendererReady_;
}

void MpvRenderHost::initializeGL()
{
    glInitialized_ = true;

    if (context() != nullptr) {
        connect(context(), &QOpenGLContext::aboutToBeDestroyed,
                this, &MpvRenderHost::onContextAboutToBeDestroyed,
                Qt::DirectConnection);
    }

    initializeRenderer();
}

void MpvRenderHost::paintGL()
{
    frameUpdateQueued_.store(false);

    if (auto *glContext = QOpenGLContext::currentContext(); glContext != nullptr) {
        if (auto *functions = glContext->functions(); functions != nullptr) {
            functions->glDisable(GL_SCISSOR_TEST);
            functions->glDisable(GL_DEPTH_TEST);
            functions->glDisable(GL_STENCIL_TEST);
            functions->glDisable(GL_BLEND);
            functions->glViewport(
                0,
                0,
                std::max(1, static_cast<int>(std::lround(width() * devicePixelRatioF()))),
                std::max(1, static_cast<int>(std::lround(height() * devicePixelRatioF()))));
            functions->glClearColor(0.02f, 0.02f, 0.02f, 1.0f);
            functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        }
    }

    if (renderContext_ == nullptr) {
        return;
    }

    mpv_render_context_update(renderContext_);

    const qreal scaleFactor = devicePixelRatioF();
    mpv_opengl_fbo fbo {
        static_cast<int>(defaultFramebufferObject()),
        std::max(1, static_cast<int>(std::lround(width() * scaleFactor))),
        std::max(1, static_cast<int>(std::lround(height() * scaleFactor))),
        GL_RGBA8,
    };
    // QOpenGLWidget ultimately presents through a framebuffer with inverted Y
    // orientation relative to mpv's render output.
    int flipY = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flipY},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    const int renderResult = mpv_render_context_render(renderContext_, params);
    if (renderResult < 0) {
        emit rendererError(renderErrorMessage(QStringLiteral("mpv render failed"), renderResult));
    }
}

void MpvRenderHost::resizeGL(int, int)
{
    update();
}

void MpvRenderHost::mouseDoubleClickEvent(QMouseEvent *event)
{
    emit doubleClicked(event != nullptr ? event->button() : Qt::NoButton);
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void MpvRenderHost::requestFrameUpdate()
{
    update();
}

void MpvRenderHost::onContextAboutToBeDestroyed()
{
    if (context() != nullptr) {
        makeCurrent();
        cleanupRenderer();
        doneCurrent();
        return;
    }

    cleanupRenderer();
}

void *MpvRenderHost::getProcAddress(void *, const char *name)
{
    const auto *currentContext = QOpenGLContext::currentContext();
    if (currentContext == nullptr || name == nullptr) {
        return nullptr;
    }

    union {
        QFunctionPointer function;
        void *pointer;
    } address {};

    address.function = currentContext->getProcAddress(QByteArray(name));
    return address.pointer;
}

void MpvRenderHost::onRenderUpdate(void *ctx)
{
    auto *host = static_cast<MpvRenderHost *>(ctx);
    if (host == nullptr) {
        return;
    }

    if (host->frameUpdateQueued_.exchange(true)) {
        return;
    }

    QMetaObject::invokeMethod(host, &MpvRenderHost::requestFrameUpdate, Qt::QueuedConnection);
}

void MpvRenderHost::initializeRenderer()
{
    if (!glInitialized_ || mpvHandle_ == nullptr || renderContext_ != nullptr) {
        return;
    }

    mpv_opengl_init_params glInitParams {
        .get_proc_address = &MpvRenderHost::getProcAddress,
        .get_proc_address_ctx = nullptr,
    };
    auto *apiType = const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL);
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, apiType},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInitParams},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    const int createResult = mpv_render_context_create(&renderContext_, mpvHandle_, params);
    if (createResult < 0) {
        emit rendererError(renderErrorMessage(QStringLiteral("Failed to create mpv render context"), createResult));
        renderContext_ = nullptr;
        rendererReady_ = false;
        return;
    }

    mpv_render_context_set_update_callback(renderContext_, &MpvRenderHost::onRenderUpdate, this);
    rendererReady_ = true;
    emit rendererReady();
    update();
}

void MpvRenderHost::cleanupRenderer()
{
    frameUpdateQueued_.store(false);

    if (renderContext_ != nullptr) {
        mpv_render_context_set_update_callback(renderContext_, nullptr, nullptr);
        mpv_render_context_free(renderContext_);
        renderContext_ = nullptr;
    }

    rendererReady_ = false;
}

}  // namespace revaplayer::infrastructure::mpv
