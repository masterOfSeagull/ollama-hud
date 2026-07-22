#include "CaptureService.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <stdexcept>

QImage CaptureService::capturePrimaryMonitor()
{
    QCoreApplication *application = QGuiApplication::instance();
    if (!application || QThread::currentThread() != application->thread()) {
        throw std::runtime_error("Primary monitor capture must run on the Qt GUI thread.");
    }
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        throw std::runtime_error("Could not open primary monitor.");
    }
    QPixmap pixmap = screen->grabWindow(0);
    if (pixmap.isNull()) {
        throw std::runtime_error("Primary monitor capture returned an empty image.");
    }
    return pixmap.toImage().convertToFormat(QImage::Format_RGB888);
}

QImage CaptureService::resizePreservingAspect(const QImage &image, int maxEdge)
{
    if (maxEdge <= 0) {
        throw std::invalid_argument("max_edge must be positive");
    }
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    const int longest = std::max(rgb.width(), rgb.height());
    if (longest <= maxEdge) {
        return rgb;
    }
    const double scale = static_cast<double>(maxEdge) / static_cast<double>(longest);
    const QSize size(std::max(1, static_cast<int>(std::round(rgb.width() * scale))),
                     std::max(1, static_cast<int>(std::round(rgb.height() * scale))));
    return rgb.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB888);
}

QString CaptureService::encodeJpegBase64(const QImage &image, int maxEdge, int quality)
{
    const QImage resized = resizePreservingAspect(image, maxEdge);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    resized.save(&buffer, "JPEG", quality);
    return QString::fromLatin1(bytes.toBase64());
}

QString CaptureService::imageFingerprint(const QImage &image)
{
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (int y = 0; y < rgb.height(); ++y) {
        hash.addData(reinterpret_cast<const char *>(rgb.constScanLine(y)), rgb.width() * 3);
    }
    return QString::fromLatin1(hash.result().toHex().left(10));
}
