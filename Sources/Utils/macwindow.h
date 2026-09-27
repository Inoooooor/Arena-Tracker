#ifndef MACWINDOW_H
#define MACWINDOW_H

#include <QObject>
#include <QPoint>
#include <QWidget>
#include <QPointer>
#include <QTimer>

//macOS window behaviour the frameless Qt windows don't get by themselves
namespace MacWindow
{
    //Frameless windows can't be minimized to the Dock unless their style allows it
    void allowMiniaturize(QWidget *window);
}


//Qt only tracks the mouse on macOS while the app is active: with Hearthstone in front, hovering the
//tracker did nothing until it was clicked. While the app is inactive this polls the cursor and sends
//the enter, leave and move events Qt would send if it were active.
class MacHoverTracker : public QObject
{
    Q_OBJECT
public:
    explicit MacHoverTracker(QObject *parent);

private:
    QTimer timer;
    QPointer<QWidget> lastWidget;
    QPoint lastPos;

    void dispatchEnterLeave(QWidget *enter, QWidget *leave, const QPoint &globalPos);

private slots:
    void check();
};

#endif // MACWINDOW_H
