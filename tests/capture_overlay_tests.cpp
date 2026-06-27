#include "../src/core/capture/CapturedScreen.h"
#include "../src/ui/capture/CaptureAnnotation.h"
#include "../src/ui/capture/CaptureOverlay.h"

#include <QImage>
#include <QTest>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Create a single CapturedScreen filled with white at the given DPR.
CapturedScreen makeTestScreen(int logicalW, int logicalH, qreal dpr = 1.0, QPoint origin = {0, 0})
{
    const int physW = qRound(logicalW * dpr);
    const int physH = qRound(logicalH * dpr);

    QImage img(physW, physH, QImage::Format_ARGB32);
    img.fill(Qt::white);
    img.setDevicePixelRatio(dpr);

    CapturedScreen s;
    s.logicalGeometry = QRect(origin, QSize(logicalW, logicalH));
    s.image = std::move(img);
    s.devicePixelRatio = dpr;
    return s;
}

/// Create a single-screen CaptureResult.
CaptureResult makeSingleScreenResult(int logicalW, int logicalH, qreal dpr = 1.0)
{
    return CaptureResult({makeTestScreen(logicalW, logicalH, dpr)});
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class CaptureOverlayTests : public QObject
{
    Q_OBJECT

private slots:
    // Existing tests (updated for new constructor)
    void overlayStartsInSelectingState();
    void overlayEditingRenderUsesSelectionSize();
    void overlayAnnotationsUseSelectionRelativeCoordinates();
    void overlayUndoRedoUpdatesRenderedResult();

    // New DPR-aware tests
    void singleScreenExportAtDpr1_0IsLossless();
    void singleScreenExportAtDpr1_5UsesPhysicalPixels();
    void singleScreenExportAtDpr2_0DoublesPixels();
    void selectionClippedToScreenBoundary();
    void multiScreenCrossDprSingleScreenRegionIsLossless();
    void emptyCaptureResultIsSafe();
    void captureResultLogicalToPhysicalDpr1_0();
    void captureResultLogicalToPhysicalDpr1_5();
    void captureResultLogicalToPhysicalDpr2_0();
    void captureResultLogicalToPhysicalRectOutOfBounds();
};

// ---------------------------------------------------------------------------
// Existing tests (updated)
// ---------------------------------------------------------------------------

void CaptureOverlayTests::overlayStartsInSelectingState()
{
    CaptureResult cr = makeSingleScreenResult(80, 60);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 80, 60));

    QCOMPARE(overlay.state(), CaptureState::Selecting);
    QVERIFY(overlay.renderResultImage().isNull());
}

void CaptureOverlayTests::overlayEditingRenderUsesSelectionSize()
{
    CaptureResult cr = makeSingleScreenResult(80, 60);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 80, 60));

    overlay.enterEditing(QRect(10, 12, 30, 20));
    const QImage result = overlay.renderResultImage();

    QCOMPARE(overlay.state(), CaptureState::Editing);
    // At DPR 1.0, output size equals logical selection size.
    QCOMPARE(result.size(), QSize(30, 20));
}

void CaptureOverlayTests::overlayAnnotationsUseSelectionRelativeCoordinates()
{
    CaptureResult cr = makeSingleScreenResult(80, 60);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 80, 60));
    overlay.enterEditing(QRect(20, 10, 30, 20));

    overlay.addAnnotation(std::make_unique<RectAnnotation>(QRectF(0.0, 0.0, 12.0, 8.0),
                                                           QPen(Qt::red, 3.0)));
    const QImage result = overlay.renderResultImage();

    QCOMPARE(result.pixelColor(1, 1), QColor(Qt::red));
    QCOMPARE(result.pixelColor(14, 10), QColor(Qt::white));
}

void CaptureOverlayTests::overlayUndoRedoUpdatesRenderedResult()
{
    CaptureResult cr = makeSingleScreenResult(80, 60);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 80, 60));
    overlay.enterEditing(QRect(0, 0, 30, 20));

    overlay.addAnnotation(std::make_unique<RectAnnotation>(QRectF(0.0, 0.0, 12.0, 8.0),
                                                           QPen(Qt::red, 3.0)));
    QVERIFY(overlay.canUndo());

    overlay.undo();
    QVERIFY(!overlay.canUndo());
    QVERIFY(overlay.canRedo());
    QCOMPARE(overlay.renderResultImage().pixelColor(1, 1), QColor(Qt::white));

    overlay.redo();
    QCOMPARE(overlay.renderResultImage().pixelColor(1, 1), QColor(Qt::red));
}

// ---------------------------------------------------------------------------
// DPR-aware export tests
// ---------------------------------------------------------------------------

void CaptureOverlayTests::singleScreenExportAtDpr1_0IsLossless()
{
    // DPR 1.0: output size = logical selection size, no scaling.
    CaptureResult cr = makeSingleScreenResult(200, 150, 1.0);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 200, 150));
    overlay.enterEditing(QRect(0, 0, 200, 150));

    const QImage result = overlay.renderResultImage();
    QCOMPARE(result.size(), QSize(200, 150));
    QCOMPARE(result.devicePixelRatio(), 1.0);
}

void CaptureOverlayTests::singleScreenExportAtDpr1_5UsesPhysicalPixels()
{
    // DPR 1.5: logical 200x150 → physical 300x225.
    constexpr qreal dpr = 1.5;
    constexpr int logicalW = 200;
    constexpr int logicalH = 150;

    // Create a screen image with a distinctive pattern in physical pixels.
    const int physW = qRound(logicalW * dpr);  // 300
    const int physH = qRound(logicalH * dpr);  // 225
    QImage img(physW, physH, QImage::Format_ARGB32);
    img.fill(Qt::white);

    // Paint a red dot in the physical image to verify 1:1 passthrough.
    const int redX = qRound(100.0 * dpr);  // 150
    const int redY = qRound(50.0 * dpr);   // 75
    img.setPixelColor(redX, redY, Qt::red);

    CapturedScreen s;
    s.logicalGeometry = QRect(0, 0, logicalW, logicalH);
    s.image = img;
    s.devicePixelRatio = dpr;

    CaptureResult cr({s});
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, logicalW, logicalH));
    overlay.enterEditing(QRect(0, 0, logicalW, logicalH));

    const QImage result = overlay.renderResultImage();
    QCOMPARE(result.size(), QSize(physW, physH));
    QCOMPARE(result.devicePixelRatio(), dpr);

    // The red dot at logical (100, 50) should appear at physical (150, 75).
    QCOMPARE(result.pixelColor(redX, redY), QColor(Qt::red));
    // A point not painted should be white.
    QCOMPARE(result.pixelColor(0, 0), QColor(Qt::white));
}

void CaptureOverlayTests::singleScreenExportAtDpr2_0DoublesPixels()
{
    // DPR 2.0: logical 100x80 → physical 200x160.
    constexpr qreal dpr = 2.0;
    constexpr int logicalW = 100;
    constexpr int logicalH = 80;

    const int physW = qRound(logicalW * dpr);
    const int physH = qRound(logicalH * dpr);

    QImage img(physW, physH, QImage::Format_ARGB32);
    img.fill(Qt::white);
    img.setPixelColor(0, 0, Qt::blue);

    CapturedScreen s;
    s.logicalGeometry = QRect(0, 0, logicalW, logicalH);
    s.image = img;
    s.devicePixelRatio = dpr;

    CaptureResult cr({s});
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, logicalW, logicalH));
    overlay.enterEditing(QRect(10, 10, 30, 20));

    const QImage result = overlay.renderResultImage();
    QCOMPARE(result.size(), QSize(60, 40));   // 30*2 x 20*2
    QCOMPARE(result.devicePixelRatio(), 2.0);
}

void CaptureOverlayTests::selectionClippedToScreenBoundary()
{
    // Selection partially outside screen — export should only include the
    // intersecting physical region.
    CaptureResult cr = makeSingleScreenResult(100, 80);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 100, 80));

    // Selection extends beyond the right and bottom edges.
    overlay.enterEditing(QRect(90, 70, 30, 30));
    // After intersection: (90, 70, 10, 10).

    const QImage result = overlay.renderResultImage();
    QCOMPARE(result.size(), QSize(10, 10));
}

void CaptureOverlayTests::multiScreenCrossDprSingleScreenRegionIsLossless()
{
    // Two screens side-by-side with different DPRs.
    // Left:  DPR 1.0, 100x100 logical
    // Right: DPR 2.0, 100x100 logical
    //
    // Select only the left screen → effective DPR = 1.0, zero scaling.
    auto left = makeTestScreen(100, 100, 1.0, {0, 0});
    auto right = makeTestScreen(100, 100, 2.0, {100, 0});

    // Paint a distinctive pixel in the left screen's physical image.
    left.image.setPixelColor(50, 50, Qt::green);

    CaptureResult cr({left, right});
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 200, 100));
    overlay.enterEditing(QRect(0, 0, 100, 100));   // left screen only

    const QImage result = overlay.renderResultImage();
    // Effective DPR = 1.0 (only left screen intersects).
    QCOMPARE(result.size(), QSize(100, 100));
    QCOMPARE(result.devicePixelRatio(), 1.0);
    QCOMPARE(result.pixelColor(50, 50), QColor(Qt::green));
}

void CaptureOverlayTests::emptyCaptureResultIsSafe()
{
    CaptureResult cr;   // no screens
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 800, 600));

    QCOMPARE(overlay.state(), CaptureState::Selecting);
    QVERIFY(overlay.renderResultImage().isNull());

    // enterEditing with valid rect should not crash.
    overlay.enterEditing(QRect(0, 0, 100, 100));
    // State should still be Selecting because no image bounds intersect.
    QCOMPARE(overlay.state(), CaptureState::Selecting);
}

// ---------------------------------------------------------------------------
// CaptureResult coordinate conversion tests
// ---------------------------------------------------------------------------

void CaptureOverlayTests::captureResultLogicalToPhysicalDpr1_0()
{
    auto screen = makeTestScreen(200, 100, 1.0);
    CaptureResult cr({screen});

    // Point conversion.
    const QPoint physPoint = cr.logicalToPhysical(screen, QPoint(50, 30));
    QCOMPARE(physPoint, QPoint(50, 30));

    // Rect conversion.
    const QRect physRect = cr.logicalToPhysical(screen, QRect(20, 10, 100, 80));
    QCOMPARE(physRect, QRect(20, 10, 100, 80));
}

void CaptureOverlayTests::captureResultLogicalToPhysicalDpr1_5()
{
    auto screen = makeTestScreen(200, 100, 1.5);
    CaptureResult cr({screen});

    // Point conversion.
    const QPoint physPoint = cr.logicalToPhysical(screen, QPoint(100, 50));
    QCOMPARE(physPoint, QPoint(150, 75));

    // Rect conversion: logical (100, 50, 20, 10) → physical (150, 75, 30, 15).
    const QRect physRect = cr.logicalToPhysical(screen, QRect(100, 50, 20, 10));
    QCOMPARE(physRect, QRect(150, 75, 30, 15));
}

void CaptureOverlayTests::captureResultLogicalToPhysicalDpr2_0()
{
    auto screen = makeTestScreen(100, 80, 2.0);
    CaptureResult cr({screen});

    // Point conversion.
    const QPoint physPoint = cr.logicalToPhysical(screen, QPoint(10, 20));
    QCOMPARE(physPoint, QPoint(20, 40));

    // Rect conversion: logical (10, 10, 30, 20) → physical (20, 20, 60, 40).
    const QRect physRect = cr.logicalToPhysical(screen, QRect(10, 10, 30, 20));
    QCOMPARE(physRect, QRect(20, 20, 60, 40));
}

void CaptureOverlayTests::captureResultLogicalToPhysicalRectOutOfBounds()
{
    // Logical rect with no intersection → null physical rect.
    auto screen = makeTestScreen(200, 100, 1.0);
    CaptureResult cr({screen});

    const QRect physRect = cr.logicalToPhysical(screen, QRect(300, 300, 50, 50));
    QVERIFY(physRect.isEmpty());
}

// ---------------------------------------------------------------------------

QTEST_MAIN(CaptureOverlayTests)

#include "capture_overlay_tests.moc"
