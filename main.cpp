#include "MainWindow.h"
#include <QApplication>

int main(int argc, char *argv[]) {

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 3); // or your desired OpenGL version
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setOption(QSurfaceFormat::DebugContext);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setRenderableType(QSurfaceFormat::OpenGL);

    QApplication app(argc, argv);
    
    QCoreApplication::setOrganizationName("Sharjith N");
	app.setApplicationName("Assimp Qt Viewer");
    app.setWindowIcon(QIcon(":/icons/res/icon.png"));
    MainWindow w;
    w.showMaximized();
    return app.exec();
}
