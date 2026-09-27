#include "secretcard.h"
#include "../themehandler.h"
#include <QtWidgets>

SecretCard::SecretCard() : DeckCard("")
{
    this->manaText = "";
}

SecretCard::SecretCard(QString code) : DeckCard(code)
{
    this->manaText = "";
}

SecretCard::SecretCard(QString code, QString manaText) : DeckCard(code)
{
    this->manaText = manaText;
}

SecretCard::~SecretCard()
{

}


void SecretCard::draw()
{
    QPixmap canvas;
    QPainter painter;

    if(!this->code.isEmpty())
    {
        canvas = DeckCard::draw(1, true, BLACK, manaText);
    }
    else if(!this->createdByCode.isEmpty())
    {
        canvas = drawCustomCard(this->createdByCode, "BY:");
    }
    else
    {
        canvas = newCanvas(CARD_SIZE);

        painter.begin(&canvas);
            painter.fillRect(QRect(QPoint(0,0), CARD_SIZE), Qt::black);
            painter.drawPixmap(0,0,QPixmap(ThemeHandler::handCardFile()));
        painter.end();
    }

    if(this->listItem != nullptr)
    {
        if(remaining == 0)  disablePixmap(canvas);
        this->listItem->setIcon(QIcon(canvas));
    }
}
