#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "Sources/winratesdownloader.h"
#include "detachwindow.h"
#include "logloader.h"
#include "gamewatcher.h"
#include "hscarddownloader.h"
#include "deckhandler.h"
#include "arenahandler.h"
#include "drafthandler.h"
#include "Widgets/cardwindow.h"
#include "Widgets/mascotwindow.h"
#include <QMainWindow>
#include <QPointer>
#include <QJsonObject>

#define DIVIDE_TABS_H 444
#define DIVIDE_TABS_H2 666
#define DIVIDE_TABS_V 500
#define BIG_BUTTONS_H 48
#define SMALL_BUTTONS_H 19
#define HSJSON_CARDS_URL "https://api.hearthstonejson.com/v1/latest/all/cards.json"
#define HEARTHARENA_TIERLIST_URL "https://www.heartharena.com/tierlist"
//#define HEARTHARENA_TIERLIST_URL "https://www.heartharena.com/tierlist/preview" //Problematico, mejor evitar
#define EXTRA_URL AT_REPO_RAW_URL "/Extra"
#define IMAGES_URL AT_REPO_RAW_URL "/Images"
#define HA_URL AT_REPO_RAW_URL "/HearthArena"
#define ARENA_URL AT_REPO_RAW_URL "/Arena"
#define SYNERGIES_URL AT_REPO_RAW_URL "/Synergies"
#define CARDS_URL AT_REPO_RAW_URL "/CardsJson"
#define USER_GUIDE_URL "https://triodo.gitbook.io/arena-tracker-documentation/en"


namespace Ui {
class Extended;
}

class DetachWindow;

//TODO: the Patreon page and the Discord server, once they exist
#define MASCOT_SUPPORT_WINS 5   //The support ask comes after a win, on the Ready Up screen, from these wins on
#define MASCOT_SUPPORT_URL "https://www.patreon.com/"
#define MASCOT_DISCORD_URL "https://discord.gg/"

class MainWindow : public QMainWindow
{
    Q_OBJECT

friend class DetachWindow;

//Constructor
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() Q_DECL_OVERRIDE;


//Variables
private:
    Ui::Extended *ui;
    QString progressBarText;   //Full text; the bar shows it shrunk or elided to its width
    LogLoader *logLoader;
    GameWatcher *gameWatcher;
    HSCardDownloader *cardDownloader;
    WinratesDownloader *winratesDownloader;
    DeckHandler *deckHandler;
    ArenaHandler *arenaHandler;
    DraftHandler * draftHandler;
    CardWindow *cardWindow;
    QMap<QString, QJsonObject> cardsJson;
    QPoint dragPosition;
    QFile* atLogFile;
    bool mouseInApp;
    Transparency transparency;
    bool oneWindow;
    DetachWindow *deckWindow, *arenaWindow, *enemyWindow, *enemyDeckWindow, *graveyardWindow, *planWindow;
    QList<QPointer<QWidget>> hiddenToDock;   //macOS: windows hidden by the minimize button, shown again from the Dock
    MascotWindow *mascotWindow;
    int mascotRedraftScreenShown = 0;           //RedraftScreen the mascot talks about
    bool mascotSupportAsked = false;            //Once per arena run
    bool mascotInGame = false;
    bool splashOpen = false, initDone = false;  //The mascot shows after both
    bool mascotNoRun = false;                   //The last run ended (rewards screen) and no new draft yet
    bool mascotRetired = false;
    bool mascotRewardsRetired = false;          //The run of the rewards screen being read ended by a retire                 //The run ends by a retire (until its rewards screen)
    QString mascotLastStatus;                   //The draft status behind the current status line
    QHash<QString, qint64> mascotStatusShownAt; //When each draft status was last shown (anti flip-flop)
    int mascotSecretsSeen = 0;                  //Enemy secrets since the app started
    bool mascotSaysStatus = false;              //The bubble shows a draft status, cleared with it
    bool mascotSaysAdvice = false;              //The bubble shows the pick advice: only a new pick or a problem replaces it
    bool mascotLive = false;                    //Game events replayed from the logs at startup are ignored
    bool mascotLastWon = false;
    int mascotLastLosses = 0;
    int cardHeight;
    int drawDisappear;
    QNetworkAccessManager *networkManager;
    QStringList allCardsDownloadList;
    int allCardsDownloadTotal = 0;
    //Gestionan si es necesario bajar todas las cartas usadas en arena debido a que el directorio de cartas se haya borrado
    //o haya una nueva version de tier list (rotacion sets)
    //Si es necesario tambien se reconstruira el string de sets activos en arena "arenaSets" que se usa para saber que secretos mostrar
    bool cardsJsonLoaded, arenaSetsLoaded, allCardsDownloadNeeded;



//Metodos
public:
    void setSplashOpen();
    void splashClosed();
    LoadingScreenState getLoadingScreen();

private:
    void initVariables();
    void createLogLoader();
    void createArenaHandler();
    void createGameWatcher();
    void createCardWindow();
    static QColor mascotRarityColor(const QString &code);
    static QStringList mascotGoodLuckLines();
    void createCardDownloader();
    void createWinratesDownloader();
    void createDeckHandler();
    void createDraftHandler();
    void createVersionChecker();
    void readSettings();
    void writeSettings();
    void completeUI();
    void completeUIButtons();
    void completeUITabNames();
    void completeConfigTab();
    void addDraftMenu(QPushButton *button);
    void spreadTransparency(Transparency newTransparency);
    void updateOtherTabsTransparency();
    void spreadTheme();
    void updateMainUITheme();
    void updateAllDetachWindowTheme(const QString &mainCSS);
    void updateDetachWindowTheme(QWidget *paneWidget);
    void updateButtonsTheme();
    void updateTabWidgetsTheme(bool transparent, bool resizing);
    QString getHSLanguage();
    void createCardsJsonMap(QByteArray &jsonData);
    void resizeTopButtons(int right, int top);
    void resizeChecks();
    void moveTabTo(QWidget *widget, QTabWidget *tabWidget);
    void resetSettings();
    void createLogFile();
    void closeLogFile();
    void createDataDir();
    void calculateCardWindowMinimumWidth(DetachWindow *detachWindow, bool hasBorders);
    void initConfigTab(int tooltipScale, int cardHeight, bool autoSize, bool showClassColor, bool showSpellColor,
                       bool showManaLimits, bool showTotalAttack, bool showRngList, bool twitchChatVotes,
                       bool draftMethodHA, bool draftMethodLF, QString draftAvg,
                       int popularCardsShown, bool showSecrets, bool showWildSecrets,
                       bool showDraftScoresOverlay, bool showDraftMechanicsOverlay, bool draftLearningMode, bool draftShowDrops,
                       bool showMyWR, bool downloadLB, bool wantedMechanics[]);
    void moveInScreen(QPoint pos, QSize size);
    int getScreenHighest();
    void completeHighResConfigTab();
    void spreadTamCard(int value);
    int getTamCard();
    int getAutoTamCard();
    void createNetworkManager();
    void initCardsJson();
    void removeHSCards(bool forceRemove = false);
    void removeExtraAndHistograms();
    void removeHistograms();
    void checkCardsJsonVersion(QString cardsJsonVersion);
    void askLinuxShortcut();
    void showMessageAppImageShortcut();
    void createLinuxShortcut();
    void createDebugPack();
    void showWindowFrame(bool showFrame=true);
    void spreadDraftMethod(bool draftMethodHA, bool draftMethodLF);
    DraftMethod draftMethodFromString(QString draftAvg);
    void showProgressBar(bool animated=true);
    void setProgressBarText(const QString &text);
    void fitProgressBarText();
    void checkFirstRunNewVersion();
    void startProgressBarMini(int maximum);
    void hideProgressBarMini();
    void advanceProgressBarMini(int remaining);
    void updateProgressAllCardsDownload(QString code);
    void completeConfigComboAvg();
    void initConfigTheme();
    void downloadExtraFile(QString nameFile);
    void downloadExtraFiles();
    void downloadHearthArenaVersion();
    void downloadHearthArenaJson(int version);
    void downloadArenaVersion();
    void checkArenaVersionJson(const QJsonObject &jsonObject);
    void downloadSynergiesVersion();
    void downloadSynergiesJson(int version);
    void updateTabIcons();
    void initHeroesWinrate();
    void checkArenaCards();
    void downloadAllArenaCodes(const QStringList &codeList);
    void initWRCards();
    void downloadHearthArenaTierlistOriginal();
    void saveHearthArenaTierlistOriginal(const QByteArray &html="");
    void initConfigAvgScore(QString draftAvg);
    void setWantedMechanic(uint mechanicIcon, bool value);
    void initWantedMechanics(bool wantedMechanics[]);
    void downloadCardsJsonVersion();
    void downloadCardsJson(int version);
    void testDownloadCardsJson();
    void testHearthArenaTL();
    void HAnames2codes(bool infoOnly=false);
    void checkHearthArenaTLCodes(bool infoOnly=false);
    void HAreplace(const QMap<QString, QString> &swapCodes);

protected:
    //Override events
    void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
    void changeEvent(QEvent *event) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent *e) Q_DECL_OVERRIDE;
    void enterEvent(QEnterEvent *e) Q_DECL_OVERRIDE;

//Signals
signals:
    //The first run downloads every arena card image: the splash shows it, then closes when ready
    void startupProgress(int done, int total);
    void startupReady();


//Slots
public slots:
    //GameWatcher
    void resetDeck(bool deckRead=false);
    void resetDeckDontRead();

    //Multi Handlers
    bool checkCardImage(QString code, bool isHero=false);

    //HSCardDownloader
    void redrawDownloadedCardImage(QString code);

    //Widgets
    void resizeSlot(QSize size);

    //MainWindow
    void pDebug(QString line, DebugLevel debugLevel=Normal, QString file="MainWindow");
    void pDebug(QString line, qint64 numLine, DebugLevel debugLevel, QString file);


private slots:
    void test();
    void testArenaGames();
    void testDelay();
    void testSynergies();
    void testHeroPortraits();
    void downloadHeroPortrait(QString code);
    void testDownloadRotation(bool fromHearth, const QString &miniSet="");
    void testDraft();
    void confirmNewArenaDraft(QString hero);
    void transparentAlways();
    void transparentAuto();
    void transparentNever();
    void transparentFramed();
    void updateTamCard(int value);
    void updateShowDraftScoresOverlay(bool checked);
    void updateShowDraftMechanicsOverlay(bool checked);
    void updateDraftLearningMode(bool checked);
    void updateDraftShowDrops(bool checked);
    void updateTooltipScale(int value);
    void closeApp();
    void minimizeToDock();
    void createMascotWindow();
    void mascotDraftStatus(QString text);
    void mascotStartGame();
    void mascotEndGame(bool playerWon, bool playerUnknown);
    void mascotEnemySecret();
    void mascotRewards(int wins);
    void mascotReadyUpWins(int wins);
    void mascotDraftFinished(int knownCards, float avgFire, float avgHA);
    void mascotGreeting();
    void mascotRedraftScreen(int screen);
    void mascotArenaRecord(int wins, int losses, bool lastWon);
    void mascotRunComplete();
    void mascotHeroes(int classOrder0, int classOrder1, int classOrder2);
    void mascotCards();
    void restoreFromDock(Qt::ApplicationState state);
    void updateShowClassColor(bool checked);
    void updateShowSpellColor(bool checked);
    void updateShowManaLimits(bool checked);
    void fadeBarAndButtons(bool fadeOut);
    void spreadMouseInApp();
    void logReset();
    void spreadCorrectTamCard();
    void completeArenaDeck();
    void changingTabResetSizePlan();
    void setLocalLang();
    void replyFinished(QNetworkReply *reply);
    void checkLinuxShortcut();
    void spreadTransparency();
    void startProgressBar(int maximum, QString text);
    void advanceProgressBar(int remaining, QString text="");
    void showMessageProgressBar(QString text, int hideDelay = 5000);
    void hideProgressBar();
    void missingOnWeb(QString code);
    void allCardsDownloaded();
    void init();
    void createDetachWindow(int index, const QPoint &dropPoint);
    void createDetachWindow(QWidget *paneWidget, const QPoint& dropPoint = QPoint());
    void closedDetachWindow(DetachWindow *detachWindow, QWidget *paneWidget);
    void calculateMinimumWidth();
    void changingTabUpdateDraftSize();
    void openUserGuide();
    void spreadDraftMethod();
    void spreadDraftAvg(QString draftAvg);
    void newGameResult(GameResult gameResult, LoadingScreenState loadingScreen);
    void updateShowMyWR(bool checked);
    void updateDrop2(bool checked);
    void updateDrop3(bool checked);
    void updateDrop4(bool checked);
    void updateReach(bool checked);
    void updateTaunt(bool checked);
    void updateSurvival(bool checked);
    void updateDraw(bool checked);
    void updatePing(bool checked);
    void updateDamage(bool checked);
    void updateDestroy(bool checked);
    void updateAoe(bool checked);
    void newDeckCardDraft(QString code);
    void leaveArena();
    void readyFireWRMap(QMap<QString, float> *fireWRMap);
    void readyFireSamplesMap(QMap<QString, int> *fireSamplesMap);
};

#endif // MAINWINDOW_H
