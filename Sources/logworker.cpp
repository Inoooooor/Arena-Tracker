#include "logworker.h"
#include <QtWidgets>


LogWorker::LogWorker(QObject *parent, const QString &logsDirPath, const QString &logComponentString) : QObject(parent)
{
    this->logSeek = 0;
    this->logNumLine = 0;
    this->logComponentString = logComponentString;
    this->logPath = logsDirPath + "/" + logComponentString + ".log";
    this->logSize = QFileInfo (logPath).size();
    initLogComponent(logComponentString);
}

LogWorker::~LogWorker()
{

}


void LogWorker::initLogComponent(QString logComponentString)
{
    if(logComponentString == "LoadingScreen")
    {
        this->logComponent = logLoadingScreen;
    }
    else if(logComponentString == "Power")
    {
        this->logComponent = logPower;
    }
    else if(logComponentString == "Zone")
    {
        this->logComponent = logZone;
    }
    else if(logComponentString == "Arena")
    {
        this->logComponent = logArena;
    }
    else if(logComponentString == "Asset")
    {
        this->logComponent = logAsset;
    }
    else
    {
        this->logComponent = logInvalid;
    }
}


void LogWorker::reset()
{
    logSeek = 0;
    logNumLine = 0;
    logSize = 0;
}


int LogWorker::readLine(QFile &file, QString &utf8Line)
{
    char c;
    int lineLength = 0;
    QByteArray line;

    while(true)
    {
        if(!file.getChar(&c))    return -1;
        line.append(c);
        lineLength++;
        if(c == '\n')
        {
            utf8Line = QString::fromUtf8(line);
            return lineLength;
        }
    }
}


void LogWorker::readLog()
{
    QFile logFile(logPath);
    if(!logFile.exists())
    {
        if(this->logComponent == logLoadingScreen)  emit pDebug("Missing log LoadingScreen.", DebugLevel::Warning);
        return;
    }
    if(!logFile.open(QIODevice::ReadOnly))
    {
        emit pDebug("Cannot open existing log " + this->logComponentString, DebugLevel::Error);
        return;
    }

    logFile.seek(logSeek);

    QString line;
    int lineLenght;

    while((lineLenght = readLine(logFile, line)) > 0)
    {
        emit newLogLineRead(logComponent, line, ++logNumLine, logSeek);
        logSeek += lineLenght;
    }

    logFile.close();
}




//Moves the read position to the start of the last line containing marker, or to the end of the log if none does
void LogWorker::skipToLastLine(const QString &marker)
{
    QFile logFile(logPath);
    if(!logFile.open(QIODevice::ReadOnly))  return;

    qint64 seek = 0, numLine = 0;
    qint64 markerSeek = -1, markerNumLine = 0;
    QString line;
    int lineLength;
    while((lineLength = readLine(logFile, line)) > 0)
    {
        if(line.contains(marker))
        {
            markerSeek = seek;
            markerNumLine = numLine;
        }
        seek += lineLength;
        numLine++;
    }
    logFile.close();

    if(markerSeek == -1)
    {
        logSeek = seek;
        logNumLine = numLine;
    }
    else
    {
        logSeek = markerSeek;
        logNumLine = markerNumLine;
    }
}
