#ifndef CAPTUREIMAGECOMPOSER_H
#define CAPTUREIMAGECOMPOSER_H

#include "CapturedScreen.h"

#include <QImage>
#include <QPixmap>
#include <QRect>

class CaptureImageComposer
{
public:
    CaptureImageComposer(const CaptureResult& captureResult,
                         const QRect& virtualGeometry);

    QRect toGlobalLogical(const QRect& overlayLocalRect) const;
    qreal effectiveDevicePixelRatio(const QRect& overlayLocalRect) const;

    QImage createTransparentCanvas(const QRect& overlayLocalRect) const;
    QImage composeSelectionImage(const QRect& overlayLocalRect) const;
    QPixmap composeSelectionPixmap(const QRect& overlayLocalRect) const;

private:
    QRect toOverlayLogical(const QRect& globalLogicalRect) const;

    const CaptureResult& m_captureResult;
    QRect m_virtualGeometry;
};

#endif // CAPTUREIMAGECOMPOSER_H
