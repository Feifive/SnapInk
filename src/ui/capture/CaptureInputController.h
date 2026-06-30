#ifndef CAPTUREINPUTCONTROLLER_H
#define CAPTUREINPUTCONTROLLER_H

#include "CaptureSelectionModel.h"

#include <QPoint>
#include <QPointF>

#include <functional>

class AnnotationSceneController;
class QEvent;
class QGraphicsView;
class QKeyEvent;
class QMouseEvent;
class QObject;
class QWheelEvent;

class CaptureInputController
{
public:
    struct Callbacks
    {
        std::function<QPoint(const QPoint& viewportPos)> viewportToOverlayPos;
        std::function<QPointF(const QPointF& viewportPos)> viewportToSelectionPos;

        std::function<SelectionHandle(const QPoint& overlayPos)> hitTestHandle;
        std::function<bool(const QPoint& overlayPos)> isInsideSelection;
        std::function<bool(const QPoint& overlayPos)> canAdjustSelectionAt;

        std::function<void(SelectionHandle handle, const QPoint& overlayPos)> beginResize;
        std::function<void(const QPoint& overlayPos)> updateResize;
        std::function<void(const QPoint& overlayPos)> beginMove;
        std::function<void(const QPoint& overlayPos)> updateMove;
        std::function<void()> finishSelectionInteraction;

        std::function<void(int pixelDelta)> resizeSelectionByWheel;
        std::function<void(const QPoint& overlayPos)> updateSelectionCursor;

        std::function<void(const QPointF& selectionPos)> beginAnnotation;
        std::function<void(const QPointF& selectionPos)> updateAnnotation;
        std::function<void(const QPointF& selectionPos)> finishAnnotation;
        std::function<void(const QPointF& selectionPos)> beginTextEditing;

        std::function<void()> commitActiveTextEditing;
        std::function<bool(const QPoint& viewportPos)> activeTextItemContainsViewPos;

        std::function<void(QKeyEvent* event)> forwardShortcut;
        std::function<void()> reselect;
    };

    CaptureInputController(AnnotationSceneController& annotationController,
                           CaptureSelectionModel& selectionModel,
                           Callbacks callbacks);

    bool handleViewportEvent(QObject* watched,
                             QEvent* event,
                             QGraphicsView* annotationView,
                             bool isEditing);

private:
    bool handleKeyPress(QKeyEvent* event);
    bool handleMousePress(QMouseEvent* event);
    bool handleMouseMove(QMouseEvent* event);
    bool handleMouseRelease(QMouseEvent* event);
    bool handleWheel(QWheelEvent* event);

    QPoint overlayPosFor(const QPoint& viewportPos) const;
    QPointF selectionPosFor(const QPointF& viewportPos) const;
    SelectionHandle hitTestHandle(const QPoint& overlayPos) const;
    bool isInsideSelection(const QPoint& overlayPos) const;
    bool canAdjustSelectionAt(const QPoint& overlayPos) const;
    bool activeTextItemContainsViewPos(const QPoint& viewportPos) const;

    AnnotationSceneController& m_annotationController;
    CaptureSelectionModel& m_selectionModel;
    Callbacks m_callbacks;
};

#endif // CAPTUREINPUTCONTROLLER_H
