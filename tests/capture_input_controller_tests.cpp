#include "../src/ui/capture/CaptureInputController.h"

#include "../src/ui/capture/AnnotationSceneController.h"
#include "../src/ui/capture/CaptureSelectionModel.h"

#include <QGraphicsView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTest>
#include <QVector>
#include <QWheelEvent>
#include <QWidget>

namespace
{
struct CallbackLog
{
    int beginResizeCount = 0;
    int updateResizeCount = 0;
    int beginMoveCount = 0;
    int updateMoveCount = 0;
    int finishSelectionInteractionCount = 0;
    int resizeSelectionByWheelCount = 0;
    int updateSelectionCursorCount = 0;
    int beginAnnotationCount = 0;
    int updateAnnotationCount = 0;
    int finishAnnotationCount = 0;
    int beginTextEditingCount = 0;
    int commitActiveTextEditingCount = 0;
    int activeTextHitTestCount = 0;
    int forwardShortcutCount = 0;
    int reselectCount = 0;

    SelectionHandle resizeHandle = SelectionHandle::None;
    QPoint lastOverlayPos;
    QPointF lastSelectionPos;
    int lastWheelDelta = 0;
    bool activeTextContainsViewPos = false;
    bool forwardShortcutAccepts = true;
    QVector<QString> callOrder;
};

CaptureInputController::Callbacks makeCallbacks(CallbackLog& log,
                                                AnnotationSceneController& annotationController,
                                                CaptureSelectionModel& selectionModel)
{
    CaptureInputController::Callbacks callbacks;
    callbacks.viewportToOverlayPos = [](const QPoint& viewportPos) {
        return viewportPos + QPoint(10, 10);
    };
    callbacks.viewportToSelectionPos = [](const QPointF& viewportPos) {
        return viewportPos;
    };
    callbacks.hitTestHandle = [&selectionModel](const QPoint& overlayPos) {
        return selectionModel.hitTestHandle(overlayPos);
    };
    callbacks.isInsideSelection = [&selectionModel](const QPoint& overlayPos) {
        return selectionModel.contains(overlayPos);
    };
    callbacks.canAdjustSelectionAt = [&selectionModel](const QPoint& overlayPos) {
        return selectionModel.hitTestHandle(overlayPos) != SelectionHandle::None
               || selectionModel.contains(overlayPos);
    };
    callbacks.beginResize = [&log, &selectionModel](SelectionHandle handle, const QPoint& overlayPos) {
        ++log.beginResizeCount;
        log.callOrder.append(QStringLiteral("beginResize"));
        log.resizeHandle = handle;
        log.lastOverlayPos = overlayPos;
        selectionModel.beginResize(handle, overlayPos);
    };
    callbacks.updateResize = [&log, &selectionModel](const QPoint& overlayPos) {
        ++log.updateResizeCount;
        log.callOrder.append(QStringLiteral("updateResize"));
        log.lastOverlayPos = overlayPos;
        selectionModel.updateResize(overlayPos);
    };
    callbacks.beginMove = [&log, &selectionModel](const QPoint& overlayPos) {
        ++log.beginMoveCount;
        log.callOrder.append(QStringLiteral("beginMove"));
        log.lastOverlayPos = overlayPos;
        selectionModel.beginMove(overlayPos);
    };
    callbacks.updateMove = [&log, &selectionModel](const QPoint& overlayPos) {
        ++log.updateMoveCount;
        log.callOrder.append(QStringLiteral("updateMove"));
        log.lastOverlayPos = overlayPos;
        selectionModel.updateMove(overlayPos);
    };
    callbacks.finishSelectionInteraction = [&log, &selectionModel]() {
        ++log.finishSelectionInteractionCount;
        log.callOrder.append(QStringLiteral("finishSelectionInteraction"));
        selectionModel.finishInteraction();
    };
    callbacks.resizeSelectionByWheel = [&log, &selectionModel](int pixelDelta) {
        ++log.resizeSelectionByWheelCount;
        log.callOrder.append(QStringLiteral("resizeSelectionByWheel"));
        log.lastWheelDelta = pixelDelta;
        selectionModel.resizeByWheel(pixelDelta);
    };
    callbacks.updateSelectionCursor = [&log](const QPoint& overlayPos) {
        ++log.updateSelectionCursorCount;
        log.callOrder.append(QStringLiteral("updateSelectionCursor"));
        log.lastOverlayPos = overlayPos;
    };
    callbacks.beginAnnotation = [&log, &annotationController](const QPointF& selectionPos) {
        ++log.beginAnnotationCount;
        log.callOrder.append(QStringLiteral("beginAnnotation"));
        log.lastSelectionPos = selectionPos;
        annotationController.beginAnnotation(selectionPos);
    };
    callbacks.updateAnnotation = [&log, &annotationController](const QPointF& selectionPos) {
        ++log.updateAnnotationCount;
        log.callOrder.append(QStringLiteral("updateAnnotation"));
        log.lastSelectionPos = selectionPos;
        annotationController.updateAnnotation(selectionPos);
    };
    callbacks.finishAnnotation = [&log, &annotationController](const QPointF& selectionPos) {
        ++log.finishAnnotationCount;
        log.callOrder.append(QStringLiteral("finishAnnotation"));
        log.lastSelectionPos = selectionPos;
        annotationController.finishAnnotation(selectionPos);
    };
    callbacks.beginTextEditing = [&log, &annotationController](const QPointF& selectionPos) {
        ++log.beginTextEditingCount;
        log.callOrder.append(QStringLiteral("beginTextEditing"));
        log.lastSelectionPos = selectionPos;
        annotationController.beginTextEditing(selectionPos);
    };
    callbacks.commitActiveTextEditing = [&log, &annotationController]() {
        ++log.commitActiveTextEditingCount;
        log.callOrder.append(QStringLiteral("commitActiveTextEditing"));
        annotationController.commitActiveTextEditing();
    };
    callbacks.activeTextItemContainsViewPos = [&log](const QPoint&) {
        ++log.activeTextHitTestCount;
        return log.activeTextContainsViewPos;
    };
    callbacks.forwardShortcut = [&log](QKeyEvent* event) {
        ++log.forwardShortcutCount;
        log.callOrder.append(QStringLiteral("forwardShortcut"));
        if (log.forwardShortcutAccepts) {
            event->accept();
        } else {
            event->ignore();
        }
    };
    callbacks.reselect = [&log]() {
        ++log.reselectCount;
        log.callOrder.append(QStringLiteral("reselect"));
    };
    return callbacks;
}

struct ControllerHarness
{
    AnnotationSceneController annotationController;
    CaptureSelectionModel selectionModel{QRect(0, 0, 100, 80)};
    CallbackLog log;

    ControllerHarness()
    {
        selectionModel.setSelectionRect(QRect(10, 10, 40, 30));
        annotationController.setSceneRect(QRectF(0.0, 0.0, 40.0, 30.0));
        annotationController.view()->setGeometry(QRect(10, 10, 40, 30));
    }

    CaptureInputController makeController()
    {
        return CaptureInputController(annotationController,
                                      selectionModel,
                                      makeCallbacks(log, annotationController, selectionModel));
    }

    QGraphicsView* view() const
    {
        return annotationController.view();
    }
};

QMouseEvent mouseEvent(QEvent::Type type, const QPoint& pos, Qt::MouseButton button)
{
    return QMouseEvent(type,
                       QPointF(pos),
                       QPointF(pos),
                       button,
                       button == Qt::NoButton ? Qt::NoButton : Qt::MouseButtons(button),
                       Qt::NoModifier);
}

QWheelEvent wheelEvent(const QPoint& pos, int angleDeltaY)
{
    return QWheelEvent(QPointF(pos),
                       QPointF(pos),
                       QPoint(),
                       QPoint(0, angleDeltaY),
                       Qt::NoButton,
                       Qt::NoModifier,
                       Qt::NoScrollPhase,
                       false);
}
}

class CaptureInputControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void selectToolPressOnHandleBeginsResize();
    void selectToolPressInsideSelectionBeginsMove();
    void selectToolMoveWhileMovingUpdatesMove();
    void selectToolMoveWhileResizingUpdatesResize();
    void selectToolReleaseFinishesSelectionInteraction();
    void wheelInsideSelectionResizesByOnePixel();
    void wheelOutsideSelectionIsIgnored();
    void rectangleToolPressMoveReleaseRoutesAnnotationCallbacks();
    void textToolPressBeginsTextEditing();
    void textEditingPressInsideTextPassesThrough();
    void textEditingPressOutsideCommitsBeforeRouting();
    void shortcutKeyPressForwardsToOverlay();
    void plainCharacterKeyPressPassesThrough();
    void eventsFromOtherObjectsAreIgnored();
    void eventsOutsideEditingStateAreIgnored();
    void rightClickRequestsReselectAndConsumesEvent();
};

void CaptureInputControllerTests::selectToolPressOnHandleBeginsResize()
{
    ControllerHarness harness;
    auto controller = harness.makeController();
    QMouseEvent event = mouseEvent(QEvent::MouseButtonPress, QPoint(0, 0), Qt::LeftButton);

    const bool handled = controller.handleViewportEvent(harness.view()->viewport(),
                                                        &event,
                                                        harness.view(),
                                                        true);

    QVERIFY(handled);
    QCOMPARE(harness.log.beginResizeCount, 1);
    QCOMPARE(harness.log.resizeHandle, SelectionHandle::TopLeft);
    QCOMPARE(harness.log.lastOverlayPos, QPoint(10, 10));
    QCOMPARE(harness.selectionModel.interaction(), SelectionInteraction::Resizing);
}

void CaptureInputControllerTests::selectToolPressInsideSelectionBeginsMove()
{
    ControllerHarness harness;
    auto controller = harness.makeController();
    QMouseEvent event = mouseEvent(QEvent::MouseButtonPress, QPoint(20, 15), Qt::LeftButton);

    const bool handled = controller.handleViewportEvent(harness.view()->viewport(),
                                                        &event,
                                                        harness.view(),
                                                        true);

    QVERIFY(handled);
    QCOMPARE(harness.log.beginMoveCount, 1);
    QCOMPARE(harness.log.lastOverlayPos, QPoint(30, 25));
    QCOMPARE(harness.selectionModel.interaction(), SelectionInteraction::Moving);
}

void CaptureInputControllerTests::selectToolMoveWhileMovingUpdatesMove()
{
    ControllerHarness harness;
    harness.selectionModel.beginMove(QPoint(30, 25));
    auto controller = harness.makeController();
    QMouseEvent event = mouseEvent(QEvent::MouseMove, QPoint(24, 18), Qt::NoButton);

    const bool handled = controller.handleViewportEvent(harness.view()->viewport(),
                                                        &event,
                                                        harness.view(),
                                                        true);

    QVERIFY(handled);
    QCOMPARE(harness.log.updateMoveCount, 1);
    QCOMPARE(harness.log.lastOverlayPos, QPoint(34, 28));
}

void CaptureInputControllerTests::selectToolMoveWhileResizingUpdatesResize()
{
    ControllerHarness harness;
    harness.selectionModel.beginResize(SelectionHandle::BottomRight, QPoint(49, 39));
    auto controller = harness.makeController();
    QMouseEvent event = mouseEvent(QEvent::MouseMove, QPoint(44, 33), Qt::NoButton);

    const bool handled = controller.handleViewportEvent(harness.view()->viewport(),
                                                        &event,
                                                        harness.view(),
                                                        true);

    QVERIFY(handled);
    QCOMPARE(harness.log.updateResizeCount, 1);
    QCOMPARE(harness.log.lastOverlayPos, QPoint(54, 43));
}

void CaptureInputControllerTests::selectToolReleaseFinishesSelectionInteraction()
{
    ControllerHarness harness;
    harness.selectionModel.beginMove(QPoint(30, 25));
    auto controller = harness.makeController();
    QMouseEvent event = mouseEvent(QEvent::MouseButtonRelease, QPoint(24, 18), Qt::LeftButton);

    const bool handled = controller.handleViewportEvent(harness.view()->viewport(),
                                                        &event,
                                                        harness.view(),
                                                        true);

    QVERIFY(handled);
    QCOMPARE(harness.log.finishSelectionInteractionCount, 1);
    QCOMPARE(harness.selectionModel.interaction(), SelectionInteraction::None);
}

void CaptureInputControllerTests::wheelInsideSelectionResizesByOnePixel()
{
    ControllerHarness harness;
    auto controller = harness.makeController();
    QWheelEvent event = wheelEvent(QPoint(20, 15), 120);

    const bool handled = controller.handleViewportEvent(harness.view()->viewport(),
                                                        &event,
                                                        harness.view(),
                                                        true);

    QVERIFY(handled);
    QCOMPARE(harness.log.resizeSelectionByWheelCount, 1);
    QCOMPARE(harness.log.lastWheelDelta, 1);
}

void CaptureInputControllerTests::wheelOutsideSelectionIsIgnored()
{
    ControllerHarness harness;
    auto controller = harness.makeController();
    QWheelEvent event = wheelEvent(QPoint(60, 55), 120);

    const bool handled = controller.handleViewportEvent(harness.view()->viewport(),
                                                        &event,
                                                        harness.view(),
                                                        true);

    QVERIFY(!handled);
    QCOMPARE(harness.log.resizeSelectionByWheelCount, 0);
}

void CaptureInputControllerTests::rectangleToolPressMoveReleaseRoutesAnnotationCallbacks()
{
    ControllerHarness harness;
    harness.annotationController.setCurrentTool(CaptureTool::Rect);
    auto controller = harness.makeController();
    QMouseEvent press = mouseEvent(QEvent::MouseButtonPress, QPoint(2, 3), Qt::LeftButton);
    QMouseEvent move = mouseEvent(QEvent::MouseMove, QPoint(12, 14), Qt::NoButton);
    QMouseEvent release = mouseEvent(QEvent::MouseButtonRelease, QPoint(12, 14), Qt::LeftButton);

    QVERIFY(controller.handleViewportEvent(harness.view()->viewport(), &press, harness.view(), true));
    QVERIFY(harness.annotationController.hasActivePreview());
    QVERIFY(controller.handleViewportEvent(harness.view()->viewport(), &move, harness.view(), true));
    QVERIFY(controller.handleViewportEvent(harness.view()->viewport(), &release, harness.view(), true));

    QCOMPARE(harness.log.beginAnnotationCount, 1);
    QCOMPARE(harness.log.updateAnnotationCount, 1);
    QCOMPARE(harness.log.finishAnnotationCount, 1);
    QCOMPARE(harness.log.lastSelectionPos, QPointF(12.0, 14.0));
    QVERIFY(!harness.annotationController.hasActivePreview());
}

void CaptureInputControllerTests::textToolPressBeginsTextEditing()
{
    ControllerHarness harness;
    harness.annotationController.setCurrentTool(CaptureTool::Text);
    auto controller = harness.makeController();
    QMouseEvent event = mouseEvent(QEvent::MouseButtonPress, QPoint(7, 8), Qt::LeftButton);

    const bool handled = controller.handleViewportEvent(harness.view()->viewport(),
                                                        &event,
                                                        harness.view(),
                                                        true);

    QVERIFY(handled);
    QCOMPARE(harness.log.beginTextEditingCount, 1);
    QCOMPARE(harness.log.lastSelectionPos, QPointF(7.0, 8.0));
    QVERIFY(harness.annotationController.hasActiveTextEditing());
}

void CaptureInputControllerTests::textEditingPressInsideTextPassesThrough()
{
    ControllerHarness harness;
    harness.annotationController.setCurrentTool(CaptureTool::Text);
    harness.annotationController.beginTextEditing(QPointF(4.0, 5.0));
    harness.log.activeTextContainsViewPos = true;
    auto controller = harness.makeController();
    QMouseEvent event = mouseEvent(QEvent::MouseButtonPress, QPoint(5, 6), Qt::LeftButton);

    const bool handled = controller.handleViewportEvent(harness.view()->viewport(),
                                                        &event,
                                                        harness.view(),
                                                        true);

    QVERIFY(!handled);
    QCOMPARE(harness.log.activeTextHitTestCount, 1);
    QCOMPARE(harness.log.commitActiveTextEditingCount, 0);
}

void CaptureInputControllerTests::textEditingPressOutsideCommitsBeforeRouting()
{
    ControllerHarness harness;
    harness.annotationController.setCurrentTool(CaptureTool::Text);
    harness.annotationController.beginTextEditing(QPointF(4.0, 5.0));
    harness.log.activeTextContainsViewPos = false;
    auto controller = harness.makeController();
    QMouseEvent event = mouseEvent(QEvent::MouseButtonPress, QPoint(12, 9), Qt::LeftButton);

    const bool handled = controller.handleViewportEvent(harness.view()->viewport(),
                                                        &event,
                                                        harness.view(),
                                                        true);

    QVERIFY(handled);
    QCOMPARE(harness.log.commitActiveTextEditingCount, 1);
    QCOMPARE(harness.log.beginTextEditingCount, 1);
    QCOMPARE(harness.log.callOrder.at(0), QStringLiteral("commitActiveTextEditing"));
    QCOMPARE(harness.log.callOrder.at(1), QStringLiteral("beginTextEditing"));
}

void CaptureInputControllerTests::shortcutKeyPressForwardsToOverlay()
{
    ControllerHarness harness;
    auto controller = harness.makeController();
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);

    const bool handled = controller.handleViewportEvent(harness.view()->viewport(),
                                                        &event,
                                                        harness.view(),
                                                        true);

    QVERIFY(handled);
    QCOMPARE(harness.log.forwardShortcutCount, 1);
}

void CaptureInputControllerTests::plainCharacterKeyPressPassesThrough()
{
    ControllerHarness harness;
    auto controller = harness.makeController();
    QKeyEvent event(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));

    const bool handled = controller.handleViewportEvent(harness.view()->viewport(),
                                                        &event,
                                                        harness.view(),
                                                        true);

    QVERIFY(!handled);
    QCOMPARE(harness.log.forwardShortcutCount, 0);
}

void CaptureInputControllerTests::eventsFromOtherObjectsAreIgnored()
{
    ControllerHarness harness;
    auto controller = harness.makeController();
    QWidget other;
    QMouseEvent event = mouseEvent(QEvent::MouseButtonPress, QPoint(20, 15), Qt::LeftButton);

    const bool handled = controller.handleViewportEvent(&other, &event, harness.view(), true);

    QVERIFY(!handled);
    QCOMPARE(harness.log.beginMoveCount, 0);
}

void CaptureInputControllerTests::eventsOutsideEditingStateAreIgnored()
{
    ControllerHarness harness;
    auto controller = harness.makeController();
    QMouseEvent event = mouseEvent(QEvent::MouseButtonPress, QPoint(20, 15), Qt::LeftButton);

    const bool handled = controller.handleViewportEvent(harness.view()->viewport(),
                                                        &event,
                                                        harness.view(),
                                                        false);

    QVERIFY(!handled);
    QCOMPARE(harness.log.beginMoveCount, 0);
}

void CaptureInputControllerTests::rightClickRequestsReselectAndConsumesEvent()
{
    ControllerHarness harness;
    auto controller = harness.makeController();
    QMouseEvent event = mouseEvent(QEvent::MouseButtonPress, QPoint(20, 15), Qt::RightButton);

    const bool handled = controller.handleViewportEvent(harness.view()->viewport(),
                                                        &event,
                                                        harness.view(),
                                                        true);

    QVERIFY(handled);
    QCOMPARE(harness.log.reselectCount, 1);
}

QTEST_MAIN(CaptureInputControllerTests)

#include "capture_input_controller_tests.moc"
