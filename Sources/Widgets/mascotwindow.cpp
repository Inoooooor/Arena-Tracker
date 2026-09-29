#include "mascotwindow.h"
#include <QtWidgets>
#ifdef Q_OS_MAC
    #include "../Utils/macwindow.h"
#endif


#define MASCOT_SPRITE_HEIGHT    163     //Points; the sprites have room above the hat for the grabbed one
#define MASCOT_BUBBLE_MAX_WIDTH 240
#define MASCOT_PIXEL            3       //Size of one "pixel" of the bubble frame
#define MASCOT_BUBBLE_PADDING   9
#define MASCOT_TAIL_HEIGHT      15      //5 frame pixels
#define MASCOT_BUTTON_HEIGHT    27
#define MASCOT_BUTTON_GAP       8
#define MASCOT_BUTTON_COLOR     QColor(107, 27, 155)    //The hat's purple


MascotWindow::MascotWindow(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint)
{
    //A tool window is a panel on macOS: made non-activating (MacFullScreenOverlay) it shows over fullscreen Hearthstone.
    //Tool windows hide when the app is inactive unless told otherwise.
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowTitle("AT Mascot");

    //Moods without their own art yet use a close one
    const char *files[NumMoods] = {"idle", "popcorn", "thinking", "point", "smile", "grin", "smug", "happy", "sweat",
                                   "grabbed", "stars"};
    const Mood fallbacks[NumMoods] = {Idle, Popcorn, Thinking, Point, Smile, Grin, Smug, Happy, Sweat, Sweat, Happy};
    for(int i=0; i<NumMoods; i++)   sprites[i] = QPixmap(QStringLiteral(":/Images/Mascot/%1.png").arg(files[i]));
    for(int i=0; i<NumMoods; i++)   if(sprites[i].isNull())     sprites[i] = sprites[fallbacks[i]];

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
    if(dragging && dragMoved)
    {
        moodBeforeDrag = mood;      //Shown when it's dropped
        return;
    }
    if(this->mood == mood)  return;
    this->mood = mood;
    update();
}


void MascotWindow::say(const QString &text, int msec, const QString &button, std::function<void()> action)
{
    sayTimer.stop();
    if(msec > 0 && !text.isEmpty())     sayTimer.start(msec);
    buttonAction = text.isEmpty() ? nullptr : action;
    QString newButton = text.isEmpty() ? QString() : button;
    if(this->text == text && buttonText == newButton)   return;
    this->text = text;
    buttonText = newButton;
    relayout();
    update();
}


//Bubble on top, the character under it. The window is placed so the character's bottom center is on the anchor.
void MascotWindow::relayout()
{
    const QPixmap &sprite = sprites[Idle];
    int spriteW = sprite.isNull() ? MASCOT_SPRITE_HEIGHT : MASCOT_SPRITE_HEIGHT * sprite.width() / sprite.height();
    int spriteH = MASCOT_SPRITE_HEIGHT;
    const int inset = MASCOT_BUBBLE_PADDING + MASCOT_PIXEL;

    QSize bubbleSize(0, 0);
    QRect textBox, buttonBox;
    if(!text.isEmpty())
    {
        QFontMetrics fm(bubbleFont);
        textBox = fm.boundingRect(QRect(0, 0, MASCOT_BUBBLE_MAX_WIDTH - 2*inset, 1000), Qt::TextWordWrap, text);
        int contentW = textBox.width();
        int contentH = textBox.height();
        if(!buttonText.isEmpty())
        {
            buttonBox = QRect(0, 0, fm.horizontalAdvance(buttonText) + 4*MASCOT_PIXEL + 2*MASCOT_BUBBLE_PADDING, MASCOT_BUTTON_HEIGHT);
            contentW = std::max(contentW, buttonBox.width());
            contentH += MASCOT_BUTTON_GAP + MASCOT_BUTTON_HEIGHT;
        }
        bubbleSize = QSize(contentW + 2*inset, contentH + 2*inset);
    }

    int w = std::max(spriteW, bubbleSize.width());
    int bubbleBlockH = text.isEmpty() ? 0 : bubbleSize.height() + MASCOT_TAIL_HEIGHT;
    int h = bubbleBlockH + spriteH;

    bubbleRect = QRect((w - bubbleSize.width())/2, 0, bubbleSize.width(), bubbleSize.height());
    textRect = QRect(bubbleRect.x() + inset, bubbleRect.y() + inset, bubbleSize.width() - 2*inset, textBox.height());
    buttonRect = buttonText.isEmpty() ? QRect() :
                 QRect(bubbleRect.x() + inset, textRect.bottom() + 1 + MASCOT_BUTTON_GAP, buttonBox.width(), MASCOT_BUTTON_HEIGHT);
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


//Always above the tracker's own stay on top windows, like the old main window opened from its menu
void MascotWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
#ifdef Q_OS_MAC
    QTimer::singleShot(0, this, [this]() { MacWindow::raiseAboveFloating(this); });
#endif
}


//A box with a black frame made of square pixels and notched corners
void MascotWindow::drawFrame(QPainter &painter, const QRect &r, const QColor &fill)
{
    const int p = MASCOT_PIXEL;
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRect(r.adjusted(p, p, -p, -p));
    painter.setBrush(Qt::black);
    painter.drawRect(r.x() + p, r.y(), r.width() - 2*p, p);                     //Top
    painter.drawRect(r.x() + p, r.bottom() - p + 1, r.width() - 2*p, p);        //Bottom
    painter.drawRect(r.x(), r.y() + p, p, r.height() - 2*p);                    //Left
    painter.drawRect(r.right() - p + 1, r.y() + p, p, r.height() - 2*p);        //Right
}


//White box with a stepped tail towards the head, the text and an optional button
void MascotWindow::drawBubble(QPainter &painter)
{
    const int p = MASCOT_PIXEL;
    const QRect r = bubbleRect;
    drawFrame(painter, r, Qt::white);

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
    painter.drawText(textRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignVCenter, text);

    if(!buttonText.isEmpty())
    {
        QRect button = buttonRect.translated(0, buttonPressed ? p : 0);
        if(!buttonPressed)  drawFrame(painter, buttonRect.translated(0, p), Qt::black);     //Shadow
        drawFrame(painter, button, MASCOT_BUTTON_COLOR);
        painter.setPen(Qt::white);
        painter.drawText(button, Qt::AlignCenter, buttonText);
    }
}


void MascotWindow::mousePressEvent(QMouseEvent *event)
{
    if(event->button() != Qt::LeftButton)   return;
    if(buttonRect.contains(event->position().toPoint()))
    {
        buttonPressed = true;
        update();
        return;
    }
    dragging = true;
    dragMoved = false;
    pressPos = event->globalPosition().toPoint();
    dragOffset = pressPos - anchor;
}


void MascotWindow::mouseMoveEvent(QMouseEvent *event)
{
    if(!dragging)   return;
    QPoint pos = event->globalPosition().toPoint();
    if(!dragMoved && (pos - pressPos).manhattanLength() > 3)
    {
        //Lifted by the scruff
        moodBeforeDrag = mood;
        setMood(Grabbed);
        dragMoved = true;
    }
    anchor = pos - dragOffset;
    move(anchor.x() - width()/2, anchor.y() - height());
}


void MascotWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() != Qt::LeftButton)   return;
    if(buttonPressed)
    {
        buttonPressed = false;
        update();
        if(buttonRect.contains(event->position().toPoint()) && buttonAction)
        {
            std::function<void()> action = buttonAction;
            say("");
            action();
        }
        return;
    }
    if(!dragging)   return;
    dragging = false;
    if(dragMoved)
    {
        dragMoved = false;
        Mood restore = moodBeforeDrag;
        mood = Grabbed;
        setMood(restore);
        saveAnchor();
    }
}


//Next to the character, so the menu doesn't cover it
void MascotWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.addAction("Open tracker", this, &MascotWindow::openTrackerRequested);
    menu.addSeparator();
    menu.addAction("Quit", this, &MascotWindow::quitRequested);

    QPoint pos = mapToGlobal(QPoint(spriteRect.right() + 1, spriteRect.top() + spriteRect.height()/3));
    QScreen *screen = QGuiApplication::screenAt(pos);
    QSize size = menu.sizeHint();
    if(screen != nullptr && pos.x() + size.width() > screen->availableGeometry().right())
        pos.setX(mapToGlobal(spriteRect.topLeft()).x() - size.width() - 1);
    if(screen != nullptr && pos.y() + size.height() > screen->availableGeometry().bottom())
        pos.setY(screen->availableGeometry().bottom() - size.height());
    menu.exec(pos);
    (void)event;
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
