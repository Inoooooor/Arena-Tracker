#ifndef MACOCR_H
#define MACOCR_H

#include <QImage>
#include <QStringList>

//Reconocimiento de texto con Apple Vision (solo macOS). Se usa para leer el nombre de las cartas del draft.
namespace MacOcr
{
    //language: codigo de idioma de Hearthstone (enUS, esES...). Devuelve las lineas de texto encontradas.
    QStringList recognizeLines(const QImage &image, const QString &language);
}

#endif // MACOCR_H
