#include "macocr.h"
#import <Foundation/Foundation.h>
#import <Vision/Vision.h>
#import <CoreGraphics/CoreGraphics.h>

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
        //Card names are not dictionary words
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


QRect MacOcr::hearthstoneWindowRect()
{
    QRect bestRect;

    @autoreleasepool
    {
        CFArrayRef windows = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
                                                        kCGNullWindowID);
        if(windows == nullptr)  return bestRect;

        for(NSDictionary *window in (__bridge NSArray *)windows)
        {
            if(![window[(__bridge NSString *)kCGWindowOwnerName] isEqualToString:@"Hearthstone"])  continue;
            if([window[(__bridge NSString *)kCGWindowLayer] intValue] != 0)                        continue;

            CGRect bounds;
            if(!CGRectMakeWithDictionaryRepresentation((__bridge CFDictionaryRef)window[(__bridge NSString *)kCGWindowBounds], &bounds))
                continue;
            QRect rect(bounds.origin.x, bounds.origin.y, bounds.size.width, bounds.size.height);
            if(rect.width()*rect.height() > bestRect.width()*bestRect.height())    bestRect = rect;
        }
        CFRelease(windows);
    }
    return bestRect;
}
