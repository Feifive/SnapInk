#ifndef SCREENCAPTURESERVICE_H
#define SCREENCAPTURESERVICE_H

#include "CapturedScreen.h"

#include <QRect>

class ScreenCaptureService
{
public:
    /// Union of all QScreen::geometry() rects in logical (device-independent) pixels.
    static QRect virtualDesktopGeometry();

    /// Capture every screen independently, preserving each screen's original
    /// physical-pixel image and device-pixel-ratio.
    ///
    /// The returned CaptureResult is suitable for both preview (overlay) and
    /// lossless export.
    static CaptureResult captureScreens();
};

#endif // SCREENCAPTURESERVICE_H
