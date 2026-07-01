#ifndef CAPTUREIMAGECOMPOSER_H
#define CAPTUREIMAGECOMPOSER_H

#include "CapturedScreen.h"

#include <QImage>
#include <QRect>

class CaptureImageComposer
{
public:
    CaptureImageComposer(const CaptureResult& captureResult,
                         const QRect& virtualGeometry);

    QRect overlayLocalToGlobalLogical(const QRect& overlayLocalRect) const;
    qreal effectiveDevicePixelRatio(const QRect& overlayLocalRect) const;

    QImage createTransparentCanvas(const QRect& overlayLocalRect) const;
    QImage composeSelectionImage(const QRect& overlayLocalRect) const;

private:
    QRect globalToOverlayLocalLogical(const QRect& globalLogicalRect) const;

    const CaptureResult& m_captureResult;
    QRect m_virtualGeometry;
};

#endif // CAPTUREIMAGECOMPOSER_H
