#include "mainwindow.h"
#include "utility.h"
#include "Widgets/mascotwindow.h"
#include <QApplication>
#include <QSplashScreen>
#include <QStyleFactory>
#include <QTimer>
#include <QPainter>


//The mascot blowing the dust off the game box, with "Starting..." in a pixel bubble in the empty top right corner
static QPixmap mascotSplash()
{
    const qreal dpr = 2;    //The art is drawn at twice its size in points
    QPixmap art(":/Images/Mascot/splash.png");
    QPixmap pixmap(art.size());
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.drawPixmap(0, 0, art.width()/dpr, art.height()/dpr, art);
    const QString text = "Starting...";
    const QFont font = MascotWindow::pixelFont(20);
    const QFontMetrics fm(font);
    const QSize size(fm.horizontalAdvance(text) + 24, fm.height() + 12);
    const QRect bubble(QPoint(static_cast<int>(art.width()/dpr) - size.width(), 0), size);
    MascotWindow::drawPixelFrame(painter, bubble, Qt::white);
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(bubble, Qt::AlignCenter, text);
    return pixmap;
}


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));

#ifdef OLD_TRACKER_WINDOWS
    QSplashScreen splash(QPixmap(":/Images/splash.png"), Qt::WindowStaysOnTopHint);
#else
    QSplashScreen splash(mascotSplash(), Qt::WindowStaysOnTopHint);
    splash.setAttribute(Qt::WA_TranslucentBackground);
#endif
    splash.show();
    app.processEvents();

    MainWindow window;
#ifdef OLD_TRACKER_WINDOWS
    window.show();
    splash.finish(&window);
#else
    //Only the mascot is shown, by MainWindow::init a second later: the splash stays until then
    QTimer::singleShot(1000, &splash, &QSplashScreen::close);
#endif

    return app.exec();
}

