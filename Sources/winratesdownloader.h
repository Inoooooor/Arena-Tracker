#ifndef WINRATESDOWNLOADER_H
#define WINRATESDOWNLOADER_H

#include "Sources/utility.h"
#include "qfuturewatcher.h"
#include <QObject>
#include <QNetworkAccessManager>

#define FIRE_CARDS_URL "https://static.zerotoheroes.com/api/arena/stats/cards/arena-underground/last-patch/"
#define FIRE_CLASSES_URL "https://static.zerotoheroes.com/api/arena/stats/classes/arena-underground/last-patch/overview.gz.json"

#define FIRE_CLASSES_FILE "fireClasses.json"


class FireData
{
public:
    QMap<QString, float> fireWRMap;
    QMap<QString, int> fireSamplesMap;
};


class WinratesDownloader : public QObject
{
    Q_OBJECT
public:
    WinratesDownloader(QObject *parent);
    ~WinratesDownloader();

//Variables
private:
    QNetworkAccessManager *networkManager;
    QRegularExpressionMatch *match;
    QFutureWatcher<FireData> futureFire[NUM_HEROS];
    int fireDataThreads;
    QMap<QString, float> *fireWRMap;
    QMap<QString, int> *fireSamplesMap;


//Metodos
private:
    void initFireCards();
    void localHeroesWinrate();
    void localFireCards(const int classOrder);
    void startProcessFireCards(const QJsonObject &jsonObject, const int classOrder);
    void processHeroesWinrate(const QJsonObject &jsonObject);
    void showDataProgressBar();
    int url2classOrder(QString url);

public:
    void initWRCards();
    void initHeroesWinrate();
    void waitFinishThreads();

signals:
    void startProgressBar(int maximum, QString text);
    void advanceProgressBar(int remaining, QString text="");
    void showMessageProgressBar(QString text, int hideDelay = 5000);
    void pDebug(QString line, DebugLevel debugLevel=Normal, QString file="WinratesDownloader");
    void readyFireWRMap(QMap<QString, float> *fireWRMap);
    void readyFireSamplesMap(QMap<QString, int> *fireSamplesMap);
    void readyHeroesWinrate();

private slots:
    void replyFinished(QNetworkReply *reply);
};

#endif // WINRATESDOWNLOADER_H
