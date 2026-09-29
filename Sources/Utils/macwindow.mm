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

    //Check right away on a Space switch, and again once the switch animation is over
    [[NSWorkspace sharedWorkspace].notificationCenter addObserverForName:NSWorkspaceActiveSpaceDidChangeNotification
                                                                  object:nil queue:[NSOperationQueue mainQueue]
                                                              usingBlock:^(NSNotification *) {
        check();
        QTimer::singleShot(500, this, &MacFullScreenOverlay::check);
    }];
}


//Private CoreGraphics Spaces API (used by window managers like yabai): a Space of type 4 is a fullscreen one
extern "C" int CGSMainConnectionID(void);
extern "C" CFArrayRef CGSCopyManagedDisplaySpaces(int connection);


//A Hearthstone window is on screen: in the current Space and not minimized
bool MacFullScreenOverlay::isHearthstoneOnScreen()
{
    bool onScreen = false;

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
                onScreen = true;
                break;
            }
        }
        CFRelease(windows);
    }
    return onScreen;
}


//The current Space of some display is a fullscreen one. The menu bar can't tell: on displays with a camera
//notch it stays visible over fullscreen apps.
bool MacFullScreenOverlay::isFullScreenSpace()
{
    bool fullScreenSpace = false;

    @autoreleasepool
    {
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


bool MacFullScreenOverlay::isHearthstoneFullScreen()
{
    return isHearthstoneOnScreen() && isFullScreenSpace();
}


bool MacFullScreenOverlay::isDraftOverlay(QWidget *widget)
{
    return widget->inherits("DraftScoreWindow") || widget->inherits("DraftHeroWindow") ||
            widget->inherits("DraftMechanicsWindow") || widget->inherits("MascotWindow");
}


//Hidden through the NSWindow alpha, so the draft code keeps showing and hiding them as usual
void MacFullScreenOverlay::showDraftOverlay(QWidget *widget)
{
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(widget->winId());
    if(view == nil || view.window == nil)   return;
    view.window.alphaValue = hsOnScreen ? 1.0 : 0.0;
    view.window.ignoresMouseEvents = !hsOnScreen;
}


//The window server's view: on screen means in the current Space. NSWindow.isOnActiveSpace says yes for windows that
//join all Spaces, even when macOS kept them out of a fullscreen Space.
static bool isWindowOnScreen(NSWindow *window)
{
    bool onScreen = false;
    CFArrayRef windows = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, (CGWindowID)window.windowNumber);
    if(windows == nullptr)  return false;
    NSArray *list = (__bridge NSArray *)windows;
    if(list.count > 0)  onScreen = [list[0][(__bridge NSString *)kCGWindowIsOnscreen] boolValue];
    CFRelease(windows);
    return onScreen;
}


void MacFullScreenOverlay::check()
{
    bool onScreen = isHearthstoneOnScreen();
    bool changed = (onScreen != hsOnScreen);
    hsOnScreen = onScreen;

    //macOS only lets an accessory app's windows into another app's fullscreen Space
    bool fullScreen = onScreen && isFullScreenSpace();
    if(fullScreen != accessory)
    {
        accessory = fullScreen;
        [NSApp setActivationPolicy:(accessory ? NSApplicationActivationPolicyAccessory : NSApplicationActivationPolicyRegular)];
    }

    for(QWidget *widget: QApplication::topLevelWidgets())
    {
        if(!widget->isVisible() || !widget->windowFlags().testFlag(Qt::WindowStaysOnTopHint))  continue;
        if(changed && isDraftOverlay(widget))   showDraftOverlay(widget);
        if(!hsOnScreen)     continue;

        //A window shown before this Space became active (the mascot, shown at startup) stays out of it even joining
        //all Spaces, and ordering it in during the Space switch animation leaves it in the old Space: every check,
        //a window missing from the active Space is ordered out and in again
        NSView *view = (__bridge NSView *)reinterpret_cast<void *>(widget->winId());
        if(view == nil || view.window == nil || isWindowOnScreen(view.window))    continue;
        [view.window orderOut:nil];
        [view.window orderFrontRegardless];
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
            if(isDraftOverlay(widget))  showDraftOverlay(widget);
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
