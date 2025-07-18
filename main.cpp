#include "MainWindow.h"
#include <QApplication>
#include <QFileInfo>
#include <QImageReader>


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

#if QT_VERSION_MAJOR == 6
    // Disable allocation limit for images
    QImageReader::setAllocationLimit(0);
#endif

    QCoreApplication::setOrganizationName("Sharjith N");
    app.setApplicationName("Assimp Qt Viewer");
    app.setWindowIcon(QIcon(":/icons/res/icon.png"));
    MainWindow w;
    w.showMaximized();

    if (argc > 1)
    {
        QString fileName(argv[1]);
        QFileInfo fi(fileName);
        if (fi.exists())
        {
            w.openFile(fileName);
        }
    }

    return app.exec();
}
