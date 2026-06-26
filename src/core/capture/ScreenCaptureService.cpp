#include "ScreenCaptureService.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QScreen>

QRect ScreenCaptureService::virtualDesktopGeometry()
{
    QRect virtualGeometry;

    const QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
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

QImage ScreenCaptureService::captureVirtualDesktop()
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    const QRect virtualGeometry = virtualDesktopGeometry();
    if (screens.isEmpty() || virtualGeometry.isEmpty()) {
        return {};
    }

    QImage result(virtualGeometry.size(), QImage::Format_ARGB32);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    bool capturedAnyScreen = false;

    for (QScreen* screen : screens) {
        if (screen == nullptr) {
            continue;
        }

        const QRect screenGeometry = screen->geometry();
        const QPixmap pixmap = screen->grabWindow(0);
        if (pixmap.isNull()) {
            continue;
        }

        const QImage captured = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
        const QPoint targetTopLeft = screenGeometry.topLeft() - virtualGeometry.topLeft();

        // Draw into the logical screen rectangle so mixed-DPI pixmaps still map to
        // the same coordinate space used by the overlay and editor.
        painter.drawImage(QRect(targetTopLeft, screenGeometry.size()), captured);
        capturedAnyScreen = true;
    }

    painter.end();

    if (!capturedAnyScreen) {
        return {};
    }

    return result;
}
