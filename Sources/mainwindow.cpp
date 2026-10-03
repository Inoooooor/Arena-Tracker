#include "mainwindow.h"
#include "Widgets/scorebutton.h"
#ifdef Q_OS_MAC
#include "Utils/macwindow.h"
#include "Utils/macocr.h"
#endif
#include "Widgets/ui_extended.h"
#include "utility.h"
#include "Widgets/cardwindow.h"
#include "versionchecker.h"
#include "themehandler.h"
#include "Utils/hdimages.h"
#include "Utils/hdicons.h"
#include "Utils/pickrating.h"
#include <QtConcurrent/QtConcurrent>
#include <QtWidgets>

#ifdef Q_OS_LINUX
    #include "Utils/capturemanager.h"
#endif

using namespace cv;


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent, Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint),
    ui(new Ui::Extended)
{
    QApplication::setWheelScrollLines(1);
    QFontDatabase::addApplicationFont(":Fonts/hsFont.ttf");
    QFontDatabase::addApplicationFont(":Fonts/LuckiestGuy.ttf");


    ui->setupUi(this);

    initVariables();
    createNetworkManager();
    createDataDir();
    createLogFile();
    completeUI();
    initCardsJson();
    downloadArenaVersion();
    downloadHearthArenaVersion();
    downloadExtraFiles();

    HDImages::create(this);
    HDIcons::prefetch();
    createCardDownloader();
    createWinratesDownloader();
    createDeckHandler();//-->EnemyDeckHandler
    createArenaHandler();//-->DeckHandler -->TrackobotUploader -->PlanHandler
    createDraftHandler();//-->CardDownloader -->DeckHandler -->ArenaHandler -->PlanHandler
    createGameWatcher();//-->A lot
    createLogLoader();//-->GameWatcher -->DraftHandler
    createCardWindow();//-->A lot
    createMascotWindow();//-->DraftHandler -->GameWatcher

    //Se hace despues de descargar el nuevo cards json o cuando se sabe que no hay
    //uno nuevo que descargar.
    //initWRCards();//-->DraftHandler -->SecretHandler -->PopularCardsHandler

    readSettings();
    checkFirstRunNewVersion();
    createVersionChecker();//Despues de createDataDir (removeHSDir) y checkFirstRunNewVersion() ya que reescribe el settings "runVersion"


    QTimer::singleShot(1000, this, SLOT(init()));

#ifdef Q_OS_MAC
    new MacHoverTracker(this);
    new MacFullScreenOverlay(this);
#endif

    //Cards drawn before their HD image arrived are drawn again, once for a burst of downloads
    QTimer *hdRedrawTimer = new QTimer(this);
    hdRedrawTimer->setSingleShot(true);
    hdRedrawTimer->setInterval(500);
    connect(hdRedrawTimer, &QTimer::timeout, this, [this]() {
        deckHandler->redrawAllCards();
        draftHandler->redrawAllCards();
    });
    connect(HDImages::instance(), &HDImages::ready, this, [hdRedrawTimer](int kind) {
        if(kind == HDImages::Tile)  hdRedrawTimer->start();
    });
}


MainWindow::~MainWindow()
{
    if(networkManager != nullptr)      delete networkManager;
    if(logLoader != nullptr)           delete logLoader;
    if(gameWatcher != nullptr)         delete gameWatcher;
    if(arenaHandler != nullptr)        delete arenaHandler;
    if(cardDownloader != nullptr)      delete cardDownloader;
    if(winratesDownloader != nullptr)  delete winratesDownloader;
    if(draftHandler != nullptr)        delete draftHandler;
    if(deckHandler != nullptr)         delete deckHandler;
    if(ui != nullptr)                  delete ui;
    closeLogFile();
    QFontDatabase::removeAllApplicationFonts();
}


void MainWindow::initVariables()
{
    QSettings settings("Arena Tracker", "Arena Tracker");

    atLogFile = nullptr;
    mouseInApp = false;
    deckWindow = nullptr;
    arenaWindow = nullptr;
    enemyWindow = nullptr;
    enemyDeckWindow = nullptr;
    graveyardWindow = nullptr;
    planWindow = nullptr;
    cardHeight = -1;
    transparency = AutoTransparent;
    cardsJsonLoaded = arenaSetsLoaded = false;
    allCardsDownloadNeeded = !settings.value("allCardsDownloaded", false).toBool();
    Utility::setTrustHA(settings.value("trustHA", true).toBool());
    Utility::setArenaSets(settings.value("arenaSets", QStringList()).toStringList());

    logLoader = nullptr;
    gameWatcher = nullptr;
    arenaHandler = nullptr;
    cardDownloader = nullptr;
    winratesDownloader = nullptr;
    draftHandler = nullptr;
    deckHandler = nullptr;

#ifdef Q_OS_LINUX
    CaptureManager::init(this);
#endif
}


void MainWindow::init()
{
    spreadTransparency();

    //Shown here, after MacFullScreenOverlay exists, so it can show over fullscreen Hearthstone too; not over the splash
    initDone = true;
    if(!splashOpen)     mascotWindow->show();

#ifdef Q_OS_LINUX
    checkLinuxShortcut();
#endif

#ifdef QT_DEBUG
    test();
#endif
}


void MainWindow::createDetachWindow(int index, const QPoint& dropPoint)
{
    QWidget *paneWidget = ui->tabWidget->widget(index);
    createDetachWindow(paneWidget, dropPoint);
}


void MainWindow::createDetachWindow(QWidget *paneWidget, const QPoint& dropPoint)
{
    if(paneWidget != ui->tabArena && paneWidget != ui->tabEnemy && paneWidget != ui->tabDeck &&
            paneWidget != ui->tabEnemyDeck && paneWidget != ui->tabGraveyard && paneWidget != ui->tabPlan)    return;

    DetachWindow *detachWindow = nullptr;

    if(paneWidget == ui->tabArena)
    {
        detachWindow = new DetachWindow(paneWidget, "Games", this->transparency, dropPoint);
        arenaWindow = detachWindow;
    }
    else if(paneWidget == ui->tabEnemy)
    {
        detachWindow = new DetachWindow(paneWidget, "Hand", this->transparency, dropPoint);
        enemyWindow = detachWindow;
    }
    else if(paneWidget == ui->tabDeck)
    {
        detachWindow = new DetachWindow(paneWidget, "Deck", this->transparency, dropPoint);
        deckWindow = detachWindow;
        connect(deckWindow, SIGNAL(resized()),
                this, SLOT(spreadCorrectTamCard()));
    }
    else if(paneWidget == ui->tabEnemyDeck)
    {
        detachWindow = new DetachWindow(paneWidget, "Enemy Deck", this->transparency, dropPoint);
        enemyDeckWindow = detachWindow;
    }
    else if(paneWidget == ui->tabGraveyard)
    {
        detachWindow = new DetachWindow(paneWidget, "Graveyard", this->transparency, dropPoint);
        graveyardWindow = detachWindow;
    }
    else /*if(paneWidget == ui->tabPlan)*/
    {
        detachWindow = new DetachWindow(paneWidget, "Replay", this->transparency, dropPoint);
        planWindow = detachWindow;
    }

#ifndef Q_OS_MAC
    connect(ui->minimizeButton, SIGNAL(clicked()),
            detachWindow, SLOT(showMinimized()));
#endif
    connect(detachWindow, SIGNAL(closed(DetachWindow*,QWidget*)),
            this, SLOT(closedDetachWindow(DetachWindow*,QWidget*)));
    connect(detachWindow, SIGNAL(pDebug(QString,DebugLevel,QString)),
            this, SLOT(pDebug(QString,DebugLevel,QString)));

    updateMainUITheme();//Crea el fondo de la nueva ventana y sus botones
    resizeChecks();//Recoloca botones -X y reajusta tabBar size
    calculateMinimumWidth();//Recalcula minimumWidth de mainWindow
}


void MainWindow::closedDetachWindow(DetachWindow *detachWindow, QWidget *paneWidget)
{
    Q_UNUSED(paneWidget)
    disconnect(ui->minimizeButton, nullptr, detachWindow, nullptr);
    disconnect(detachWindow, nullptr, this, nullptr);

    if(detachWindow == deckWindow)      deckWindow = nullptr;
    if(detachWindow == arenaWindow)     arenaWindow = nullptr;
    if(detachWindow == enemyWindow)     enemyWindow = nullptr;
    if(detachWindow == enemyDeckWindow) enemyDeckWindow = nullptr;
    if(detachWindow == graveyardWindow) graveyardWindow = nullptr;
    if(detachWindow == planWindow)
    {
        //Antes de hacer el close de la detach window se llama esta funcion.
        //Volver plan a su size normal si se cierra Plan
        planWindow = nullptr;
    }

    updateMainUITheme();//Elimina el fondo de la ventana al unir la tab a mainWindow
    updateTabIcons();//Inserta el tab y los ordena
    resizeChecks();//Recoloca botones -X y reajusta tabBar size
    calculateMinimumWidth();//Recalcula minimumWidth de mainWindow
}


void MainWindow::resetDeckDontRead()
{
    resetDeck(true);
}


void MainWindow::resetDeck(bool deckRead)
{
    gameWatcher->setDeckRead(deckRead);
    deckHandler->reset();
}


QString MainWindow::getHSLanguage()
{
    QString lang = "";

    if(logLoader != nullptr)
    {
        QDir dir(QFileInfo(logLoader->getLogConfigPath()).absolutePath() + "/Cache/UberText");
        dir.setFilter(QDir::Files);
        dir.setSorting(QDir::Time);


        switch (dir.count())
        {
        case 0:
            lang = "enUS";
            break;
        case 1:
            for(const QString &file: (const QStringList)dir.entryList())
            {
                lang = file.mid(5,4);
            }
            break;
        default:
            QStringList files = dir.entryList();
            lang = files.takeFirst().mid(5,4);

            //Remove old languages files
            for(const QString &file: qAsConst(files))
            {
                dir.remove(file);
                pDebug(file + " removed.");
            }

            //Remove old image cards
//            QDir dirHSCards(Utility::hscardsPath());
//            dirHSCards.setFilter(QDir::Files);
//            QStringList filters("*_*.png");
//            dirHSCards.setNameFilters(filters);

//            for(const QString &file: dirHSCards.entryList())
//            {
//                dirHSCards.remove(file);
//                pDebug(file + " removed.");
//            }

            break;
        }
    }


    if(lang != "enGB" && lang != "enUS" && lang != "esES" && lang != "esMX" &&
            lang != "deDE" && lang != "frFR" && lang != "itIT" &&
            lang != "plPL" && lang != "ptBR" && lang != "ruRU" &&
            lang != "koKR" && lang != "zhCN" && lang != "zhTW" &&
            lang != "jaJP" && lang != "thTH")
    {
        pDebug("Language: " + lang + " not supported. Using enUS.");
        lang = "enUS";
    }
    else if(lang == "enGB")
    {
        pDebug("Language: " + lang + ". Using enUS.");
        lang = "enUS";
    }
    else
    {
        pDebug("Language: " + lang + ".");
    }

    cardDownloader->setLang(lang);
    // lang = "enUS";//Test secrets
    return lang;
}


void MainWindow::createCardsJsonMap(QByteArray &jsonData)
{
    pDebug("Create Json Map.");

    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
    const QJsonArray jsonArray = jsonDoc.array();
    for(const QJsonValue &jsonCard: jsonArray)
    {
        QJsonObject jsonCardObject = jsonCard.toObject();
        cardsJson[jsonCardObject.value("id").toString()] = jsonCardObject;
    }

    cardsJsonLoaded = true;
    if(draftHandler != nullptr) draftHandler->buildHeroCodesList();
    checkArenaCards();
}


void MainWindow::replyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    QString fullUrl = reply->url().toString();
    QString endUrl = fullUrl.split("/").last();

    if(reply->error() != QNetworkReply::NoError)
    {
        pDebug(reply->url().toString() + " --> Failed. Retrying...");
        networkManager->get(QNetworkRequest(reply->url()));
    }
    else
    {
        //Cards version - Github
        if(endUrl == "cardsVersion.json")
        {
            int cardsVersion = QJsonDocument::fromJson(reply->readAll()).object().value("cardsVersion").toInt();
            downloadCardsJson(cardsVersion);
        }
        //Cards json
        else if(endUrl == "cards.json")
        {
            //Old redirection
            if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 302)
            {
                checkCardsJsonVersion(reply->rawHeader("Location"));
            }
            //Cards json - HearthstoneJSON (Debug)
            else if(fullUrl == HSJSON_CARDS_URL)
            {
                qDebug() << "DEBUG CARDS: Json Cards --> Download Success.";
                QSettings settings("Arena Tracker", "Arena Tracker");
                settings.setValue("cardsJsonVersion", fullUrl);

                QByteArray jsonData = reply->readAll();
                QString cardsJsonLocal = Utility::extraPath() + "/cards.json";
                QString cardsJsonSource = QDir::homePath() + "/Documentos/ArenaTracker/CardsJson/cards.json";
                Utility::dumpOnFile(jsonData, cardsJsonLocal);
                Utility::dumpOnFile(jsonData, cardsJsonSource);
                qDebug() << "DEBUG CARDS: cards.json created (source and local)";
            }
            //Cards json - Github
            else//CARDS_URL + QString("/cards.json")
            {
                pDebug("Extra: Json Cards --> Download Success.");
                QByteArray jsonData = reply->readAll();
                QString cardsJsonLocal = Utility::extraPath() + "/cards.json";
                Utility::dumpOnFile(jsonData, cardsJsonLocal);

                //Histograms/arena cards could have been built with the old cards.json (new sets missing).
                removeHistograms();
                Utility::createDir(Utility::histogramsPath());
                allCardsDownloadNeeded = true;
                Utility::setCardsJsonUpToDate(true);
                createCardsJsonMap(jsonData);
                initWRCards();
            }
        }
#ifdef QT_DEBUG
        //Hearth Arena (Debug)
        else if(fullUrl == HEARTHARENA_TIERLIST_URL)
        {
            qDebug()<<"DEBUG TL: Heartharena Tierlist --> Download Success.";
            QByteArray html = reply->readAll();
            saveHearthArenaTierlistOriginal(html);
        }
#endif
        //Arena version
        else if(endUrl == "arenaVersion.json")
        {
            checkArenaVersionJson(QJsonDocument::fromJson(reply->readAll()).object());
        }
        //HearthArena version
        else if(endUrl == "haVersion.json")
        {
            int haVersion = QJsonDocument::fromJson(reply->readAll()).object().value("haVersion").toInt();
            downloadHearthArenaJson(haVersion);
        }
        //HearthArena json
        else if(endUrl == "hearthArena.json")
        {
            pDebug("Extra: Json HearthArena --> Download Success.");
            QByteArray jsonData = reply->readAll();
            Utility::dumpOnFile(jsonData, Utility::extraPath() + "/hearthArena.json");
        }
        //Extra files
        else
        {
            pDebug("Extra: " + endUrl + " --> Download Success.");
            QByteArray data = reply->readAll();
            QString targetPath = Utility::extraPath() + "/" + endUrl;
            Utility::dumpOnFile(data, targetPath);

            if(endUrl == "captureHelper")   Utility::setExecutablePermissions(targetPath);
        }
    }
}


//Old redirection
void MainWindow::checkCardsJsonVersion(QString cardsJsonVersion)
{
    QSettings settings("Arena Tracker", "Arena Tracker");
    QString storedCardsJsonVersion = settings.value("cardsJsonVersion", "").toString();
    QFile cardsJsonFile(Utility::extraPath() + "/cards.json");
    pDebug("Extra: Json Cards --> Latest version: " + cardsJsonVersion);
    pDebug("Extra: Json Cards --> Stored version: " + storedCardsJsonVersion);

    //Need download
    if(cardsJsonVersion != storedCardsJsonVersion || !cardsJsonFile.exists())
    {
        pDebug("Extra: Json Cards --> Download from: " + cardsJsonVersion);
        networkManager->get(QNetworkRequest(QUrl(cardsJsonVersion)));
    }
    //No download
    else
    {
        pDebug("Extra: Json Cards --> Use local cards.json");
        initWRCards();
    }
}


void MainWindow::setLocalLang()
{
    QString lang = getHSLanguage();
    Utility::setLocalLang(lang);
}


void MainWindow::initCardsJson()
{
    Utility::setCardsJson(&cardsJson);
    downloadCardsJsonVersion();

    //Load local cards.json (Incluso aunque haya una version nueva para bajar)
    QFile cardsJsonFile(Utility::extraPath() + "/cards.json");
    if(cardsJsonFile.exists())
    {
        if(!cardsJsonFile.open(QIODevice::ReadOnly))
        {
            pDebug("ERROR: Failed to open cards.json");
            return;
        }
        QByteArray jsonData = cardsJsonFile.readAll();
        cardsJsonFile.close();
        createCardsJsonMap(jsonData);
    }
}


void MainWindow::initHeroesWinrate()
{
    winratesDownloader->initHeroesWinrate();
}


void MainWindow::readyFireWRMap(QMap<QString, float> *fireWRMap)
{
    draftHandler->setFireWRMap(fireWRMap);
}
void MainWindow::readyFireSamplesMap(QMap<QString, int> *fireSamplesMap)
{
    draftHandler->setFireSamplesMap(fireSamplesMap);
}


void MainWindow::initWRCards()
{
    winratesDownloader->initWRCards();
}


void MainWindow::downloadArenaVersion()
{
    networkManager->get(QNetworkRequest(QUrl(ARENA_URL + QString("/arenaVersion.json"))));
}


void MainWindow::checkArenaVersionJson(const QJsonObject &jsonObject)
{
    int version = jsonObject.value("arenaVersion").toInt();
    bool needProcess = false;
    QSettings settings("Arena Tracker", "Arena Tracker");
    int storedVersion = settings.value("arenaVersion", 0).toInt();
    if(version != storedVersion)    needProcess = true;
    if(settings.value("arenaSets", QStringList()).toStringList().isEmpty()) needProcess = true;

    pDebug("Extra: Json Arena github: Local(" + QString::number(storedVersion) + ") - "
                        "Web(" + QString::number(version) + ")" + (!needProcess?" up-to-date":""));

    if(needProcess)
    {
        bool multiclassArena = jsonObject.value("multiclassArena").toBool(false);
        if(draftHandler != nullptr) draftHandler->setMulticlassArena(multiclassArena);
        settings.setValue("multiclassArena", multiclassArena);
        pDebug("CheckArenaVersion: multiclassArena: " + QString(multiclassArena?"true":"false"));

        bool redownloadCards = jsonObject.value("resetCards").toInt() > storedVersion;
        bool redownloadHeroes = jsonObject.value("resetHeroes").toInt() > storedVersion;
        pDebug("CheckArenaVersion: redownloadCards: " + QString(redownloadCards?"true":"false"));
        pDebug("CheckArenaVersion: redownloadHeroes: " + QString(redownloadHeroes?"true":"false"));
        if(redownloadCards)
        {
            removeHSCards(true);
            Utility::createDir(Utility::hscardsPath());
        }
        else if(redownloadHeroes)
        {
            QDir dir(Utility::hscardsPath());
            dir.setFilter(QDir::Files);
            dir.setSorting(QDir::Time);
            QStringList filterName;
            filterName << "*.png";
            dir.setNameFilters(filterName);

            const QStringList files = dir.entryList();

            for(const QString &file: files)
            {
                //--------------------------------------------------------
                //----NEW HERO CLASS
                //--------------------------------------------------------
                if(file.startsWith("HERO_0") || file.startsWith("HERO_1"))
                {
                    dir.remove(file);
                    pDebug(file + " removed.");
                }
            }
        }

        //Arena Sets
        QStringList arenaSets;
        const QJsonArray jsonArray = jsonObject.value("arenaSets").toArray();
        for(const QJsonValue &jsonValue: jsonArray) arenaSets.append(jsonValue.toString());

        if(settings.value("arenaSets", QStringList()).toStringList() != arenaSets)
        {
            settings.setValue("arenaSets", arenaSets);
            Utility::setArenaSets(arenaSets);
            pDebug("CheckArenaVersion: New arena sets: " + arenaSets.join(" "));

            //New rotation date
            settings.setValue("rotationDate", QDate::currentDate());
        }
        else
        {
            pDebug("CheckArenaVersion: Unchanged arena sets: " +
                        settings.value("arenaSets", QStringList()).toStringList().join(" "));
        }

        //Remove histograms
        removeHistograms();
        Utility::createDir(Utility::histogramsPath());

        allCardsDownloadNeeded = true;
        settings.setValue("arenaVersion", version);

        //TrustHA
        bool trustHA = jsonObject.value("trustHA").toBool(false);
        Utility::setTrustHA(trustHA);
        settings.setValue("trustHA", trustHA);
        pDebug("CheckArenaVersion: trustHA: " + QString(trustHA?"true":"false"));
    }
    else
    {
        QStringList arenaSets = settings.value("arenaSets", QStringList()).toStringList();
        bool trustHA = settings.value("trustHA", true).toBool();
        pDebug(QStringLiteral("CheckArenaVersion: Unchanged arena sets: %1").arg(arenaSets.join(" ")));
        pDebug(QStringLiteral("CheckArenaVersion: Unchanged trustHA: %1").arg(trustHA?"true":"false"));
    }

    arenaSetsLoaded = true;
    checkArenaCards();
}


void MainWindow::createNetworkManager()
{
    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(replyFinished(QNetworkReply*)));
}


void MainWindow::createVersionChecker()
{
    VersionChecker *versionChecker = new VersionChecker(this);
    connect(versionChecker, SIGNAL(startProgressBar(int,QString)),
            this, SLOT(startProgressBar(int,QString)));
    connect(versionChecker, SIGNAL(advanceProgressBar(int,QString)),
            this, SLOT(advanceProgressBar(int,QString)));
    connect(versionChecker, SIGNAL(showMessageProgressBar(QString,int)),
            this, SLOT(showMessageProgressBar(QString,int)));
    connect(versionChecker, SIGNAL(pDebug(QString,DebugLevel,QString)),
            this, SLOT(pDebug(QString,DebugLevel,QString)));
}


//Durante un redraft repasamos los scores de todo el deck despues de cada pick, (despues de incluirlo en el deck)
void MainWindow::newDeckCardDraft(QString code)
{
    deckHandler->newDeckCardDraft(code);
    draftHandler->setDeckScores();
}


void MainWindow::createDraftHandler()
{
    draftHandler = new DraftHandler(this, ui, deckHandler);
    connect(winratesDownloader, SIGNAL(readyHeroesWinrate()),
            draftHandler, SLOT(updateHeroScores()));
    connect(draftHandler, SIGNAL(startProgressBar(int,QString)),
            this, SLOT(startProgressBar(int,QString)));
    connect(draftHandler, SIGNAL(advanceProgressBar(int,QString)),
            this, SLOT(advanceProgressBar(int,QString)));
    connect(draftHandler, SIGNAL(showMessageProgressBar(QString,int)),
            this, SLOT(showMessageProgressBar(QString,int)));
    connect(draftHandler, SIGNAL(checkCardImage(QString,bool)),
            this, SLOT(checkCardImage(QString,bool)));
    connect(draftHandler, SIGNAL(calculateMinimumWidth()),
            this, SLOT(calculateMinimumWidth()));

    connect(draftHandler, SIGNAL(newDeckCard(QString)),
            this, SLOT(newDeckCardDraft(QString)));
    connect(draftHandler, SIGNAL(draftEnded(QString)),
            deckHandler, SLOT(saveDraftDeck(QString)));
    connect(draftHandler, SIGNAL(saveDraftDeck(QString)),
            deckHandler, SLOT(saveDraftDeck(QString)));
    connect(draftHandler, SIGNAL(deleteDraftDeck(QString)),
            deckHandler, SLOT(deleteDraftDeck(QString)));

    connect(draftHandler, SIGNAL(draftEnded(QString)),
            arenaHandler, SLOT(newArena(QString)));
    connect(draftHandler, SIGNAL(scoreAvg(int,float,QString)),
            arenaHandler, SLOT(setCurrentAvgScore(int,float,QString)));

    //Connect en logLoader
//    connect(draftHandler, SIGNAL(draftEnded()),
//            logLoader, SLOT(setUpdateTimeMax()));
//    connect(draftHandler, SIGNAL(draftStarted()),
//            logLoader, SLOT(setUpdateTimeMin()));

    connect(draftHandler, SIGNAL(pDebug(QString,DebugLevel,QString)),
            this, SLOT(pDebug(QString,DebugLevel,QString)));

    connect(ui->minimizeButton, SIGNAL(clicked()),
            draftHandler, SLOT(minimizeScoreWindow()));

    initHeroesWinrate();
    if(cardsJsonLoaded) draftHandler->buildHeroCodesList();

    QSettings settings("Arena Tracker", "Arena Tracker");
    bool multiclassArena = settings.value("multiclassArena", false).toBool();
    draftHandler->setMulticlassArena(multiclassArena);
    pDebug("multiclassArena = " + QString(multiclassArena?"true":"false"));
}


void MainWindow::createArenaHandler()
{
    arenaHandler = new ArenaHandler(this);
    connect(arenaHandler, SIGNAL(pDebug(QString,DebugLevel,QString)),
            this, SLOT(pDebug(QString,DebugLevel,QString)));
    arenaHandler->loadStatsJsonFile();
}


void MainWindow::createDeckHandler()
{
    deckHandler = new DeckHandler(this, ui);
    connect(deckHandler, SIGNAL(checkCardImage(QString)),
            this, SLOT(checkCardImage(QString)));
    connect(deckHandler, SIGNAL(needMainWindowFade(bool)),
            this, SLOT(fadeBarAndButtons(bool)));
    connect(deckHandler, SIGNAL(showMessageProgressBar(QString)),
            this, SLOT(showMessageProgressBar(QString)));
    connect(deckHandler, SIGNAL(pDebug(QString,DebugLevel,QString)),
            this, SLOT(pDebug(QString,DebugLevel,QString)));
    connect(deckHandler, SIGNAL(deckSizeChanged()),
            this, SLOT(spreadCorrectTamCard()));

    deckHandler->loadDecks();
}


void MainWindow::createCardWindow()
{
    cardWindow = new CardWindow(this);
    connect(deckHandler, SIGNAL(cardEntered(QString,QRect,int,int)),
            cardWindow, SLOT(loadCard(QString,QRect,int,int)));
    connect(draftHandler, SIGNAL(cardEntered(QString,QRect,int,int)),
            cardWindow, SLOT(loadCard(QString,QRect,int,int)));
    connect(draftHandler, SIGNAL(cardLeave()),
            cardWindow, SLOT(hide()));
    connect(draftHandler, SIGNAL(overlayCardEntered(QString,QRect,int,int,bool)),
            cardWindow, SLOT(loadCard(QString,QRect,int,int,bool)));

    connect(ui->tabWidget, SIGNAL(currentChanged(int)),
            cardWindow, SLOT(hide()));
    connect(ui->deckListWidget, SIGNAL(leave()),
            cardWindow, SLOT(hide()));
    connect(ui->enemyDeckListWidget, SIGNAL(leave()),
            cardWindow, SLOT(hide()));
    connect(ui->graveyardListWidgetPlayer, SIGNAL(leave()),
            cardWindow, SLOT(hide()));
    connect(ui->graveyardListWidgetEnemy, SIGNAL(leave()),
            cardWindow, SLOT(hide()));
    connect(ui->drawListWidget, SIGNAL(leave()),
            cardWindow, SLOT(hide()));
    connect(ui->enemyHandListWidget, SIGNAL(leave()),
            cardWindow, SLOT(hide()));
    connect(ui->secretsListWidget, SIGNAL(leave()),
            cardWindow, SLOT(hide()));
    connect(ui->popularCardsListWidget, SIGNAL(leave()),
            cardWindow, SLOT(hide()));
    connect(draftHandler, SIGNAL(overlayCardLeave()),
            cardWindow, SLOT(hide()));
    connect(draftHandler, SIGNAL(draftStarted()),
            cardWindow, SLOT(hide()));
}


void MainWindow::createCardDownloader()
{
    cardDownloader = new HSCardDownloader(this);
    connect(cardDownloader, SIGNAL(downloaded(QString)),
            this, SLOT(redrawDownloadedCardImage(QString)));
    connect(cardDownloader, SIGNAL(missingOnWeb(QString)),
            this, SLOT(missingOnWeb(QString)));
    connect(cardDownloader, SIGNAL(allCardsDownloaded()),
            this, SLOT(allCardsDownloaded()));
    connect(cardDownloader, SIGNAL(pDebug(QString,DebugLevel,QString)),
            this, SLOT(pDebug(QString,DebugLevel,QString)));
}


void MainWindow::createWinratesDownloader()
{
    winratesDownloader = new WinratesDownloader(this);
    connect(winratesDownloader, SIGNAL(readyFireWRMap(QMap<QString,float>*)),
            this, SLOT(readyFireWRMap(QMap<QString,float>*)));
    connect(winratesDownloader, SIGNAL(readyFireSamplesMap(QMap<QString,int>*)),
            this, SLOT(readyFireSamplesMap(QMap<QString,int>*)));
    connect(winratesDownloader, SIGNAL(startProgressBar(int,QString)),
            this, SLOT(startProgressBar(int,QString)));
    connect(winratesDownloader, SIGNAL(advanceProgressBar(int,QString)),
            this, SLOT(advanceProgressBar(int,QString)));
    connect(winratesDownloader, SIGNAL(showMessageProgressBar(QString,int)),
            this, SLOT(showMessageProgressBar(QString,int)));
    connect(winratesDownloader, SIGNAL(pDebug(QString,DebugLevel,QString)),
            this, SLOT(pDebug(QString,DebugLevel,QString)));
}


//Al salir de arena queremos guardar el deck de arena antes de que se reinicie (por deckHandler)
void MainWindow::leaveArena()
{
    draftHandler->leaveArena();
    deckHandler->leaveArena();
}


void MainWindow::createGameWatcher()
{
    gameWatcher = new GameWatcher(this);

    connect(gameWatcher, SIGNAL(newArena(QString)),
            this, SLOT(resetDeckDontRead()));
    connect(gameWatcher, SIGNAL(needResetDeck()),
            this, SLOT(resetDeck()));
    connect(gameWatcher, SIGNAL(arenaDeckRead()),
            this, SLOT(completeArenaDeck()));
    connect(gameWatcher, SIGNAL(leaveArena()),
            this, SLOT(leaveArena()));
    connect(gameWatcher, SIGNAL(pDebug(QString,qint64,DebugLevel,QString)),
            this, SLOT(pDebug(QString,qint64,DebugLevel,QString)));

    connect(gameWatcher, SIGNAL(newGameResult(GameResult,LoadingScreenState)),
            this, SLOT(newGameResult(GameResult,LoadingScreenState)));
//    connect(gameWatcher, SIGNAL(newArena(QString)),//Arena stat se crea ahora en endDraft - SIGNAL(draftEnded(QString))
//            arenaHandler, SLOT(newArena(QString)));
    //Rewards input disabled with track-o-bot stats
//    connect(gameWatcher, SIGNAL(inRewards()),
//            arenaHandler, SLOT(showRewards()));

    connect(gameWatcher, SIGNAL(newDeckCard(QString)),
            deckHandler, SLOT(newDeckCardAsset(QString)));
    connect(gameWatcher, &GameWatcher::deckSnapshotRead, deckHandler, &DeckHandler::syncDeckSnapshot);
    connect(gameWatcher, SIGNAL(playerCardDraw(QString,int)),
            deckHandler, SLOT(playerCardDraw(QString,int)));
    connect(gameWatcher, SIGNAL(playerReturnToDeck(QString,int)),
            deckHandler, SLOT(returnToDeck(QString,int)));
    connect(gameWatcher, SIGNAL(startGame()),
            deckHandler, SLOT(lockDeckInterface()));
    connect(gameWatcher, SIGNAL(endGame(bool,bool)),
            deckHandler, SLOT(unlockDeckInterface()));
    connect(gameWatcher, SIGNAL(enterArena()),
            deckHandler, SLOT(enterArena()));
    // connect(gameWatcher, SIGNAL(leaveArena()),//MainWindow::leaveArena()
    //         deckHandler, SLOT(leaveArena()));
    connect(gameWatcher, SIGNAL(specialCardTrigger(QString,QString,int,int)),
            deckHandler, SLOT(setLastCreatedByCode(QString,QString)));
    connect(gameWatcher, SIGNAL(coinIdFound(int)),
            deckHandler, SLOT(setFirstOutsiderId(int)));


    connect(gameWatcher, SIGNAL(newArena(QString)),
            draftHandler, SLOT(beginDraft(QString)));
    connect(gameWatcher, SIGNAL(continueDraft()),
            draftHandler, SLOT(continueDraft()));
    connect(gameWatcher, SIGNAL(redraft()),
            draftHandler, SLOT(redraft()));
    connect(gameWatcher, SIGNAL(checkRedraft()),
            draftHandler, SLOT(checkRedraft()));
    connect(gameWatcher, SIGNAL(arenaChoosingHeroe()),
            draftHandler, SLOT(beginHeroDraft()));
    connect(gameWatcher, SIGNAL(heroDraftDeck(QString)),
            draftHandler, SLOT(heroDraftDeck(QString)));
    connect(gameWatcher, SIGNAL(activeDraftDeck()),
            draftHandler, SLOT(activeDraftDeck()));
    connect(gameWatcher, SIGNAL(startGame()),
            draftHandler, SLOT(stopDraft()));
    connect(gameWatcher, SIGNAL(pickCard(QString)),
            draftHandler, SLOT(pickCard(QString)));
    //Lo usabamos para reanudar el drafting, pero ya no lo pausamos, lo terminamos completamente al salir de arena
    //y lo iniciamos de cero al entrar otra vez, DraftHandler::continueDraft
    // connect(gameWatcher, SIGNAL(enterArena()),
    //         draftHandler, SLOT(enterArena()));
    // connect(gameWatcher, SIGNAL(leaveArena()),//MainWindow::leaveArena()
    //         draftHandler, SLOT(leaveArena()));
}


void MainWindow::createLogLoader()
{
    logLoader = new LogLoader(this);
    connect(logLoader, SIGNAL(logReset()),
            this, SLOT(logReset()));
    connect(logLoader, SIGNAL(newLogLineRead(LogComponent,QString,qint64,qint64)),
            gameWatcher, SLOT(processLogLine(LogComponent,QString,qint64,qint64)));
    connect(logLoader, SIGNAL(logConfigSet()),
            this, SLOT(setLocalLang()));
    connect(logLoader, SIGNAL(showMessageProgressBar(QString)),
            this, SLOT(showMessageProgressBar(QString)));
    connect(logLoader, SIGNAL(pDebug(QString,DebugLevel,QString)),
            this, SLOT(pDebug(QString,DebugLevel,QString)));

    //Connect de draftHandler
    connect(draftHandler, SIGNAL(draftEnded(QString)),
            logLoader, SLOT(setUpdateTimeMax()));
    connect(draftHandler, SIGNAL(draftStarted()),
            logLoader, SLOT(setUpdateTimeMin()));

    if(!logLoader->init())  QTimer::singleShot(1, this, SLOT(closeApp()));
}


void MainWindow::newGameResult(GameResult gameResult, LoadingScreenState loadingScreen)
{
    arenaHandler->newGameResult(gameResult, loadingScreen);
}


void MainWindow::completeUI()
{
    ThemeHandler::defaultEmptyValues();
    ui->tabWidget->clear();//Rellenado en spreadTheme

    ui->progressBar->setVisible(false);
    ui->progressBar->setMaximum(100);
    ui->progressBar->setValue(100);

    ui->progressBarMini->setFixedHeight(8);
    ui->progressBarMini->setVisible(false);
    ui->progressBarMini->setMaximum(100);
    ui->progressBarMini->setValue(100);

    completeConfigTab();

    connect(ui->tabWidget, SIGNAL(currentChanged(int)),
            this, SLOT(spreadMouseInApp()));
    connect(ui->tabWidget, SIGNAL(currentChanged(int)),
            this, SLOT(changingTabResetSizePlan()));
    connect(ui->tabWidget, SIGNAL(detachTab(int,QPoint)),
            this, SLOT(createDetachWindow(int,QPoint)));

#ifdef QT_DEBUG
    pDebug("MODE DEBUG");
#endif

#ifdef Q_OS_WIN
    pDebug("Platform: Windows");
#endif

#ifdef Q_OS_MAC
    pDebug("Platform: Mac");
#endif

#ifdef Q_OS_LINUX
    #ifdef APPIMAGE
        pDebug("Platform: Linux AppImage");
    #else
        pDebug("Platform: Linux Static");
    #endif
#endif

    completeUITabNames();
    completeUIButtons();

    pDebug("Path Arena Tracker Dir: " + Utility::dataPath());
}


void MainWindow::completeUITabNames()
{
    ui->tabArena->setObjectName("TabArena");
    ui->tabEnemy->setObjectName("TabEnemy");
    ui->tabDeck->setObjectName("TabDeck");
    ui->tabEnemyDeck->setObjectName("TabEnemyDeck");
    ui->tabGraveyard->setObjectName("TabGraveyard");
    ui->tabPlan->setObjectName("TabPlan");
    ui->tabConfig->setObjectName("TabConfig");
}


void MainWindow::completeUIButtons()
{
    ui->closeButton = new QPushButton("", this);
    ui->closeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->closeButton->setFlat(true);
    ui->closeButton->setToolTip("Close");
    connect(ui->closeButton, SIGNAL(clicked()),
            this, SLOT(closeApp()));


    ui->minimizeButton = new QPushButton("", this);
    ui->minimizeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->minimizeButton->setFlat(true);
    ui->minimizeButton->setToolTip("Minimize");
#ifdef Q_OS_MAC
    //macOS won't minimize these frameless always on top windows to the Dock (and not at all with Stage Manager):
    //they are hidden, and a click on the Dock icon (or switching to the app) shows them again
    connect(ui->minimizeButton, SIGNAL(clicked()),
            this, SLOT(minimizeToDock()));
    connect(qApp, &QGuiApplication::applicationStateChanged,
            this, &MainWindow::restoreFromDock);
#else
    connect(ui->minimizeButton, SIGNAL(clicked()),
            this, SLOT(showMinimized()));
#endif

    ui->resizeButton = new ResizeButton(this);
    ui->resizeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->resizeButton->resize(24, 24);
    ui->resizeButton->setIconSize(QSize(24, 24));
    ui->resizeButton->setFlat(true);
    ui->resizeButton->setToolTip("Resize");
    connect(ui->resizeButton, SIGNAL(newSize(QSize)),
            this, SLOT(resizeSlot(QSize)));
}


void MainWindow::closeApp()
{
    //Check unsaved decks
    if(ui->deckButtonSave->isEnabled() && !deckHandler->askSaveDeck())   return;
    draftHandler->stopDraft();
    hide();
    draftHandler->closeFindScreenRects();
    winratesDownloader->waitFinishThreads();
    close();
    //On macOS closing the window used to leave the app running in the Dock, with no way to show it again
    qApp->quit();
}


//One of the lines at random, so the mascot doesn't repeat itself
static QString mascotPick(const QStringList &lines)
{
    return lines[QRandomGenerator::global()->bounded(lines.count())];
}


void MainWindow::createMascotWindow()
{
    mascotWindow = new MascotWindow();
    connect(mascotWindow, SIGNAL(quitRequested()),
            this, SLOT(closeApp()));
    connect(mascotWindow, &MascotWindow::discordRequested, this, []() {
        QDesktopServices::openUrl(QUrl(MASCOT_DISCORD_URL));
    });
    connect(mascotWindow, &MascotWindow::supportRequested, this, []() {
        QDesktopServices::openUrl(QUrl(MASCOT_SUPPORT_URL));
    });
    //The log goes with the report: shown in Finder, ready to drop into the Discord
    connect(mascotWindow, &MascotWindow::reportRequested, this, []() {
        const QString logPath = Utility::dataPath() + "/ArenaTrackerLog.txt";
#ifdef Q_OS_MAC
        QProcess::startDetached("open", {"-R", logPath});
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(Utility::dataPath()));
#endif
        QDesktopServices::openUrl(QUrl(MASCOT_DISCORD_URL));
    });
    connect(mascotWindow, SIGNAL(cardEntered(QString,QRect,int,int)),
            cardWindow, SLOT(loadCard(QString,QRect,int,int)));
    connect(mascotWindow, SIGNAL(cardLeave()),
            cardWindow, SLOT(hide()));
    connect(draftHandler, SIGNAL(draftStatusChanged(QString)),
            this, SLOT(mascotDraftStatus(QString)));
    connect(gameWatcher, SIGNAL(startGame()),
            this, SLOT(mascotStartGame()));
    connect(gameWatcher, SIGNAL(endGame(bool,bool)),
            this, SLOT(mascotEndGame(bool,bool)));
    connect(gameWatcher, SIGNAL(enemySecretPlayed(int,CardClass,LoadingScreenState)),
            this, SLOT(mascotEnemySecret()));
    connect(draftHandler, SIGNAL(redraftScreenChanged(int)),
            this, SLOT(mascotRedraftScreen(int)));
    connect(arenaHandler, SIGNAL(arenaRecordChanged(int,int,bool)),
            this, SLOT(mascotArenaRecord(int,int,bool)));
    connect(gameWatcher, &GameWatcher::arenaRetired, this, [this]() { mascotRetired = true; });
    //No run between its rewards screen and the next draft (also for the logs replayed at startup)
    connect(gameWatcher, &GameWatcher::inRewards, this, [this]() { mascotNoRun = true; });
    connect(gameWatcher, &GameWatcher::arenaChoosingHeroe, this, [this]() { mascotNoRun = false; });
    connect(gameWatcher, &GameWatcher::newArena, this, [this]() { mascotNoRun = false; });
    connect(gameWatcher, SIGNAL(inRewards()),
            this, SLOT(mascotRunComplete()));
    connect(draftHandler, &DraftHandler::rewardsWinsRead, this, &MainWindow::mascotRewards);
    connect(draftHandler, &DraftHandler::readyUpWinsRead, this, &MainWindow::mascotReadyUpWins);
    connect(draftHandler, SIGNAL(heroesScored(int,int,int)),
            this, SLOT(mascotHeroes(int,int,int)));
    connect(draftHandler, SIGNAL(cardsScored()),
            this, SLOT(mascotCards()));
    connect(draftHandler, SIGNAL(draftFinished(int,float,float)),
            this, SLOT(mascotDraftFinished(int,float,float)));
    connect(logLoader, &LogLoader::logsCaughtUp, arenaHandler, &ArenaHandler::setLogsCaughtUp);
    connect(mascotWindow, &MascotWindow::said, this, [this](const QString &text) { pDebug("Mascot: " + text); });
    connect(logLoader, &LogLoader::logsCaughtUp, this, [this]() {
        mascotLive = true;
        mascotGreeting();
    });
}


//The draft status in the mascot's words, with a matching face
void MainWindow::mascotDraftStatus(QString text)
{
    if(!mascotLive)     return;     //The picks replayed at startup; mascotGreeting says where we are
    if(text.isEmpty())
    {
        //Only its own bubble: the discard screen clears the status right when the mascot shows the cards to remove
        if(!mascotSaysStatus)   return;
        mascotSaysStatus = false;
        mascotWindow->setMood(MascotWindow::Idle);
        mascotWindow->say("");
        return;
    }

    //The pick advice stays until a new pick is scanned or something goes wrong: the other statuses (reading a
    //legendary group's preview or the deck list, notices) come while the player still looks at the pick
    if(mascotSaysAdvice)
    {
        const bool newPickOrProblem = (text.startsWith("Scanning") && !text.startsWith("Scanning the deck list")) ||
                                      text.startsWith("Looking for the arena") ||
                                      text.startsWith("Can't see") || text.contains("Game Mode");
        if(!newPickOrProblem)   return;
        mascotSaysAdvice = false;
    }

    //The same status again (e.g. a retry) keeps the line: the random variants would flicker
    if(mascotSaysStatus && text == mascotLastStatus)    return;
    //Two loops disagreeing (a status coming back within seconds of the current one) would flip the line every
    //second: the current one stays
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 flipTime = 5000;
    if(mascotSaysStatus && now - mascotStatusShownAt.value(mascotLastStatus, 0) < flipTime &&
            now - mascotStatusShownAt.value(text, 0) < flipTime)
    {
        mascotStatusShownAt[mascotLastStatus] = now;
        return;
    }
    mascotStatusShownAt[text] = now;
    mascotLastStatus = text;

    MascotWindow::Mood mood = text.endsWith("...") ? MascotWindow::Thinking : MascotWindow::Idle;
    QString line = text;
    if(text.contains("Game Mode"))
    {
        mood = MascotWindow::Smug;
        line = "Everything's under control. But macOS Game Mode slows me down: turn it off with the gamepad icon in the menu bar.";
    }
    else if(text.startsWith("Can't see the arena screen"))
    {
        mood = MascotWindow::Sweat;
        line = "I can't see the arena! Give me Screen Recording permission and I'm all yours.";
    }
    else if(text.startsWith("Looking for the discard screen"))
    {
        //On the discard or Ready Up screen the mascot already said something better
        if(mascotRedraftScreenShown != RedraftScreenOther)  return;
        line = "Waiting for the discard screen...";
    }
    else if(text.startsWith("Looking for the arena screen"))    line = "Where's the arena? Show me the draft.";
    else if(text.startsWith("Scanning heroes"))                 line = mascotPick({"Picking a hero? Let me see...",
                                                                                  "Heroes, huh. Let me take a look..."});
    else if(text.startsWith("Scanning"))                        line = mascotPick({"Hmm... let me look at these cards...",
                                                                                  "Let me take a look...",
                                                                                  "Checking the numbers...",
                                                                                  "Hold on, genius at work..."});
    else if(text.startsWith("Analyzing bundle"))                line = "Let me peek at this group...";
    else if(text.startsWith("Bundle read"))                     line = "Got the group's cards. Choose whenever you're ready.";
    else if(text.startsWith("Can't read this bundle"))          line = "Can't read this group. I'll catch its cards after the pick.";
    else if(text.startsWith("Downloading card images"))         line = "Grabbing card pics" + text.mid(QString("Downloading card images").length());
    mascotWindow->setMood(mood);
    mascotWindow->say(line);
    mascotSaysStatus = true;
}


//After a draft or a redraft, on the Ready Up screen
QStringList MainWindow::mascotGoodLuckLines()
{
    return {"Go get 'em!",
            "Good luck. Not that you need it with my picks.",
            "Queue up. I've got the popcorn ready.",
            "Go win. I'll be judging every misplay.",
            "You've got this. And you've got me."};
}


void MainWindow::setSplashOpen()
{
    splashOpen = true;
}


//The first run downloads the card images under the splash: the mascot comes after it
void MainWindow::splashClosed()
{
    splashOpen = false;
    if(initDone)    mascotWindow->show();
}


//Once the logs are caught up: the tracker may start in the middle of a draft or a run
void MainWindow::mascotGreeting()
{
    bool hearthstoneRunning = true;
#ifdef Q_OS_MAC
    hearthstoneRunning = !MacOcr::hearthstoneWindowRect().isNull();
#endif
    bool screenRecording = true;
#ifdef Q_OS_MAC
    screenRecording = MacWindow::hasScreenRecording();
    if(!screenRecording)    MacWindow::requestScreenRecording();
#endif
    //Without it the mascot sees no draft: asked first. Stays until something else is said.
    if(!screenRecording)
    {
        mascotWindow->setMood(MascotWindow::Sweat);
        //The log settings written now aren't checked again at the next start: Hearthstone's restart is asked here too
        QString line = "I can't see your screen yet. Allow me in System Settings > Privacy & Security > "
                       "Screen Recording, then restart me.";
        if(logLoader->isHearthstoneRestartNeeded() && hearthstoneRunning)
            line += " Restart Hearthstone too, so I can read its logs.";
        mascotWindow->say(line);
    }
    //First run with Hearthstone already open: it only logs after a restart. Stays until something else is said.
    else if(logLoader->isHearthstoneRestartNeeded() && hearthstoneRunning)
    {
        mascotWindow->setMood(MascotWindow::Sweat);
        mascotWindow->say("First time here? Restart Hearthstone so I can read its logs. I'll wait.");
    }
    else if(draftHandler->isRedrafting())
    {
        mascotWindow->setMood(MascotWindow::Smile);
        mascotWindow->say(mascotPick({"Redraft time! Let's patch this deck up.",
                                      "A redraft? Good, I had some notes on this deck anyway."}), 10000);
    }
    else if(draftHandler->isDrafting() && !draftHandler->isEmptyDeck())
    {
        mascotWindow->setMood(MascotWindow::Sweat);
        mascotWindow->say("You started without me? No worries, I missed a few picks but I'll help with the rest.", 10000);
    }
    else if(!draftHandler->isDrafting() && getLoadingScreen() == arena && mascotNoRun)
    {
        mascotWindow->setMood(MascotWindow::Smile);
        mascotWindow->say(mascotPick({"Fresh start. Let's draft a winner.",
                                      "New run? I've been waiting for this. Let's go."}), 8000);
    }
    else if(!draftHandler->isDrafting() && getLoadingScreen() == arena)
    {
        mascotWindow->setMood(MascotWindow::Smile);
        mascotWindow->say(mascotPick({"Oh, you're back. I kept the popcorn warm.",
                                      "Welcome back. Let's pick up where we left off.",
                                      "There you are. The deck's been waiting."}), 8000);
    }
    //The main menu, or Hearthstone not started yet: the line waits until the mascot shows with Hearthstone
    else if(!draftHandler->isDrafting())
    {
        mascotWindow->setMood(MascotWindow::Smile);
        mascotWindow->say(mascotPick({"Hey! Open the arena, I'll help you draft something great.",
                                      "Arena time? Relax, I've got the brains here.",
                                      "Ready when you are. Open the arena, let's win something."}), 10000);
    }
}


//A new deck is done (the Ready Up screen): its average score and good luck
void MainWindow::mascotDraftFinished(int knownCards, float avgFire, float avgHA)
{
    if(!mascotLive)     return;
    mascotSaysAdvice = false;
    mascotSaysStatus = false;

    QString line;
    MascotWindow::Mood mood = MascotWindow::Smug;
    if(avgFire > 0 || avgHA > 0)
    {
        //Thresholds are a first guess of what a good arena deck averages
        const bool byFire = avgFire > 0;
        const float avg = byFire ? avgFire : avgHA;
        line = byFire ? QStringLiteral("Deck avg: %1% winrate. ").arg(avgFire, 0, 'f', 1)
                      : QStringLiteral("Deck avg: %1 on HearthArena. ").arg(qRound(avgHA));
        if(avg >= (byFire ? 54 : 80))
        {
            mood = MascotWindow::Stars;
            line += mascotPick({"A monster. We did good. ", "A monster. I've outdone myself. "});
        }
        else if(avg >= (byFire ? 52.5f : 65))
        {
            mood = MascotWindow::Grin;
            line += mascotPick({"Solid deck, nice drafting. ", "Solid. My work, obviously. "});
        }
        else if(avg >= (byFire ? 51 : 55))  line += mascotPick({"Decent. We can win with this. ", "Decent. The cards just didn't deserve me. "});
        else
        {
            mood = MascotWindow::Sweat;
            line += mascotPick({"Rough one, but we'll make it work. ", "Rough. The arena offered garbage, I made it edible. "});
        }
        if(knownCards < 30)     line += QStringLiteral("(Only the %1 cards I saw.) ").arg(knownCards);
    }
    line += mascotPick(mascotGoodLuckLines());
    mascotWindow->setMood(mood);
    mascotWindow->say(line, 15000);
}


//The discard screen of a redraft, or the Ready Up screen after it
void MainWindow::mascotRedraftScreen(int screen)
{
    mascotSaysAdvice = false;
    mascotRedraftScreenShown = screen;
    mascotSaysStatus = false;
    if(screen == RedraftScreenDiscard)
    {
        QList<MascotWindow::Section> sections;
        const QList<RedraftSuggestion> suggestions = draftHandler->getRedraftRemoveSuggestions();
        for(const RedraftSuggestion &suggestion: suggestions)
        {
            MascotWindow::Section section{suggestion.source, {}};
            for(const RedraftSuggestionCard &card: suggestion.cards)
                section.rows << MascotWindow::Row{card.name, card.score, card.code, mascotRarityColor(card.code)};
            sections << section;
        }
        mascotWindow->setMood(MascotWindow::Point);
        if(sections.isEmpty())  mascotWindow->say("Cut the weakest ones. I have no scores for this deck, sorry.");
        else                    mascotWindow->saySections(mascotPick({"Cut these, if you ask me:",
                                                                      "These go. Trust me:",
                                                                      "Cut these, and don't get sentimental:"}), sections);
    }
    else if(screen == RedraftScreenReadyUp)
    {
        mascotWindow->setMood(MascotWindow::Smug);
        mascotWindow->say("Deck's ready. " + mascotPick(mascotGoodLuckLines()), 10000);
    }
    else
    {
        mascotWindow->setMood(MascotWindow::Idle);
        mascotWindow->say("");
    }
}


//Praise by wins, sympathy by losses, and once per good run a nudge to support the development
void MainWindow::mascotArenaRecord(int wins, int losses, bool lastWon)
{
    if(!mascotLive)     return;
    mascotSaysStatus = false;
    if(wins + losses <= 1)  mascotSupportAsked = false;     //A new run
    mascotLastWon = lastWon;
    mascotLastLosses = losses;

    //The game's line came at its end (mascotEndGame): here only the run's milestones
    QString line;
    MascotWindow::Mood mood = MascotWindow::Happy;
    if(lastWon)
    {
        static const QMap<int, QString> winLines = {
            {5, "Five wins. Not bad for someone who listens to me."},
            {6, "Six! Now we're cooking."},
            {7, "SEVEN WINS. Told you this deck was a beast."},
            {8, "Eight. We're basically geniuses. Mostly me."},
            {9, "Nine wins! The opponents are starting to cry."},
            {10, "Ten! Somebody call Blizzard, we broke the arena."},
            {11, "Eleven. One more. Don't choke. No pressure. Okay, some pressure."},
            {12, "TWELVE WINS! I drafted it, you just clicked. We're legends."}
        };
        line = winLines.value(wins);
        if(wins >= 7)   mood = MascotWindow::Stars;
        else if(wins >= 5)  mood = MascotWindow::Grin;
    }
    else
    {
        mood = MascotWindow::Sweat;
        //The final wins are said on the rewards screen, from its chest: the tracker may have missed games
        if(losses >= 3)         line = mascotPick({"That's three. Let's go see the loot.",
                                                   "Three losses, run's over. Chin up, loot time."});
        else if(losses == 2)    line = mascotPick({"Two losses. Careful now, one more and we're done.",
                                                   "Two down. Deep breath, we've still got this."});
    }
    if(!line.isEmpty())
    {
        mascotWindow->setMood(mood);
        mascotWindow->say(line, 8000);
    }

    //After a win, back on the Ready Up screen, its medal tells the real wins: the support ask comes on a good run,
    //after the win line
    if(lastWon && !mascotSupportAsked)
        QTimer::singleShot(6000, this, [this]() { if(!mascotInGame)    draftHandler->readReadyUpWins(); });
}


//The support ask, once per run, right after a win that takes the run to MASCOT_SUPPORT_WINS or more
void MainWindow::mascotReadyUpWins(int wins)
{
    if(!mascotLive || mascotInGame || mascotSupportAsked || wins < MASCOT_SUPPORT_WINS)  return;
    mascotSupportAsked = true;
    mascotSaysStatus = false;
    mascotWindow->setMood(MascotWindow::Smile);
    mascotWindow->say(QStringLiteral("%1 wins and counting! Enjoying them? Support my development. Genius runs on popcorn.").arg(wins),
                      15000, "Support", []() {
        QDesktopServices::openUrl(QUrl(MASCOT_SUPPORT_URL));
    });
}


//The run is over (the rewards screen): its chest shows the final wins, read before saying them
void MainWindow::mascotRunComplete()
{
    mascotRewardsRetired = mascotRetired;
    mascotRetired = false;
    if(!mascotLive)     return;
    mascotSaysStatus = false;
    draftHandler->readRewardsWins();
}


//The final wins (-1: the chest couldn't be read, the tracker's record is used)
void MainWindow::mascotRewards(int wins)
{
    if(!mascotLive)     return;
    MascotWindow::Mood mood = MascotWindow::Sweat;
    QString line;
    if(mascotRewardsRetired)
    {
        mood = MascotWindow::Smile;
        line = mascotPick({"Retired? Fair call. Some decks just aren't meant to be. Next one's ours.",
                           "A tactical retreat. Let's draft a better one.",
                           "Retiring? Smart. I never liked that deck anyway."});
    }
    else if(wins < 0)
    {
        line = mascotLastWon ? "TWELVE WINS! I drafted it, you just clicked. We're legends."
                             : "Run's over. Good run anyway. Next draft will be even better.";
        mood = mascotLastWon ? MascotWindow::Stars : MascotWindow::Sweat;
    }
    else if(wins == 12)
    {
        mood = MascotWindow::Stars;
        line = "TWELVE WINS! I drafted it, you just clicked. We're legends.";
    }
    else if(wins >= 7)
    {
        mood = MascotWindow::Stars;
        line = QStringLiteral("%1 wins! ").arg(wins) + mascotPick({"That's a monster run. Told you that deck was good.",
                                                                     "What a run. We make a great team. Mostly me."});
    }
    else if(wins >= MASCOT_SUPPORT_WINS)
    {
        mood = MascotWindow::Grin;
        line = QStringLiteral("%1 wins. ").arg(wins) + mascotPick({"Solid run, enjoy the loot.",
                                                                     "Not bad at all. Next one goes even deeper."});
    }
    else
    {
        line = QStringLiteral("Run's over: %1 win%2. ").arg(wins).arg(wins == 1 ? "" : "s") +
               mascotPick({"Good run anyway. Next draft will be even better.",
                           "The arena wasn't kind today. We'll get it next time."});
    }
    mascotWindow->setMood(mood);
    mascotWindow->say(line, 10000);
}


//The hero with the best class winrate, said by how far ahead it is, and a Rescan in case the heroes were read wrong
void MainWindow::mascotHeroes(int classOrder0, int classOrder1, int classOrder2)
{
    static const QStringList classNames = {"Death Knight", "Demon Hunter", "Druid", "Hunter", "Mage", "Paladin",
                                           "Priest", "Rogue", "Shaman", "Warlock", "Warrior"};
    auto className = [](int classOrder) { return (classOrder >= 0 && classOrder < classNames.count())?classNames[classOrder]:QString("?"); };

    //Best and second best by winrate
    QList<QPair<float, int>> heroes;
    for(int classOrder: {classOrder0, classOrder1, classOrder2})    heroes << qMakePair(ScoreButton::getHeroScore(classOrder), classOrder);
    std::sort(heroes.begin(), heroes.end(), [](const QPair<float, int> &a, const QPair<float, int> &b) { return a.first > b.first; });

    QString line;
    if(heroes[0].first <= 0)    line = "No winrates for these heroes yet. Go with your gut.";
    else
    {
        const QString best = className(heroes[0].second), second = className(heroes[1].second);
        const QString winrate = QString::number(heroes[0].first, 'f', 1) + "%";
        const float lead = heroes[0].first - heroes[1].first;
        QStringList lines;
        if(lead >= 3)
        {
            lines = {QStringLiteral("%1 is a no-brainer here. %2 winrate.").arg(best, winrate),
                     QStringLiteral("%1. Don't even think about the other two. %2.").arg(best, winrate),
                     QStringLiteral("%1, obviously. %2 winrate, the rest is trash.").arg(best, winrate)};
        }
        else if(lead >= 1)
        {
            lines = {QStringLiteral("I'd take %1. %2 winrate, a notch above %3.").arg(best, winrate, second),
                     QStringLiteral("%1 has the edge: %2 winrate.").arg(best, winrate),
                     QStringLiteral("Go %1. %2, the others can't keep up.").arg(best, winrate)};
        }
        else
        {
            lines = {QStringLiteral("Coin flip between %1 and %2. I'd go %1, %3.").arg(best, second, winrate),
                     QStringLiteral("%1 or %2, basically the same. %1 by a hair: %3.").arg(best, second, winrate),
                     QStringLiteral("Tough one, even for me. %1 at %3, %2 right behind.").arg(best, second, winrate)};
        }
        line = lines[QRandomGenerator::global()->bounded(lines.count())];
    }

    mascotSaysAdvice = true;
    mascotSaysStatus = false;
    mascotWindow->setMood(MascotWindow::Point);
    mascotWindow->say(line + "\n\nAm I hallucinating? Try:", 0, "Rescan", [this]() {
        mascotWindow->setMood(MascotWindow::Thinking);
        mascotWindow->say(mascotPick({"Rescanning... let me take a better look.", "Rescanning... okay, even geniuses blink."}));
        mascotSaysAdvice = false;
        mascotSaysStatus = true;    //Replaced by the draft status or the heroes again
        draftHandler->rescan();
    });
}


//Card names in the mascot's bubble, in their rarity color (darker than in the game, to read on white)
QColor MainWindow::mascotRarityColor(const QString &code)
{
    switch(Utility::getRarityFromCode(code))
    {
        case RARE:      return QColor(0, 112, 221);
        case EPIC:      return QColor(163, 53, 238);
        case LEGENDARY: return QColor(230, 120, 0);
        case COMMON:
        case FREE:      return QColor(120, 120, 120);
        default:        return Qt::black;
    }
}


//The pick with the best score (Firestone, else HearthArena), said by how far ahead it is
void MainWindow::mascotCards()
{
    const PickScores pick = draftHandler->getPickScores();
    const bool byFire = pick.showFire && std::max({pick.fire[0], pick.fire[1], pick.fire[2]}) > 0;

    //Ordered by the rating of both sources (Firestone trusted by its games, HearthArena); the line quotes Firestone
    float ratings[3];
    for(int i=0; i<3; i++)
    {
        ratings[i] = PickRating::rating({pick.showFire ? pick.fire[i] : 0, pick.fireGames[i], pick.showHA ? pick.ha[i] : 0});
    }
    int order[3] = {0, 1, 2};
    std::sort(order, order+3, [&ratings](int a, int b) { return ratings[a] > ratings[b]; });
    const int bestIndex = order[0];
    const bool anyScore = std::max({pick.fire[0], pick.fire[1], pick.fire[2], pick.ha[0], pick.ha[1], pick.ha[2]}) > 0;

    QString line;
    if(!anyScore || !PickRating::isReady())  line = "No scores for these. Trust your gut, you've got this.";
    else
    {
        QString best = MascotWindow::colored(pick.names[bestIndex], mascotRarityColor(pick.codes[bestIndex]));
        QString second = MascotWindow::colored(pick.names[order[1]], mascotRarityColor(pick.codes[order[1]]));
        if(pick.legendaryGroup)
        {
            best += "'s group";
            second += "'s group";
        }
        const QString score = (byFire && pick.fire[bestIndex] > 0) ? QString::number(pick.fire[bestIndex], 'f', 1) + "% winrate" :
                                                                     QString::number(qRound(pick.ha[bestIndex])) + " on HearthArena";
        const float lead = ratings[bestIndex] - ratings[order[1]];
        const float big = 0.6f, small = 0.25f;
        QStringList lines;
        if(lead >= big)
        {
            lines = {QStringLiteral("%1 is a no-brainer. %2.").arg(best, score),
                     QStringLiteral("%1, easy. %2, the other two are filler.").arg(best, score),
                     QStringLiteral("Take %1 and don't look back. %2.").arg(best, score),
                     QStringLiteral("%1. %2. I'd bet my hat on it.").arg(best, score)};
        }
        else if(lead >= small)
        {
            lines = {QStringLiteral("I'd take %1. %2, a notch above %3.").arg(best, score, second),
                     QStringLiteral("%1 has the edge: %2.").arg(best, score),
                     QStringLiteral("Go %1. %2. Trust me on this one.").arg(best, score)};
        }
        else
        {
            lines = {QStringLiteral("Coin flip between %1 and %2. I'd go %1: %3.").arg(best, second, score),
                     QStringLiteral("%1 or %2, basically the same. %1 by a hair.").arg(best, second),
                     QStringLiteral("Tough one, even for me. %1 at %3, %2 right behind.").arg(best, second, score)};
        }
        line = lines[QRandomGenerator::global()->bounded(lines.count())];
        if(byFire && pick.fireGames[bestIndex] >= 0 && pick.fireGames[bestIndex] < 200)
            line += QStringLiteral(" Only %1 games though, grain of salt.").arg(pick.fireGames[bestIndex]);
        //Not the best Firestone winrate: HearthArena made the difference
        int bestFire = 0;
        for(int i=1; i<3; i++)  if(pick.fire[i] > pick.fire[bestFire])    bestFire = i;
        if(byFire && pick.showHA && bestFire != bestIndex && pick.ha[bestIndex] > pick.ha[bestFire])
            line += mascotPick({" HearthArena rates it way higher, and so do I.", " Winrates are close, HearthArena breaks the tie."});
    }

    mascotSaysAdvice = true;
    mascotSaysStatus = false;
    mascotWindow->setMood(MascotWindow::Point);
    mascotWindow->say(line + "\n\nAm I hallucinating? Try:", 0, "Rescan", [this]() {
        mascotWindow->setMood(MascotWindow::Thinking);
        mascotWindow->say(mascotPick({"Rescanning... let me take a better look.", "Rescanning... okay, even geniuses blink."}));
        mascotSaysAdvice = false;
        mascotSaysStatus = true;    //Replaced by the draft status or the cards again
        draftHandler->rescan();
    });
}


void MainWindow::mascotStartGame()
{
    if(!mascotLive)     return;
    mascotSaysAdvice = false;
    mascotSaysStatus = false;
    mascotInGame = true;
    mascotWindow->setMood(MascotWindow::Popcorn);
    mascotWindow->say(mascotPick({"Popcorn time. Show me what this deck can do.",
                                  "Good luck! I'll be right here with the popcorn.",
                                  "Game on. I'll be judging every misplay."}), 5000);
}


//Secrets are not read yet: the mascot only notices them, and the first time asks for support to learn it
void MainWindow::mascotEnemySecret()
{
    if(!mascotLive || !mascotInGame)    return;
    mascotSaysStatus = false;
    const int msec = 7000;
    mascotWindow->setMood(MascotWindow::Detective);
    if(mascotSecretsSeen++ == 0)
    {
        mascotWindow->say("A secret! I can see it... but I can't read secrets yet. Help me learn?", 12000, "Support", []() {
            QDesktopServices::openUrl(QUrl(MASCOT_SUPPORT_URL));
        });
    }
    else
    {
        static const QStringList lines = {
            "Another secret. My magnifier is ready, my brain isn't. Coming soon.",
            "Secret spotted. What is it? No idea. Yet.",
            "Hmm, a secret. Detective school costs popcorn money.",
            "Elementary! It's a secret. That's all I've got."
        };
        mascotWindow->say(lines[QRandomGenerator::global()->bounded(lines.count())], msec);
    }
    //Back to the popcorn when the line is over, unless the game or the mood moved on
    QTimer::singleShot(mascotSecretsSeen == 1 ? 12000 : msec, this, [this]() {
        if(mascotInGame && mascotWindow->currentMood() == MascotWindow::Detective)
            mascotWindow->setMood(MascotWindow::Popcorn);
    });
}


void MainWindow::mascotEndGame(bool playerWon, bool playerUnknown)
{
    if(!mascotLive)     return;
    mascotSaysStatus = false;
    mascotInGame = false;
    if(playerUnknown)
    {
        mascotWindow->setMood(MascotWindow::Idle);
        return;
    }
    //Right away; back in the arena menu mascotArenaRecord only adds the run's milestones
    mascotWindow->setMood(playerWon ? MascotWindow::Happy : MascotWindow::Sweat);
    mascotWindow->say(playerWon ? mascotPick({"GG! Told you that deck was good.",
                                              "Nice one! As I calculated.",
                                              "GG. Great game, great deck.",
                                              "GG. I'd say you played well, but I watched."})
                                : mascotPick({"Unlucky. RNG hates us today.",
                                              "Shake it off. Next one's ours.",
                                              "A loss. Happens to the best of us. Even me, apparently."}), 8000);
}


void MainWindow::minimizeToDock()
{
    hiddenToDock.clear();
    const QList<QWidget *> windows = {this, deckWindow, arenaWindow, enemyWindow, enemyDeckWindow, graveyardWindow, planWindow};
    for(QWidget *window: windows)
    {
        if(window != nullptr && window->isVisible())
        {
            hiddenToDock << window;
            window->hide();
        }
    }
}


//The Dock icon click comes as an activation, even when the app is already active
void MainWindow::restoreFromDock(Qt::ApplicationState state)
{
    if(state != Qt::ApplicationActive || hiddenToDock.isEmpty())    return;
    for(const QPointer<QWidget> &window: std::as_const(hiddenToDock))
    {
        if(window != nullptr)   window->show();
    }
    hiddenToDock.clear();
    activateWindow();
    raise();
}


//The built-in look of the old windows: theme downloads are gone
void MainWindow::initConfigTheme()
{
    ThemeHandler::defaultEmptyValues();
    spreadTheme();
}


void MainWindow::moveInScreen(QPoint pos, QSize size)
{
    QRect appRect(pos, size);
    QPoint midPoint = appRect.center();

    QString message =
            "Window Pos: (" + QString::number(pos.x()) + "," + QString::number(pos.y()) +
            ") - Size: (" + QString::number(size.width()) + "," + QString::number(size.height()) +
            ") - Mid: (" + QString::number(midPoint.x()) + "," + QString::number(midPoint.y()) + ")";
    pDebug(message);

    for(QScreen *screen: (const QList<QScreen *>)QGuiApplication::screens())
    {
        if (!screen)    continue;
        QRect geometry = screen->geometry();

        if(geometry.contains(midPoint))
        {
            message =
                    "Window in screen: (" + QString::number(geometry.left()) + "," + QString::number(geometry.top()) + "," +
                    QString::number(geometry.right()) + "," + QString::number(geometry.bottom()) + ")";
            pDebug(message);
            move(pos);
            return;
        }
    }

    message = "Window outside screens. Move to (0,0)";
    pDebug(message);
    move(QPoint(0,0));
}


void MainWindow::readSettings()
{
    //New Config Step 1 - Cargar valores

    QSettings settings("Arena Tracker", "Arena Tracker");
    QPoint pos;
    QSize size;

    pos = settings.value("pos", QPoint(0,0)).toPoint();
    size = settings.value("size", QSize(255, 600)).toSize();

    this->transparency = static_cast<Transparency>(settings.value("transparent", AutoTransparent).toInt());
    int cardHeight = settings.value("cardHeight", 35).toInt();
    this->drawDisappear = settings.value("drawDisappear", 5).toInt();
    int popularCardsShown = settings.value("popularCardsShown", 5).toInt();
    bool showDraftScoresOverlay = settings.value("showDraftScoresOverlay", true).toBool();
    bool draftLearningMode = settings.value("draftLearningMode", false).toBool();
    bool draftMethodHA = settings.value("draftMethodHA", true).toBool();
    bool draftMethodLF = settings.value("draftMethodFire", true).toBool();
    int tooltipScale = settings.value("tooltipScale", 10).toInt();
    bool autoSize = false;//settings.value("autoSize", false).toBool();//Disable autoSize
    bool showClassColor = settings.value("showClassColor", true).toBool();
    bool showSpellColor = settings.value("showSpellColor", true).toBool();
    bool showManaLimits = settings.value("showManaLimits", true).toBool();
    bool showTotalAttack = settings.value("showTotalAttack", true).toBool();
    bool showRngList = settings.value("showRngList", true).toBool();
    bool showSecrets = settings.value("showSecrets", true).toBool();
    bool showWildSecrets = settings.value("showWildSecrets", false).toBool();
    bool twitchChatVotes = settings.value("twitchChatVotes", false).toBool();
    bool showMyWR = settings.value("showMyWR", true).toBool();
    bool downloadLB = settings.value("downloadLB", true).toBool();


    initConfigTab(tooltipScale, cardHeight, autoSize, showClassColor, showSpellColor, showManaLimits, showTotalAttack, showRngList,
                  twitchChatVotes, draftMethodHA, draftMethodLF, popularCardsShown,
                  showSecrets, showWildSecrets, showDraftScoresOverlay, draftLearningMode,
                  showMyWR, downloadLB);


    this->setAttribute(Qt::WA_TranslucentBackground, transparency!=Framed);
    this->showWindowFrame(transparency == Framed);
    this->setMinimumSize(100,200);  //El minimumSize inicial es incorrecto
    resize(size);
    moveInScreen(pos, size);
    calculateMinimumWidth();

#ifdef OLD_TRACKER_WINDOWS
    //Detach Windows
    if(settings.value("deckWindow", true).toBool())         createDetachWindow(ui->tabDeck);
    if(settings.value("arenaWindow", false).toBool())       createDetachWindow(ui->tabArena);
    if(settings.value("enemyWindow", false).toBool())       createDetachWindow(ui->tabEnemy);
    if(settings.value("enemyDeckWindow", false).toBool())   createDetachWindow(ui->tabEnemyDeck);
    if(settings.value("graveyardWindow", false).toBool())   createDetachWindow(ui->tabGraveyard);
    if(settings.value("planWindow", false).toBool())        createDetachWindow(ui->tabPlan);
#endif
}


void MainWindow::writeSettings()
{
    //New Config Step 2 - Guardar valores

    QSettings settings("Arena Tracker", "Arena Tracker");
    settings.setValue("pos", pos());
    settings.setValue("size", size());
    settings.setValue("transparent", static_cast<int>(this->transparency));
    settings.setValue("cardHeight", ui->configSliderCardSize->value());
    settings.setValue("drawDisappear", this->drawDisappear);
    settings.setValue("popularCardsShown", ui->configSliderPopular->value());
    settings.setValue("showDraftScoresOverlay", ui->configCheckScoresOverlay->isChecked());
    settings.setValue("draftLearningMode", ui->configCheckLearning->isChecked());
    settings.setValue("draftMethodHA", ui->configCheckHA->isChecked());
    settings.setValue("draftMethodFire", ui->configCheckLF->isChecked());
    settings.setValue("tooltipScale", ui->configSliderTooltipSize->value());
    settings.setValue("autoSize", ui->configCheckAutoSize->isChecked());
    settings.setValue("showClassColor", ui->configCheckClassColor->isChecked());
    settings.setValue("showSpellColor", ui->configCheckSpellColor->isChecked());
    settings.setValue("showManaLimits", ui->configCheckManaLimits->isChecked());
    settings.setValue("showTotalAttack", ui->configCheckTotalAttack->isChecked());
    settings.setValue("showRngList", ui->configCheckRngList->isChecked());
    settings.setValue("showSecrets", ui->configCheckSecrets->isChecked());
    settings.setValue("showWildSecrets", ui->configCheckWildSecrets->isChecked());
    settings.setValue("twitchChatVotes", ui->configCheckVotes->isChecked());
    settings.setValue("showMyWR", ui->configCheckWR->isChecked());
    settings.setValue("downloadLB", ui->configCheckLB->isChecked());
    settings.setValue("deckWindow", deckWindow != nullptr);
    settings.setValue("arenaWindow", arenaWindow != nullptr);
    settings.setValue("enemyWindow", enemyWindow != nullptr);
    settings.setValue("enemyDeckWindow", enemyDeckWindow != nullptr);
    settings.setValue("graveyardWindow", graveyardWindow != nullptr);
    settings.setValue("planWindow", planWindow != nullptr);

}


void MainWindow::initConfigTab(int tooltipScale, int cardHeight, bool autoSize, bool showClassColor, bool showSpellColor,
                               bool showManaLimits, bool showTotalAttack, bool showRngList,
                               bool twitchChatVotes, bool draftMethodHA, bool draftMethodLF,
                               int popularCardsShown, bool showSecrets, bool showWildSecrets, bool showDraftScoresOverlay,
                               bool draftLearningMode, bool showMyWR,
                               bool downloadLB)
{
    //New Config Step 3 - Actualizar UI con valores cargados

    //UI
    switch(transparency)
    {
        case Transparent:
            ui->configRadioTransparent->setChecked(true);
            break;
        case AutoTransparent:
            ui->configRadioAuto->setChecked(true);
            break;
        case Opaque:
            ui->configRadioOpaque->setChecked(true);
            break;
        case Framed:
            ui->configRadioFramed->setChecked(true);
            break;
//        default:
//            transparency = AutoTransparent;
//            ui->configRadioAuto->setChecked(true);
//            break;
    }

    initConfigTheme();

    //Games
    if(downloadLB)                  ui->configCheckLB->setChecked(true);

    //Deck
    if(cardHeight<ui->configSliderCardSize->minimum() || cardHeight>ui->configSliderCardSize->maximum())  cardHeight = 35;
    if(ui->configSliderCardSize->value() == cardHeight)   updateTamCard(cardHeight);
    else    ui->configSliderCardSize->setValue(cardHeight);

    if(tooltipScale<ui->configSliderTooltipSize->minimum() || tooltipScale>ui->configSliderTooltipSize->maximum())  tooltipScale = 10;
    if(ui->configSliderTooltipSize->value() == tooltipScale) updateTooltipScale(tooltipScale);
    else ui->configSliderTooltipSize->setValue(tooltipScale);

    ui->configCheckAutoSize->setChecked(autoSize);

    ui->configCheckClassColor->setChecked(showClassColor);
    updateShowClassColor(showClassColor);

    ui->configCheckSpellColor->setChecked(showSpellColor);
    updateShowSpellColor(showSpellColor);

    ui->configCheckManaLimits->setChecked(showManaLimits);
    updateShowManaLimits(showManaLimits);

    //Hand
    //Slider            0  - Ns - 11
    //DrawDissapear     -1 - Ns - 0
    switch(this->drawDisappear)
    {
        case -1:
            ui->configSliderDrawTime->setValue(0);
            break;
        case 0:
            ui->configSliderDrawTime->setValue(11);
            break;
        default:
            if(this->drawDisappear<-1 || this->drawDisappear>10)    this->drawDisappear = 5;
            ui->configSliderDrawTime->setValue(this->drawDisappear);
            break;
    }

    ui->configSliderPopular->setValue(popularCardsShown);

    ui->configCheckTotalAttack->setChecked(showTotalAttack);

    ui->configCheckRngList->setChecked(showRngList);

    ui->configCheckSecrets->setChecked(showSecrets);

    ui->configCheckWildSecrets->setChecked(showWildSecrets);


    //Draft
    if(showDraftScoresOverlay)      ui->configCheckScoresOverlay->setChecked(true);
    updateShowDraftScoresOverlay(showDraftScoresOverlay);

    if(draftLearningMode)           ui->configCheckLearning->setChecked(true);

    if(showMyWR)                    ui->configCheckWR->setChecked(true);
    updateShowMyWR(showMyWR);

    ui->configCheckHA->setChecked(draftMethodHA);
    ui->configCheckLF->setChecked(draftMethodLF);
    spreadDraftMethod(draftMethodHA, draftMethodLF);

    //Twitch
    ui->configCheckVotes->setChecked(twitchChatVotes);
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    QMainWindow::closeEvent(event);

    hide();
    writeSettings();
    if(deckWindow != nullptr)
    {
        deckWindow->close();
        deckWindow = nullptr;
    }
    if(arenaWindow != nullptr)
    {
        arenaWindow->close();
        arenaWindow = nullptr;
    }
    if(enemyWindow != nullptr)
    {
        enemyWindow->close();
        enemyWindow = nullptr;
    }
    if(enemyDeckWindow != nullptr)
    {
        enemyDeckWindow->close();
        enemyDeckWindow = nullptr;
    }
    if(graveyardWindow != nullptr)
    {
        graveyardWindow->close();
        graveyardWindow = nullptr;
    }
    if(planWindow != nullptr)
    {
        planWindow->close();
        planWindow = nullptr;
    }
    event->accept();
}


void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}


void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        QPoint newPosition = event->globalPos() - dragPosition;
        int top = newPosition.y();
        int bottom = top + this->height();
        int left = newPosition.x();
        int right = left + this->width();
        int midX = (left + right)/2;
        int midY = (top + bottom)/2;

        const int stickyMargin = 10;

        for(QScreen *screen: (const QList<QScreen *>)QGuiApplication::screens())
        {
            if (!screen)    continue;
            QRect screenRect = screen->geometry();
            int topScreen = screenRect.y();
            int bottomScreen = topScreen + screenRect.height();
            int leftScreen = screenRect.x();
            int rightScreen = leftScreen + screenRect.width();

            if(midX < leftScreen || midX > rightScreen ||
                    midY < topScreen || midY > bottomScreen) continue;

            if(std::abs(top - topScreen) < stickyMargin)
            {
                newPosition.setY(topScreen);
            }
            else if(std::abs(bottom - bottomScreen) < stickyMargin)
            {
                newPosition.setY(bottomScreen - this->height());
            }
            if(std::abs(left - leftScreen) < stickyMargin)
            {
                newPosition.setX(leftScreen);
            }
            else if(std::abs(right - rightScreen) < stickyMargin)
            {
                newPosition.setX(rightScreen - this->width());
            }
            move(newPosition);
            event->accept();
            return;
        }

        move(newPosition);
        event->accept();
    }
}


void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if(event->key() != Qt::Key_Control)
    {
        if(event->modifiers()&Qt::ControlModifier)
        {
            if(event->key() == Qt::Key_R)       resetSettings();
            else if(event->key() == Qt::Key_D)  createDebugPack();
            else if(event->key() == Qt::Key_1)  draftHandler->pickCard("0");
            else if(event->key() == Qt::Key_2)  draftHandler->pickCard("1");
            else if(event->key() == Qt::Key_3)  draftHandler->pickCard("2");
            else if(event->key() == Qt::Key_5)  draftHandler->activeDraftDeck();
#ifdef Q_OS_LINUX
            else if(event->key() == Qt::Key_S)  askLinuxShortcut();
            else if(event->key() == Qt::Key_Z)
            {
                if(this->planWindow == nullptr)    createDetachWindow(ui->tabPlan);
                planWindow->resize(QSize(960, 1080));
                planWindow->move(1920, 0);

                if(this->deckWindow == nullptr)    createDetachWindow(ui->tabDeck);
                deckWindow->resize(QSize(deckWindow->width(), 1000));
                deckWindow->move(1920-deckWindow->width(), 0);

                if(this->enemyWindow == nullptr)    createDetachWindow(ui->tabEnemy);
                enemyWindow->resize(QSize(enemyWindow->width(), 750));
                enemyWindow->move(0, 120);

                if(this->graveyardWindow == nullptr)    createDetachWindow(ui->tabGraveyard);
                graveyardWindow->resize(QSize(graveyardWindow->width(), 540));
                graveyardWindow->move(1920+planWindow->width(), 0);

                this->resize(QSize(400, 540));
                this->move(1920+planWindow->width()+graveyardWindow->width(), 0);
            }
#endif
#ifdef QT_DEBUG
            else if(event->key() == Qt::Key_8)  QtConcurrent::run(&DraftHandler::craftGoldenCopy, this->draftHandler, 0);
            else if(event->key() == Qt::Key_9)  QtConcurrent::run(&DraftHandler::craftGoldenCopy, this->draftHandler, 1);
            else if(event->key() == Qt::Key_0)  QtConcurrent::run(&DraftHandler::craftGoldenCopy, this->draftHandler, 2);
            else if(event->key() == Qt::Key_6)  draftHandler->beginHeroDraft();
            else if(event->key() == Qt::Key_7)
                draftHandler->beginDraft(Utility::classEnum2classLogNumber(WARLOCK), deckHandler->getDeckCardList(), true);
#endif
        }
    }

    QMainWindow::keyPressEvent(event);
}


//Restaura ambas ventanas minimizadas
void MainWindow::changeEvent(QEvent * event)
{
    if(event->type() == QEvent::WindowStateChange)
    {
        if((windowState() & Qt::WindowMinimized) == 0)
        {
            if(deckWindow != nullptr)      deckWindow->setWindowState(Qt::WindowActive);
            if(arenaWindow != nullptr)     arenaWindow->setWindowState(Qt::WindowActive);
            if(enemyWindow != nullptr)     enemyWindow->setWindowState(Qt::WindowActive);
            if(enemyDeckWindow != nullptr) enemyDeckWindow->setWindowState(Qt::WindowActive);
            if(graveyardWindow != nullptr) graveyardWindow->setWindowState(Qt::WindowActive);
            if(planWindow != nullptr)      planWindow->setWindowState(Qt::WindowActive);
            if(draftHandler != nullptr)    draftHandler->deMinimizeScoreWindow();
        }
    }
}


void MainWindow::leaveEvent(QEvent * e)
{
    QMainWindow::leaveEvent(e);

    this->mouseInApp = false;
    spreadMouseInApp();
}


void MainWindow::enterEvent(QEnterEvent * e)
{
    QMainWindow::enterEvent(e);

    this->mouseInApp = true;
    spreadMouseInApp();
}


void MainWindow::spreadMouseInApp()
{
    QWidget *currentTab = ui->tabWidget->currentWidget();

    if(currentTab == ui->tabDeck)           deckHandler->setMouseInApp(mouseInApp);
    else                                    updateOtherTabsTransparency();

    //Fade Bar
    //En Transparent --> EnemyHandHandler::updateTransparency() se encarga de llamar a fadeBarAndButtons
    if(transparency==Transparent && currentTab != ui->tabEnemy)
    {
        if(mouseInApp)      fadeBarAndButtons(false);
        else                fadeBarAndButtons(true);
    }
    //En AutoTransparent --> DeckHandler/EnemyDeckHandler/EnemyHandHandler::updateTransparency() se encarga de llamar a fadeBarAndButtons
    else if(transparency==AutoTransparent &&
            currentTab != ui->tabDeck && currentTab != ui->tabEnemy && currentTab != ui->tabEnemyDeck && currentTab != ui->tabGraveyard)
    {
        fadeBarAndButtons(false);
    }
}


void MainWindow::changingTabResetSizePlan()
{
}


void MainWindow::resizeSlot(QSize size)
{
    resize(size);
}


void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    resizeChecks();
    fitProgressBarText();
    event->accept();
}


void MainWindow::resizeChecks()
{
    QWidget *widget = this->centralWidget();

    updateTabWidgetsTheme(false, true);

    int top = widget->pos().y();
    int bottom = top + widget->height();
    int left = widget->pos().x();
    int right = left + widget->width();

    resizeTopButtons(right - ThemeHandler::borderWidth(), top + ThemeHandler::borderWidth());
    ui->resizeButton->move(right-24, bottom-24);

    if(deckWindow == nullptr) spreadCorrectTamCard(); //Solo es necesario si ui->tabDeck esta en mainWindow, debido a auto size
}


void MainWindow::resizeTopButtons(int right, int top)
{
    int limitWidth = ui->tabWidget->tabBar()->width() + BIG_BUTTONS_H;

    int buttonsWidth;
    bool smallButtons = (this->width() - ThemeHandler::borderWidth()*2) < limitWidth;
    if(smallButtons)
    {
        buttonsWidth = SMALL_BUTTONS_H;
        ui->closeButton->move(right-buttonsWidth, top);
        ui->minimizeButton->move(right-buttonsWidth, top+buttonsWidth);
    }
    else
    {
        buttonsWidth = 24;
        ui->closeButton->move(right-buttonsWidth, top);
        ui->minimizeButton->move(right-2*buttonsWidth, top);
    }

    ui->closeButton->resize(buttonsWidth, buttonsWidth);
    ui->closeButton->setIconSize(QSize(buttonsWidth, buttonsWidth));
    ui->minimizeButton->resize(buttonsWidth, buttonsWidth);
    ui->minimizeButton->setIconSize(QSize(buttonsWidth, buttonsWidth));
}


void MainWindow::moveTabTo(QWidget *widget, QTabWidget *tabWidget)
{
    QIcon icon;
    QString tooltip;
    if(widget == ui->tabDraft)
    {
        icon = HDIcons::tab(HDIcons::TabArena);
        tooltip = "Draft";
    }
    else if(widget == ui->tabArena)
    {
        icon = HDIcons::tab(HDIcons::TabGames);
        tooltip = "Games";
    }
    else if(widget == ui->tabDeck)
    {
        icon = HDIcons::tab(HDIcons::TabDeck);
        tooltip = "Deck";
    }
    else if(widget == ui->tabEnemy)
    {
        icon = HDIcons::tab(HDIcons::TabHand);
        tooltip = "Hand";
    }
    else if(widget == ui->tabPlan)
    {
        icon = HDIcons::tab(HDIcons::TabPlan);
        tooltip = "Replay";
    }
    else if(widget == ui->tabEnemyDeck)
    {
        icon = HDIcons::tab(HDIcons::TabEnemyDeck);
        tooltip = "Enemy Deck";
    }
    else if(widget == ui->tabGraveyard)
    {
        icon = HDIcons::tab(HDIcons::TabGraveyard);
        tooltip = "Graveyard";
    }
    else if(widget == ui->tabConfig)
    {
        icon = HDIcons::tab(HDIcons::TabConfig);
        tooltip = "Config";
    }
    else if(draftHandler != nullptr && widget == draftHandler->getRedraftTab())
    {
        icon = QIcon(ThemeHandler::buttonRemoveDeckFile());
        tooltip = "Redraft: cards to remove";
    }
    tabWidget->addTab(widget, icon, "");
    tabWidget->setTabToolTip(tabWidget->count()-1, tooltip);
}


//Se llama al meter o sacar tabs en tabBar o al cambiar theme (por si tiene bordes)
void MainWindow::calculateMinimumWidth()
{
    //El menor ancho de una tab es 38 y el menor de los botones 19;
    int minWidth = ui->tabWidget->count()*38 + SMALL_BUTTONS_H + ThemeHandler::borderWidth()*2;
    this->setMinimumWidth(minWidth);
}


//Fija la anchura de la ventana de deck, enemyHand y enemyDeck.
void MainWindow::calculateCardWindowMinimumWidth(DetachWindow *detachWindow, bool hasBorders)
{
    if(detachWindow == nullptr)    return;

    int deckWidth = ui->deckListWidget->sizeHintForColumn(0) + (hasBorders?2*ThemeHandler::borderWidth():0);
    if(detachWindow == deckWindow)      deckWindow->setFixedWidth(deckWidth);
    if(detachWindow == enemyWindow)     enemyWindow->setFixedWidth(deckWidth);
    if(detachWindow == enemyDeckWindow) enemyDeckWindow->setFixedWidth(deckWidth);
    if(detachWindow == graveyardWindow) graveyardWindow->setFixedWidth(deckWidth);
}


void MainWindow::pDebug(QString line, DebugLevel debugLevel, QString file)
{
    pDebug(line, 0, debugLevel, file);
}


void MainWindow::pDebug(QString line, qint64 numLine, DebugLevel debugLevel, QString file)
{
    (void)debugLevel;
    QString logLine = "";
    QString timeStamp = QDateTime::currentDateTime().toString("hh:mm:ss");

    while(line.length() > 0 && line[0]==QChar('\n'))
    {
        line.remove(0, 1);
        logLine += '\n';
    }

    if(!line.isEmpty())
    {
        logLine += timeStamp + " - " + file;
        if(numLine > 0) logLine += "(" + QString::number(numLine) + ")";
        logLine += ": " + line;
    }

    qDebug().noquote() << logLine;

    if(atLogFile != nullptr)
    {
        QTextStream stream(atLogFile);
        stream << logLine << Qt::endl;
    }
}


void MainWindow::logReset()
{
    deckHandler->unlockDeckInterface();
    leaveArena();
    gameWatcher->reset();
}


bool MainWindow::checkCardImage(QString code, bool isHero)
{
    if(code.isEmpty())  return true;

    QFileInfo cardFile(Utility::hscardsPath() + "/" + code + ".png");

    if(!cardFile.exists())
    {
        //La bajamos de Github/Hearthpawn
        cardDownloader->downloadWebImage(code, isHero);
        return false;
    }
    return true;
}


void MainWindow::redrawDownloadedCardImage(QString code)
{
    deckHandler->redrawDownloadedCardImage(code);
    draftHandler->reHistDownloadedCardImage(code);
    if(!allCardsDownloadList.isEmpty())     this->updateProgressAllCardsDownload(code);
}


void MainWindow::missingOnWeb(QString code)
{
    draftHandler->reHistDownloadedCardImage(code, true);
    if(!allCardsDownloadList.isEmpty())     this->updateProgressAllCardsDownload(code);
}


void MainWindow::resetSettings()
{
    int ret = QMessageBox::warning(this, tr("Reset settings"),
                                   tr("Do you want to reset Arena Tracker settings?"),
                                   QMessageBox::Ok | QMessageBox::Cancel);

    if(ret == QMessageBox::Ok)
    {
        QSettings settings("Arena Tracker", "Arena Tracker");
        settings.setValue("logsDirPath", "");
        settings.setValue("logConfig", "");
        settings.setValue("playerTag", "");
        settings.setValue("sizeDraft", QSize(255, 600));
        settings.setValue("shortcutAsked", false);
        settings.setValue("arenaVersion", 0);
        settings.setValue("draftingScreenIndex", -1);
        settings.setValue("heroDraftingScreenIndex", -1);

        resize(QSize(255, 600));
        move(QPoint(0,0));
        close();

        //Write detachWindows settings after they are closed
        settings.setValue("deckWindow", false);
        settings.setValue("arenaWindow", false);
        settings.setValue("enemyWindow", false);
        settings.setValue("enemyDeckWindow", false);
        settings.setValue("graveyardWindow", false);
        settings.setValue("planWindow", false);
    }
}


void MainWindow::createLogFile()
{
    QString logPath = Utility::dataPath() + "/ArenaTrackerLog.txt";
    QString logOldPath = Utility::dataPath() + "/ArenaTrackerLog.old";

    //Copy log from previous session
    QFile::remove(logOldPath);
    QFile::rename(logPath, logOldPath);

    atLogFile = new QFile(logPath);
    if(atLogFile->exists())  atLogFile->remove();
    if(!atLogFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        pDebug("Failed to create Arena Tracker log on disk.", DebugLevel::Error);
        atLogFile = nullptr;
    }
}


void MainWindow::closeLogFile()
{
    if(atLogFile == nullptr)   return;
    atLogFile->close();
    delete atLogFile;
    atLogFile = nullptr;
}


void MainWindow::createDataDir()
{
    Utility::createDir(Utility::dataPath());
    if(REMOVE_CARDS_ON_VERSION_UPDATE)  removeHSCards();//Redownload HSCards en esta version
    if(REMOVE_EXTRA_AND_HISTOGRAMS_ON_VERSION_UPDATE)  removeExtraAndHistograms();//Redownload Extra en esta version y recrea histogramas
    if(Utility::createDir(Utility::hscardsPath()))  allCardsDownloadNeeded = true;
    Utility::createDir(Utility::extraPath());
    Utility::createDir(Utility::histogramsPath());
    Utility::createDir(Utility::arenaStatsPath());
}


void MainWindow::downloadExtraFile(QString nameFile)
{
    QFileInfo file = QFileInfo(Utility::extraPath() + "/" + nameFile);
    if(!file.exists())  networkManager->get(QNetworkRequest(QUrl(EXTRA_URL + QString("/") + nameFile)));
}


void MainWindow::downloadExtraFiles()
{
    downloadExtraFile("arenaTemplate.png");
    downloadExtraFile("arenaTemplate2.png");
    downloadExtraFile("heroesTemplate.png");
    downloadExtraFile("heroesTemplate2.png");
    downloadExtraFile("redraftTemplate.png");
    downloadExtraFile("MANA.dat");
    downloadExtraFile("RARITY.dat");

#ifdef Q_OS_LINUX
    if(CaptureManager::isWaylandSession())
    {
        downloadExtraFile("captureHelper");
    }
#endif

    QFileInfo file = QFileInfo(Utility::extraPath() + "/icon.png");
    if(!file.exists())  networkManager->get(QNetworkRequest(QUrl(IMAGES_URL + QString("/icon.png"))));
}


void MainWindow::downloadHearthArenaVersion()
{
    networkManager->get(QNetworkRequest(QUrl(HA_URL + QString("/haVersion.json"))));
}


void MainWindow::downloadHearthArenaJson(int version)
{
    bool needDownload = false;
    QSettings settings("Arena Tracker", "Arena Tracker");
    int storedVersion = settings.value("haVersion", 0).toInt();

    QFileInfo fileInfo(Utility::extraPath() + "/hearthArena.json");
    if(!fileInfo.exists())          needDownload = true;
    if(version != storedVersion)    needDownload = true;

    pDebug("Extra: Json HearthArena: Local(" + QString::number(storedVersion) + ") - "
                        "Web(" + QString::number(version) + ")" + (!needDownload?" up-to-date":""));

    if(needDownload)
    {
        if(fileInfo.exists())
        {
            QFile file(Utility::extraPath() + "/hearthArena.json");
            file.remove();
            pDebug("Extra: Json HearthArena removed.");
        }

        //Remove histograms
        removeHistograms();
        Utility::createDir(Utility::histogramsPath());

        settings.setValue("haVersion", version);
        networkManager->get(QNetworkRequest(QUrl(HA_URL + QString("/hearthArena.json"))));
        pDebug("Extra: Json HearthArena --> Download from: " + QString(HA_URL) + QString("/hearthArena.json"));
    }
}


void MainWindow::downloadCardsJsonVersion()
{
    networkManager->get(QNetworkRequest(QUrl(CARDS_URL + QString("/cardsVersion.json"))));
}


void MainWindow::testDownloadCardsJson()
{
    networkManager->get(QNetworkRequest(QUrl(HSJSON_CARDS_URL)));
    qDebug() << "DEBUG CARDS: Json Cards --> Download from:" << QString(HSJSON_CARDS_URL);
}


void MainWindow::downloadCardsJson(int version)
{
    bool needDownload = false;
    QSettings settings("Arena Tracker", "Arena Tracker");
    int storedVersion = settings.value("cardsVersion", 0).toInt();

    QFileInfo fileInfo(Utility::extraPath() + "/cards.json");
    if(!fileInfo.exists())          needDownload = true;
    if(version != storedVersion)    needDownload = true;

    pDebug("Extra: Json Cards: Local(" + QString::number(storedVersion) + ") - "
                        "Web(" + QString::number(version) + ")" + (!needDownload?" up-to-date":""));

    if(needDownload)
    {
        settings.setValue("cardsVersion", version);
        networkManager->get(QNetworkRequest(QUrl(CARDS_URL + QString("/cards.json"))));
        pDebug("Extra: Json Cards --> Download from: " + QString(CARDS_URL) + QString("/cards.json"));
    }
    else
    {
        Utility::setCardsJsonUpToDate(true);
        checkArenaCards();
        initWRCards();
    }
}


void MainWindow::removeHSCards(bool forceRemove)
{
    QSettings settings("Arena Tracker", "Arena Tracker");
    QString runVersion = settings.value("runVersion", "").toString();

    if(runVersion != VERSION || forceRemove)
    {
        QDir cardsDir = QDir(Utility::hscardsPath());
        cardsDir.removeRecursively();
        pDebug(Utility::hscardsPath() + " removed.");
    }
}


void MainWindow::removeExtraAndHistograms()
{
    QSettings settings("Arena Tracker", "Arena Tracker");
    QString runVersion = settings.value("runVersion", "").toString();

    if(runVersion != VERSION)
    {
        QDir extraDir = QDir(Utility::extraPath());
        extraDir.removeRecursively();
        pDebug(Utility::extraPath() + " removed.");

        removeHistograms();
    }
}


void MainWindow::removeHistograms()
{
    QDir dir = QDir(Utility::histogramsPath());
    dir.removeRecursively();
    pDebug(Utility::histogramsPath() + " removed.");
}


void MainWindow::checkLinuxShortcut()
{
    QSettings settings("Arena Tracker", "Arena Tracker");
    bool shortcutAsked = settings.value("shortcutAsked", false).toBool();

    if(!shortcutAsked)
    {
#ifdef APPIMAGE
        QFile appFile(Utility::dataPath() + "/ArenaTracker.Linux.AppImage");
        if(appFile.exists())
        {
            settings.setValue("shortcutAsked", true);
            createLinuxShortcut();
            showMessageAppImageShortcut();
        }
#else
        settings.setValue("shortcutAsked", true);
        askLinuxShortcut();
#endif
    }
}


void MainWindow::askLinuxShortcut()
{
    int answer = QMessageBox::question(this, tr("Create shortcut?"), tr("Do you want to create a desktop shortcut\nand a menu item for Arena Tracker?"),
                             QMessageBox::Yes, QMessageBox::No);
    if(answer == QMessageBox::Yes)
    {
        createLinuxShortcut();
    }
}


void MainWindow::showMessageAppImageShortcut()
{
    QMessageBox::information(this, tr("Use the shorcut"),
                    tr("Arena Tracker AppImage has been copied to \n(~/.local/share/Arena Tracker) and a new shortcut has been created "
                       "in your desktop linked to that AppImage."
                       "\n\nFrom now on you should run Arena Tracker from that shortcut."
                       "\nYou can also remove the AppImage you downloaded."),
                    QMessageBox::Ok);
}


void MainWindow::createLinuxShortcut()
{
#ifdef APPIMAGE
    QString appImagePath = Utility::dataPath() + "/ArenaTracker.Linux.AppImage";
#else
    QString appImagePath = Utility::appPath() + "/ArenaTracker";
#endif

    //Menu Item shortcut
    QFile shortcutFile(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/ArenaTracker.desktop");
    if(shortcutFile.exists())   shortcutFile.remove();
    if(!shortcutFile.open(QIODevice::WriteOnly))
    {
        pDebug("ERROR: Cannot create ArenaTracker.desktop", DebugLevel::Error);
        return;
    }
    shortcutFile.setPermissions(QFileDevice::ExeOwner | QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    QTextStream out(&shortcutFile);

    out << "[Desktop Entry]" << Qt::endl;
    out << "Type=Application" << Qt::endl;
    out << "Name=ArenaTracker" << Qt::endl;
    out << "Comment=Hearthstone Arena Assistant" << Qt::endl;
    out << "Exec=\"" + appImagePath + "\"" << Qt::endl;
    out << "Icon=" + Utility::extraPath() + "/icon.png" << Qt::endl;
    out << "Categories=Game;StrategyGame;" << Qt::endl;

    shortcutFile.close();

    //Desktop shortcut
    QString appShorcutFilename = QDir::homePath() + "/.local/share/applications/ArenaTracker.desktop";
    QFile appShortcut(appShorcutFilename);
    if(appShortcut.exists())    appShortcut.remove();
    shortcutFile.copy(appShorcutFilename);

    pDebug("Desktop and menu shorcut created pointing to " + appImagePath);
}


void MainWindow::completeArenaDeck()
{
    if(draftHandler == nullptr || deckHandler == nullptr)   return;

    deckHandler->completeArenaDeck(Utility::classEnum2classLogNumber(draftHandler->getArenaHero()));
}


//Config Tab
void MainWindow::addDraftMenu(QPushButton *button)
{
    QMenu *newArenaMenu = new QMenu(button);

    QSignalMapper* mapper = new QSignalMapper(button);

    for(int i=0; i<NUM_HEROS; i++)
    {
        QAction *action = newArenaMenu->addAction(Utility::classOrder2classULName(i));
        mapper->setMapping(action, Utility::classOrder2classULName(i));
        connect(action, SIGNAL(triggered()), mapper, SLOT(map()));
    }

    connect(mapper, SIGNAL(mapped(QString)), this, SLOT(confirmNewArenaDraft(QString)));

    button->setMenu(newArenaMenu);
}


void MainWindow::confirmNewArenaDraft(QString hero)
{
    int ret = QMessageBox::question(this, tr("New arena: ") + hero,
                                   "Make sure you have already picked " + hero + " in hearthstone. "
                                   "You shouldn't move hearthstone window until the end of the draft.\n\n"
                                   "Do you want to continue?",
                                   QMessageBox::Ok | QMessageBox::Cancel);

    if(ret == QMessageBox::Ok)
    {
        pDebug("Manual draft: " + hero);
        QString heroLog = Utility::className2classLogNumber(hero);
        draftHandler->beginDraft(heroLog, deckHandler->getDeckCardList(), true);
    }
}


void MainWindow::transparentAlways()
{
    spreadTransparency(Transparent);
}


void MainWindow::transparentAuto()
{
    spreadTransparency(AutoTransparent);
}


void MainWindow::transparentNever()
{
    spreadTransparency(Opaque);
}


void MainWindow::transparentFramed()
{
    spreadTransparency(Framed);
}


void MainWindow::spreadTransparency()
{
    spreadTransparency(this->transparency);
}


void MainWindow::spreadTransparency(Transparency newTransparency)
{
    this->transparency = newTransparency;

    bool kindOfTransparent = (transparency==Transparent || transparency==AutoTransparent);
    deckHandler->setTransparency(
                (this->deckWindow != nullptr && kindOfTransparent)?
                    Transparent:transparency);
    updateOtherTabsTransparency();

    showWindowFrame(transparency == Framed);

    if(arenaWindow != nullptr)
    {
        arenaWindow->showWindowFrame(transparency == Framed);
        updateDetachWindowTheme(ui->tabArena);
    }
    if(enemyWindow != nullptr)
    {
        enemyWindow->showWindowFrame(transparency == Framed);
        updateDetachWindowTheme(ui->tabEnemy);
    }
    if(deckWindow != nullptr)
    {
        deckWindow->showWindowFrame(transparency == Framed);
        updateDetachWindowTheme(ui->tabDeck);
    }
    if(enemyDeckWindow != nullptr)
    {
        enemyDeckWindow->showWindowFrame(transparency == Framed);
        updateDetachWindowTheme(ui->tabEnemyDeck);
    }
    if(graveyardWindow != nullptr)
    {
        graveyardWindow->showWindowFrame(transparency == Framed);
        updateDetachWindowTheme(ui->tabGraveyard);
    }
    if(planWindow != nullptr)
    {
        planWindow->showWindowFrame(transparency == Framed);
        updateDetachWindowTheme(ui->tabPlan);
    }
}


void MainWindow::showWindowFrame(bool showFrame)
{
    if(showFrame)
    {
        this->setWindowFlags(Qt::Window);//Incluir Qt::WindowStaysOnTopHint causa problemas en Mac (Bug no deminimizing)
    }
    else
    {
        this->setWindowFlags(Qt::Window|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
    }
#ifdef OLD_TRACKER_WINDOWS
    this->show();
#ifdef Q_OS_MAC
    MacWindow::allowMiniaturize(this);
#endif
#endif
}


//Update Config and Log tabs transparency
void MainWindow::updateOtherTabsTransparency()
{
    //New Config Step 4 - CSS nuevos controles

    if(!mouseInApp && transparency==Transparent)
    {
        ui->tabConfig->setAttribute(Qt::WA_OpaquePaintEvent);
        ui->tabConfig->repaint();

        QString groupBoxCSS =
                "QGroupBox {border: 2px solid " + ThemeHandler::themeColor2() + "; border-radius: 5px; margin-top: 5px; " +
                    ThemeHandler::bgWidgets() + " color: white;}"
                "QGroupBox::title {subcontrol-origin: margin; subcontrol-position: top center;}";
        ui->configBoxActions->setStyleSheet(groupBoxCSS);
        ui->configBoxUI->setStyleSheet(groupBoxCSS);
        ui->configBoxGames->setStyleSheet(groupBoxCSS);
        ui->configBoxDeck->setStyleSheet(groupBoxCSS);
        ui->configBoxHand->setStyleSheet(groupBoxCSS);
        ui->configBoxDraft->setStyleSheet(groupBoxCSS);
        ui->configBoxDraftMethod->setStyleSheet(groupBoxCSS);
        ui->configBoxDraftScores->setStyleSheet(groupBoxCSS);
        ui->configBoxTwitch->setStyleSheet(groupBoxCSS);

        QString labelCSS = "QLabel {background-color: transparent; color: white;}";
        ui->configLabelDeckNormal->setStyleSheet(labelCSS);
        ui->configLabelDeckNormal2->setStyleSheet(labelCSS);
        ui->configLabelDeckTooltip->setStyleSheet(labelCSS);
        ui->configLabelDeckTooltip2->setStyleSheet(labelCSS);
        ui->configLabelDrawTime->setStyleSheet(labelCSS);
        ui->configLabelDrawTimeValue->setStyleSheet(labelCSS);
        ui->configLabelPopular->setStyleSheet(labelCSS);
        ui->configLabelPopularValue->setStyleSheet(labelCSS);
        ui->configLabelTheme->setStyleSheet(labelCSS);
        ui->configLabelVotesStatus->setStyleSheet(labelCSS);

        QString radioCSS = "QRadioButton {background-color: transparent; color: white;}";
        ui->configRadioTransparent->setStyleSheet(radioCSS);
        ui->configRadioAuto->setStyleSheet(radioCSS);
        ui->configRadioOpaque->setStyleSheet(radioCSS);
        ui->configRadioFramed->setStyleSheet(radioCSS);

        QString checkCSS = "QCheckBox {background-color: transparent; color: white;}";
        ui->configCheckClassColor->setStyleSheet(checkCSS);
        ui->configCheckSpellColor->setStyleSheet(checkCSS);
        ui->configCheckScoresOverlay->setStyleSheet(checkCSS);
        ui->configCheckLearning->setStyleSheet(checkCSS);
        ui->configCheckAutoSize->setStyleSheet(checkCSS);
        ui->configCheckManaLimits->setStyleSheet(checkCSS);
        ui->configCheckTotalAttack->setStyleSheet(checkCSS);
        ui->configCheckRngList->setStyleSheet(checkCSS);
        ui->configCheckSecrets->setStyleSheet(checkCSS);
        ui->configCheckWildSecrets->setStyleSheet(checkCSS);
        ui->configCheckVotes->setStyleSheet(checkCSS);
        ui->configCheckWR->setStyleSheet(checkCSS);
        ui->configCheckLB->setStyleSheet(checkCSS);
        ui->configCheckHA->setStyleSheet(checkCSS);
        ui->configCheckLF->setStyleSheet(checkCSS);
    }
    else
    {
        ui->tabConfig->setAttribute(Qt::WA_OpaquePaintEvent, false);
        ui->tabConfig->repaint();

        ui->configBoxActions->setStyleSheet("");
        ui->configBoxUI->setStyleSheet("");
        ui->configBoxGames->setStyleSheet("");
        ui->configBoxDeck->setStyleSheet("");
        ui->configBoxHand->setStyleSheet("");
        ui->configBoxDraft->setStyleSheet("");
        ui->configBoxDraftMethod->setStyleSheet("");
        ui->configBoxDraftScores->setStyleSheet("");
        ui->configBoxTwitch->setStyleSheet("");


        ui->configLabelDeckNormal->setStyleSheet("");
        ui->configLabelDeckNormal2->setStyleSheet("");
        ui->configLabelDeckTooltip->setStyleSheet("");
        ui->configLabelDeckTooltip2->setStyleSheet("");
        ui->configLabelDrawTime->setStyleSheet("");
        ui->configLabelDrawTimeValue->setStyleSheet("");
        ui->configLabelPopular->setStyleSheet("");
        ui->configLabelPopularValue->setStyleSheet("");
        ui->configLabelTheme->setStyleSheet("");
        ui->configLabelVotesStatus->setStyleSheet("");

        ui->configRadioTransparent->setStyleSheet("");
        ui->configRadioAuto->setStyleSheet("");
        ui->configRadioOpaque->setStyleSheet("");
        ui->configRadioFramed->setStyleSheet("");

        ui->configCheckClassColor->setStyleSheet("");
        ui->configCheckSpellColor->setStyleSheet("");
        ui->configCheckScoresOverlay->setStyleSheet("");
        ui->configCheckLearning->setStyleSheet("");
        ui->configCheckAutoSize->setStyleSheet("");
        ui->configCheckManaLimits->setStyleSheet("");
        ui->configCheckTotalAttack->setStyleSheet("");
        ui->configCheckRngList->setStyleSheet("");
        ui->configCheckSecrets->setStyleSheet("");
        ui->configCheckWildSecrets->setStyleSheet("");
        ui->configCheckVotes->setStyleSheet("");
        ui->configCheckWR->setStyleSheet("");
        ui->configCheckLB->setStyleSheet("");
        ui->configCheckHA->setStyleSheet("");
        ui->configCheckLF->setStyleSheet("");
    }
}


void MainWindow::fadeBarAndButtons(bool fadeOut)
{
    if(fadeOut)
    {
        Utility::fadeOutWidget(ui->tabWidget->tabBar());
        Utility::fadeOutWidget(ui->minimizeButton);
        Utility::fadeOutWidget(ui->closeButton);
        Utility::fadeOutWidget(ui->resizeButton);
    }
    else
    {
        Utility::fadeInWidget(ui->tabWidget->tabBar());
        Utility::fadeInWidget(ui->minimizeButton);
        Utility::fadeInWidget(ui->closeButton);
        Utility::fadeInWidget(ui->resizeButton);
    }

    updateTabWidgetsTheme(fadeOut, false);//Si usamos un theme con bordes los oculta
}


void MainWindow::spreadTheme()
{
    updateMainUITheme();
    updateTabIcons();
    deckHandler->setTheme();
    draftHandler->setTheme();
    deckHandler->redrawAllCards();
    draftHandler->redrawAllCards();
    resizeChecks();//Recoloca botones -X
    calculateMinimumWidth();//Si hay borde cambia el minimumWidth
}


void MainWindow::updateTabIcons()
{
    QWidget * currentTab = ui->tabWidget->currentWidget();

    bool drafting = false;
    if(ui->tabWidget->indexOf(ui->tabDraft) != -1)  drafting = true;
    QWidget *redraftTab = (draftHandler == nullptr)?nullptr:draftHandler->getRedraftTab();
    bool redrafting = (redraftTab != nullptr && ui->tabWidget->indexOf(redraftTab) != -1);

    ui->tabWidget->hide();
    ui->tabWidget->clear();
    if(drafting)                                        moveTabTo(ui->tabDraft, ui->tabWidget);
    if(redrafting)                                      moveTabTo(redraftTab, ui->tabWidget);
    if(arenaWindow == nullptr)                          moveTabTo(ui->tabArena, ui->tabWidget);
    if(enemyWindow == nullptr)                          moveTabTo(ui->tabEnemy, ui->tabWidget);
    if(deckWindow == nullptr)                           moveTabTo(ui->tabDeck, ui->tabWidget);
    if(enemyDeckWindow == nullptr)                      moveTabTo(ui->tabEnemyDeck, ui->tabWidget);
    moveTabTo(ui->tabConfig, ui->tabWidget);
    ui->tabWidget->show();

    ui->tabWidget->setCurrentWidget(currentTab);
}


void MainWindow::updateTabWidgetsTheme(bool transparent, bool resizing)
{
    int maxWidth = ui->tabWidget->width() - ThemeHandler::borderWidth()*2 - SMALL_BUTTONS_H;
    ui->tabWidget->setTheme("left", maxWidth, resizing, transparent);
}


void MainWindow::updateMainUITheme()
{
    QFont font(ThemeHandler::defaultFont());
    font.setPixelSize(15);
    QApplication::setFont(font);
    fitProgressBarText();

    updateTabWidgetsTheme(false, false);
    updateButtonsTheme();

    QString mainCSS = "";
    mainCSS +=
            "QMenu {background: " + ThemeHandler::bgMenuColor() + "; color: " + ThemeHandler::fgMenuColor() + ";}"
            "QMenu::item {padding: 2px 25px 2px 20px;border: 1px solid transparent;}"
            "QMenu::item:selected {background-color: " + ThemeHandler::bgSelectedItemMenuColor() + "; "
                "color: " + ThemeHandler::fgSelectedItemMenuColor() + "; "
                "border-color: " + ThemeHandler::bgSelectedItemMenuColor() + ";}"

            "QScrollBar:vertical {background-color: transparent; border: 2px solid " + ThemeHandler::themeColor2() + "; "
                "width: 15px; margin: 15px 0px 15px 0px;}"
            "QScrollBar::handle:vertical {background: " + ThemeHandler::themeColor1() + "; min-height: 20px;}"
            "QScrollBar::add-line:vertical {border: 2px solid " + ThemeHandler::themeColor2() + ";background: " + ThemeHandler::themeColor1() + "; "
                "height: 15px; subcontrol-position: bottom; subcontrol-origin: margin;}"
            "QScrollBar::sub-line:vertical {border: 2px solid " + ThemeHandler::themeColor2() + ";background: " + ThemeHandler::themeColor1() + "; "
                "height: 15px; subcontrol-position: top; subcontrol-origin: margin;}"
            "QScrollBar:up-arrow:vertical, QScrollBar::down-arrow:vertical {border: 2px solid " + ThemeHandler::themeColor1() + "; "
                "width: 3px; height: 3px; background: " + ThemeHandler::themeColor2() + ";}"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {background: none;}"

            "QScrollBar:horizontal {background-color: transparent; border: 2px solid " + ThemeHandler::themeColor2() + "; "
                "height: 15px; margin: 0px 15px 0px 15px;}"
            "QScrollBar::handle:horizontal {background: " + ThemeHandler::themeColor1() + "; min-width: 20px;}"
            "QScrollBar::add-line:horizontal {border: 2px solid " + ThemeHandler::themeColor2() + ";background: " + ThemeHandler::themeColor1() + "; "
                "width: 15px; subcontrol-position: right; subcontrol-origin: margin;}"
            "QScrollBar::sub-line:horizontal {border: 2px solid " + ThemeHandler::themeColor2() + ";background: " + ThemeHandler::themeColor1() + "; "
                "width: 15px; subcontrol-position: left; subcontrol-origin: margin;}"
            "QScrollBar:left-arrow:horizontal, QScrollBar::right-arrow:horizontal {border: 2px solid " + ThemeHandler::themeColor1() + "; "
                "width: 3px; height: 3px; background: " + ThemeHandler::themeColor2() + ";}"
            "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {background: none;}"

            "QAbstractScrollArea::corner {background: transparent;}"

            "QProgressBar {border: 2px solid " + ThemeHandler::borderProgressBarColor() + "; color: " + ThemeHandler::fgProgressBarColor() + "; "
                "background-color: " + ThemeHandler::bgProgressBarColor() + ";}"
            "QProgressBar::chunk {background-color: " + ThemeHandler::chunkProgressBarColor() + ";}"

            "QDialog {" + ThemeHandler::bgApp() + ";}"
            "QPushButton {background: " + ThemeHandler::themeColor1() + "; color: " + ThemeHandler::fgColor() + ";}"
            "QToolTip {border: 2px solid " + ThemeHandler::borderTooltipColor() + "; border-radius: 2px; "
                "color: " + ThemeHandler::fgTooltipColor() + "; background: " + ThemeHandler::bgTooltipColor() + ";}"

            "QGroupBox {border: 2px solid " + ThemeHandler::themeColor2() + "; border-radius: 5px; "
                "margin-top: 5px; " + ThemeHandler::bgWidgets() + " color: " + ThemeHandler::fgColor() + ";}"
            "QGroupBox::title {subcontrol-origin: margin; subcontrol-position: top center;}"
            "QLabel {background-color: transparent; color: " + ThemeHandler::fgColor() + ";}"
            "QTextBrowser {" + ThemeHandler::bgWidgets() + " color: " + ThemeHandler::fgColor() + ";}"
            "QRadioButton {background-color: transparent; color: " + ThemeHandler::fgColor() + ";}"
            "QCheckBox {background-color: transparent; color: " + ThemeHandler::fgColor() + ";}"
            "QTextEdit{" + ThemeHandler::bgWidgets() + " color: " + ThemeHandler::fgColor() + ";}"

            "QLineEdit {border: 2px solid " + ThemeHandler::borderLineEditColor() + ";border-radius: 5px; "
                "background: " + ThemeHandler::bgLineEditColor() + "; color: " + ThemeHandler::fgLineEditColor() + "; "
                "selection-background-color: " + ThemeHandler::bgSelectionLineEditColor() + "; "
                "selection-color: " + ThemeHandler::fgSelectionLineEditColor() + ";}"

            "QComboBox {background: " + ThemeHandler::bgMenuColor() + "; color: " + ThemeHandler::fgMenuColor() + "; "
                "selection-background-color: " + ThemeHandler::bgSelectedItemMenuColor() + ";"
                "selection-color: "+ ThemeHandler::fgSelectedItemMenuColor() +";}"
            "QComboBox QAbstractItemView{background: " + ThemeHandler::bgMenuColor() + "; "
                "color: " + ThemeHandler::fgMenuColor() + "; "
                "selection-background-color: " + ThemeHandler::bgSelectedItemMenuColor() + "; "
                "selection-color: "+ ThemeHandler::fgSelectedItemMenuColor() +";}"
            ;

    this->setStyleSheet(mainCSS);
    updateAllDetachWindowTheme(mainCSS);
}


void MainWindow::updateAllDetachWindowTheme(const QString &mainCSS)
{
    if(arenaWindow != nullptr)
    {
        arenaWindow->setStyleSheet(mainCSS);
        arenaWindow->spreadTheme();
    }
    if(enemyWindow != nullptr)
    {
        enemyWindow->setStyleSheet(mainCSS);
        enemyWindow->spreadTheme();
    }
    if(deckWindow != nullptr)
    {
        deckWindow->setStyleSheet(mainCSS);
        deckWindow->spreadTheme();
    }
    if(enemyDeckWindow != nullptr)
    {
        enemyDeckWindow->setStyleSheet(mainCSS);
        enemyDeckWindow->spreadTheme();
    }
    if(graveyardWindow != nullptr)
    {
        graveyardWindow->setStyleSheet(mainCSS);
        graveyardWindow->spreadTheme();
    }
    if(planWindow != nullptr)
    {
        planWindow->setStyleSheet(mainCSS);
        planWindow->spreadTheme();
    }

    updateDetachWindowTheme(ui->tabArena);
    updateDetachWindowTheme(ui->tabEnemy);
    updateDetachWindowTheme(ui->tabDeck);
    updateDetachWindowTheme(ui->tabEnemyDeck);
    updateDetachWindowTheme(ui->tabGraveyard);
    updateDetachWindowTheme(ui->tabPlan);
}


void MainWindow::updateDetachWindowTheme(QWidget *paneWidget)
{
    DetachWindow *detachWindow;
    QString paneWidgetName;
    bool showThemeBackground = false;
    bool paneBorder;

    if(paneWidget == ui->tabArena)
    {
            detachWindow = arenaWindow;
            paneWidgetName = "TabArena";
            paneBorder = false;
            showThemeBackground = (detachWindow != nullptr &&
                    (transparency == Framed || transparency == Opaque || transparency == AutoTransparent));
    }
    else if(paneWidget == ui->tabEnemy)
    {
            detachWindow = enemyWindow;
            paneWidgetName = "TabEnemy";
            paneBorder = false;
            showThemeBackground = (detachWindow != nullptr &&
                    (transparency == Framed || transparency == Opaque));
    }
    else if(paneWidget == ui->tabDeck)
    {
            detachWindow = deckWindow;
            paneWidgetName = "TabDeck";
            paneBorder = false;
            showThemeBackground = (detachWindow != nullptr &&
                    (transparency == Framed || transparency == Opaque));
    }
    else if(paneWidget == ui->tabEnemyDeck)
    {
            detachWindow = enemyDeckWindow;
            paneWidgetName = "TabEnemyDeck";
            paneBorder = false;
            showThemeBackground = (detachWindow != nullptr &&
                    (transparency == Framed || transparency == Opaque));
    }
    else if(paneWidget == ui->tabGraveyard)
    {
            detachWindow = graveyardWindow;
            paneWidgetName = "TabGraveyard";
            paneBorder = false;
            showThemeBackground = (detachWindow != nullptr &&
                    (transparency == Framed || transparency == Opaque));
    }
    else /*if(paneWidget == ui->tabPlan)*/
    {
            detachWindow = planWindow;
            paneWidgetName = "TabPlan";
            paneBorder = true;
            showThemeBackground = (detachWindow != nullptr &&
                    (transparency == Framed || transparency == Opaque || transparency == AutoTransparent));
    }

    if(showThemeBackground)
    {
        paneWidget->setStyleSheet("QWidget#" + paneWidgetName + " { " +
            ThemeHandler::bgApp() + ThemeHandler::borderApp(false) +" }");
        paneWidget->layout()->setContentsMargins(ThemeHandler::borderWidth() + (paneBorder?10:0),
                                                 ThemeHandler::borderWidth() + (paneBorder?10:0),
                                                 ThemeHandler::borderWidth() + (paneBorder?10:0),
                                                 ThemeHandler::borderWidth() + (paneBorder?10:0));
        calculateCardWindowMinimumWidth(detachWindow, true);
    }
    else
    {
        paneWidget->setStyleSheet("");
        if(detachWindow == nullptr)
        {
            paneWidget->layout()->setContentsMargins((paneBorder?10:0), 40 + (paneBorder?5:0),
                                                     (paneBorder?10:0), (paneBorder?10:0));
        }
        else
        {
            paneWidget->layout()->setContentsMargins((paneBorder?10:0), (paneBorder?10:0),
                                                     (paneBorder?10:0), (paneBorder?10:0));
            calculateCardWindowMinimumWidth(detachWindow, false);
        }
    }
}


void MainWindow::updateButtonsTheme()
{
    ui->closeButton->setStyleSheet("QPushButton {background: " + ThemeHandler::bgTopButtonsColor() + "; border: none;}"
                                   "QPushButton:hover {background: " + ThemeHandler::hoverTopButtonsColor() + ";}");
    ui->minimizeButton->setStyleSheet("QPushButton {background: " + ThemeHandler::bgTopButtonsColor() + "; border: none;}"
                                      "QPushButton:hover {background: " + ThemeHandler::hoverTopButtonsColor() + ";}");

    ui->closeButton->setIcon(QIcon(ThemeHandler::buttonCloseFile()));
    ui->minimizeButton->setIcon(QIcon(ThemeHandler::buttonMinimizeFile()));
    ui->configButtonForceDraft->setIcon(QIcon(ThemeHandler::buttonForceDraftFile()));

    QList<QAction *> actions = ui->configButtonForceDraft->menu()->actions();
    for(int i=0; i<actions.count(); i++)
    {
        actions[i]->setIcon(HDIcons::hero(i));
    }

    ui->guideButton->setIcon(QIcon(ThemeHandler::buttonGamesGuideFile()));
    ui->resizeButton->setIcon(QIcon(ThemeHandler::buttonResizeFile()));

}


int MainWindow::getAutoTamCard()
{
    int numCards = deckHandler->getNumCardRows();
    int deckHeight = ui->tabDeck->height();
    if(this->deckWindow == nullptr)   deckHeight -= 40;

    if(numCards > 0)    return deckHeight/numCards;
    else                return -1;
}


int MainWindow::getTamCard()
{
    bool autoSize = ui->configCheckAutoSize->isChecked();

    int tamCardSlider = ui->configSliderCardSize->value();

    if(autoSize)
    {
        int tamCardAuto = getAutoTamCard();
        if(tamCardAuto == -1)   return tamCardSlider;
        else                    return std::min(tamCardAuto, tamCardSlider);
    }
    else
    {
        return tamCardSlider;
    }
}


void MainWindow::spreadTamCard(int value)
{
    if(value < ui->configSliderCardSize->minimum()) value = ui->configSliderCardSize->minimum();
    if(this->cardHeight == value) return;

    this->cardHeight = value;
    DeckCard::setCardHeight(value);

    if(deckHandler != nullptr)
    {
        deckHandler->updateIconSize(value);
        deckHandler->redrawAllCards();
    }

    if(draftHandler != nullptr)     draftHandler->redrawAllCards();

    bool windowsWithBorders = (transparency == Framed || transparency == Opaque);
    if(deckWindow != nullptr)      calculateCardWindowMinimumWidth(deckWindow, windowsWithBorders);
    if(enemyWindow != nullptr)     calculateCardWindowMinimumWidth(enemyWindow, windowsWithBorders);
    if(enemyDeckWindow != nullptr) calculateCardWindowMinimumWidth(enemyDeckWindow, windowsWithBorders);
    if(graveyardWindow != nullptr) calculateCardWindowMinimumWidth(graveyardWindow, windowsWithBorders);
}


void MainWindow::spreadCorrectTamCard()
{
    spreadTamCard(getTamCard());
}


void MainWindow::updateTamCard(int value)
{
    spreadCorrectTamCard();

    QString labelText = QString::number(value) + " px";
    ui->configSliderCardSize->setToolTip(labelText);
    ui->configLabelDeckNormal2->setText(labelText);
}


void MainWindow::updateTooltipScale(int value)
{
    cardWindow->scale(value);

    QString labelText;
    if(value < 10)  labelText = "OFF";
    else            labelText = "x"+QString::number(value/10.0);
    ui->configSliderTooltipSize->setToolTip(labelText);
    ui->configLabelDeckTooltip2->setText(labelText);
}


void MainWindow::updateShowClassColor(bool checked)
{
    DeckCard::setDrawClassColor(checked);
    deckHandler->redrawClassCards();
    draftHandler->redrawAllCards();
}


void MainWindow::updateShowSpellColor(bool checked)
{
    DeckCard::setDrawSpellWeaponColor(checked);
    deckHandler->redrawSpellWeaponCards();
    draftHandler->redrawAllCards();
}


void MainWindow::updateShowManaLimits(bool checked)
{
    deckHandler->setShowManaLimits(checked);
}


void MainWindow::updateShowDraftScoresOverlay(bool checked)
{
    draftHandler->setShowDraftScoresOverlay(checked);
}


void MainWindow::updateShowMyWR(bool checked)
{
    draftHandler->setShowMyWR(checked);
}


void MainWindow::spreadDraftMethod()
{
    spreadDraftMethod(ui->configCheckHA->isChecked(), ui->configCheckLF->isChecked());
}


void MainWindow::spreadDraftMethod(bool draftMethodHA, bool draftMethodLF)
{
    draftHandler->setDraftMethod(draftMethodHA, draftMethodLF);
}


void MainWindow::openUserGuide()
{
    QDesktopServices::openUrl(QUrl(USER_GUIDE_URL));
}


void MainWindow::completeConfigTab()
{
    //New Config Step 6 - Ocultar opciones premium - Ajustar tamanos
    //New Config Step 7 - Connect controles - funciones y crear funciones

    //Cambiar en Designer margenes/spacing de nuevos configBox a 5-9-5-9/5
    //Actions
    addDraftMenu(ui->configButtonForceDraft);
    connect(ui->guideButton, SIGNAL(clicked()),
            this, SLOT(openUserGuide()));
    //connect en createDeckHandler

    //UI
    connect(ui->configRadioTransparent, SIGNAL(clicked()), this, SLOT(transparentAlways()));
    connect(ui->configRadioAuto, SIGNAL(clicked()), this, SLOT(transparentAuto()));
    connect(ui->configRadioOpaque, SIGNAL(clicked()), this, SLOT(transparentNever()));
    connect(ui->configRadioFramed, SIGNAL(clicked()), this, SLOT(transparentFramed()));

    //Games
    ui->configBoxGames->hide();
    ui->configCheckLB->hide();

    //Deck
    ui->configCheckAutoSize->hide();//Disable autoSize
    connect(ui->configSliderCardSize, SIGNAL(valueChanged(int)), this, SLOT(updateTamCard(int)));
    connect(ui->configSliderTooltipSize, SIGNAL(valueChanged(int)), this, SLOT(updateTooltipScale(int)));
    connect(ui->configCheckAutoSize, SIGNAL(clicked()), this, SLOT(spreadCorrectTamCard()));
    connect(ui->configCheckClassColor, SIGNAL(clicked(bool)), this, SLOT(updateShowClassColor(bool)));
    connect(ui->configCheckSpellColor, SIGNAL(clicked(bool)), this, SLOT(updateShowSpellColor(bool)));
    connect(ui->configCheckManaLimits, SIGNAL(clicked(bool)), this, SLOT(updateShowManaLimits(bool)));

    //Hand
    ui->configLabelPopular->hide();
    ui->configLabelPopularValue->hide();
    ui->configSliderPopular->hide();
    ui->configCheckRngList->hide();
    ui->configCheckWildSecrets->hide();

    //Draft
    ui->configCheckWR->hide();
    connect(ui->configCheckScoresOverlay, SIGNAL(clicked(bool)), this, SLOT(updateShowDraftScoresOverlay(bool)));
    connect(ui->configCheckMechanicsOverlay, SIGNAL(clicked(bool)), this, SLOT(updateShowDraftMechanicsOverlay(bool)));
    connect(ui->configCheckShowDrops, SIGNAL(clicked(bool)), this, SLOT(updateDraftShowDrops(bool)));
    connect(ui->configCheckWR, SIGNAL(clicked(bool)), this, SLOT(updateShowMyWR(bool)));
    connect(ui->configCheckHA, SIGNAL(clicked(bool)), this, SLOT(spreadDraftMethod()));
    connect(ui->configCheckLF, SIGNAL(clicked(bool)), this, SLOT(spreadDraftMethod()));
    connect(ui->iconDrop2, SIGNAL(clicked(bool)), this, SLOT(updateDrop2(bool)));
    connect(ui->iconDrop3, SIGNAL(clicked(bool)), this, SLOT(updateDrop3(bool)));
    connect(ui->iconDrop4, SIGNAL(clicked(bool)), this, SLOT(updateDrop4(bool)));
    connect(ui->iconDraw, SIGNAL(clicked(bool)), this, SLOT(updateDraw(bool)));
    connect(ui->iconPing, SIGNAL(clicked(bool)), this, SLOT(updatePing(bool)));
    connect(ui->iconDamage, SIGNAL(clicked(bool)), this, SLOT(updateDamage(bool)));
    connect(ui->iconDestroy, SIGNAL(clicked(bool)), this, SLOT(updateDestroy(bool)));
    connect(ui->iconAoe, SIGNAL(clicked(bool)), this, SLOT(updateAoe(bool)));
    connect(ui->iconReach, SIGNAL(clicked(bool)), this, SLOT(updateReach(bool)));
    connect(ui->iconTaunt, SIGNAL(clicked(bool)), this, SLOT(updateTaunt(bool)));
    connect(ui->iconSurvival, SIGNAL(clicked(bool)), this, SLOT(updateSurvival(bool)));

    //Twitch
    ui->configLabelVotesStatus->setPixmap(ThemeHandler::loseFile());
    ui->configCheckVotes->setEnabled(false);
    ui->configLabelVotesStatus->setEnabled(false);

    completeHighResConfigTab();
}


void MainWindow::completeHighResConfigTab()
{
    int screenHeight = getScreenHighest();
    if(screenHeight < 1000) return;

    int maxCard = static_cast<int>(screenHeight/1000.0*50);
    maxCard -= maxCard%5;
    ui->configSliderCardSize->setMaximum(maxCard);

    int maxTooltip = static_cast<int>(screenHeight/1000.0*15);
    maxTooltip -= maxTooltip%5;
    ui->configSliderTooltipSize->setMaximum(maxTooltip);
}


int MainWindow::getScreenHighest()
{
    int height = 0;

    for(QScreen *screen: (const QList<QScreen *>)QGuiApplication::screens())
    {
        if (!screen)    continue;
        QRect geometry = screen->geometry();
        if(geometry.height()>height)    height = geometry.height();
    }
    return height;
}


LoadingScreenState MainWindow::getLoadingScreen()
{
    if(gameWatcher != nullptr) return gameWatcher->getLoadingScreen();
    else                    return menu;
}


void MainWindow::createDebugPack()
{
    QString timeStamp = QDateTime::currentDateTime().toString("MMMM-d hh-mm-ss");
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/ATbugs/" + timeStamp;
    QDir dir(dirPath);
    dir.mkpath(dirPath);

    QList<QScreen *> screens = QGuiApplication::screens();
    for(int screenIndex=0; screenIndex<screens.count(); screenIndex++)
    {
        QScreen *screen = screens[screenIndex];
        QImage image = Utility::getScreenshot(screen);
        if(image.isNull())  continue;

        image.save(dirPath + "/screenshot" + QString::number(screenIndex) + ".png");

#ifdef Q_OS_LINUX
        if(CaptureManager::isWaylandSession())
        {
            break;
        }
#endif

        // cv::Mat mat(image.height(),image.width(),CV_8UC4,image.bits(), static_cast<size_t>(image.bytesPerLine()));
        // cv::resize(mat, mat, cv::Size(1280, 720));
        // cv::imshow("Screenshot", mat);
    }

    QFile atLog(Utility::dataPath() + "/ArenaTrackerLog.txt");
    atLog.copy(dirPath + "/ArenaTrackerLog.txt");

    QString hsLogsPath = logLoader->getLogsDirPath() + "/" + logLoader->getRecentLogDir();
    QFile arenaLog(hsLogsPath + "/Arena.log");
    arenaLog.copy(dirPath + "/Arena.log");
    QFile loadingScreenLog(hsLogsPath + "/LoadingScreen.log");
    loadingScreenLog.copy(dirPath + "/LoadingScreen.log");
    QFile powerLog(hsLogsPath + "/Power.log");
    powerLog.copy(dirPath + "/Power.log");
    QFile zoneLog(hsLogsPath + "/Zone.log");
    zoneLog.copy(dirPath + "/Zone.log");

    pDebug("Bug pack " + dirPath + " created.");
}


void MainWindow::showMessageProgressBar(QString text, int hideDelay)
{
    setProgressBarText(text);

    if(ui->progressBar->value() != ui->progressBar->maximum())
    {
        pDebug("Progress bar message received while counting. " + text, Warning);
        if(!ui->progressBar->isVisible())   showProgressBar(false);
    }
    else
    {
        if(!ui->progressBar->isVisible())   showProgressBar(true);
        QTimer::singleShot(hideDelay, this, SLOT(hideProgressBar()));
    }
}


void MainWindow::startProgressBar(int maximum, QString text)
{
    ui->progressBar->setMaximum(maximum);
    ui->progressBar->setMinimum(0);
    ui->progressBar->setValue(0);
    setProgressBarText(text);

    if(!ui->progressBar->isVisible())   showProgressBar(false);
}


void MainWindow::advanceProgressBar(int remaining, QString text)
{
    if(remaining <= 0)
    {
        ui->progressBar->setValue(ui->progressBar->maximum());
    }
    else
    {
        if(remaining > ui->progressBar->maximum())  ui->progressBar->setMaximum(remaining);
        ui->progressBar->setValue(ui->progressBar->maximum()-remaining);
    }
    if(!text.isEmpty())     setProgressBarText(text);
}


void MainWindow::setProgressBarText(const QString &text)
{
    progressBarText = text;
    ui->progressBar->setToolTip(text);
    fitProgressBarText();
}


//Shrinks the font until the text fits the bar, and elides it if it still doesn't at the smallest size
void MainWindow::fitProgressBarText()
{
    //Room for the borders and, on both sides to keep it centered, the resize corner
    int available = ui->progressBar->width() - 2*28;
    if(available < 20)  return;

    QFont font(ThemeHandler::defaultFont());
    QString text = progressBarText;
    int pixelSize = 16;
    for(; pixelSize>10; pixelSize--)
    {
        font.setPixelSize(pixelSize);
        if(QFontMetrics(font).horizontalAdvance(text) <= available)     break;
    }
    font.setPixelSize(pixelSize);
    text = QFontMetrics(font).elidedText(text, Qt::ElideRight, available);

    ui->progressBar->setFont(font);
    ui->progressBar->setFormat(text);
}


void MainWindow::showProgressBar(bool animated)
{
    if(ui->progressBar->isVisible())
    {
        pDebug("Trying to show progress bar already shown.", Warning);
        return;
    }

    ui->progressBar->setVisible(true);
    fitProgressBarText();

    if(animated)
    {
        QPropertyAnimation *animation = new QPropertyAnimation(ui->progressBar, "maximumHeight");
        animation->setDuration(ANIMATION_TIME);
        animation->setStartValue(0);
        animation->setEndValue(ui->progressBar->height());
        animation->setEasingCurve(SHOW_EASING_CURVE);
        animation->start(QPropertyAnimation::DeleteWhenStopped);
    }
    else
    {
        ui->progressBar->setMaximumHeight(16777215);
    }
}


void MainWindow::hideProgressBar()
{
    if(ui->progressBar->value() != ui->progressBar->maximum())
    {
        pDebug("Trying to hide progress bar while counting.", Warning);
        return;
    }

    if(!ui->progressBar->isVisible())
    {
        pDebug("Trying to hide progress bar already hidden.", Warning);
        return;
    }

    QPropertyAnimation *animation = new QPropertyAnimation(ui->progressBar, "maximumHeight");
    animation->setDuration(ANIMATION_TIME);
    animation->setStartValue(ui->progressBar->height());
    animation->setEndValue(0);
    animation->setEasingCurve(HIDE_EASING_CURVE);
    animation->start(QPropertyAnimation::DeleteWhenStopped);

    connect(
        animation, &QPropertyAnimation::finished, ui->progressBar,
        [this]()
        {
            if(ui->progressBar->value() != ui->progressBar->maximum())
            {
                pDebug("Finish hiding progress bar while counting.", Warning);
                ui->progressBar->setVisible(true);
            }
            else
            {
                ui->progressBar->setVisible(false);
            }
            ui->progressBar->setMaximumHeight(16777215);
        }
    );
}


void MainWindow::startProgressBarMini(int maximum)
{
    ui->progressBarMini->setMaximum(maximum);
    ui->progressBarMini->setMinimum(0);
    ui->progressBarMini->setValue(0);
    ui->progressBarMini->setVisible(true);
}


void MainWindow::hideProgressBarMini()
{
    advanceProgressBarMini(0);
}


void MainWindow::advanceProgressBarMini(int remaining)
{
    if(remaining <= 0)
    {
        ui->progressBarMini->setVisible(false);
        ui->progressBarMini->setValue(ui->progressBarMini->maximum());
    }
    else
    {
        if(remaining > ui->progressBarMini->maximum())  ui->progressBarMini->setMaximum(remaining);
        ui->progressBarMini->setValue(ui->progressBarMini->maximum()-remaining);
    }
}


void MainWindow::checkFirstRunNewVersion()
{
    QSettings settings("Arena Tracker", "Arena Tracker");
    QString runVersion = settings.value("runVersion", "").toString();

    if(runVersion != VERSION)
    {
        pDebug("First run of new version.");
        settings.setValue("neoInt", 0);
    }
}


//Solo baja las cartas si arenaVersion ha cambiado de version o HSCards se ha borrado
void MainWindow::checkArenaCards()
{
    if(draftHandler == nullptr || !cardsJsonLoaded || !arenaSetsLoaded ||
        !Utility::isCardsJsonUpToDate())    return;

    QSettings settings("Arena Tracker", "Arena Tracker");

    if(allCardsDownloadNeeded)
    {
        settings.setValue("allCardsDownloaded", false);
        QStringList codeList = Utility::getAllArenaCodes();
        downloadAllArenaCodes(codeList);
    }
    else
    {
        pDebug("CheckArenaCards: No arena cards downloads.");
        QTimer::singleShot(0, this, &MainWindow::startupReady);     //After main() connects the splash
    }
}


void MainWindow::downloadAllArenaCodes(const QStringList &codeList)
{
    pDebug("CheckArenaCards: Downloading all arena cards.");
    allCardsDownloadList.clear();

    for(const QString &code: codeList)
    {
        if(!checkCardImage(code))
        {
            allCardsDownloadList.append(code);
        }
        //FALSO: Solo bajamos golden cards de cartas colleccionables
        if(/*Utility::getCardAttribute(code, "collectible").toBool() &&*/
            !checkCardImage(code + "_premium"))
        {
            allCardsDownloadList.append(code + "_premium");
        }
    }

    const QStringList heroList = draftHandler->getAllHeroCodes();
    for(const QString &code: heroList)
    {
        if(!checkCardImage(code, true))
        {
            allCardsDownloadList.append(code);
        }
    }

    if(allCardsDownloadList.isEmpty())  this->allCardsDownloaded();
    else
    {
        allCardsDownloadTotal = allCardsDownloadList.count();
        QTimer::singleShot(0, this, [this]() { emit startupProgress(0, allCardsDownloadTotal); });
        startProgressBarMini(allCardsDownloadList.count());
        showMessageProgressBar("Downloading cards...", 10000);
    }
}


void MainWindow::updateProgressAllCardsDownload(QString code)
{
    if(allCardsDownloadList.removeOne(code))
    {
        emit startupProgress(allCardsDownloadTotal - allCardsDownloadList.count(), allCardsDownloadTotal);
        advanceProgressBarMini(allCardsDownloadList.count());
    }
}


void MainWindow::allCardsDownloaded()
{
    QTimer::singleShot(0, this, &MainWindow::startupReady);     //After main() connects the splash
    QSettings settings("Arena Tracker", "Arena Tracker");

    if(allCardsDownloadNeeded)
    {
        settings.setValue("allCardsDownloaded", true);
        allCardsDownloadNeeded = false;
        allCardsDownloadList.clear();
        hideProgressBarMini();
        showMessageProgressBar("All cards downloaded");
        pDebug("CheckArenaCards: All arena cards have been downloaded.");
    }
}


void MainWindow::downloadHearthArenaTierlistOriginal()
{
    networkManager->get(QNetworkRequest(QUrl(HEARTHARENA_TIERLIST_URL)));
    qDebug()<<"DEBUG TL: Heartharena Tierlist --> Download from:" << QString(HEARTHARENA_TIERLIST_URL);
}


void MainWindow::saveHearthArenaTierlistOriginal(const QByteArray &html)
{
    if(!html.isEmpty())
    {
        QString haDir = QDir::homePath() + "/Documentos/ArenaTracker/HearthArena/Json extract/";
        Utility::dumpOnFile(html, haDir + "haTL.html");
    }

    //Lanza script HATLsed.sh
    QProcess p;
    p.start("\"/home/triodo/Documentos/ArenaTracker/HearthArena/Json extract/HATLsedNames.sh\"");
    // p.start("\"/home/triodo/Documentos/ArenaTracker/HearthArena/Json extract/HATLsedCodes.sh\"");
    p.waitForFinished(-1);

    //Copy to local and source
    QString fixedLF1 = QDir::homePath() + "/Documentos/ArenaTracker/HearthArena/Json extract/hearthArena.json";
    QString fixedLF2 = QDir::homePath() + "/Documentos/ArenaTracker/HearthArena/hearthArena.json";
    QString fixedLF3 = Utility::extraPath() + "/hearthArena.json";
    QFile::remove(fixedLF2);
    QFile::remove(fixedLF3);
    QFile::copy(fixedLF1, fixedLF2);
    QFile::copy(fixedLF1, fixedLF3);
    QFile::remove(fixedLF1);

    qDebug()<<"DEBUG TL: heartharena.json created (source and local)";
}


void MainWindow::testArenaGames()
{
    GameResult gameResult;
    LoadingScreenState gameMode = arena;
    gameResult.isFirst = gameResult.isWinner = true;
    gameResult.playerHero = gameResult.enemyHero = "08";
    arenaHandler->newArena(gameResult.playerHero);
    arenaHandler->newGameResult(gameResult, gameMode);
    gameResult.isFirst = gameResult.isWinner = false;
    arenaHandler->newGameResult(gameResult, gameMode);
    arenaHandler->newGameResult(gameResult, gameMode);
    arenaHandler->newGameResult(gameResult, gameMode);

    gameResult.playerHero = gameResult.enemyHero = "05";
    gameMode = arena;
    arenaHandler->newArena(gameResult.playerHero);
    arenaHandler->newGameResult(gameResult, gameMode);
    arenaHandler->newGameResult(gameResult, gameMode);
    arenaHandler->newGameResult(gameResult, gameMode);
    gameResult.isFirst = gameResult.isWinner = true;
    arenaHandler->newGameResult(gameResult, gameMode);
}


void MainWindow::test()
{
    QTimer::singleShot(1000, this, SLOT(testDelay()));
}


void MainWindow::testHeroPortraits()
{
    bool everythingOk = true;
    for(const QString &code: (const QStringList)draftHandler->getAllHeroCodes())
    {
        if(!Utility::checkHeroPortrait(code))
        {
            qDebug() << "DEBUG HEROES: Built" << code << "-" << Utility::cardEnNameFromCode(code);
            downloadHeroPortrait(code);
            everythingOk = false;
        }
    }

    for(const QString &code: (const QStringList)Utility::getWildCodes())
    {
        if(Utility::getTypeFromCode(code) == HERO)
        {
            if(code.startsWith("HERO_"))
            {
                if(!Utility::checkHeroPortrait(code))
                {
                    qDebug() << "DEBUG HEROES: Should be built" << code << "-" << Utility::cardEnNameFromCode(code);
                    everythingOk = false;
                }
            }
            else
            {
                if(!Utility::checkHeroPortrait("HERO_" + code))
                {
                    qDebug() << "DEBUG HEROES: Manual build" << "HERO_" + code << "-" << Utility::cardEnNameFromCode(code);
                    if(code.startsWith("CORE_"))
                    {
                        QString noCoreCode = code.mid(5);
                        if(Utility::checkHeroPortrait("HERO_" + noCoreCode))
                        {
                            qDebug() << "DEBUG HEROES: Duplicate code" << code << "of" << noCoreCode << "-" << Utility::cardEnNameFromCode(code);
                        }
                    }
                    everythingOk = false;
                }
            }
        }
    }

    if(everythingOk)    qDebug()<<"DEBUG HEROES: OK - All portraits in place.";
}


void MainWindow::downloadHeroPortrait(QString code)
{
    QNetworkAccessManager *nm = new QNetworkAccessManager(this);
    QString urlString = "https://cards.hearthpwn.com/enUS/" + code + ".png";
    nm->get(QNetworkRequest(QUrl(urlString)));

    connect(nm, &QNetworkAccessManager::finished,
        [=](QNetworkReply *reply)
        {
            reply->deleteLater();
            nm->deleteLater();
            QByteArray data = reply->readAll();
            QString heroDir = QDir::homePath() + "/Documentos/ArenaTracker/HearthstoneHeroPortraits";

            QImage heroImage;
            heroImage.loadFromData(data);
            //Normal
            heroImage = heroImage.copy(-35, -80, 354, 537);
            heroImage = heroImage.scaledToHeight(464, Qt::SmoothTransformation);
            //Special / Quitar marco
            // heroImage = heroImage.copy(-22, -15, 329, 449);
            // heroImage = heroImage.scaled(306, 464, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

            QImage marcoImage(heroDir + "/Marco.png", "png");

            QPainter painter;
            painter.begin(&heroImage);
                painter.drawImage(0, 0, marcoImage);
            painter.end();

            heroImage.save(heroDir + "/" + code + ".png", "png");
        }
    );
}


//mage warrior druid hunter
void MainWindow::testDraft()
{
    // draftHandler->beginHeroDraft();

    QTimer::singleShot(2000, this, [=] () {
        draftHandler->beginDraft(Utility::classEnum2classLogNumber(MAGE), deckHandler->getDeckCardList());});

    for(int i=0; i<3; i++)
    {
        // 1. Crear el botón
        QString num = QString::number(i);
        QPushButton* miBoton = new QPushButton(num);

        // 2. Conectar la señal 'clicked' a una función lambda
        QObject::connect(miBoton, &QPushButton::clicked, [this, num](){
            draftHandler->pickCard(num);
        });

        // 3. Añadir el botón al layout
        ui->draftHorizontalLayout->addWidget(miBoton);
    }
}


void MainWindow::HAnames2codes(bool infoOnly)
{
    QJsonObject haJsonObj = Utility::loadHearthArena();
    QMap<QString, QString> swapCodes;
    for(int i=0; i<NUM_HEROS; i++)
    {
        const QString &heroLog = Utility::classOrder2classLogNumber(i);
        const QString heroString = Utility::classLogNumber2classUL_ULName(heroLog);

        const QStringList haNames = haJsonObj.value(heroString).toObject().keys();
        for(const QString &name: haNames)
        {
            if(name.contains("_")) continue;
            if(swapCodes.contains('"'+name+'"'))    continue;
            QStringList codes = Utility::cardEnCodesFromName(name);
            if(codes.isEmpty())  codes = Utility::cardEnCodesFromName(name, false);
            if(codes.isEmpty())  qDebug()<<"HearthArena WRONG NAME!!!"<<name;
            else
            {
                // qDebug()<<name<<"-->"<<codes;
                QString searchKey = name;
                if(searchKey.contains('"')) searchKey.replace('"', "\\\"");
                swapCodes.insert('"'+searchKey+'"', '"'+codes.first()+'"');
            }
        }
        qDebug()<<heroString<<swapCodes.count();
    }
    qDebug()<<swapCodes.count()<<"needed swaps.";
    if(!infoOnly)    HAreplace(swapCodes);
}


//Verifica HATL codes son los correctos (los que aparecen en HS), mirando que esten en los winrates de Firestone.
void MainWindow::checkHearthArenaTLCodes(bool infoOnly)
{
    QStringList arenaCodes = Utility::getAllArenaCodes();

    //Buscamos reemplazos
    QMap<QString, QString> swapCodes;
    //ha-->fire
    if(Utility::getTrustHA())
    {
        for(const QString &code: qAsConst(arenaCodes))
        {
            QList<CardClass> heroClassList = Utility::getClassFromCode(code);
            CardClass heroClass = heroClassList.first();
            if(heroClass == NEUTRAL)    heroClass = MAGE;
            QString fireCode = draftHandler->getFireCode(code, heroClass);
            if(fireCode != code)
            {
                swapCodes.insert('"'+code+'"', '"'+fireCode+'"');
                qDebug()<<"HATL wrong code:"<<code<<"-->"<<fireCode;
            }
        }
    }
    //ha-->sets
    else
    {
        draftHandler->initCheckHearthArena();
        for(const QString &code: qAsConst(arenaCodes))
        {
            QString haCode = draftHandler->getHACode(code);
            if(haCode != code && !arenaCodes.contains(haCode))
            {
                swapCodes.insert('"'+haCode+'"', '"'+code+'"');
                qDebug()<<"HATL wrong code:"<<haCode<<"-->"<<code;
            }
        }
        draftHandler->clearTierLists();
    }
    qDebug()<<swapCodes.count()<<"needed swaps.";
    if(!infoOnly)    HAreplace(swapCodes);
}

void MainWindow::HAreplace(const QMap<QString, QString> &swapCodes)
{
    //Realizamos reemplazos en hearthArena.json
    QString HAlocal = Utility::extraPath() + "/hearthArena.json";
    QString HAsource = QDir::homePath() + "/Documentos/ArenaTracker/HearthArena/hearthArena.json";
    QFile file(HAlocal);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Error opening hearthArena.json";
        return;
    }

    QTextStream stream(&file);
    QString content = stream.readAll();
    file.close();

    int numSwap = 0;
    QMapIterator<QString, QString> i(swapCodes);
    while(i.hasNext())
    {
        i.next();
        if(content.contains(i.key()))
        {
            content.replace(i.key(), i.value());
            numSwap++;
        }
    }

    if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        qWarning() << "Error opening hearthArena.json";
        return;
    }

    stream.setDevice(&file);
    stream << content;
    file.close();

    QFile::remove(HAsource);
    QFile::copy(HAlocal, HAsource);

    qDebug() << numSwap << "swaps made." << (swapCodes.count()-numSwap) << "not found (bundles).";
}


//Solo tiene sentido hacer la comparacion con !trustHA ya que con trustHA arenaCodes y HA es lo mismo
//La comparacion se hace con HA de names
void MainWindow::testHearthArenaTL()
{
    QStringList arenaSets;
    arenaSets <<  "WHIZBANGS_WORKSHOP" <<
        "ISLAND_VACATION" <<
        "SPACE" <<
        "EMERALD_DREAM" <<
        "THE_LOST_CITY" <<
        "TIME_TRAVEL";

    Utility::setArenaSets(arenaSets);
    QStringList arenaCodes = Utility::getAllArenaCodes(false);
    Utility::checkTierlistsCount(arenaCodes);
}

/*
 *  Whizbang’s Workshop "WHIZBANGS_WORKSHOP"
 *  Perils in Paradise "ISLAND_VACATION"
 *  The Great Dark Beyond "SPACE"
 *  Into the Emerald Dream "EMERALD_DREAM"
 *  The Lost City of Un’Goro "THE_LOST_CITY"
 *  Across the Timeways "TIME_TRAVEL"
 */
void MainWindow::testDownloadRotation(bool fromHearth, const QString &miniSet)
{
    //Download new set cards
    QStringList arenaSets;
    arenaSets << "WHIZBANGS_WORKSHOP" <<
        "ISLAND_VACATION" <<
        "SPACE" <<
        "EMERALD_DREAM" <<
        "THE_LOST_CITY" <<
        "TIME_TRAVEL";

    Utility::setArenaSets(arenaSets);
    Utility::setTrustHA(false);

    if(fromHearth)
    {
        QStringList codeList = Utility::getAllArenaCodes();
        for(const QString &code: qAsConst(codeList))
        {
            if(miniSet.isEmpty() || code.startsWith(miniSet))
            {
                cardDownloader->downloadWebImage(code, false, false, true);
                cardDownloader->downloadWebImage(code + "_premium", false, false, true);
            }
        }
    }
    else
    {
        allCardsDownloadNeeded = true;
        checkArenaCards();
    }
    //Fallo --> Failed to download card image
}


void MainWindow::testDelay()
{
    qDebug() << Qt::endl << "--------------------------" << "DEBUG TESTS" << "--------------------------";
    // testHeroPortraits();

    //HA en orden
    // downloadHearthArenaTierlistOriginal();
    // testHearthArenaTL();
    // HAnames2codes(false);
    // Utility::setTrustHA(false);//Si necesitamos revisar HATL antes de modificar arena.json a no trust en la rotacion nueva
    // checkHearthArenaTLCodes(false);//Ahora (no trustHA) / 1 semana despues (si trustHA) / ("EDR_001" --> "CORE_EDR_001") es incorrecto

    // testDownloadCardsJson();///v1/229984
    // testDownloadRotation(true/*, "DINO_"*/);//Force hearthpwn true
    // Utility::resizeSignatureCards();


    // testDraft();
    // Utility::checkMissingGoldenCards();

    // testArenaGames();
}


/*
 * Para mini-sets actualizar a las 19:00 arenaVersion.json y HATL con los nuevos sets, esto detectara todo menos las 38 nuevas cartas.
 * No esperes a la actualizacion de cards.json pq puede llevar horas, mejor hacer las sinergias al dia siguiente y volver a cambiar
 * la version de arenaVersion.json para forzar rehacer los histogramas incluyendo las nuevas 35 cartas que apareceran cuando cards.json
 * se actualice.
 */

/* Para rotaciones de CORE, en el pre patch el json tendra las nuevas cartas que se incluiran en el CORE en "set":"PLACEHOLDER_202204"
 * Aunque el json cambia el dia de la expansion los de Hearthsim no han cambiado su version, por lo que para forzar que se vuelva
 * a descargar usamos la nueva version de AT (REMOVE_EXTRA_AND_HISTOGRAMS_ON_VERSION_UPDATE true) y tambien reset de cards en update
 * (REMOVE_CARDS_ON_VERSION_UPDATE true) estan en utility.h
 */

//NUEVA EXPANSION (All servers 19:00 CEST)
//Update Json HA tierlist --> downloadHearthArenaTierlistOriginal()
//Update Json arenaVersion --> Update arenaSets/arenaVersion
//Update Json cards --> testDownloadCardsJson();
//Update Utility::isFromStandardSet(QString code) --> TIME_TRAVEL
//Subir cartas al github.
    //-Si hay modificaciones en cartas: arenaVersion.json --> "redownloadCards": true
//Crear imagenes de nuevos heroes en el github (HERO_***) (donde *** es el code de la carta, para hero cards)
    //-Si son nuevos retratos de heroe: arenaVersion.json --> "redownloadHeroes": true
    //-requiere forzar redownload cartas pq si lo ha necesitado antes habra bajado del github el heroe standard (HERO_02) y
    //-guardado como el especifico (HERO_02c), tenemos que borrarlo para que AT baje el correcto.
//Crear new signature cards, subirlas al github como _premium y guardarlas en HearthstoneSignatureCards (referencia ETC_081_premium)
    //-(https://blizzard.gamespress.com/Hearthstone)
    //-Update DraftHandler::isSignatureCard
//Update secrets
//Cartas especiales --> SynergyHandler::testSynergies()
    //Update bombing cards --> PlanHandler::isCardBomb (Hearthpwn Search: damage randomly)
    //Update cartas que dan mana inmediato --> CardGraphicsItem::getManaSpent (Hearthpwn Search: gain mana this turn only)
    //Update cartas que en la practica tienen un coste diferente --> Utility::getCorrectedCardMana (Hearthpwn Search: cost / spend all your mana)
    //Update cartas que roban un tipo especifico de carta (Curator) --> EnemyHandHandler::isDrawSpecificCards (Hearthpwn Search: draw from your deck)
    //Update cartas que roban una carta y la clonan (Mimic Pod) --> EnemyHandHandler::isClonerCard (Hearthpwn Search: draw cop)
    //Update AOE que marcan un objetivo principal y le hacen algo diferente que al resto (Swipe) --> MinionGraphicsItem::isAoeWithTarget (Hearthpwn Search: draw from your deck)

//Update synergies.json --> SynergyHandler::debugSynergiesSet()
//|-Check synergies in the new set --> New synergy keys
//|-Check evolveSyn cards
//|-Check direct links
//|-Check drops (1 semana despues) --> SynergyHandler::debugDrops()
//  In AT Dir/Extra (https://static.zerotoheroes.com/api/arena/stats/cards/arena-underground/last-patch/global.gz.json)

//Cards changes
//|-Imagenes cartas --> testDownloadRotation() --> Sobreescribir con HearthstoneSignatureCards (script moveCards.sh)
//|-Synergy / Code  --> 34.0 Patch Notes

//Rotacion CORE
//|-Revisar cartas github CORE
    //Prelanzamiento - No quitar antiguo - Incluir nuevo (set "PLACEHOLDER_202204")
    //Json set CORE actualizado - Eliminar CORE_* - Incluir nuevo (set "CORE)

//1 semana despues
//checkHearthArenaTLCodes(true);//1 semana despues (si trustHA)
//SynergyHandler::debugDrops()
//New leaderboard season
//|-En arenaVersion.json, aumentar ("arenaVersion" y "seasonId")
//Signature cards


//VM funciona en 5.15.0-113-generic


//Modelo IA
//source 1sourceCuda.sh
//./2datos.sh
//|-Descargar enUS cards.json de https://api.hearthstonejson.com/v1/latest/ en ~/modelo
//./3entrenar.sh
//SynergyHandler::testSynergies() prepara modelo/synergiesSet.json con los codigos a predecir
//./4predecir.sh
//|-synergiesSetOutSimple.json --> Sinergias predichas para copiar facilmente a synergies.json mientras las revisamos
//|-synergiesSetOut.json --> Muestra una fila con las sinergias predichas y otra con las manuales obtenidas de AT con:
//  |-(--) Aparece en las dos.
//  |-(++) No aparece en la prediccion, quizas haya que incluirla.
//  |-(!!) No apare en el manual, quizas haya que quitarla.


//NUEVA SYNERGY
//Ejemplo a copiar V_SPAWN_ENEMY/spawnEnemyGen/spawnEnemySyn
//Ejemplo gen-gen V_JADE_GOLEM/jadeGolemGen
//Marcado codigo con //New Synergy Step

//NUEVOS HERO CLASS
//Buscar NEW HERO CLASS

//ELIMINAR NAMES synergiesNames.json --> synergies.json
// +\w[ \w\.\,\'\:\-\!]+"

//NUEVOS BACKGROUND
//Coger el color de una parte clara de un carta de clase
//Colores->Colorear...(4 opcion por abajo)
//Colores->Tono y saturacion...(2 opcion) Luminosidad +50

//NUEVOS CONTROLES CONFIG TAB
//Marcado codigo con //New Config Step
//New Config Step 1 - Cargar valores --> readSettings
//New Config Step 2 - Guardar valores --> writeSettings
//New Config Step 3 - Actualizar UI con valores cargados --> initConfigTab
//New Config Step 4 - CSS nuevos controles --> updateOtherTabsTransparency
//New Config Step 5 - Mostrar opciones premium --> setPremium
//New Config Step 6 - Ocultar opciones premium --> completeConfigTab
//New Config Step 7 - Connect controles - funciones y crear funciones --> completeConfigTab

//NUEVA HEBRA
//QFutureWatcher<QString> futureFUNCION;
//connect(&futureFUNCION, SIGNAL(finished()), this, SLOT(finishFUNCION()));
//void DeckHandler::startFUNCION()
//{
//    if(!futureFUNCION.isRunning()) futureFUNCION.setFuture(QtConcurrent::run(&DeckHandler::FUNCION, this));
//}
//void DeckHandler::finishFUNCION()
//{
//    QString message = futureFUNCION.result();
//    emit pDebug(message);
//}

//FOR EACH C++
//Warning allocating an unneeded temporary container [clazy-container-anti-pattern]
//const auto codeList = map->keys();
//for(const QString &code: qAsConst(codeList))

//Sin const
//QList<SecretIcon> secretIconList = copy->secretsList;
//for(SecretIcon &secretIcon: secretIconList)

//Sort list
//std::sort(list.begin(), list.end());

//Warning pass a context object as 3rd connect parameter [clazy-connect-3arg-lambda]
//https://www.kdab.com/nailing-13-signal-slot-mistakes-clazy-1-3/

// PDEBUG
// emit pDebug(QStringLiteral("Good matches: %1 Screen: %2 Template: %3")
//             .arg(goodMatches, screenIndex, arenaTemplate));

//Lambda
//Connect, function def inline
//connect(animation, &QPropertyAnimation::finished,
//    [this]()
//    {
//    }
//https://medium.com/genymobile/how-c-lambda-expressions-can-improve-your-qt-code-8cd524f4ed9f
//QTimer::singleShot(100, this, [] () {MySlot(0); });
//connect(nm, &QNetworkAccessManager::finished,
//    [=](QNetworkReply *reply)
//    {})
//QtConcurrent::run([=]() {
//    // Code in this block will run in another thread
//});

//Sort QPair
// QList<QPair<QString, float>> lista;
// lista << qMakePair(code, score);
// std::sort(lista.begin(), lista.end(), [](const QPair<QString, float> &a, const QPair<QString, float> &b) {
//     return a.second < b.second;
// });


//REPLAY BUGS
//Mandar a pending tag changes durante 5 segundos, carta robada por mana blind no se pone a 0 mana. Aceptable

//Cambios al ataque de un arma en el turno del otro jugador no crean addons ya que el ataque del heroe estara oculto. Aceptable

//Renuncia a la oscuridad muestra como jugadas las cartas sustituidas. Van a zone vacia como los hechizos asi que no se puede distinguir. Aceptable
//Mismo problema con Experimentador gnomo al convertir un esbirro en pollo.

//Al robar un minion de un zone con auras, aparecera un addon extra en el minion robado, al cambiar su ATK/HEALTH.
//El addon es de la fuente que lo robo. Aceptable

//Viejo ojosombrio incrementa su ATK al aparecer otros murlocs, Si los murlocs nuevos son TRIGGERED,
//el addon sobre ojosombrio sera incorrecto. Aceptable

//Efectos que cambien el max vida pondran addons de vida incorrectos, igualdad. Aceptable
//Dificil de arreglar, se cambia el damage antes del health.
//Al morir stormwind champion, apareceran addons de vida de lo que lo mato en el resto de minions heridos de la zona.

//Al lanzar la maldicion del brujo la carta se roba y se juega como hechizo en el enemigo
//ZoneChangeList.ProcessChanges() - id=87 local=False [id=152 cardId= type=INVALID zone=HAND zonePos=6 player=2] zone from  -> OPPOSING HAND
//ZoneChangeList.ProcessChanges() - id=87 local=False [name=¡Maldito! id=152 zone=HAND zonePos=6 cardId=LOE_007t player=2] zone from OPPOSING HAND ->

//Se produce entre el PLAY y el POWER
//PowerTaskList.DebugPrintPower() -     TAG_CHANGE Entity=[name=Jaina Valiente id=64 zone=PLAY zonePos=0 cardId=HERO_08 player=1] tag=HEAVILY_ARMORED value=1
//GameWatcher(41192): Trigger(TRIGGER): Eremita Cho

//El minion copiado por mirror entity no tiene las modificaciones a ataque o vida ya que estas no estan en append el suficiente tiempo.

//Algunos esbirros creados con efecto de copia (manipulador ignoto) no tienen mareo de invocacion, en el log reciben exhausted --> 0

//Lich heroes no ponen 5 armadura, no aparece el TAG_CHANGE en el log


//SPECTATOR GAMES
//Si empiezan desde el principio es correcto. A veces las cartas iniciales no apareceran en la draw list, se debe a que a veces vienen del vacio en lugar del DECK.
//Por lo tanto no seran restadas del mazo, y si son devueltas en el mulligan apareceran como OUTSIDERS
//Si empiezan a medias faltara: name1, name2, playerTag, firstPlayer

//BUGS CONOCIDOS
//Tab Config ScrollArea slider transparent CSS
//Solo mode da problemas con las cartas iniciales en el enemigo, son de turn 1 y no hay moneda.
//Baron seboso (Blubber baron) no tiene atk/health correctos en el replay ya que modifica sus atributos en mano y no usa TAG_CHANGE ARMS_DEALING
//Acechador solitario (Forlorn Stalker), los minions que buffan tienen atk/health correctos por la misma razon.
//Cartas a mano sin code, arreglado obteniendo el code del nombre:
//id=10 local=False [name=Clériga de Villanorte id=55 zone=HAND zonePos=1 cardId= player=2] zone from OPPOSING PLAY -> OPPOSING HAND
//Comadreja aprece como OUTSIDER y OUTSIDER BY en tu deck, se debe a que va a tu deck 2 veces una como conocida y otra como desconocida.
//UNTIMELY_DEATH secret deberia descartarse solo si el minion muerto fue jugado el turno pasado, pero se descarta incluso si fue jugado anteriormente.


//TODO
//Manual


