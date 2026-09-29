#ifndef MASCOTWINDOW_H
#define MASCOTWINDOW_H

#include <QWidget>
#include <QPixmap>
#include <QTimer>
#include <QPoint>


//The mascot: a draggable pixel character that tells the tracker's status and advice in a speech bubble.
//The window grows upwards when the bubble shows, so the character stays where the user left it.
class MascotWindow : public QWidget
{
    Q_OBJECT
public:
    enum Mood { Idle, Popcorn, Thinking, Point, Smile, Grin, Smug, Happy, Sweat, NumMoods };

    explicit MascotWindow(QWidget *parent = nullptr);

    void setMood(Mood mood);
    //An empty text hides the bubble. With msec > 0 the bubble hides by itself after that time.
    void say(const QString &text, int msec = 0);

signals:
    void openTrackerRequested();
    void quitRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QPixmap sprites[NumMoods];
    Mood mood = Idle;
    QString text;
    QTimer sayTimer;
    QFont bubbleFont;
    QRect bubbleRect, spriteRect;
    QPoint anchor;              //Global position of the character's bottom center
    QPoint dragOffset;
    bool dragging = false;

    void relayout();
    void drawBubble(QPainter &painter);
    void loadAnchor();
    void saveAnchor();
};

#endif // MASCOTWINDOW_H
