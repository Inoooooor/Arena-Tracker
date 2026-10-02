#ifndef DECKCARD_H
#define DECKCARD_H

#include <QListWidgetItem>
#include <QString>
#include <QMap>
#include "../constants.h"
#include "../utility.h"


#define CARD_SIZE QSize(218,35)
#define DISABLE_OPACITY 150


class DeckCard
{
public:
    DeckCard(QString code, bool outsider=false);
    ~DeckCard();

//Variables
public:
    QListWidgetItem *listItem;
    int total;
    int remaining;
    bool special;
    int id;
    QList<int> outsiderIds;


protected:
    QString code, name;
    CardRarity rarity;
    CardType type;
    QList<CardClass> cardClass;
    QList<CardRace> cardRace;
    CardSchool cardSchool;
    int cost;
    QString createdByCode;

    static bool drawClassColor, drawSpellWeaponColor;
    static int cardHeight;

private:
    bool topManaLimit, bottomManaLimit;
    bool outsider;
    //Usados para pintar los score
    bool showScores, showHA, showFire;
    bool redraftingReview;
    int scoreHA;
    float scoreFire;
    int samplesFire;
    int classOrder;

//Metodos
private:

protected:
    QPixmap draw(int total, bool drawRarity, QColor nameColor=BLACK, QString manaText="", int cardWidth=0, QStringList mechanics={});
    QPixmap drawCustomCard(QString customCode, QString customText);
    QColor getRarityColor();
    QPixmap resizeCardHeight(QPixmap &canvas);
    void disablePixmap(QPixmap &canvas);
    static void drawArt(QPainter &painter, const QRectF &target, const QString &code, const QRectF &source);

public:
    void draw();
    bool isCode(const QString &code);
    QString getCode() const;
    CardType getType();
    QString getName();
    CardRarity getRarity();
    QList<CardClass> getCardClass();
    QList<CardRace> getRace();
    CardSchool getSchool();
    int getCost() const;
    void setCode(QString code);
    void setManaLimit(bool top);
    void resetManaLimits();
    bool isOutsider();
    void setCreatedByCode(QString code);
    QString getCreatedByCode();
    void setEachShowScores(bool showHA, bool showFire, bool redraw);
    void hideScores();
    void setShowScores(bool showScores);
    void setScores(int haTier, float fireWR, int classOrder, int samplesFire);
    float getScore(DraftMethod draftMethod) const;
    void setRedraftingReview(bool show=true);
    bool operator<(const DeckCard &other) const;
    bool operator==(const DeckCard& other) const;
    bool operator!=(const DeckCard& other) const;
    bool operator>(const DeckCard& other) const;
    bool operator<=(const DeckCard& other) const;
    bool operator>=(const DeckCard& other) const;

    //Canvas at the screen resolution (Retina): drawn in logical points, sharp on screen
    static QPixmap newCanvas(const QSize &size);
    static QSize logicalSize(const QPixmap &pixmap);
    static void setDrawClassColor(bool value);
    static void setDrawSpellWeaponColor(bool value);
    static void setCardHeight(int value);
    static int getCardHeight();
    static int getCardWidth();
};

#endif // DECKCARD_H
