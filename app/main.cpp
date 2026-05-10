#include "app/AppBootstrap.hpp"

#include <clocale>
#include <cstring>
#include <cstdio>

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QSurfaceFormat>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

    QSurfaceFormat defaultFormat;
    defaultFormat.setRenderableType(QSurfaceFormat::OpenGL);
    defaultFormat.setProfile(QSurfaceFormat::CompatibilityProfile);
    defaultFormat.setVersion(2, 1);
    defaultFormat.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    defaultFormat.setSwapInterval(1);
    defaultFormat.setRedBufferSize(8);
    defaultFormat.setGreenBufferSize(8);
    defaultFormat.setBlueBufferSize(8);
    defaultFormat.setAlphaBufferSize(8);
    defaultFormat.setDepthBufferSize(24);
    defaultFormat.setStencilBufferSize(8);
    defaultFormat.setSamples(0);
    QSurfaceFormat::setDefaultFormat(defaultFormat);

    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--version") == 0 || std::strcmp(argv[index], "-v") == 0) {
            std::printf("Reva Player 1.0.0\n");
            return 0;
        }
        if (std::strcmp(argv[index], "--help") == 0 || std::strcmp(argv[index], "-h") == 0) {
            std::printf(
                "Usage: RevaPlayer [options] [media...]\n"
                "\n"
                "Linux desktop media player built with Qt Widgets and libmpv.\n"
                "\n"
                "Options:\n"
                "  -h, --help       Show this help message and exit.\n"
                "  -v, --version    Show version information and exit.\n"
                "  -u, --url <url>  Open a media URL on startup.\n"
            );
            return 0;
        }
    }

    QApplication application(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QApplication::setApplicationName(QStringLiteral("Reva Player"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setOrganizationName(QStringLiteral("RevaPlayer"));
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.moayad30.revaplayer"));
    QApplication::setWindowIcon(QIcon::fromTheme(
        QStringLiteral("revaplayer"),
        QIcon(QStringLiteral(":/icons/revaplayer.xpm"))));

    revaplayer::app::AppBootstrap bootstrap;
    return bootstrap.run(application, application.arguments());
}
