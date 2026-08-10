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
        throw std::runtime_error("주 모니터 캡처는 Qt GUI 스레드에서 실행해야 합니다.");
    }
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        throw std::runtime_error("주 모니터를 열 수 없습니다.");
    }
    QPixmap pixmap = screen->grabWindow(0);
    if (pixmap.isNull()) {
        throw std::runtime_error("주 모니터 캡처가 빈 이미지를 반환했습니다.");
    }
    return pixmap.toImage().convertToFormat(QImage::Format_RGB888);
}

QImage CaptureService::resizePreservingAspect(const QImage &image, int maxEdge)
{
    if (maxEdge <= 0) {
        throw std::invalid_argument("최대 변 길이는 양수여야 합니다.");
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
