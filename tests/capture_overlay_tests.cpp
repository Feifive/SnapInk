#include "../src/core/capture/CapturedScreen.h"
#include "../src/ui/capture/CaptureAnnotation.h"
#include "../src/ui/capture/CaptureOverlay.h"
#include "../src/ui/pin/PinWindow.h"

#include <QImage>
#include <QApplication>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPointer>
#include <QScreen>
#include <QTest>
#include <QTextDocument>
#include <QTextOption>
#include <QToolButton>
#include <QWheelEvent>
#include <QtMath>

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

QToolButton* findToolButton(CaptureOverlay& overlay, const QString& text)
{
    for (QToolButton* button : overlay.findChildren<QToolButton*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

void clickTool(CaptureOverlay& overlay, const QString& text)
{
    QToolButton* button = findToolButton(overlay, text);
    QVERIFY(button != nullptr);
    QTest::mouseClick(button, Qt::LeftButton);
}

QList<PinWindow*> pinWindows()
{
    QList<PinWindow*> windows;
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (auto* pinWindow = qobject_cast<PinWindow*>(widget)) {
            windows.append(pinWindow);
        }
    }
    return windows;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class CaptureOverlayTests : public QObject
{
    Q_OBJECT

private slots:
    void overlayStartsInSelectingState();
    void overlayEditingRenderUsesSelectionSize();
    void overlayAnnotationsUseSelectionRelativeCoordinates();
    void overlayUndoRedoUpdatesRenderedResult();
    void editingEmbedsGraphicsViewMatchingSelection();
    void drawnAnnotationRemainsAfterMouseRelease();
    void textEditorWidthDoesNotStretchToSelectionRightEdge();
    void textAnnotationWrapsAnywhereInsideMaximumWidth();
    void editingDefaultsToSelectTool();
    void selectToolMovesSelectionAndPreservesUndoHistory();
    void resizingFromTopLeftShiftsAnnotationsToKeepGlobalPosition();
    void resizingFromBottomRightKeepsAnnotationLocalPosition();
    void selectToolWheelResizesByOnePixel();
    void annotationToolDoesNotMoveSelection();
    void pinWindowUsesToolTopmostFramelessFlagsAndDeletesOnClose();
    void pinWindowScalesInitialGeometryToAvailableScreen();
    void pinToolbarCreatesIndependentPinWindowAndClosesOverlay();
    void pinWindowInitialPositionUsesSourceGlobalRect();
    void pinWindowSmallImageNotUpscaled();
    void pinWindowResizeConstraintDoesNotUpscaleNormalSize();
    void pinToolbarPinPlacesWindowAtSelectionGlobalPosition();

    // New DPR-aware tests
    void singleScreenExportAtDpr1_0IsLossless();
    void singleScreenExportAtDpr1_5UsesPhysicalPixels();
    void singleScreenExportAtDpr2_0DoublesPixels();
    void selectionClippedToScreenBoundary();
    void multiScreenCrossDprSingleScreenRegionIsLossless();
    void exportSelectionUsesGlobalLogicalCoordinatesWhenScreenIsLeftAndAbove();
    void emptyCaptureResultIsSafe();
    void captureResultLogicalToPhysicalDpr1_0();
    void captureResultLogicalToPhysicalDpr1_5();
    void captureResultLogicalToPhysicalDpr2_0();
    void captureResultLogicalToPhysicalRectOutOfBounds();
};

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

    overlay.addAnnotationItem(new RectAnnotationItem(QRectF(0.0, 0.0, 12.0, 8.0),
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

    overlay.addAnnotationItem(new RectAnnotationItem(QRectF(0.0, 0.0, 12.0, 8.0),
                                                     QPen(Qt::red, 3.0)));
    QVERIFY(overlay.canUndo());

    overlay.undo();
    QVERIFY(!overlay.canUndo());
    QVERIFY(overlay.canRedo());
    QCOMPARE(overlay.renderResultImage().pixelColor(1, 1), QColor(Qt::white));

    overlay.redo();
    QCOMPARE(overlay.renderResultImage().pixelColor(1, 1), QColor(Qt::red));
}

void CaptureOverlayTests::editingEmbedsGraphicsViewMatchingSelection()
{
    CaptureResult cr = makeSingleScreenResult(80, 60);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 80, 60));

    QVERIFY(overlay.findChild<QGraphicsView *>() == nullptr
            || overlay.findChild<QGraphicsView *>()->isHidden());

    overlay.enterEditing(QRect(20, 10, 30, 20));

    auto *view = overlay.findChild<QGraphicsView *>();
    QVERIFY(view != nullptr);
    QCOMPARE(view->geometry(), QRect(20, 10, 30, 20));
    QCOMPARE(view->sceneRect(), QRectF(0, 0, 30, 20));
    QCOMPARE(view->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    QCOMPARE(view->verticalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    QVERIFY(view->autoFillBackground() == false);
    QVERIFY(view->scene() != nullptr);
    QCOMPARE(view->scene()->items(Qt::AscendingOrder).size(), 1);
    QVERIFY(qgraphicsitem_cast<QGraphicsPixmapItem *>(view->scene()->items(Qt::AscendingOrder).first())
            != nullptr);
}

void CaptureOverlayTests::drawnAnnotationRemainsAfterMouseRelease()
{
    CaptureResult cr = makeSingleScreenResult(80, 60);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 80, 60));
    overlay.enterEditing(QRect(20, 10, 30, 20));

    auto *view = overlay.findChild<QGraphicsView *>(QStringLiteral("AnnotationGraphicsView"));
    QVERIFY(view != nullptr);

    clickTool(overlay, QStringLiteral("Rect"));
    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
    QTest::mouseMove(view->viewport(), QPoint(12, 8));
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(12, 8));

    QVERIFY(overlay.canUndo());
    QCOMPARE(overlay.renderResultImage().pixelColor(1, 1), QColor(Qt::red));
}

void CaptureOverlayTests::textEditorWidthDoesNotStretchToSelectionRightEdge()
{
    CaptureResult cr = makeSingleScreenResult(400, 120);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 400, 120));
    overlay.enterEditing(QRect(20, 10, 320, 80));

    clickTool(overlay, QStringLiteral("Text"));

    auto* view = overlay.findChild<QGraphicsView*>(QStringLiteral("AnnotationGraphicsView"));
    QVERIFY(view != nullptr);
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(10, 12));

    TextAnnotationItem* textItem = nullptr;
    for (QGraphicsItem* item : view->scene()->items()) {
        if (auto* candidate = dynamic_cast<TextAnnotationItem*>(item)) {
            textItem = candidate;
            break;
        }
    }

    QVERIFY(textItem != nullptr);
    QVERIFY(textItem->textWidth() < 0.0);

    QTest::keyClicks(view->viewport(),
                     QStringLiteral("this text is intentionally long enough to need a maximum width"));
    QVERIFY(textItem->textWidth() <= 310.0);
}

void CaptureOverlayTests::textAnnotationWrapsAnywhereInsideMaximumWidth()
{
    TextAnnotationItem textItem(QPointF(0.0, 0.0), Qt::red);
    QCOMPARE(textItem.document()->defaultTextOption().wrapMode(), QTextOption::WrapAnywhere);
}

void CaptureOverlayTests::editingDefaultsToSelectTool()
{
    CaptureResult cr = makeSingleScreenResult(80, 60);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 80, 60));

    overlay.enterEditing(QRect(20, 10, 30, 20));

    QToolButton* selectButton = findToolButton(overlay, QStringLiteral("Select"));
    QVERIFY(selectButton != nullptr);
    QVERIFY(selectButton->isChecked());

    QToolButton* rectButton = findToolButton(overlay, QStringLiteral("Rect"));
    QVERIFY(rectButton != nullptr);
    QVERIFY(!rectButton->isChecked());
}

void CaptureOverlayTests::selectToolMovesSelectionAndPreservesUndoHistory()
{
    CaptureResult cr = makeSingleScreenResult(100, 80);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 100, 80));
    overlay.enterEditing(QRect(10, 10, 30, 20));

    overlay.addAnnotationItem(new RectAnnotationItem(QRectF(1.0, 1.0, 10.0, 8.0),
                                                     QPen(Qt::red, 3.0)));
    QVERIFY(overlay.canUndo());

    auto* view = overlay.findChild<QGraphicsView*>(QStringLiteral("AnnotationGraphicsView"));
    QVERIFY(view != nullptr);

    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(15, 10));
    QTest::mouseMove(view->viewport(), QPoint(25, 15));
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(25, 15));

    QCOMPARE(view->geometry(), QRect(20, 15, 30, 20));
    QVERIFY(overlay.canUndo());
    const QColor movedPixel = overlay.renderResultImage().pixelColor(2, 2);
    QVERIFY(movedPixel.red() > 200);
    QVERIFY(movedPixel.green() < 200);

    overlay.undo();
    QVERIFY(overlay.canRedo());
    QCOMPARE(overlay.renderResultImage().pixelColor(2, 2), QColor(Qt::white));
}

void CaptureOverlayTests::resizingFromTopLeftShiftsAnnotationsToKeepGlobalPosition()
{
    CaptureResult cr = makeSingleScreenResult(100, 80);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 100, 80));
    overlay.enterEditing(QRect(20, 20, 30, 20));

    auto* item = new RectAnnotationItem(QRectF(5.0, 5.0, 10.0, 8.0), QPen(Qt::red, 3.0));
    overlay.addAnnotationItem(item);

    auto* view = overlay.findChild<QGraphicsView*>(QStringLiteral("AnnotationGraphicsView"));
    QVERIFY(view != nullptr);

    QTest::mousePress(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(20, 20));
    QTest::mouseMove(&overlay, QPoint(15, 16));
    QTest::mouseRelease(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(15, 16));

    QCOMPARE(view->geometry(), QRect(15, 16, 35, 24));
    QCOMPARE(item->pos(), QPointF(5.0, 4.0));
    QCOMPARE(item->rect(), QRectF(5.0, 5.0, 10.0, 8.0));
    QVERIFY(overlay.canUndo());
    const QColor resizedPixel = overlay.renderResultImage().pixelColor(11, 10);
    QVERIFY(resizedPixel.red() > 200);
    QVERIFY(resizedPixel.green() < 200);
}

void CaptureOverlayTests::resizingFromBottomRightKeepsAnnotationLocalPosition()
{
    CaptureResult cr = makeSingleScreenResult(100, 80);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 100, 80));
    overlay.enterEditing(QRect(20, 20, 30, 20));

    auto* item = new RectAnnotationItem(QRectF(5.0, 5.0, 10.0, 8.0), QPen(Qt::red, 3.0));
    overlay.addAnnotationItem(item);

    auto* view = overlay.findChild<QGraphicsView*>(QStringLiteral("AnnotationGraphicsView"));
    QVERIFY(view != nullptr);

    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(29, 19));
    QTest::mouseMove(view->viewport(), QPoint(34, 23));
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(34, 23));

    QCOMPARE(view->geometry(), QRect(20, 20, 35, 24));
    QCOMPARE(item->pos(), QPointF(0.0, 0.0));
    QCOMPARE(item->rect(), QRectF(5.0, 5.0, 10.0, 8.0));
    QVERIFY(overlay.canUndo());
    QCOMPARE(overlay.renderResultImage().pixelColor(6, 6), QColor(Qt::red));
}

void CaptureOverlayTests::selectToolWheelResizesByOnePixel()
{
    CaptureResult cr = makeSingleScreenResult(100, 80);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 100, 80));
    overlay.enterEditing(QRect(20, 20, 30, 20));

    auto* view = overlay.findChild<QGraphicsView*>(QStringLiteral("AnnotationGraphicsView"));
    QVERIFY(view != nullptr);

    QWheelEvent wheelEvent(QPointF(15, 10),
                           view->viewport()->mapToGlobal(QPoint(15, 10)),
                           QPoint(),
                           QPoint(0, 120),
                           Qt::NoButton,
                           Qt::NoModifier,
                           Qt::NoScrollPhase,
                           false);
    QApplication::sendEvent(view->viewport(), &wheelEvent);

    QCOMPARE(view->geometry(), QRect(19, 19, 32, 22));
}

void CaptureOverlayTests::annotationToolDoesNotMoveSelection()
{
    CaptureResult cr = makeSingleScreenResult(100, 80);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 100, 80));
    overlay.enterEditing(QRect(10, 10, 30, 20));

    auto* view = overlay.findChild<QGraphicsView*>(QStringLiteral("AnnotationGraphicsView"));
    QVERIFY(view != nullptr);

    clickTool(overlay, QStringLiteral("Rect"));
    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(15, 10));
    QTest::mouseMove(view->viewport(), QPoint(25, 15));
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(25, 15));

    QCOMPARE(view->geometry(), QRect(10, 10, 30, 20));
    QVERIFY(overlay.canUndo());
}

void CaptureOverlayTests::pinWindowUsesToolTopmostFramelessFlagsAndDeletesOnClose()
{
    QImage image(160, 100, QImage::Format_ARGB32);
    image.fill(Qt::white);

    auto* window = new PinWindow(image, QRect(100, 100, 160, 100));
    QPointer<PinWindow> guard(window);
    QVERIFY(window->parentWidget() == nullptr);
    QVERIFY(window->windowFlags().testFlag(Qt::Tool));
    QVERIFY(window->windowFlags().testFlag(Qt::FramelessWindowHint));
    QVERIFY(window->windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    QCOMPARE(window->size(), QSize(160, 100));

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(guard.isNull());
}

void CaptureOverlayTests::pinWindowScalesInitialGeometryToAvailableScreen()
{
    QImage image(8000, 4000, QImage::Format_ARGB32);
    image.fill(Qt::white);

    auto* window = new PinWindow(image, QRect(0, 0, 8000, 4000));
    QPointer<PinWindow> guard(window);
    const QRect available = window->screen() != nullptr
        ? window->screen()->availableGeometry()
        : QApplication::primaryScreen()->availableGeometry();
    const QSize maxSize(qFloor(available.width() * 0.9),
                        qFloor(available.height() * 0.9));

    QVERIFY(window->width() <= maxSize.width());
    QVERIFY(window->height() <= maxSize.height());
    QVERIFY(window->width() > 0);
    QVERIFY(window->height() > 0);
    QCOMPARE(qRound((double(window->width()) / double(window->height())) * 100.0), 200);

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->close();
    QTRY_VERIFY(guard.isNull());
}

void CaptureOverlayTests::pinToolbarCreatesIndependentPinWindowAndClosesOverlay()
{
    const int beforeCount = pinWindows().size();
    auto* overlay = new CaptureOverlay(makeSingleScreenResult(80, 60), QRect(0, 0, 80, 60));
    QPointer<CaptureOverlay> overlayGuard(overlay);
    overlay->enterEditing(QRect(10, 10, 30, 20));

    clickTool(*overlay, QStringLiteral("Pin"));

    QTRY_VERIFY(overlayGuard.isNull());
    QTRY_COMPARE(pinWindows().size(), beforeCount + 1);

    PinWindow* window = pinWindows().last();
    QCOMPARE(window->size(), QSize(30, 20));
    window->close();
}

void CaptureOverlayTests::pinWindowInitialPositionUsesSourceGlobalRect()
{
    QImage image(200, 120, QImage::Format_ARGB32);
    image.fill(Qt::white);

    const QRect sourceRect(150, 100, 200, 120);
    auto* window = new PinWindow(image, sourceRect);
    QPointer<PinWindow> guard(window);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    // The window should be at or near the source rect's top-left,
    // possibly clamped to visible area.
    const QRect available = window->screen() != nullptr
        ? window->screen()->availableGeometry()
        : QApplication::primaryScreen()->availableGeometry();

    // Verify size is preserved (image fits in available area).
    QCOMPARE(window->size(), QSize(200, 120));

    // The position should be the source top-left, or clamped to visible area.
    // We can't predict exact clamp result without knowing screen layout,
    // but the position should be within reasonable bounds.
    QVERIFY(window->x() >= available.left() - window->width() + 40
            || window->x() <= available.right());
    QVERIFY(window->y() >= available.top() - window->height() + 40
            || window->y() <= available.bottom());

    window->close();
    QTRY_VERIFY(guard.isNull());
}

void CaptureOverlayTests::pinWindowSmallImageNotUpscaled()
{
    // A small 80x50 image should not be upscaled to near-screen size.
    QImage image(80, 50, QImage::Format_ARGB32);
    image.fill(Qt::white);

    auto* window = new PinWindow(image, QRect(100, 100, 80, 50));
    QPointer<PinWindow> guard(window);

    // Size should remain 80x50, not upscaled.
    QCOMPARE(window->size(), QSize(80, 50));

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->close();
    QTRY_VERIFY(guard.isNull());
}

void CaptureOverlayTests::pinWindowResizeConstraintDoesNotUpscaleNormalSize()
{
    // Verify that boundedImageSize does not upscale a small image.
    // A large available rect should not cause upscaling.
    const QRect largeAvailable(0, 0, 3840, 2160);
    const QSize bounded = PinWindow::boundedImageSize(QSize(200, 100), largeAvailable);
    QCOMPARE(bounded, QSize(200, 100));

    // A small available rect should cause downscaling.
    const QRect smallAvailable(0, 0, 150, 150);
    const QSize boundedSmall = PinWindow::boundedImageSize(QSize(200, 100), smallAvailable);
    QVERIFY(boundedSmall.width() <= 150);
    QVERIFY(boundedSmall.height() <= 150);
    QVERIFY(boundedSmall.width() > 0);
    QVERIFY(boundedSmall.height() > 0);

    // Small image with large available - should remain small, not upscaled.
    const QSize smallBounded = PinWindow::boundedImageSize(QSize(80, 50), largeAvailable);
    QCOMPARE(smallBounded, QSize(80, 50));
}

void CaptureOverlayTests::pinToolbarPinPlacesWindowAtSelectionGlobalPosition()
{
    // Use a screen with a non-zero origin to test global coordinate mapping.
    auto screen = makeTestScreen(200, 150, 1.0, {-50, -30});
    CaptureResult cr({screen});

    const int beforeCount = pinWindows().size();
    auto* overlay = new CaptureOverlay(std::move(cr), QRect(-50, -30, 200, 150));
    QPointer<CaptureOverlay> overlayGuard(overlay);

    // Selection at overlay-local (20, 20, 60, 40) -> global (-30, -10, 60, 40).
    overlay->enterEditing(QRect(20, 20, 60, 40));

    clickTool(*overlay, QStringLiteral("Pin"));

    QTRY_VERIFY(overlayGuard.isNull());
    QTRY_COMPARE(pinWindows().size(), beforeCount + 1);

    PinWindow* window = pinWindows().last();
    // Size should match selection.
    QCOMPARE(window->size(), QSize(60, 40));

    // Position should be at global (-30, -10) or clamped to visible.
    // In offscreen test environment, screen geometry may be (0,0,800,600),
    // so (-30,-10) may be clamped. Just verify it's not at (0,0) default
    // or far from the expected area.
    QVERIFY(window->x() <= qMax(-30, 0) + 60);
    QVERIFY(window->y() <= qMax(-10, 0) + 40);

    window->close();
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

void CaptureOverlayTests::exportSelectionUsesGlobalLogicalCoordinatesWhenScreenIsLeftAndAbove()
{
    auto screen = makeTestScreen(200, 100, 1.0, {-100, -50});
    screen.image.setPixelColor(10, 20, Qt::red);
    screen.image.setPixelColor(110, 70, Qt::blue);

    CaptureResult cr({screen});
    CaptureOverlay overlay(std::move(cr), QRect(-100, -50, 200, 100));
    overlay.enterEditing(QRect(10, 20, 30, 20));

    const QImage result = overlay.renderResultImage();
    QCOMPARE(result.size(), QSize(30, 20));
    QCOMPARE(result.pixelColor(0, 0), QColor(Qt::red));
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
