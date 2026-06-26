#ifndef SCREENCAPTURESERVICE_H
#define SCREENCAPTURESERVICE_H

#include <QImage>
#include <QRect>

class ScreenCaptureService
{
public:
    static QRect virtualDesktopGeometry();
    static QImage captureVirtualDesktop();
};

#endif // SCREENCAPTURESERVICE_H
