#include "core/capture/CaptureImageComposer.h"

#include <QColor>
#include <QImage>
#include <QRect>
#include <QTest>
#include <QtMath>

namespace
{
CapturedScreen makeTestScreen(const QRect& logicalGeometry,
                              qreal dpr,
                              const QColor& fillColor)
{
    const int physW = qMax(1, qRound(logicalGeometry.width() * dpr));
    const int physH = qMax(1, qRound(logicalGeometry.height() * dpr));

    QImage image(physW, physH, QImage::Format_ARGB32);
    image.fill(fillColor);
    image.setDevicePixelRatio(dpr);

    CapturedScreen screen;
    screen.logicalGeometry = logicalGeometry;
    screen.image = std::move(image);
    screen.devicePixelRatio = dpr;
    return screen;
}
}

class CaptureImageComposerTests : public QObject
{
    Q_OBJECT

private slots:
    void singleScreenDpr1ComposesSelectionAtLogicalSize();
    void singleScreenDpr15UsesPhysicalSizeAndDpr();
    void singleScreenDpr2UsesPhysicalSizeAndDpr();
    void selectionAtScreenBoundaryComposesLastPixels();
    void horizontalScreensComposeCrossScreenSelection();
    void negativeVirtualOriginComposesExpectedGlobalRegion();
    void mixedDprSelectionUsesMaximumIntersectingDpr();
    void mixedDprCrossScreenSelectionHasNoBoundarySeam();
    void emptyCaptureResultProducesTransparentImage();
};

void CaptureImageComposerTests::singleScreenDpr1ComposesSelectionAtLogicalSize()
{
    const CapturedScreen screen = makeTestScreen(QRect(0, 0, 100, 80), 1.0, Qt::red);
    const CaptureResult captureResult({screen});
    const CaptureImageComposer composer(captureResult, QRect(0, 0, 100, 80));

    const QImage image = composer.composeSelectionImage(QRect(10, 20, 30, 25));

    QCOMPARE(image.size(), QSize(30, 25));
    QCOMPARE(image.devicePixelRatio(), 1.0);
    QCOMPARE(image.pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(image.pixelColor(29, 24), QColor(Qt::red));
}

void CaptureImageComposerTests::singleScreenDpr15UsesPhysicalSizeAndDpr()
{
    constexpr qreal dpr = 1.5;
    CapturedScreen screen = makeTestScreen(QRect(0, 0, 200, 150), dpr, Qt::blue);
    screen.image.setPixelColor(qRound(20 * dpr), qRound(10 * dpr), Qt::green);

    const CaptureResult captureResult({screen});
    const CaptureImageComposer composer(captureResult, QRect(0, 0, 200, 150));

    const QImage image = composer.composeSelectionImage(QRect(20, 10, 40, 30));

    QCOMPARE(image.size(), QSize(60, 45));
    QCOMPARE(image.devicePixelRatio(), dpr);
    QCOMPARE(image.pixelColor(0, 0), QColor(Qt::green));
    QCOMPARE(image.pixelColor(59, 44), QColor(Qt::blue));
}

void CaptureImageComposerTests::singleScreenDpr2UsesPhysicalSizeAndDpr()
{
    constexpr qreal dpr = 2.0;
    const CapturedScreen screen = makeTestScreen(QRect(0, 0, 100, 80), dpr, Qt::yellow);
    const CaptureResult captureResult({screen});
    const CaptureImageComposer composer(captureResult, QRect(0, 0, 100, 80));

    const QImage image = composer.composeSelectionImage(QRect(10, 10, 30, 20));

    QCOMPARE(image.size(), QSize(60, 40));
    QCOMPARE(image.devicePixelRatio(), dpr);
    QCOMPARE(image.pixelColor(0, 0), QColor(Qt::yellow));
    QCOMPARE(image.pixelColor(59, 39), QColor(Qt::yellow));
}

void CaptureImageComposerTests::selectionAtScreenBoundaryComposesLastPixels()
{
    CapturedScreen screen = makeTestScreen(QRect(0, 0, 100, 80), 1.0, Qt::white);
    screen.image.setPixelColor(99, 79, Qt::magenta);

    const CaptureResult captureResult({screen});
    const CaptureImageComposer composer(captureResult, QRect(0, 0, 100, 80));

    const QImage image = composer.composeSelectionImage(QRect(95, 75, 5, 5));

    QCOMPARE(image.size(), QSize(5, 5));
    QCOMPARE(image.pixelColor(4, 4), QColor(Qt::magenta));
}

void CaptureImageComposerTests::horizontalScreensComposeCrossScreenSelection()
{
    const CapturedScreen left = makeTestScreen(QRect(0, 0, 100, 50), 1.0, Qt::red);
    const CapturedScreen right = makeTestScreen(QRect(100, 0, 100, 50), 1.0, Qt::blue);
    const CaptureResult captureResult({left, right});
    const CaptureImageComposer composer(captureResult, QRect(0, 0, 200, 50));

    const QImage image = composer.composeSelectionImage(QRect(80, 10, 40, 20));

    QCOMPARE(image.size(), QSize(40, 20));
    QCOMPARE(image.devicePixelRatio(), 1.0);
    QCOMPARE(image.pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(image.pixelColor(19, 0), QColor(Qt::red));
    QCOMPARE(image.pixelColor(20, 0), QColor(Qt::blue));
    QCOMPARE(image.pixelColor(39, 19), QColor(Qt::blue));
}

void CaptureImageComposerTests::negativeVirtualOriginComposesExpectedGlobalRegion()
{
    CapturedScreen screen = makeTestScreen(QRect(-100, -50, 200, 100), 1.0, Qt::yellow);
    screen.image.setPixelColor(10, 20, Qt::red);
    screen.image.setPixelColor(39, 39, Qt::blue);

    const CaptureResult captureResult({screen});
    const CaptureImageComposer composer(captureResult, QRect(-100, -50, 200, 100));

    QCOMPARE(composer.overlayLocalToGlobalLogical(QRect(10, 20, 30, 20)),
             QRect(-90, -30, 30, 20));

    const QImage image = composer.composeSelectionImage(QRect(10, 20, 30, 20));

    QCOMPARE(image.size(), QSize(30, 20));
    QCOMPARE(image.pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(image.pixelColor(29, 19), QColor(Qt::blue));
}

void CaptureImageComposerTests::mixedDprSelectionUsesMaximumIntersectingDpr()
{
    const CapturedScreen left = makeTestScreen(QRect(0, 0, 100, 50), 1.0, Qt::red);
    const CapturedScreen right = makeTestScreen(QRect(100, 0, 100, 50), 2.0, Qt::blue);
    const CaptureResult captureResult({left, right});
    const CaptureImageComposer composer(captureResult, QRect(0, 0, 200, 50));

    const QRect selection(80, 0, 40, 20);
    const QImage image = composer.composeSelectionImage(selection);

    QCOMPARE(composer.effectiveDevicePixelRatio(selection), 2.0);
    QCOMPARE(image.size(), QSize(80, 40));
    QCOMPARE(image.devicePixelRatio(), 2.0);
    QCOMPARE(image.pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(image.pixelColor(39, 10), QColor(Qt::red));
    QCOMPARE(image.pixelColor(40, 10), QColor(Qt::blue));
    QCOMPARE(image.pixelColor(79, 39), QColor(Qt::blue));
}

void CaptureImageComposerTests::mixedDprCrossScreenSelectionHasNoBoundarySeam()
{
    const CapturedScreen left = makeTestScreen(QRect(0, 0, 100, 50), 1.0, Qt::red);
    const CapturedScreen right = makeTestScreen(QRect(100, 0, 100, 50), 2.0, Qt::blue);
    const CaptureResult captureResult({left, right});
    const CaptureImageComposer composer(captureResult, QRect(0, 0, 200, 50));

    const QImage image = composer.composeSelectionImage(QRect(80, 0, 40, 20));

    QCOMPARE(image.devicePixelRatio(), 2.0);
    QCOMPARE(image.size(), QSize(80, 40));

    for (int y = 0; y < image.height(); ++y) {
        QCOMPARE(image.pixelColor(38, y), QColor(Qt::red));
        QCOMPARE(image.pixelColor(39, y), QColor(Qt::red));
        QCOMPARE(image.pixelColor(40, y), QColor(Qt::blue));
        QCOMPARE(image.pixelColor(41, y), QColor(Qt::blue));
        QCOMPARE(image.pixelColor(39, y).alpha(), 255);
        QCOMPARE(image.pixelColor(40, y).alpha(), 255);
    }
}

void CaptureImageComposerTests::emptyCaptureResultProducesTransparentImage()
{
    const CaptureResult captureResult;
    const CaptureImageComposer composer(captureResult, QRect(0, 0, 100, 80));
    const QRect selection(10, 10, 20, 15);

    const QImage canvas = composer.createTransparentCanvas(selection);
    QCOMPARE(composer.effectiveDevicePixelRatio(selection), 1.0);
    QCOMPARE(canvas.size(), QSize(20, 15));
    QCOMPARE(canvas.devicePixelRatio(), 1.0);
    QCOMPARE(canvas.pixelColor(0, 0).alpha(), 0);

    const QImage image = composer.composeSelectionImage(selection);
    QCOMPARE(image.size(), QSize(20, 15));
    QCOMPARE(image.devicePixelRatio(), 1.0);
    QCOMPARE(image.pixelColor(19, 14).alpha(), 0);

    QVERIFY(composer.composeSelectionImage(QRect()).isNull());
}

QTEST_MAIN(CaptureImageComposerTests)

#include "capture_image_composer_tests.moc"
