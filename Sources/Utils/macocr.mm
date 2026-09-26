#include "macocr.h"
#import <Foundation/Foundation.h>
#import <Vision/Vision.h>

QStringList MacOcr::recognizeLines(const QImage &image, const QString &language)
{
    QStringList lines;
    if(image.isNull())  return lines;

    @autoreleasepool
    {
        CGImageRef cgImage = image.toCGImage();
        if(cgImage == nullptr)  return lines;

        VNRecognizeTextRequest *request = [[VNRecognizeTextRequest alloc] init];
        request.recognitionLevel = VNRequestTextRecognitionLevelAccurate;
        //Los nombres de cartas no son palabras de diccionario
        request.usesLanguageCorrection = NO;
        //enUS --> en-US
        if(language.length() == 4)
        {
            QString bcp47 = language.left(2) + "-" + language.right(2);
            request.recognitionLanguages = @[bcp47.toNSString()];
        }

        VNImageRequestHandler *handler = [[VNImageRequestHandler alloc] initWithCGImage:cgImage options:@{}];
        NSError *error = nil;
        if([handler performRequests:@[request] error:&error])
        {
            for(VNRecognizedTextObservation *observation in request.results)
            {
                VNRecognizedText *text = [[observation topCandidates:1] firstObject];
                if(text != nil) lines << QString::fromNSString(text.string);
            }
        }
        CGImageRelease(cgImage);
    }
    return lines;
}
