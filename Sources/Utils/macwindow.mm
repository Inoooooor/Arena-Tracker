#include "macwindow.h"
#include <QtWidgets>
#import <AppKit/AppKit.h>


void MacWindow::allowMiniaturize(QWidget *window)
{
    if(window == nullptr)   return;
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    if(view == nil || view.window == nil)   return;
    view.window.styleMask |= NSWindowStyleMaskMiniaturizable;
}


MacHoverTracker::MacHoverTracker(QObject *parent) : QObject(parent)
{
    connect(&timer, &QTimer::timeout, this, &MacHoverTracker::check);
    timer.start(30);
}


void MacHoverTracker::check()
{
    //Active: Qt handles the mouse itself
    if(QGuiApplication::applicationState() == Qt::ApplicationActive)
    {
        lastWidget = nullptr;
        return;
    }

    const QPoint pos = QCursor::pos();
    QWidget *widget = QApplication::widgetAt(pos);

    if(widget != lastWidget)
    {
        dispatchEnterLeave(widget, lastWidget, pos);
        lastWidget = widget;
    }

    if(widget != nullptr && pos != lastPos && widget->hasMouseTracking())
    {
        QMouseEvent event(QEvent::MouseMove, widget->mapFromGlobal(QPointF(pos)), QPointF(pos),
                          Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(widget, &event);
    }
    lastPos = pos;
}


//Leave for the old widget and its parents not under the cursor anymore, then enter for the new ones, outer first
void MacHoverTracker::dispatchEnterLeave(QWidget *enter, QWidget *leave, const QPoint &globalPos)
{
    QList<QWidget *> enterChain, leaveChain;
    for(QWidget *w=enter; w!=nullptr; w=(w->isWindow()?nullptr:w->parentWidget()))  enterChain << w;
    for(QWidget *w=leave; w!=nullptr; w=(w->isWindow()?nullptr:w->parentWidget()))  leaveChain << w;
    while(!enterChain.isEmpty() && !leaveChain.isEmpty() && enterChain.last() == leaveChain.last())
    {
        enterChain.removeLast();
        leaveChain.removeLast();
    }

    for(QWidget *w: std::as_const(leaveChain))
    {
        QEvent event(QEvent::Leave);
        QApplication::sendEvent(w, &event);
    }
    for(int i=enterChain.count()-1; i>=0; i--)
    {
        QWidget *w = enterChain[i];
        QEnterEvent event(w->mapFromGlobal(QPointF(globalPos)), w->mapFromGlobal(QPointF(globalPos)), QPointF(globalPos));
        QApplication::sendEvent(w, &event);
    }
}
