#include "mascotwindow.h"
#include <QtWidgets>


#define MASCOT_SPRITE_HEIGHT    150     //Points
#define MASCOT_BUBBLE_MAX_WIDTH 240
#define MASCOT_PIXEL            3       //Size of one "pixel" of the bubble frame
#define MASCOT_BUBBLE_PADDING   9
#define MASCOT_TAIL_HEIGHT      15      //5 frame pixels


MascotWindow::MascotWindow(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowTitle("AT Mascot");

    const char *files[NumMoods] = {"idle", "popcorn", "thinking", "point", "smile", "grin", "smug", "happy", "sweat"};
    for(int i=0; i<NumMoods; i++)   sprites[i] = QPixmap(QStringLiteral(":/Images/Mascot/%1.png").arg(files[i]));

    int fontId = QFontDatabase::addApplicationFont(":/Fonts/PixelifySans.ttf");
    QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    bubbleFont = QFont(families.isEmpty() ? QString() : families.first());
    bubbleFont.setPixelSize(15);

    sayTimer.setSingleShot(true);
    connect(&sayTimer, &QTimer::timeout, this, [this]() { say(""); });

    loadAnchor();
    relayout();
}


void MascotWindow::setMood(Mood mood)
{
    if(this->mood == mood)  return;
    this->mood = mood;
    update();
}


void MascotWindow::say(const QString &text, int msec)
{
    sayTimer.stop();
    if(msec > 0 && !text.isEmpty())     sayTimer.start(msec);
    if(this->text == text)  return;
    this->text = text;
    relayout();
    update();
}


//Bubble on top, the character under it. The window is placed so the character's bottom center is on the anchor.
void MascotWindow::relayout()
{
    const QPixmap &sprite = sprites[Idle];
    int spriteW = sprite.isNull() ? MASCOT_SPRITE_HEIGHT : MASCOT_SPRITE_HEIGHT * sprite.width() / sprite.height();
    int spriteH = MASCOT_SPRITE_HEIGHT;

    QSize bubbleSize(0, 0);
    if(!text.isEmpty())
    {
        QFontMetrics fm(bubbleFont);
        int maxTextW = MASCOT_BUBBLE_MAX_WIDTH - 2*(MASCOT_BUBBLE_PADDING + MASCOT_PIXEL);
        QRect textRect = fm.boundingRect(QRect(0, 0, maxTextW, 1000), Qt::TextWordWrap, text);
        bubbleSize = QSize(textRect.width() + 2*(MASCOT_BUBBLE_PADDING + MASCOT_PIXEL),
                           textRect.height() + 2*(MASCOT_BUBBLE_PADDING + MASCOT_PIXEL));
    }

    int w = std::max(spriteW, bubbleSize.width());
    int bubbleBlockH = text.isEmpty() ? 0 : bubbleSize.height() + MASCOT_TAIL_HEIGHT;
    int h = bubbleBlockH + spriteH;

    bubbleRect = QRect((w - bubbleSize.width())/2, 0, bubbleSize.width(), bubbleSize.height());
    spriteRect = QRect((w - spriteW)/2, bubbleBlockH, spriteW, spriteH);

    setFixedSize(w, h);
    move(anchor.x() - w/2, anchor.y() - h);

    //Clicks go through the empty parts of the window
    QRegion region(spriteRect);
    if(!text.isEmpty())     region += QRegion(bubbleRect.adjusted(0, 0, 0, MASCOT_TAIL_HEIGHT));
    setMask(region);
}


void MascotWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if(!text.isEmpty())     drawBubble(painter);
    painter.drawPixmap(spriteRect, sprites[mood]);
}


//White box with a black frame made of square pixels, notched corners and a stepped tail towards the head
void MascotWindow::drawBubble(QPainter &painter)
{
    const int p = MASCOT_PIXEL;
    const QRect r = bubbleRect;
    painter.setPen(Qt::NoPen);

    painter.setBrush(Qt::white);
    painter.drawRect(r.adjusted(p, p, -p, -p));

    painter.setBrush(Qt::black);
    painter.drawRect(r.x() + p, r.y(), r.width() - 2*p, p);                     //Top
    painter.drawRect(r.x() + p, r.bottom() - p + 1, r.width() - 2*p, p);        //Bottom
    painter.drawRect(r.x(), r.y() + p, p, r.height() - 2*p);                    //Left
    painter.drawRect(r.right() - p + 1, r.y() + p, p, r.height() - 2*p);        //Right

    //Tail: a stepped triangle under the bubble, a bit left of its center, pointing at the head
    int x0 = r.x() + r.width()/2 - 6*p;
    int y0 = r.bottom() + 1;
    painter.setBrush(Qt::white);
    painter.drawRect(x0 + p, y0 - p, 7*p, p);                       //Opening in the bottom border
    for(int k=0; k<4; k++)
    {
        int left = x0 + k*p, right = x0 + (8 - k)*p, y = y0 + k*p;
        painter.setBrush(Qt::black);
        painter.drawRect(left, y, p, p);
        painter.drawRect(right, y, p, p);
        painter.setBrush(Qt::white);
        painter.drawRect(left + p, y, right - left - p, p);
    }
    painter.setBrush(Qt::black);
    painter.drawRect(x0 + 4*p, y0 + 4*p, p, p);                     //Tip

    painter.setPen(Qt::black);
    painter.setFont(bubbleFont);
    painter.drawText(r.adjusted(p + MASCOT_BUBBLE_PADDING, p + MASCOT_BUBBLE_PADDING,
                                -p - MASCOT_BUBBLE_PADDING, -p - MASCOT_BUBBLE_PADDING),
                     Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignVCenter, text);
}


void MascotWindow::mousePressEvent(QMouseEvent *event)
{
    if(event->button() != Qt::LeftButton)   return;
    dragging = true;
    dragOffset = event->globalPosition().toPoint() - anchor;
}


void MascotWindow::mouseMoveEvent(QMouseEvent *event)
{
    if(!dragging)   return;
    anchor = event->globalPosition().toPoint() - dragOffset;
    move(anchor.x() - width()/2, anchor.y() - height());
}


void MascotWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() != Qt::LeftButton || !dragging)  return;
    dragging = false;
    saveAnchor();
}


void MascotWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.addAction("Open tracker", this, &MascotWindow::openTrackerRequested);
    menu.addSeparator();
    menu.addAction("Quit", this, &MascotWindow::quitRequested);
    menu.exec(event->globalPos());
}


//Default: bottom right corner of the primary screen. A saved position off every screen falls back to it.
void MascotWindow::loadAnchor()
{
    QSettings settings("Arena Tracker", "Arena Tracker");
    QRect available = QGuiApplication::primaryScreen()->availableGeometry();
    QPoint defaultAnchor = available.bottomRight() - QPoint(110, 10);
    anchor = settings.value("mascotAnchor", defaultAnchor).toPoint();
    if(QGuiApplication::screenAt(anchor - QPoint(0, 10)) == nullptr)    anchor = defaultAnchor;
}


void MascotWindow::saveAnchor()
{
    QSettings settings("Arena Tracker", "Arena Tracker");
    settings.setValue("mascotAnchor", anchor);
}
