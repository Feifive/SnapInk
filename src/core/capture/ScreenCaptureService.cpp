#include "ScreenCaptureService.h"

#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>

QRect ScreenCaptureService::virtualDesktopGeometry()
{
    QRect virtualGeometry;

    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (screen == nullptr) {
            continue;
        }

        const QRect screenGeometry = screen->geometry();
        virtualGeometry = virtualGeometry.isNull()
                        ? screenGeometry
                        : virtualGeometry.united(screenGeometry);
    }

    return virtualGeometry;
}

CaptureResult ScreenCaptureService::captureScreens()
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    QList<CapturedScreen> capturedList;
    capturedList.reserve(screens.size());

    for (QScreen *screen : screens) {
        if (screen == nullptr) {
            continue;
        }

        // grabWindow(0) returns the entire screen in physical (device) pixels
        // on high-DPI displays.  We preserve this raw data without any scaling.
        const QPixmap pixmap = screen->grabWindow(0);
        if (pixmap.isNull()) {
            continue;
        }

        const QRect logicalGeometry = screen->geometry();
        const qreal dpr = pixmap.devicePixelRatio();

        QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
        image.setDevicePixelRatio(dpr);

        CapturedScreen captured;
        captured.screen = screen;
        captured.logicalGeometry = logicalGeometry;
        captured.image = std::move(image);
        captured.devicePixelRatio = dpr;

        capturedList.append(std::move(captured));
    }

    return CaptureResult(std::move(capturedList));
}
