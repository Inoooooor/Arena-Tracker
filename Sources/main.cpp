#include "mainwindow.h"
#include "utility.h"
#include "Widgets/splashwindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <QTimer>


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));

    //Deleted when closed
    SplashWindow *splash = new SplashWindow();
    splash->show();
    app.processEvents();

    MainWindow window;
    window.setSplashOpen();
    QObject::connect(splash, &QObject::destroyed, &window, &MainWindow::splashClosed);
    QObject::connect(&window, &MainWindow::startupProgress, splash, &SplashWindow::setProgress);
    QObject::connect(&window, &MainWindow::startupReady, splash, &SplashWindow::ready);
#ifdef OLD_TRACKER_WINDOWS
    window.show();
#endif

    return app.exec();
}

