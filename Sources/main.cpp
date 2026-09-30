#include "mainwindow.h"
#include "utility.h"
#include <QApplication>
#include <QSplashScreen>
#include <QStyleFactory>


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));

    QPixmap pixmap(":/Images/splash.png");
    QSplashScreen splash(pixmap, Qt::WindowStaysOnTopHint);
    splash.show();
    app.processEvents();

    MainWindow window;
#ifdef OLD_TRACKER_WINDOWS
    window.show();
    splash.finish(&window);
#else
    //Only the mascot is shown (MainWindow::init)
    splash.close();
#endif

    return app.exec();
}

