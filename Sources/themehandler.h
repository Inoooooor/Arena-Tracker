#ifndef THEMEHANDLER_H
#define THEMEHANDLER_H

#include <QString>
#include <QJsonObject>
#include "utility.h"

class ThemeHandler
{

//Variables
private:
    static QString bgApp_;
    static QString borderApp_;
    static QString borderTransparent_;
    static int borderWidth_;
    static QString fgColor_;
    static QString themeColor1_;
    static QString themeColor2_;
    static QString bgWidgets_;
    static QString bgTabsColor_, hoverTabsColor_, selectedTabsColor_;
    static QString bgTopButtonsColor_, hoverTopButtonsColor_;
    static QString fgMenuColor_, bgMenuColor_;
    static QString bgSelectedItemMenuColor_, fgSelectedItemMenuColor_;
    static int borderDecksWidth_;
    static QString bgDecks_, borderDecks_;
    static QString bgSelectedItemListColor_, fgSelectedItemListColor_;
    static QString borderTooltipColor_, bgTooltipColor_, fgTooltipColor_;
    static QString borderProgressBarColor_, bgProgressBarColor_, fgProgressBarColor_, chunkProgressBarColor_;
    static QString borderLineEditColor_, bgLineEditColor_, fgLineEditColor_;
    static QString bgSelectionLineEditColor_, fgSelectionLineEditColor_;
    static QString defaultFont_, cardsFont_, bigFont_;
    static int cardsFontOffsetY_;
    static QString tabArenaFile_, tabConfigFile_, tabDeckFile_, tabEnemyDeckFile_, tabGraveyardFile_;
    static QString tabGamesFile_, tabHandFile_, tabPlanFile_;
    static QString buttonRemoveDeckFile_, buttonLoadDeckFile_, buttonNewDeckFile_, buttonSaveDeckFile_;
    static QString buttonMinFile_, buttonPlusFile_, buttonRemoveFile_;
    static QString buttonCloseFile_, buttonMinimizeFile_, buttonResizeFile_;
    static QString buttonForceDraftFile_, buttonDraftRefreshFile_;
    static QString buttonGamesGuideFile_;
    static QString bgCard1Files_[NUM_HEROS+1], bgCard2Files_[NUM_HEROS+1], heroFiles_[NUM_HEROS];
    static QString branchClosedFile_, branchOpenFile_;
    static QString coinFile_, firstFile_;
    static QString loseFile_, winFile_;
    static QString haBestFile_, haCloseFile_, haOpenFile_, haTextFile_;
    static QString lfBestFile_, lfCloseFile_, lfOpenFile_, lfTextFile_;
    static QString youTextFile_;
    static QString speedCloseFile_;
    static QString handCardBYFile_, handCardBYFile2_;
    static QString starFile_, manaLimitFile_, unknownFile_;
    static bool manaLimitBehind_;


//Metodos
private:

public:
    static void defaultEmptyValues();
    static QString bgApp();
    static QString borderApp(bool transparent);
    static int borderWidth();
    static QString fgColor();
    static QString themeColor1();
    static QString themeColor2();
    static QString bgWidgets();
    static QString bgTabsColor();
    static QString hoverTabsColor();
    static QString selectedTabsColor();
    static QString bgTopButtonsColor();
    static QString hoverTopButtonsColor();
    static QString fgMenuColor();
    static QString bgSelectedItemMenuColor();
    static QString fgSelectedItemMenuColor();
    static QString bgMenuColor();
    static QString bgDecks();
    static QString borderDecks();
    static QString bgSelectedItemListColor();
    static QString fgSelectedItemListColor();
    static QString borderTooltipColor();
    static QString bgTooltipColor();
    static QString fgTooltipColor();
    static QString borderProgressBarColor();
    static QString bgProgressBarColor();
    static QString fgProgressBarColor();
    static QString chunkProgressBarColor();
    static QString borderLineEditColor();
    static QString bgLineEditColor();
    static QString fgLineEditColor();
    static QString bgSelectionLineEditColor();
    static QString fgSelectionLineEditColor();
    static QString defaultFont();
    static QString cardsFont();
    static QString bigFont();
    static int cardsFontOffsetY();
    static QString tabArenaFile();
    static QString bgCard1File(int order=NUM_HEROS);
    static QString bgCard2File(int order=NUM_HEROS);
    static QString branchClosedFile();
    static QString branchOpenFile();
    static QString buttonRemoveDeckFile();
    static QString buttonCloseFile();
    static QString coinFile();
    static QString tabConfigFile();
    static QString tabDeckFile();
    static QString buttonForceDraftFile();
    static QString buttonDraftRefreshFile();
    static QString tabEnemyDeckFile();
    static QString tabGraveyardFile();
    static QString firstFile();
    static QString tabGamesFile();
    static QString haBestFile();
    static QString haCloseFile();
    static QString tabHandFile();
    static QString handCardBYFile();
    static QString handCardBYFile2();
    static QString haOpenFile();
    static QString haTextFile();
    static QString heroFile(int order);
    static QString heroFile(QString heroLog);
    static QString starFile();
    static QString lfBestFile();
    static QString lfCloseFile();
    static QString lfOpenFile();
    static QString lfTextFile();
    static QString youTextFile();
    static QString speedCloseFile();
    static QString buttonLoadDeckFile();
    static QString loseFile();
    static QString manaLimitFile();
    static QString buttonMinimizeFile();
    static QString buttonMinFile();
    static QString buttonNewDeckFile();
    static QString tabPlanFile();
    static QString buttonPlusFile();
    static QString buttonRemoveFile();
    static QString buttonResizeFile();
    static QString buttonSaveDeckFile();
    static QString unknownFile();
    static QString buttonGamesGuideFile();
    static QString winFile();
    static bool manaLimitBehind();
};

#endif // THEMEHANDLER_H
