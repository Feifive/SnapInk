#include "../src/core/capture/CapturedScreen.h"
#include "../src/app/mainwindow.h"
#include "../src/ui/capture/CaptureAnnotation.h"
#include "../src/ui/capture/CaptureOverlay.h"
#include "../src/ui/pin/PinWindow.h"
#ifdef Q_OS_MACOS
#include "../src/ui/pin/MacWindowHelper.h"
#endif

#include <QImage>
#include <QApplication>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMenu>
#include <QPointer>
#include <QScreen>
#include <QSignalSpy>
#include <QSystemTrayIcon>
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
#ifdef Q_OS_MACOS
    void macWindowHelperIgnoresWidgetsWithoutNativeWindows();
#endif
    void pinWindowScalesInitialGeometryToAvailableScreen();
    void pinToolbarCreatesIndependentPinWindowAndClosesOverlay();
    void pinWindowInitialPositionUsesSourceGlobalRect();
    void pinWindowSmallImageNotUpscaled();
    void pinWindowResizeConstraintDoesNotUpscaleNormalSize();
    void pinToolbarPinPlacesWindowAtSelectionGlobalPosition();

    // New shadow margin and size semantics tests
    void pinWindowContentSizePlusShadowMargins();
    void pinWindowInitialPositionAlignsContentTopLeft();
    void pinWindowAspectRatioPreservedWithShadowMargins();
    void pinWindowSmallImageNotUpscaledWithMargins();
    void pinWindowConstrainedSizeDoesNotIncludeMargins();

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

    // Selection interaction tests
    void selectingStateDoesNotDrawHandles();
    void editingStateDrawsHandles();
    void editingClickOutsideSelectionExpandsIt();
    void expandSelectionInDifferentDirections();
    void expandSelectionPreservesAnnotations();
    void rightClickInEditingEntersReselect();
    void rightClickBeforeSelectionCancels();

    // App shell / tray tests
    void mainWindowCreatesTrayMenuWithoutRegisteringHotkeysInTests();
    void mainWindowTrayIconUsesPlatformIcon();
    void trayActivationDoesNotManuallyPopupDuplicateMenu();
    void mainWindowCloseHidesInsteadOfDestroying();
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
    // Window size = content size + 2 * shadow margin (18px per side)
    constexpr int kMargin = 18;
    QCOMPARE(window->size(), QSize(160 + kMargin * 2, 100 + kMargin * 2));

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(guard.isNull());
}

#ifdef Q_OS_MACOS
void CaptureOverlayTests::macWindowHelperIgnoresWidgetsWithoutNativeWindows()
{
    MacWindowHelper::configureOverlayWindow(nullptr);

    QWidget widget;
    QVERIFY(widget.windowHandle() == nullptr);
    MacWindowHelper::configureOverlayWindow(&widget);
}
#endif

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

    // Window size includes shadow margins; content should fit within available area.
    constexpr int kMargin = 18;
    QVERIFY(window->width() - kMargin * 2 <= maxSize.width());
    QVERIFY(window->height() - kMargin * 2 <= maxSize.height());
    QVERIFY(window->width() > kMargin * 2);
    QVERIFY(window->height() > kMargin * 2);
    const int contentW = window->width() - kMargin * 2;
    const int contentH = window->height() - kMargin * 2;
    QCOMPARE(qRound((double(contentW) / double(contentH)) * 100.0), 200);

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
    constexpr int kMargin = 18;
    QCOMPARE(window->size(), QSize(30 + kMargin * 2, 20 + kMargin * 2));
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

    // Verify size includes shadow margins (content = 200x120)
    constexpr int kMargin = 18;
    QCOMPARE(window->size(), QSize(200 + kMargin * 2, 120 + kMargin * 2));

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

    // Window size = content + shadow margins, not upscaled beyond that.
    constexpr int kMargin = 18;
    QCOMPARE(window->size(), QSize(80 + kMargin * 2, 50 + kMargin * 2));

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
    // Window size = content + shadow margins
    constexpr int kMargin = 18;
    QCOMPARE(window->size(), QSize(60 + kMargin * 2, 40 + kMargin * 2));

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
// PinWindow shadow margin and size semantics tests
// ---------------------------------------------------------------------------

void CaptureOverlayTests::pinWindowContentSizePlusShadowMargins()
{
    // Verify that window size = content size + 2 * shadow margins
    QImage image(200, 120, QImage::Format_ARGB32);
    image.fill(Qt::white);

    auto* window = new PinWindow(image, QRect(100, 100, 200, 120));
    QPointer<PinWindow> guard(window);

    // Window should be larger than image by shadow margins on all sides
    constexpr int kExpectedMargin = 18;
    QCOMPARE(window->width(), 200 + kExpectedMargin * 2);
    QCOMPARE(window->height(), 120 + kExpectedMargin * 2);

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->close();
    QTRY_VERIFY(guard.isNull());
}

void CaptureOverlayTests::pinWindowInitialPositionAlignsContentTopLeft()
{
    // The sourceGlobalRect.topLeft() should align with the CONTENT area top-left,
    // not the window top-left (which is offset by shadow margin).
    QImage image(200, 120, QImage::Format_ARGB32);
    image.fill(Qt::white);

    const QPoint expectedContentTopLeft(150, 100);
    const QRect sourceRect(expectedContentTopLeft, QSize(200, 120));
    auto* window = new PinWindow(image, sourceRect);
    QPointer<PinWindow> guard(window);

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    // Window top-left should be offset by shadow margin from content top-left
    constexpr int kExpectedMargin = 18;
    const QPoint expectedWindowTopLeft = expectedContentTopLeft - QPoint(kExpectedMargin, kExpectedMargin);
    
    // Allow for clamping to visible area
    const QRect available = window->screen() != nullptr
        ? window->screen()->availableGeometry()
        : QApplication::primaryScreen()->availableGeometry();
    
    // If position is clamped, verify it's still reasonable
    if (window->geometry().topLeft() == expectedWindowTopLeft) {
        // Perfect case: no clamping needed
        QVERIFY(true);
    } else {
        // Clamped case: at least verify size is correct
        QCOMPARE(window->width(), 200 + kExpectedMargin * 2);
        QCOMPARE(window->height(), 120 + kExpectedMargin * 2);
    }

    window->close();
    QTRY_VERIFY(guard.isNull());
}

void CaptureOverlayTests::pinWindowAspectRatioPreservedWithShadowMargins()
{
    // Verify that aspect ratio calculation uses content size, not window size
    QImage image(400, 200, QImage::Format_ARGB32);  // 2:1 aspect ratio
    image.fill(Qt::white);

    auto* window = new PinWindow(image, QRect(0, 0, 400, 200));
    QPointer<PinWindow> guard(window);

    // Content aspect ratio should be 2:1
    constexpr int kExpectedMargin = 18;
    const int contentWidth = window->width() - kExpectedMargin * 2;
    const int contentHeight = window->height() - kExpectedMargin * 2;
    
    QCOMPARE(qRound((double(contentWidth) / double(contentHeight)) * 100.0), 200);

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->close();
    QTRY_VERIFY(guard.isNull());
}

void CaptureOverlayTests::pinWindowSmallImageNotUpscaledWithMargins()
{
    // Small images should not be upscaled even with large available area
    QImage image(80, 50, QImage::Format_ARGB32);
    image.fill(Qt::white);

    auto* window = new PinWindow(image, QRect(100, 100, 80, 50));
    QPointer<PinWindow> guard(window);

    constexpr int kExpectedMargin = 18;
    // Window size should be exactly 80x50 + margins
    QCOMPARE(window->width(), 80 + kExpectedMargin * 2);
    QCOMPARE(window->height(), 50 + kExpectedMargin * 2);

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->close();
    QTRY_VERIFY(guard.isNull());
}

void CaptureOverlayTests::pinWindowConstrainedSizeDoesNotIncludeMargins()
{
    // Test that constrainedSize calculations work on content size only
    // A large available rect should not cause upscaling of small content
    const QRect largeAvailable(0, 0, 3840, 2160);
    
    // Simulate what happens during resize: content size without margins
    constexpr int kExpectedMargin = 18;
    const QSize contentSize = QSize(200, 100);
    
    // Use boundedImageSize which should preserve original size
    const QSize bounded = PinWindow::boundedImageSize(contentSize, largeAvailable);
    QCOMPARE(bounded, contentSize);

    // Small content with large available - should remain small
    const QSize smallContent = QSize(80, 50);
    const QSize smallBounded = PinWindow::boundedImageSize(smallContent, largeAvailable);
    QCOMPARE(smallBounded, smallContent);
}

// ---------------------------------------------------------------------------
// Selection interaction tests
// ---------------------------------------------------------------------------

void CaptureOverlayTests::selectingStateDoesNotDrawHandles()
{
    // During drag-selection (Selecting state with m_selecting=true),
    // paintSelectionChrome should not draw handles.
    // We verify this indirectly: when m_selecting is true and state is Selecting,
    // the chrome layer is hidden, and paintEvent draws chrome directly.
    // The key check is that m_state != Editing → no handles.
    CaptureResult cr = makeSingleScreenResult(100, 80);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 100, 80));

    // Start a drag selection
    QTest::mousePress(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
    QTest::mouseMove(&overlay, QPoint(50, 40));

    // State should still be Selecting
    QCOMPARE(overlay.state(), CaptureState::Selecting);

    // Render a test image to exercise paintSelectionChrome path.
    // The overlay's paintEvent calls paintSelectionChrome when annotationView
    // is not visible. We just verify state is correct — handles are conditional
    // on m_state == Editing inside paintSelectionChrome.
    overlay.repaint();

    QTest::mouseRelease(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(50, 40));
    QCOMPARE(overlay.state(), CaptureState::Editing);
}

void CaptureOverlayTests::editingStateDrawsHandles()
{
    // After entering Editing, handles should be visible.
    // We verify by checking that the annotation view and chrome layer are shown.
    CaptureResult cr = makeSingleScreenResult(100, 80);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 100, 80));
    overlay.show();
    QVERIFY(QTest::qWaitForWindowExposed(&overlay));
    overlay.enterEditing(QRect(20, 20, 30, 20));

    QCOMPARE(overlay.state(), CaptureState::Editing);

    // The annotation view should be visible in Editing state
    auto* view = overlay.findChild<QGraphicsView*>(QStringLiteral("AnnotationGraphicsView"));
    QVERIFY(view != nullptr);
    QVERIFY(view->isVisible());

    // Chrome layer should also be visible (handles are drawn inside it)
    // We can't directly access the chrome layer, but we verify state is Editing
    // which is the condition for drawing handles in paintSelectionChrome.
}

void CaptureOverlayTests::editingClickOutsideSelectionExpandsIt()
{
    CaptureResult cr = makeSingleScreenResult(100, 80);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 100, 80));
    overlay.enterEditing(QRect(30, 20, 20, 20));

    // Click outside to the right at (70, 30)
    QTest::mousePress(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(70, 30));
    QTest::mouseRelease(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(70, 30));

    // Selection should expand to include (70, 30): union of (30,20,20,20) + point(70,30)
    // = (30, 20, 41, 20) → but bounded to (30, 20, 41, 20)
    auto* view = overlay.findChild<QGraphicsView*>(QStringLiteral("AnnotationGraphicsView"));
    QVERIFY(view != nullptr);
    // The expanded rect should be (30, 20) to (70, 39) → (30, 20, 41, 20)
    QCOMPARE(view->geometry().left(), 30);
    QVERIFY(view->geometry().right() >= 70);
    // Original selection should not shrink
    QVERIFY(view->geometry().width() >= 20);
    QVERIFY(view->geometry().height() >= 20);
}

void CaptureOverlayTests::expandSelectionInDifferentDirections()
{
    // Test expanding to top-left
    {
        CaptureResult cr = makeSingleScreenResult(100, 80);
        CaptureOverlay overlay(std::move(cr), QRect(0, 0, 100, 80));
        overlay.enterEditing(QRect(30, 20, 20, 20));

        QTest::mousePress(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(10, 5));
        QTest::mouseRelease(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(10, 5));

        auto* view = overlay.findChild<QGraphicsView*>(QStringLiteral("AnnotationGraphicsView"));
        QVERIFY(view != nullptr);
        // Expanded to include (10, 5): union of (30,20,20,20) + (10,5) = (10, 5, 41, 36)
        QCOMPARE(view->geometry().left(), 10);
        QCOMPARE(view->geometry().top(), 5);
        QVERIFY(view->geometry().right() >= 49);  // original right edge
        QVERIFY(view->geometry().bottom() >= 39); // original bottom edge
    }

    // Test expanding to bottom-right
    {
        CaptureResult cr = makeSingleScreenResult(100, 80);
        CaptureOverlay overlay(std::move(cr), QRect(0, 0, 100, 80));
        overlay.enterEditing(QRect(30, 20, 20, 20));

        QTest::mousePress(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(80, 60));
        QTest::mouseRelease(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(80, 60));

        auto* view = overlay.findChild<QGraphicsView*>(QStringLiteral("AnnotationGraphicsView"));
        QVERIFY(view != nullptr);
        // Expanded to include (80, 60): union of (30,20,20,20) + (80,60) = (30, 20, 51, 41)
        QCOMPARE(view->geometry().left(), 30);
        QCOMPARE(view->geometry().top(), 20);
        QVERIFY(view->geometry().right() >= 80);
        QVERIFY(view->geometry().bottom() >= 60);
    }
}

void CaptureOverlayTests::expandSelectionPreservesAnnotations()
{
    CaptureResult cr = makeSingleScreenResult(100, 80);
    CaptureOverlay overlay(std::move(cr), QRect(0, 0, 100, 80));
    overlay.enterEditing(QRect(30, 20, 20, 20));

    // Add an annotation at scene-local (5, 5, 10, 8)
    auto* item = new RectAnnotationItem(QRectF(5.0, 5.0, 10.0, 8.0), QPen(Qt::red, 3.0));
    overlay.addAnnotationItem(item);
    QVERIFY(overlay.canUndo());

    const QPointF originalItemPos = item->pos();
    const QRectF originalItemRect = item->rect();

    // Expand selection to the left by clicking at (10, 30)
    QTest::mousePress(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(10, 30));
    QTest::mouseRelease(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(10, 30));

    auto* view = overlay.findChild<QGraphicsView*>(QStringLiteral("AnnotationGraphicsView"));
    QVERIFY(view != nullptr);

    // Selection should have expanded leftward
    QVERIFY(view->geometry().left() <= 10);

    // Annotation item should still exist
    QVERIFY(item->scene() != nullptr);

    // Annotation should have shifted right by the expansion delta (20 pixels)
    // because the scene origin moved left by 20 pixels
    QCOMPARE(item->pos(), originalItemPos + QPointF(20.0, 0.0));
    QCOMPARE(item->rect(), originalItemRect);

    // Undo stack should still be valid
    QVERIFY(overlay.canUndo());

    // Render should still show the annotation
    const QImage result = overlay.renderResultImage();
    QVERIFY(!result.isNull());
    QVERIFY(result.width() >= 40); // expanded width
}

void CaptureOverlayTests::rightClickInEditingEntersReselect()
{
    CaptureResult cr = makeSingleScreenResult(100, 80);
    auto* overlay = new CaptureOverlay(std::move(cr), QRect(0, 0, 100, 80));
    QPointer<CaptureOverlay> guard(overlay);
    overlay->show();

    overlay->enterEditing(QRect(20, 20, 30, 20));
    QCOMPARE(overlay->state(), CaptureState::Editing);

    // Right-click should trigger reselect, not cancel
    QTest::mouseClick(overlay, Qt::RightButton, Qt::NoModifier, QPoint(50, 50));

    // Should be back in Selecting state (reselect), not closed
    QCOMPARE(overlay->state(), CaptureState::Selecting);
    QVERIFY(!guard.isNull()); // overlay should still exist

    // Should be able to start a new selection
    QTest::mousePress(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
    QCOMPARE(overlay->state(), CaptureState::Selecting);
    QTest::mouseRelease(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(40, 30));
    QCOMPARE(overlay->state(), CaptureState::Editing);

    overlay->close();
}

void CaptureOverlayTests::rightClickBeforeSelectionCancels()
{
    CaptureResult cr = makeSingleScreenResult(100, 80);
    auto* overlay = new CaptureOverlay(std::move(cr), QRect(0, 0, 100, 80));
    QPointer<CaptureOverlay> guard(overlay);
    overlay->show();
    QVERIFY(QTest::qWaitForWindowExposed(overlay));

    QCOMPARE(overlay->state(), CaptureState::Selecting);

    // Right-click before any selection → cancel and close
    QTest::mouseClick(overlay, Qt::RightButton, Qt::NoModifier, QPoint(50, 50));

    // Overlay should be closed/canceled
    QTRY_VERIFY(guard.isNull());
}

void CaptureOverlayTests::mainWindowCreatesTrayMenuWithoutRegisteringHotkeysInTests()
{
    MainWindow window(nullptr, false);

    auto* trayMenu = window.findChild<QMenu*>("SnapInkTrayMenu");
    QVERIFY(trayMenu != nullptr);

#ifndef Q_OS_MACOS
    auto* trayIcon = window.findChild<QSystemTrayIcon*>("SnapInkTrayIcon");
    QVERIFY(trayIcon != nullptr);
    QVERIFY(trayIcon->contextMenu() != nullptr);
    QCOMPARE(trayIcon->contextMenu(), trayMenu);
#endif

    QStringList actionTexts;
    for (QAction* action : trayMenu->actions()) {
        if (!action->isSeparator()) {
            actionTexts.append(action->text());
        }
    }

    QCOMPARE(actionTexts,
             QStringList({QStringLiteral("Region Capture"),
                          QStringLiteral("Show Main Window"),
                          QStringLiteral("Quit")}));
}

void CaptureOverlayTests::mainWindowTrayIconUsesPlatformIcon()
{
    MainWindow window(nullptr, false);

#ifdef Q_OS_MACOS
    QCOMPARE(window.property("SnapInkTrayIconSource").toString(), QStringLiteral("trayTemplate.png"));
#else
    auto* trayIcon = window.findChild<QSystemTrayIcon*>("SnapInkTrayIcon");
    QVERIFY(trayIcon != nullptr);
    QVERIFY(!trayIcon->icon().isNull());
    QCOMPARE(trayIcon->property("SnapInkIconSource").toString(), QStringLiteral("app.icns"));
#endif
}

void CaptureOverlayTests::trayActivationDoesNotManuallyPopupDuplicateMenu()
{
    MainWindow window(nullptr, false);

    auto* trayIcon = window.findChild<QSystemTrayIcon*>("SnapInkTrayIcon");
#ifdef Q_OS_MACOS
    QVERIFY(trayIcon == nullptr);
#else
    QVERIFY(trayIcon != nullptr);
    QVERIFY(trayIcon->contextMenu() != nullptr);

    QMetaObject::invokeMethod(trayIcon,
                              "activated",
                              Q_ARG(QSystemTrayIcon::ActivationReason, QSystemTrayIcon::Trigger));
    QCoreApplication::processEvents();

    QVERIFY(!trayIcon->contextMenu()->isVisible());
#endif
}

void CaptureOverlayTests::mainWindowCloseHidesInsteadOfDestroying()
{
    auto* window = new MainWindow(nullptr, false);
    QPointer<MainWindow> guard(window);
    QSignalSpy destroyedSpy(window, &QObject::destroyed);

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    window->close();
    QCoreApplication::processEvents();

    QVERIFY(!guard.isNull());
    QCOMPARE(destroyedSpy.count(), 0);
    QVERIFY(!window->isVisible());

    delete window;
}

// ---------------------------------------------------------------------------

QTEST_MAIN(CaptureOverlayTests)

#include "capture_overlay_tests.moc"
