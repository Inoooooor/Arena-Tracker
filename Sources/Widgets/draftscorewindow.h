#ifndef DRAFTSCOREWINDOW_H
#define DRAFTSCOREWINDOW_H

#include <QMainWindow>
#include <QObject>
#include <QHBoxLayout>
#include "movelistwidget.h"
#include "scorebutton.h"
#include "hoverlabel.h"
#include "../utility.h"
#include "../Cards/synergycard.h"


#define MARGIN 10
#define CLOSE_FIRE_WINRATE  1.0     //Points under the best card winrate that still get a flat hand
#define CLOSE_HA_SCORE      5       //HearthArena points under the best score that still get a flat hand
#define SYNERGY_MOTION_UPDATE_TIME 50


class SynergyMotion
{
public:
    bool moveDown, moving, running;
    int value, maximum, stepValue;
};

class ScorePlate;
class ScorePlatesWindow;

class DraftScoreWindow : public QMainWindow
{
    Q_OBJECT

//Constructor
public:
    DraftScoreWindow(QWidget *parent, QRect rect, QSize sizeCard, int screenIndex, int classOrder);
    ~DraftScoreWindow();

//Variables
private:
    QHBoxLayout *horLayoutScores[3];
    QHBoxLayout *horLayoutScores2[3];
    QGridLayout *gridLayoutMechanics[3];
    ScorePlate *plates[3];
    ScorePlatesWindow *platesWindow;
    QRect artRects[3];                  //Global, where the draft found each card's art
    QWidget *hiddenHolder;
    float fireScores[3] = {0, 0, 0}, haScores[3] = {0, 0, 0};
    int fireGames[3] = {-1, -1, -1};
    bool platesShown = false;
    ScoreButton *scoresPushButton[3];
    ScoreButton *scoresPushButton2[3];
    ScoreButton *scoresPushButton3[3];
    MoveListWidget *synergiesListWidget[3];
    QList<SynergyCard> synergyCardLists[3];
    SynergyMotion synergyMotions[3];
    int scoreWidth;
    int maxSynergyHeight, maxSynergyHeight1Row, maxSynergyHeight2Row;
    bool scores2Rows;
    bool showHA, showLF, showHSR;
    bool wantedMechanics[M_NUM_MECHANICS];
    SynergyCard *warningCard[3];
    HoverLabel *warningCardLabel[3];
    HoverLabel *warningOkLabel[3];
    bool onWarnMode[3];


//Metodos
private:
    void resizeSynergyList();
    QString getMechanicTooltip(MechanicIcons mechanicIcon);
    QPixmap createMechanicIconPixmap(MechanicIcons mechanicIcon, int count, const MechanicBorderColor dropBorderColor);
    void checkScoresSpace();
    bool paintDropBorder(QPainter &painter, MechanicIcons mechanicIcon, const MechanicBorderColor dropBorderColor);
    void reorderMechanics();
    void createMechanicIcon(int posCard, int posMech, MechanicIcons mechanicIcon, int count,
                            const MechanicBorderColor dropBorderColor);
    void groupSynergyTags(QMap<QString, QMap<QString, int> > &synergyTagMap);
    QString getMechanicFile(MechanicIcons mechanicIcon);
    bool isWantedMechanic(uint mechanicIcon);
    void hideWarnings();
    void hideWarning(int i);
    void showScores(int i);
    void updatePlates();
    QList<QPoint> plateCenters(bool legendaryGroups);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

public:
    void setScores(float rating1, float rating2, float rating3, DraftMethod draftMethod,
                   int includedDecks1, int includedDecks2, int includedDecks3, bool restoreWindow);
    void hideScores(bool quick=false);
    void setLegendaryGroups(bool legendaryGroups);     //The first pick: the plates sit differently
    void setLearningMode(bool value);
    void setDraftMethod(bool draftMethodHA, bool draftMethodLF, bool draftMethodHSR, bool updateSynergies);
    void redrawSynergyCards();
    void setSynergies(int posCard, QMap<QString, QMap<QString, int> > &synergyTagMap, QMap<MechanicIcons, int> &mechanicIcons,
                      const MechanicBorderColor dropBorderColor);
    void setWantedMechanic(uint mechanicIcon, bool value);
    void setWantedMechanics(bool wantedMechanics[]);
    void setWarningCard(const int posCard, const QString &code);
    void setTheme();
    QList<SynergyCard> *getSynergyCardLists();

signals:
    void cardEntered(QString code, QRect rectCard, int maxTop, int maxBottom);
    void cardLeave();
    void showHSRwebPicks();
    void showFirewebPicks();
    void pDebug(QString line, DebugLevel debugLevel=Normal, QString file="DraftScoreWindow");

private slots:
    void hideSynergies();
    void showSynergies();
    void findSynergyCardEntered(QListWidgetItem *item);
    void spreadHoverScore(bool value);
    void stepScrollSynergies(int indexList);
    void resumeSynergyMotion();
    void clearMechanics();
    void findWarningCardLabelEntered(HoverLabel *hoverLabel);
    void warningOkClick(HoverLabel *hoverLabel);
};

#endif // DRAFTSCOREWINDOW_H
