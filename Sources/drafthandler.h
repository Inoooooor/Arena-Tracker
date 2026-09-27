#ifndef DRAFTHANDLER_H
#define DRAFTHANDLER_H

#include "Widgets/ui_extended.h"
#include "deckhandler.h"
#include "Cards/draftcard.h"
#include "utility.h"
#include "Widgets/draftherowindow.h"
#include "Widgets/draftscorewindow.h"
#include "Widgets/draftmechanicswindow.h"
#include "synergyhandler.h"
#include "twitchhandler.h"
#include <QObject>
#include <QFutureWatcher>
#include <QPointer>
#include <QPropertyAnimation>

#define DRAFT_DELAY_TIME        2000
#define HERODRAFT_DELAY_TIME    2000
#define MECHANICS_DELAY_TIME    1000
// #define CONTINUEDRAFT_DELAY_TIME    3000
#define REDRAFT_REVIEW_DELAY_TIME   1500
#define REDRAFT_REMOVE_CARDS        5
#define REDRAFT_REMOVE_SPARES       3
#define REDRAFT_REMOVE_SECTIONS     2
#define REDRAFT_WATCH_TIME          2000
#define REDRAFT_REVIEW_OCR_TIME     1000
#define FINDSCREEN_LOOP_TIME    1000
#define FINDSCREEN_STABLE_TIME  400

#define CAPTUREDRAFT_DELAY_TIME         1500
#define CAPTUREDRAFT_LOOP_TIME          100
#define CAPTUREDRAFT_LOOP_TIME_FADING   200
#define CAPTUREDRAFT_LOOP_TIME_REDRAFT  500

#define CARD_ACCEPTED_THRESHOLD             0.35
#define CARD_ACCEPTED_THRESHOLD_REDRAFT     0.4
#define CARD_ACCEPTED_THRESHOLD_INCREASE    0.02
#define CAPTURE_MIN_CANDIDATES                 7
#define CAPTURE_EXTENDED_CANDIDATES            15

#define MANA_L2_THRESHOLD       4.5
#define RARITY_L2_THRESHOLD     9

#define PREV_CODES_TIME         10

#define HISTOGRAM_EXT                   ".dat"


class SDBasic
{
public:
    std::vector<Point2f> screenPoints;
    int goodMatches = 0;
    int screenIndex = -1;
    int screenHeight = 1;
    QPointF screenScale = QPointF(0,0);
};


class ScreenDetection
{
public:
    cv::Rect screenRects[5];
    cv::Rect manaRects[5];
    cv::Rect rarityRects[5];
    int screenIndex = -1;
    int screenHeight = 1;
    QPointF screenScale = QPointF(0,0);
};

//Mana/rarity review of one draft slot. Computed in a worker thread and applied to the
//combo box in the GUI thread, as widgets must not be touched from other threads.
class ReviewSlot
{
public:
    QString slotCode;       //Card of the slot when the review started
    QString code;           //Suggested card, empty if the slot card is right
    int comboIndex = -1;    //Combo box index to select, -1 to keep it
    DraftCard newCard;      //Card to add to the combo box, empty code if it is already there
};


class DraftHandler : public QObject
{
    Q_OBJECT
public:
    DraftHandler(QObject *parent, Ui::Extended *ui, DeckHandler *deckHandler);
    ~DraftHandler();

//Variables
private:
    bool patreonVersion;
    Ui::Extended *ui;
    DeckHandler *deckHandler;
    SynergyHandler *synergyHandler;
    LavaButton *lavaButton;
    ScoreButton *scoreButtonLF, *scoreButtonHA, *scoreButtonHSR;
    QPointer<QWidget> redraftTab;   //Owned by the tab widget while shown, so it may be deleted before us
    //Deck cards suggested for removal after a redraft, worst first: one section by Firestone, one by HearthArena
    QLabel *redraftRemoveLabel[REDRAFT_REMOVE_SECTIONS];
    //What the draft recognition is doing, shown in the draft and redraft tabs (bottom bar in the hero choice)
    QLabel *draftStatusLabel, *redraftStatusLabel;
    QPropertyAnimation *draftStatusPulse[2];   //Opacity pulse of each status label while work goes on
    int findScreenFails;
    MoveListWidget *redraftRemoveListWidget[REDRAFT_REMOVE_SECTIONS];
    QList<DeckCard> redraftRemoveCards[REDRAFT_REMOVE_SECTIONS];
    QMap<QString, int> hearthArenaTiers;
    QMap<QString, int> lightForgeTiers;
    //Guarda los codes en la rotacion. Parte de todos los arena sets o se limita a la tier list de HA (si trustHA)
    QMap<CardClass, QStringList> codesByClass;
    QMap<QString, cv::MatND> cardsHist;
    QStringList cardsDownloading;
    DraftCard draftCards[3];
    //Guarda los mejores candidatos de esta iteracion
    QMap<QString, DraftCard> draftCardMaps[3];  //[Code(_premium)] --> DraftCard
    //Se crea al final de la iteracion para ordenar los candidatos por match score
    QMultiMap<double, QString> bestMatchesMaps[3];   //[Match] --> Code(_premium)
    bool cardDetected[3];
    CardClass arenaHero, arenaHeroMulticlassPower;
    int deckRatingHA;
    float deckRatingHSR, deckRatingFire;
    cv::Rect screenRects[5];
    cv::Rect manaRects[5];
    cv::Rect rarityRects[5];
    QPointF screenScale;
    int screenIndex;
    int numCaptured;
    bool drafting, heroDrafting, redrafting, redraftingReview, capturing, findingFrame, stopLoops;
    bool mouseInApp;
    Transparency transparency;
    DraftHeroWindow *draftHeroWindow;
    DraftScoreWindow *draftScoreWindow;
    DraftMechanicsWindow * draftMechanicsWindow;
    bool showDraftScoresOverlay, showDraftMechanicsOverlay;
    bool learningMode, showDrops, showMyWR;
    QString justPickedCard; //Evita doble pick card en Arena.log
    bool draftMethodHA, draftMethodFire, draftMethodHSR;
    DraftMethod draftMethodAvgScore;
    QFutureWatcher<ScreenDetection> futureFindScreenRects;
    QLabel *labelLFscore[3];
    QLabel *labelHAscore[3];
    QLabel *labelHSRscore[3];
    QComboBox *comboBoxCard[3];
    bool extendedCapture, resetTwitchScores;
    QStringList heroCodesList;
    QMap<QString, float> *cardsIncludedWinratesMap;
    QMap<QString, int> *cardsIncludedDecksMap;
    QMap<QString, float> *cardsPlayedWinratesMap;
    QMap<QString, float> *fireWRMap;
    QMap<QString, int> *fireSamplesMap;
    TwitchHandler *twitchHandler;
    bool multiclassArena;
    bool needSaveCardHist;
    int cardsJsonWaits;
    ScreenDetection prevScreenDetection;    //Last detection, to wait for a stable screen
    //Usado en busqueda manual (name -> code)
    QMap<QString, QString> cardsNameMap;
    int editComboBoxNum;//Numero de combo box que estamos editando
    QFutureWatcher<QString> futureFindCodeFromText;
    QString lastThreadText;
    bool wantedMechanics[M_NUM_MECHANICS];
    QFutureWatcher<QList<ReviewSlot>> futureReviewBestCards;
    //Looks for the redraft review screen ("35/30" deck counter) while in the arena menu, in case
    //the redraft picks happened when AT could not see them (Hearthstone restarted in the review screen)
    QTimer *redraftWatchTimer;
    QFutureWatcher<int> futureRedraftCounter;
    //macOS: the cards picked in the redraft review screen are found by reading their names
    QTimer *redraftReviewTimer;
    QFutureWatcher<QPair<bool, QStringList>> futureRedraftReviewCodes;
    //Legendary bundles (macOS). The log only names the previewed legendary, when its preview opens;
    //the bundle cards are read from the preview, or else from the deck list once it is picked.
    QTimer *bundleTimer;
    QFutureWatcher<QPair<bool, QStringList>> futureBundle;
    QFutureWatcher<QStringList> futureDeckList;
    QString bundleLegendary;
    QMap<QString, QStringList> bundlePreviews;  //Legendary -> its bundle cards, as read
    QMap<QString, QString> bundleNameMap;       //All collectible cards of the draft classes: bundles bring cards from outside the arena sets
    int bundleReads;
    bool bundlePending, bundlePreviewVisible;
    int bundleMisses;
    QMap<QString, QString> redraftNameMap;  //Normalized local name -> code, of the deck cards
    QList<cv::Mat> manaTemplates;
    QList<cv::Mat> rarityTemplates;
    QString prevCodes[3];
    QString ocrCodes[3];    //Card of each slot read by its name (OCR, macOS only)
    QString ocrUnmatchedText[3];    //Last OCR reading that matched no card, logged once
    qint64 prevCodesTime;
    QString bestCodesRedraftingReview[5];


//Metodos
private:
    void completeUI();
    cv::MatND getHist(const QString &code);
    cv::MatND getHist(const Mat &srcBase);
    void initCodesAndHistMaps(QList<DeckCard> &deckCardList, bool skipScreenSettings=false);
    void resetTab(bool alreadyDrafting);
    void clearLists(bool keepCounters);
    void endDraft(bool createNewArena);
    bool getScreenCardsHist(cv::MatND screenCardsHist[], int length);
    void showNewCards(DraftCard bestCards[]);
    void updateDeckScore(float cardRatingHA=0, float cardRatingFire=0, float cardRatingHSR=0);
    bool screenFound();
    ScreenDetection findScreenRects();
    void clearScore(QLabel *label, DraftMethod draftMethod, bool clearText=true);
    void highlightScore(QLabel *label, DraftMethod draftMethod);
    void deleteDraftHeroWindow();
    void deleteDraftScoreWindow();
    void showOverlay();
    void newCaptureDraftLoop(bool delayed=false);
    void updateScoresVisibility();
    void initHearthArenaTiers(const CardClass heroClass, const bool multiClassDraft);
    void initLightForgeTiers(const CardClass heroClass, const bool multiClassDraft, const QStringList &arenaCodes, bool buildCodesByClass);
    void createDraftWindows();
    void mapBestMatchingCodes(cv::MatND screenCardsHist[]);
    double getMinMatch(const QMap<QString, DraftCard> &draftCardMaps);
    bool areCardsDetected();
    bool isRepeatHero();
    void buildBestMatchesMaps();
    void removeDuplicatedPicks(QStringList slotCodes[3]);
    void readCardNames(const cv::Mat &screenCapture);
    void readHeroClasses(const cv::Mat &screenCapture);
    static QString matchCardName(const QStringList &lines, const QMap<QString, QString> &nameMap);
    void applyOcrCodes(QStringList slotCodes[3]);
    CardRarity getBestRarity();
    void getBestCards(DraftCard bestCards[3]);
    void addCardHist(QString code, bool premium, bool isHero=false);
    QString degoldCode(QString fileName);
    void createScoreItems();
    void createSynergyHandler();
    bool isGoldCode(QString fileName);
    void connectAllComboBox();
    void clearAndDisconnectAllComboBox();
    void clearAndDisconnectComboBox(int index);
    void initDraftMechanicsWindowCounters();
    void initSynergyCounters(QList<DeckCard> &deckCardList);
    void updateLabelDeckScore(float deckScoreFire, int deckScoreHA, float deckScoreHSR, int numCards);
    void showMessageDeckScore(float deckScoreFire, int deckScoreHA, float deckScoreHSR);
    void updateAvgScoresVisibility();
    void endHeroDraft();
    void showNewHeroes();
    void createTwitchHandler();
    void deleteTwitchHandler();
    QString getDeckAvgString(float deckScoreFire, int deckScoreHA, float deckScoreHSR);
    bool buildDraftMechanicsWindow();
    bool loadTemplateSettings();
    bool saveTemplateSettings();
    bool isFindScreenOk(ScreenDetection &screenDetection);
    bool isFindScreenStable(ScreenDetection &screenDetection);
    bool isFindScreenAsSettings(ScreenDetection &screenDetection);
    void refreshHeroes();
    void refreshCapturedCards();
    void processCardHist(QStringList &codes);
    bool initCardHist();
    void loadCardHist(QString classUName);
    void saveCardHist();
    CardClass findMulticlassPower(QList<DeckCard> &deckCardList);
    void initCardsNameMap();
    void reduceCardsNameMapMulticlass();
    void showLineEditCardName(const QString &name);
    void hideLineEditCardName();
    QString findCodeFromText(QString text);
    void startFindCodeFromText(const QString &text);
    void showComboBoxesCards();
    void showComboBoxesCards(DraftCard bestCards[]);
    void showSynergies();
    void showBundles(QString hsrCodes[]);
    void getBestNManaRarity(int &manaN, CardRarity &cardRarity, const cv::Mat &screenSmall, const QList<Mat> &manaTemplates, const QList<Mat> &rarityTemplates,
                            const cv::Rect &manaRectSmall, const cv::Rect &rarityRectSmall);
    double getL2Mat(const cv::Mat &matSample, const cv::Mat &matTemplate);
    void getBestNOnRect(const cv::Rect &rect, const int xOff, const int yOff, const cv::Mat &screenCapture,
                                const QList<Mat> &matTemplates, const int numTemplates, double &best, int &bestX, int &bestY, int &bestN);
    void setStartEndLoop(int &startX, int &startY, int &endX, int &endY, const int centerX, const int centerY, const int jump);
    void getBestN(int &bestNs, double &bestL2s, const Rect &rectSmall, const cv::Mat &screenCapture, const QList<Mat> &matTemplates, const int numTemplates);
    cv::Mat getScreenMat();
    ReviewSlot getBestMatchManaRarity(QList<DraftCard> candidates, const int pos, const Mat &screenBig,
                                      const int imgMana, const CardRarity imgRarity);
    DraftCard getBestAllMatchManaRarity(const MatND &screenCardHist, const int imgMana, const CardRarity imgRarity);
    void startReviewBestCards();
    QList<ReviewSlot> reviewBestCards(QList<QList<DraftCard>> candidates, QList<DraftCard> slotCards);
    void loadImgTemplates(QList<Mat> &imgTemplates, const QString &filename);
    bool areScreenRectsValid(Mat &screenCapture, int length);
    bool isSignatureCard(const QString &code);
    void setDraftMethodDeck();
    void hideDeckScores();
    void endRedraftReview();
    void captureDraftRedraftingReview();
    void setRedraftReviewCodes(const QStringList &codes);
    void startBundlePreview(const QString &code);
    void confirmBundle();
    void resetBundle();
    void readDeckList();
    void buildBundleNameMap();
    void showDraftNotice(const QString &text);
    void beginRedraftReview();
    void createRedraftRemoveList();
    void createDraftStatus();
    void setDraftStatus(const QString &text);
    void updateRedraftRemoveList();
    bool fillRedraftRemoveSection(int section, DraftMethod draftMethod);
    void updateRedraftRemoveMarks();
    void clearRedraftRemoveList();
    void showRedraftTab();
    void hideRedraftTab();
    void startRedraftWatch();
    void stopRedraftWatch();
    bool areScreenPointsValid(std::vector<Point2f> screenPoints, int screenHeight);
    void showHAScores(QString ogCodes[], QString hsrCodes[], QString cardNames[]);
    void showHSRScores(QString hsrCodes[], QString cardNames[]);
    void showFireScores(QString hsrCodes[], QString cardNames[]);
    QString getHSRCode(QString code);
    QString getFireCode(QString code);
    bool isEmptyDeck();
    void addLFCode(const QString &code, const CardClass &heroClass, const bool multiClassDraft, bool buildCodesByClass);
    QStringList getBundleCodes(const QString &code);
    int getHAScore(const QString &code);

public:
    QString getHACode(QString code);
    QString getHSRFireCode(QString code, bool HSR, CardClass heroClass);
    void setDeckScores();
    QWidget *getRedraftTab();
    static QIcon redraftTabIcon();
    void buildHeroCodesList();
    void reHistDownloadedCardImage(const QString &fileNameCode, bool missingOnWeb=false);
    void setMouseInApp(bool value);
    void setTransparency(Transparency value);
    void setShowDraftScoresOverlay(bool value);
    void setShowDraftMechanicsOverlay(bool value);
    void setLearningMode(bool value);
    void setShowDrops(bool value);
    void redrawAllCards();
    void updateTamCard();
    void setDraftMethod(bool draftMethodHA, bool draftMethodFire, bool draftMethodHSR);
    void setTheme();
    void craftGoldenCopy(int cardIndex);
    bool isDrafting();
    void deMinimizeScoreWindow();
    QStringList getAllHeroCodes();
    void setCardsIncludedWinratesMap(QMap<QString, float> cardsIncludedWinratesMap[]);
    void setCardsIncludedDecksMap(QMap<QString, int> cardsIncludedDecksMap[]);
    void setCardsPlayedWinratesMap(QMap<QString, float> cardsPlayedWinratesMap[]);
    void setFireWRMap(QMap<QString, float> fireWRMap[]);
    void setFireSamplesMap(QMap<QString, int> fireSamplesMap[]);
    void updateTwitchChatVotes();
    void setDraftMethodAvgScore(DraftMethod draftMethodAvgScore);
    void setMulticlassArena(bool multiclassArena);
    SynergyHandler *getSynergyHandler();
    void closeFindScreenRects();
    CardClass getArenaHero();
    void initTierLists(const CardClass &heroClass);
    void initCheckHearthArena();
    void clearTierLists();
    void getCodeScores(const CardClass &heroClass, const QString &code, int &ha, float &hsr, float &fire);
    void setShowMyWR(bool value);
    void setWantedMechanic(uint mechanicIcon, bool value);
    void redrawDownloadedCardImage(QString code);

signals:
    void checkCardImage(QString code, bool isHero=false);
    void newDeckCard(QString code);
    void draftStarted();
    void draftEnded(QString heroLog);
    void saveDraftDeck(QString heroLog);
    void deleteDraftDeck(QString heroLog);
    void scoreAvg(int deckScoreHA, float deckScoreHSR, float deckScoreFire, QString heroLog);
    void overlayCardEntered(QString code, QRect rectCard, int maxTop, int maxBottom, bool alignReverse=true);
    void overlayCardLeave();
    void advanceProgressBar(int remaining, QString text);
    void startProgressBar(int maximum, QString text);
    void showMessageProgressBar(QString text, int hideDelay = 5000);
    void itemEnter(QList<SynergyCard> &synergyCardList, QRect &rectCard, int maxTop=-1, int maxBottom=-1);
    void itemEnterOverlay(QList<SynergyCard> &synergyCardList, QPoint &originList, int maxLeft=-1, int maxRight=-1);
    void itemLeave();
    void cardEntered(QString code, QRect rectCard, int maxTop, int maxBottom);
    void cardLeave();
    void showPremiumDialog();
    void calculateMinimumWidth();
    void pDebug(QString line, DebugLevel debugLevel=Normal, QString file="DraftHandler");

public slots:
    void updateHeroScores();
    void beginDraft(QString hero, QList<DeckCard> deckCardList = QList<DeckCard>(), bool skipScreenSettings=false);
    void continueDraft();
    void beginHeroDraft();
    void heroDraftDeck(QString hero);
    void endDraftShowMechanicsWindow();
    void endDraftHideMechanicsWindow();
    void deleteDraftMechanicsWindow();
    void showNewRatings(const QString &cardName1, const QString &cardName2, const QString &cardName3, float rating1, float rating2, float rating3,
                        float tierScore1, float tierScore2, float tierScore3, DraftMethod draftMethod,
                        int includedDecks1=-1, int includedDecks2=-1, int includedDecks3=-1);
    void pickCard(QString code);
    // void enterArena();//OLD
    void leaveArena();
    void minimizeScoreWindow();
    void setPremium(bool premium);
    void updateMinimumHeight();
    void redraft();
    void checkRedraft();

private slots:
    void captureDraft();
    void finishFindScreenRects();
    void startFindScreenRects();
    void comboBoxActivated();
    void refreshDraft();
    void twitchHandlerConnectionOk(bool ok);
    void twitchHandlerVoteUpdate(int vote1, int vote2, int vote3, QString username);
    void showHSRwebPicks();
    void showFirewebPicks();
    void newFindScreenLoop(bool skipScreenSettings=false);
    void comboBoxHighLight(int index);
    void editCardName(const QString &text);
    void editCardNameFinish();
    void finishFindCodeFromText();
    void finishReviewBestCards();
    void checkRedraftScreen();
    void finishCheckRedraftScreen();
    void redraftRemoveCardEntered(QListWidgetItem *item);
    void captureRedraftReviewNames();
    void captureBundlePreview();
    void finishBundlePreview();
    void finishDeckList();
    void finishRedraftReviewNames();
};

#endif // DRAFTHANDLER_H
