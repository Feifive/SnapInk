#include "CaptureImageComposer.h"

#include <QPainter>
#include <QtMath>

namespace
{
qreal validDevicePixelRatio(qreal dpr)
{
    return dpr > 0.0 ? dpr : 1.0;
}
}

CaptureImageComposer::CaptureImageComposer(const CaptureResult& captureResult,
                                           const QRect& virtualGeometry)
    : m_captureResult(captureResult)
    , m_virtualGeometry(virtualGeometry)
{
}

QRect CaptureImageComposer::overlayLocalToGlobalLogical(const QRect& overlayLocalRect) const
{
    return overlayLocalRect.translated(m_virtualGeometry.topLeft());
}

QRect CaptureImageComposer::globalToOverlayLocalLogical(const QRect& globalLogicalRect) const
{
    return globalLogicalRect.translated(-m_virtualGeometry.topLeft());
}

qreal CaptureImageComposer::effectiveDevicePixelRatio(const QRect& overlayLocalRect) const
{
    if (overlayLocalRect.isEmpty()) {
        return 1.0;
    }

    const QRect globalLogicalRect = overlayLocalToGlobalLogical(overlayLocalRect);
    qreal dpr = 1.0;
    for (const CapturedScreen& screen : m_captureResult.screens()) {
        if (screen.logicalGeometry.intersects(globalLogicalRect)) {
            // Preserve SnapInk's current mixed-DPR export policy: one output
            // image uses the maximum DPR of all intersecting screens.
            dpr = qMax(dpr, validDevicePixelRatio(screen.devicePixelRatio));
        }
    }
    return dpr;
}

QImage CaptureImageComposer::createTransparentCanvas(const QRect& overlayLocalRect) const
{
    if (overlayLocalRect.isEmpty()) {
        return {};
    }

    const qreal dpr = effectiveDevicePixelRatio(overlayLocalRect);
    const int physW = qMax(1, qRound(overlayLocalRect.width() * dpr));
    const int physH = qMax(1, qRound(overlayLocalRect.height() * dpr));

    QImage image(physW, physH, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    image.setDevicePixelRatio(dpr);
    return image;
}

QImage CaptureImageComposer::composeSelectionImage(const QRect& overlayLocalRect) const
{
    QImage image = createTransparentCanvas(overlayLocalRect);
    if (image.isNull()) {
        return {};
    }

    const QRect selectionGlobal = overlayLocalToGlobalLogical(overlayLocalRect);
    const qreal dpr = image.devicePixelRatio();

    image.setDevicePixelRatio(1.0);
    QPainter painter(&image);
    painter.scale(dpr, dpr);

    for (const CapturedScreen& screen : m_captureResult.screens()) {
        const QRect overlapGlobal = selectionGlobal.intersected(screen.logicalGeometry);
        if (overlapGlobal.isEmpty()) {
            continue;
        }

        const QRect physicalSrc = m_captureResult.logicalToPhysical(screen, overlapGlobal);
        if (physicalSrc.isEmpty()) {
            continue;
        }

        const QRect overlapLocal = globalToOverlayLocalLogical(overlapGlobal);
        const QPointF targetTopLeft(overlapLocal.topLeft() - overlayLocalRect.topLeft());
        const QRectF targetRect(targetTopLeft, QSizeF(overlapLocal.size()));

        QImage sourceImage = screen.image;
        sourceImage.setDevicePixelRatio(1.0);
        painter.drawImage(targetRect, sourceImage, physicalSrc);
    }

    painter.end();
    image.setDevicePixelRatio(dpr);
    return image;
}
