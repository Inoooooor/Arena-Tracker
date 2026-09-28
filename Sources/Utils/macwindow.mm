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


MacFullScreenOverlay::MacFullScreenOverlay(QObject *parent) : QObject(parent)
{
    //With Hearthstone fullscreen the tracker's windows aren't visible, so App Nap throttled it:
    //finding the draft screen took 30 s instead of 1 s. The activity is held for the app's lifetime.
    static id activity = [[NSProcessInfo processInfo] beginActivityWithOptions:
                            (NSActivityUserInitiated | NSActivityLatencyCritical) reason:@"Tracking Hearthstone"];
    (void)activity;

    qApp->installEventFilter(this);
    connect(&timer, &QTimer::timeout, this, &MacFullScreenOverlay::check);
    timer.start(1000);
}


//Private CoreGraphics Spaces API (used by window managers like yabai): a Space of type 4 is a fullscreen one
extern "C" int CGSMainConnectionID(void);
extern "C" CFArrayRef CGSCopyManagedDisplaySpaces(int connection);


//Hearthstone is on screen and the current Space of some display is a fullscreen one. The menu bar can't tell:
//on displays with a camera notch it stays visible over fullscreen apps.
bool MacFullScreenOverlay::isHearthstoneFullScreen()
{
    bool hsOnScreen = false, fullScreenSpace = false;

    @autoreleasepool
    {
        CFArrayRef windows = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
                                                        kCGNullWindowID);
        if(windows == nullptr)  return false;
        for(NSDictionary *window in (__bridge NSArray *)windows)
        {
            if([window[(__bridge NSString *)kCGWindowOwnerName] isEqualToString:@"Hearthstone"] &&
                    [window[(__bridge NSString *)kCGWindowLayer] intValue] == 0)
            {
                hsOnScreen = true;
                break;
            }
        }
        CFRelease(windows);
        if(!hsOnScreen)     return false;

        CFArrayRef displays = CGSCopyManagedDisplaySpaces(CGSMainConnectionID());
        if(displays == nullptr)     return false;
        for(NSDictionary *display in (__bridge NSArray *)displays)
        {
            NSDictionary *space = display[@"Current Space"];
            if([space[@"type"] intValue] == 4)  fullScreenSpace = true;
        }
        CFRelease(displays);
    }
    return fullScreenSpace;
}


void MacFullScreenOverlay::check()
{
    bool fullScreen = isHearthstoneFullScreen();
    if(fullScreen == accessory)  return;
    accessory = fullScreen;

    [NSApp setActivationPolicy:(accessory ? NSApplicationActivationPolicyAccessory : NSApplicationActivationPolicyRegular)];

    //Changing the policy doesn't bring the windows to the fullscreen Space by itself
    if(accessory)
    {
        for(QWidget *widget: QApplication::topLevelWidgets())
        {
            if(!widget->isVisible() || !widget->windowFlags().testFlag(Qt::WindowStaysOnTopHint))  continue;
            NSView *view = (__bridge NSView *)reinterpret_cast<void *>(widget->winId());
            if(view != nil && view.window != nil)   [view.window orderFrontRegardless];
        }
    }
}


bool MacFullScreenOverlay::eventFilter(QObject *watched, QEvent *event)
{
    //Qt recreates the NSWindow when the window flags change, so the behaviour is set on every show
    if(event->type() == QEvent::Show && watched->isWidgetType())
    {
        QWidget *widget = static_cast<QWidget *>(watched);
        if(widget->isWindow() && widget->windowFlags().testFlag(Qt::WindowStaysOnTopHint))
        {
            NSView *view = (__bridge NSView *)reinterpret_cast<void *>(widget->winId());
            if(view != nil && view.window != nil)
            {
                NSWindowCollectionBehavior behavior = view.window.collectionBehavior;
                behavior &= ~(NSWindowCollectionBehaviorMoveToActiveSpace | NSWindowCollectionBehaviorFullScreenPrimary);
                behavior |= NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorFullScreenAuxiliary;
                view.window.collectionBehavior = behavior;
            }
        }
    }
    return QObject::eventFilter(watched, event);
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
