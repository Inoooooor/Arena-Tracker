#include "draftherowindow.h"
#include "mascotwindow.h"
#include "scorebutton.h"
#include "../Utils/hdicons.h"
#include <QtWidgets>


#define HERO_PLATE_WIDTH    156
#define HERO_PLATE_HEIGHT   54
#define HERO_PLATE_TOP      0.9     //Below the portrait, in portrait heights: under the class label and the name
#define HERO_HAND_SIZE      44
#define HERO_ICON_SIZE      28
#define HERO_CLOSE_WINRATE  1.5     //Points under the best winrate that still get a flat hand


DraftHeroWindow::DraftHeroWindow(QWidget *parent, const QList<QRect> &heroRects) :
    QMainWindow(parent, OVERLAY_WINDOW_FLAGS)
{
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
    setAttribute(Qt::WA_ShowWithoutActivating);
    //One window over the three plates
    QList<QRect> globalPlates;
    QRect area;
    for(const QRect &hero: heroRects)
    {
        QRect plate(0, 0, HERO_PLATE_WIDTH, HERO_PLATE_HEIGHT);
        plate.moveCenter(QPoint(hero.center().x(), 0));
        plate.moveTop(static_cast<int>(hero.bottom() + HERO_PLATE_TOP*hero.height()));
        globalPlates << plate;
        area = area.united(plate);
    }
    area.adjust(-4, -4, 4, 8);      //Room for the frames' shadow
    setGeometry(area);
    for(const QRect &plate: std::as_const(globalPlates))    plateRects << plate.translated(-area.topLeft());

    hands[0] = QPixmap(":/Images/Mascot/thumb_up.png");
    hands[1] = QPixmap(":/Images/Mascot/hand_flat.png");
    hands[2] = QPixmap(":/Images/Mascot/thumb_down.png");

    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setWindowTitle("AT Heroes");
}


DraftHeroWindow::~DraftHeroWindow()
{
}


int DraftHeroWindow::handFor(float rating, float bestRating)
{
    if(FLOATEQ(rating, bestRating))                 return 0;
    if(bestRating - rating <= HERO_CLOSE_WINRATE)   return 1;
    return 2;
}


void DraftHeroWindow::setScores(int classOrder[3])
{
    for(int i=0; i<3; i++)
    {
        this->classOrder[i] = classOrder[i];
        ratings[i] = ScoreButton::getHeroScore(classOrder[i]);
    }
    scoresShown = true;
    update();
}


void DraftHeroWindow::hideScores(bool quick)
{
    (void)quick;
    scoresShown = false;
    update();
}


void DraftHeroWindow::showTwitchScores(bool show)
{
    (void)show;
}


void DraftHeroWindow::showPlayerScores(bool show)
{
    (void)show;
}


void DraftHeroWindow::setTwitchScores(int vote1, int vote2, int vote3, QString username)
{
    (void)vote1; (void)vote2; (void)vote3; (void)username;
}


//[hand] [class icon] 55.6%
void DraftHeroWindow::paintEvent(QPaintEvent *)
{
    if(!scoresShown)    return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(MascotWindow::pixelFont(28));
    float bestRating = std::max(std::max(ratings[0], ratings[1]), ratings[2]);

    for(int i=0; i<3 && i<plateRects.count(); i++)
    {
        const QRect &plate = plateRects[i];
        MascotWindow::drawPixelFrame(painter, plate.translated(0, 3), Qt::black);      //Shadow
        MascotWindow::drawPixelFrame(painter, plate, Qt::white);

        int x = plate.x() + 8;
        const int midY = plate.center().y();
        if(ratings[i] > 0)
        {
            const QPixmap &hand = hands[handFor(ratings[i], bestRating)];
            QSize handSize = hand.size().scaled(HERO_HAND_SIZE, HERO_HAND_SIZE, Qt::KeepAspectRatio);
            painter.drawPixmap(QRect(QPoint(x + (HERO_HAND_SIZE - handSize.width())/2, midY - handSize.height()/2), handSize), hand);
        }
        x += HERO_HAND_SIZE + 6;

        QPixmap icon = HDIcons::hero(classOrder[i]).pixmap(QSize(HERO_ICON_SIZE, HERO_ICON_SIZE), devicePixelRatioF());
        painter.drawPixmap(QRect(x, midY - HERO_ICON_SIZE/2, HERO_ICON_SIZE, HERO_ICON_SIZE), icon);
        x += HERO_ICON_SIZE + 6;

        painter.setPen(Qt::black);
        QString text = (ratings[i] > 0) ? QString::number(ratings[i], 'f', 1) + "%" : "?";
        painter.drawText(QRect(x, plate.y(), plate.right() - 6 - x, plate.height()), Qt::AlignCenter, text);
    }
}
