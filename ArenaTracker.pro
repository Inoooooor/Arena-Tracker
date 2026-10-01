#-------------------------------------------------
#
# Project created by QtCreator 2014-12-31T12:10:21
#
#-------------------------------------------------

QT       += core gui network widgets websockets

TARGET = ArenaTracker
TEMPLATE = app

QT_CONFIG -= no-pkg-config

CONFIG += link_pkgconfig
macx:packagesExist(opencv5) {
    #Only the modules used: pkg-config links all of them, and the app bundle would carry them (and their
    #dependencies: video codecs, VTK...) for nothing
    QMAKE_CXXFLAGS += $$system(pkg-config --cflags opencv5)
    LIBS += -L$$system(pkg-config --variable=libdir opencv5) \
            -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_features -lopencv_geometry -lopencv_flann
}
else: packagesExist(opencv5): PKGCONFIG += opencv5
else: packagesExist(opencv4): PKGCONFIG += opencv4
else: PKGCONFIG += opencv
PKGCONFIG += libzip
LIBS += -lz

SOURCES += Sources/main.cpp\
    Sources/Cards/synergycard.cpp \
    Sources/Cards/synergyweightcard.cpp \
    Sources/Synergies/cardtypecounter.cpp \
    Sources/Synergies/draftdropcounter.cpp \
    Sources/Synergies/keysynergies.cpp \
    Sources/Synergies/layeredsynergies.cpp \
    Sources/Synergies/mechaniccounter.cpp \
    Sources/Synergies/racecounter.cpp \
    Sources/Synergies/schoolcounter.cpp \
    Sources/Widgets/enemyranking.cpp \
    Sources/mainwindow.cpp \
    Sources/logloader.cpp \
    Sources/logworker.cpp \
    Sources/gamewatcher.cpp \
    Sources/hscarddownloader.cpp \
    Sources/deckhandler.cpp \
    Sources/arenahandler.cpp \
    Sources/drafthandler.cpp \
    Sources/heartharenamentor.cpp \
    Sources/utility.cpp \
    Sources/Cards/deckcard.cpp \
    Sources/Cards/draftcard.cpp \
    Sources/Widgets/resizebutton.cpp \
    Sources/Widgets/draftscorewindow.cpp \
    Sources/Widgets/mascotwindow.cpp \
    Sources/Widgets/scoreplate.cpp \
    Sources/Widgets/scorebutton.cpp \
    Sources/Widgets/movelistwidget.cpp \
    Sources/Widgets/movetabwidget.cpp \
    Sources/Widgets/movetreewidget.cpp \
    Sources/Widgets/moveverticalscrollarea.cpp \
    Sources/Widgets/cardwindow.cpp \
    Sources/versionchecker.cpp \
    Sources/Utils/qcompressor.cpp \
    Sources/trackobotuploader.cpp \
    Sources/Utils/deckstringhandler.cpp \
    Sources/themehandler.cpp \
    Sources/Utils/libzippp.cpp \
    Sources/synergyhandler.cpp \
    Sources/Synergies/draftitemcounter.cpp \
    Sources/Synergies/statsynergies.cpp \
    Sources/Widgets/hoverlabel.cpp \
    Sources/Widgets/draftmechanicswindow.cpp \
    Sources/LibSmtp/smtp.cpp \
    Sources/Utils/hdimages.cpp \
    Sources/Utils/hdicons.cpp \
    Sources/premiumhandler.cpp \
    Sources/detachwindow.cpp \
    Sources/Widgets/lavabutton.cpp \
    Sources/Widgets/draftherowindow.cpp \
    Sources/twitchhandler.cpp \
    Sources/Widgets/twitchbutton.cpp \
    Sources/winratesdownloader.cpp

HEADERS  += Sources/mainwindow.h \
    Sources/Cards/synergycard.h \
    Sources/Cards/synergyweightcard.h \
    Sources/Synergies/cardtypecounter.h \
    Sources/Synergies/draftdropcounter.h \
    Sources/Synergies/keysynergies.h \
    Sources/Synergies/layeredsynergies.h \
    Sources/Synergies/mechaniccounter.h \
    Sources/Synergies/racecounter.h \
    Sources/Synergies/schoolcounter.h \
    Sources/Widgets/enemyranking.h \
    Sources/logloader.h \
    Sources/logworker.h \
    Sources/gamewatcher.h \
    Sources/hscarddownloader.h \
    Sources/deckhandler.h \
    Sources/arenahandler.h \
    Sources/drafthandler.h \
    Sources/heartharenamentor.h \
    Sources/utility.h \
    Sources/Cards/deckcard.h \
    Sources/Cards/draftcard.h \
    Sources/Widgets/resizebutton.h \
    Sources/Widgets/draftscorewindow.h \
    Sources/Widgets/mascotwindow.h \
    Sources/Widgets/scoreplate.h \
    Sources/Widgets/scorebutton.h \
    Sources/Widgets/movelistwidget.h \
    Sources/Widgets/ui_extended.h \
    Sources/Widgets/movetabwidget.h \
    Sources/Widgets/movetreewidget.h \
    Sources/Widgets/moveverticalscrollarea.h \
    Sources/Widgets/cardwindow.h \
    Sources/versionchecker.h \
    Sources/Utils/qcompressor.h \
    Sources/trackobotuploader.h \
    Sources/constants.h \
    Sources/Utils/deckstringhandler.h \
    Sources/themehandler.h \
    Sources/Utils/libzippp.h \
    Sources/synergyhandler.h \
    Sources/Synergies/draftitemcounter.h \
    Sources/Synergies/statsynergies.h \
    Sources/Widgets/hoverlabel.h \
    Sources/Widgets/draftmechanicswindow.h \
    Sources/Widgets/webenginepage.h \
    Sources/LibSmtp/smtp.h \
    Sources/Utils/hdimages.h \
    Sources/Utils/hdicons.h \
    Sources/premiumhandler.h \
    Sources/detachwindow.h \
    Sources/Widgets/lavabutton.h \
    Sources/Widgets/draftherowindow.h \
    Sources/twitchhandler.h \
    Sources/Widgets/twitchbutton.h \
    Sources/winratesdownloader.h

FORMS    += mainwindow.ui

RESOURCES += \
    arenatracker.qrc

linux{
    QMAKE_LFLAGS += -no-pie
    SOURCES += Sources/Utils/capturemanager.cpp
    HEADERS  += Sources/Utils/capturemanager.h
}
win32: RC_ICONS = ArenaTracker.ico
macx{
    ICON = ArenaTracker.icns
    QMAKE_TARGET_BUNDLE_PREFIX = com.inoooooor
    LIBS += -liconv
    OBJECTIVE_SOURCES += Sources/Utils/macocr.mm Sources/Utils/macwindow.mm
    HEADERS  += Sources/Utils/macocr.h Sources/Utils/macwindow.h
    LIBS += -framework Foundation -framework Vision -framework CoreGraphics -framework AppKit
    QMAKE_OBJECTIVE_CFLAGS += -fobjc-arc
}

#Deploy MAC
#First time errors
#(opencv development package not found)         Add PATH --> :/usr/local/bin

