#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
	app.setApplicationName("Assimp Qt Viewer");
    app.setWindowIcon(QIcon(":/icons/res/icon.png"));
    MainWindow w;
    w.showMaximized();
    return app.exec();
}
