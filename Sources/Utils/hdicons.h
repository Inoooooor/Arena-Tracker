#ifndef HDICONS_H
#define HDICONS_H

#include <QIcon>
#include <QString>

//Sharp icons for Retina screens, instead of the 32x32 theme images. Hero classes and the coin come from
//HearthstoneJSON art (HDImages) in a circle; the first turn, win and lose marks are drawn. Until the art is
//downloaded an icon paints the theme image, and switches by itself on a later repaint.
namespace HDIcons
{
    QIcon hero(int classOrder);
    QIcon hero(const QString &heroLog);
    QIcon coin();
    QIcon first();
    QIcon win();
    QIcon lose();
    void prefetch();

    //Tab icons drawn in the colors of the theme's icon
    enum Tab { TabArena, TabGames, TabHand, TabDeck, TabEnemyDeck, TabGraveyard, TabPlan, TabConfig };
    QIcon tab(Tab tab);
    QPixmap tabPixmap(Tab tab);
}

#endif // HDICONS_H
