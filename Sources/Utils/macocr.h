#ifndef MACOCR_H
#define MACOCR_H

#include <QImage>
#include <QStringList>

//Text recognition with Apple Vision (macOS only). Used to read the names of the draft cards.
namespace MacOcr
{
    //language: Hearthstone language code (enUS, esES...). Returns the text lines found.
    QStringList recognizeLines(const QImage &image, const QString &language);
}

#endif // MACOCR_H
