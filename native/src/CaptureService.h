#pragma once

#include <QImage>
#include <QString>

class CaptureService
{
public:
    static QImage capturePrimaryMonitor();
    static QImage resizePreservingAspect(const QImage &image, int maxEdge);
    static QString encodeJpegBase64(const QImage &image, int maxEdge, int quality);
    static QString imageFingerprint(const QImage &image);
};
