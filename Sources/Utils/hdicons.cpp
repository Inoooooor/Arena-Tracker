#include "hdicons.h"
#include "hdimages.h"
#include "../themehandler.h"
#include "../utility.h"
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QMap>
#include <functional>

#define HD_ICON_SIZE 128


//Paints the HD pixmap when available, else the theme file
class HDIconEngine : public QIconEngine
{
public:
    HDIconEngine(std::function<QPixmap()> hdPixmap, const QString &fallbackFile) :
        hdPixmap(hdPixmap), fallbackFile(fallbackFile) {}

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        Q_UNUSED(mode); Q_UNUSED(state);
        QPixmap pixmap = hdPixmap();
        if(pixmap.isNull())     pixmap = QPixmap(fallbackFile);
        if(pixmap.isNull())     return;
        painter->save();
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawPixmap(rect, pixmap);
        painter->restore();
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        QPixmap canvas(QSizeF(size.width()*scale, size.height()*scale).toSize());
        canvas.fill(Qt::transparent);
        QPainter painter(&canvas);
        paint(&painter, QRect(QPoint(0,0), canvas.size()), mode, state);
        painter.end();
        canvas.setDevicePixelRatio(scale);
        return canvas;
    }

    QIconEngine *clone() const override
    {
        return new HDIconEngine(hdPixmap, fallbackFile);
    }

private:
    std::function<QPixmap()> hdPixmap;
    QString fallbackFile;
};


//Art in a circle with a gold ring; null until the art is downloaded
static QPixmap circleArt(const QString &code, qreal zoom)
{
    static QMap<QString, QPixmap> cache;
    if(cache.contains(code))    return cache[code];

    const QString file = HDImages::path(HDImages::Portrait, code);
    if(file.isEmpty())  return QPixmap();
    QPixmap art(file);
    if(art.isNull())    return QPixmap();

    const qreal s = HD_ICON_SIZE;
    QPixmap canvas(HD_ICON_SIZE, HD_ICON_SIZE);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const qreal ring = s*0.06;
    QRectF inner(ring, ring, s - 2*ring, s - 2*ring);
    QPainterPath clip;
    clip.addEllipse(inner);
    painter.setClipPath(clip);
    //Center of the art, zoomed in
    qreal side = qMin(art.width(), art.height())/zoom;
    painter.drawPixmap(inner, art, QRectF((art.width()-side)/2, (art.height()-side)/2, side, side));
    painter.setClipping(false);

    QLinearGradient gold(0, 0, 0, s);
    gold.setColorAt(0, QColor(250, 222, 140));
    gold.setColorAt(0.5, QColor(196, 150, 60));
    gold.setColorAt(1, QColor(120, 82, 28));
    painter.setPen(QPen(QBrush(gold), ring));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRectF(ring/2, ring/2, s - ring, s - ring));
    painter.end();

    cache[code] = canvas;
    return canvas;
}


static QPixmap drawnIcon(const QString &key, std::function<void(QPainter &, qreal)> draw)
{
    static QMap<QString, QPixmap> cache;
    if(!cache.contains(key))
    {
        QPixmap canvas(HD_ICON_SIZE, HD_ICON_SIZE);
        canvas.fill(Qt::transparent);
        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        draw(painter, HD_ICON_SIZE);
        painter.end();
        cache[key] = canvas;
    }
    return cache[key];
}


//A stroke with a dark outline under it
static void strokeMark(QPainter &painter, const QPainterPath &path, qreal width, const QColor &light, const QColor &dark, qreal s)
{
    QPen outline(QColor(30, 20, 10), width + s*0.07, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.strokePath(path, outline);
    QLinearGradient gradient(0, 0, 0, s);
    gradient.setColorAt(0, light);
    gradient.setColorAt(1, dark);
    painter.strokePath(path, QPen(QBrush(gradient), width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
}


QIcon HDIcons::hero(int classOrder)
{
    const QString code = QStringLiteral("HERO_%1").arg(Utility::classOrder2classLogNumber(classOrder));
    return QIcon(new HDIconEngine([code]() { return circleArt(code, 1.25); }, ThemeHandler::heroFile(classOrder)));
}


QIcon HDIcons::hero(const QString &heroLog)
{
    return hero(Utility::classLogNumber2classOrder(heroLog));
}


QIcon HDIcons::coin()
{
    return QIcon(new HDIconEngine([]() { return circleArt("GAME_005", 1.6); }, ThemeHandler::coinFile()));
}


QIcon HDIcons::first()
{
    return QIcon(new HDIconEngine([]() {
        return drawnIcon("first", [](QPainter &painter, qreal s) {
            QRadialGradient gold(s*0.4, s*0.35, s*0.6);
            gold.setColorAt(0, QColor(255, 236, 160));
            gold.setColorAt(0.6, QColor(222, 170, 60));
            gold.setColorAt(1, QColor(150, 98, 30));
            painter.setPen(QPen(QColor(110, 70, 20), s*0.05));
            painter.setBrush(gold);
            painter.drawEllipse(QRectF(s*0.06, s*0.06, s*0.88, s*0.88));
            painter.setPen(QPen(QColor(170, 118, 40, 180), s*0.025));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(QRectF(s*0.16, s*0.16, s*0.68, s*0.68));
            QFont font("Georgia");
            font.setBold(true);
            font.setPixelSize(s*0.56);
            painter.setFont(font);
            painter.setPen(QColor(120, 76, 20));
            painter.drawText(QRectF(0, s*0.02, s, s), Qt::AlignCenter, "1");
        });
    }, ThemeHandler::firstFile()));
}


QIcon HDIcons::win()
{
    return QIcon(new HDIconEngine([]() {
        return drawnIcon("win", [](QPainter &painter, qreal s) {
            QPainterPath check;
            check.moveTo(s*0.2, s*0.54);
            check.lineTo(s*0.42, s*0.76);
            check.lineTo(s*0.82, s*0.28);
            strokeMark(painter, check, s*0.17, QColor(150, 235, 90), QColor(50, 150, 30), s);
        });
    }, ThemeHandler::winFile()));
}


QIcon HDIcons::lose()
{
    return QIcon(new HDIconEngine([]() {
        return drawnIcon("lose", [](QPainter &painter, qreal s) {
            QPainterPath cross;
            cross.moveTo(s*0.24, s*0.24);
            cross.lineTo(s*0.76, s*0.76);
            cross.moveTo(s*0.76, s*0.24);
            cross.lineTo(s*0.24, s*0.76);
            strokeMark(painter, cross, s*0.17, QColor(255, 110, 90), QColor(170, 20, 20), s);
        });
    }, ThemeHandler::loseFile()));
}


//Asks for the art early, so it is there when the icons are painted
void HDIcons::prefetch()
{
    for(int i=0; i<NUM_HEROS; i++)
    {
        HDImages::path(HDImages::Portrait, QStringLiteral("HERO_%1").arg(Utility::classOrder2classLogNumber(i)));
    }
    HDImages::path(HDImages::Portrait, "GAME_005");
}
