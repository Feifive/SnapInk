#include "CaptureInputController.h"

#include "AnnotationSceneController.h"
#include "CaptureTool.h"

#include <QEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QWidget>

#include <utility>

namespace
{
bool hasShortcutModifier(const QKeyEvent& event)
{
    return (event.modifiers()
            & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier))
           != Qt::NoModifier;
}
}

CaptureInputController::CaptureInputController(AnnotationSceneController& annotationController,
                                               CaptureSelectionModel& selectionModel,
                                               Callbacks callbacks)
    : m_annotationController(annotationController)
    , m_selectionModel(selectionModel)
    , m_callbacks(std::move(callbacks))
{
}

bool CaptureInputController::handleViewportEvent(QObject* watched,
                                                 QEvent* event,
                                                 QGraphicsView* annotationView,
                                                 bool isEditing)
{
    if (annotationView == nullptr || annotationView->viewport() == nullptr
        || watched != annotationView->viewport() || event == nullptr || !isEditing) {
        return false;
    }

    switch (event->type()) {
    case QEvent::KeyPress:
        return handleKeyPress(static_cast<QKeyEvent*>(event));
    case QEvent::MouseButtonPress:
        return handleMousePress(static_cast<QMouseEvent*>(event));
    case QEvent::MouseMove:
        return handleMouseMove(static_cast<QMouseEvent*>(event));
    case QEvent::MouseButtonRelease:
        return handleMouseRelease(static_cast<QMouseEvent*>(event));
    case QEvent::Wheel:
        return handleWheel(static_cast<QWheelEvent*>(event));
    default:
        return false;
    }
}

bool CaptureInputController::handleKeyPress(QKeyEvent* event)
{
    if (event == nullptr) {
        return false;
    }

    if (hasShortcutModifier(*event) || event->key() == Qt::Key_Escape) {
        if (m_callbacks.forwardShortcut) {
            m_callbacks.forwardShortcut(event);
        } else {
            event->ignore();
        }
        return event->isAccepted();
    }

    return false;
}

bool CaptureInputController::handleMousePress(QMouseEvent* event)
{
    if (event == nullptr) {
        return false;
    }

    if (event->button() == Qt::RightButton) {
        if (m_callbacks.reselect) {
            m_callbacks.reselect();
        }
        event->accept();
        return true;
    }

    if (event->button() != Qt::LeftButton) {
        event->accept();
        return true;
    }

    if (m_annotationController.hasActiveTextEditing()) {
        if (activeTextItemContainsViewportPos(event->pos())) {
            return false;
        }
        if (m_callbacks.commitActiveTextEditing) {
            m_callbacks.commitActiveTextEditing();
        }
    }

    const CaptureTool currentTool = m_annotationController.currentTool();
    if (currentTool == CaptureTool::Select) {
        const QPoint overlayPos = overlayPosFor(event->pos());
        const SelectionHandle handle = hitTestHandle(overlayPos);
        if (handle != SelectionHandle::None) {
            if (m_callbacks.beginResize) {
                m_callbacks.beginResize(handle, overlayPos);
            }
        } else if (isInsideSelection(overlayPos)) {
            if (m_callbacks.beginMove) {
                m_callbacks.beginMove(overlayPos);
            }
        }
        event->accept();
        return true;
    }

    const QPointF selectionPos = selectionPosFor(event->position());
    if (currentTool == CaptureTool::Text) {
        if (m_callbacks.beginTextEditing) {
            m_callbacks.beginTextEditing(selectionPos);
        }
    } else if (m_callbacks.beginAnnotation) {
        m_callbacks.beginAnnotation(selectionPos);
    }

    event->accept();
    return true;
}

bool CaptureInputController::handleMouseMove(QMouseEvent* event)
{
    if (event == nullptr) {
        return false;
    }

    if (m_annotationController.currentTool() == CaptureTool::Select) {
        const QPoint overlayPos = overlayPosFor(event->pos());
        if (m_selectionModel.interaction() == SelectionInteraction::Moving) {
            if (m_callbacks.updateMove) {
                m_callbacks.updateMove(overlayPos);
            }
            event->accept();
            return true;
        }
        if (m_selectionModel.interaction() == SelectionInteraction::Resizing) {
            if (m_callbacks.updateResize) {
                m_callbacks.updateResize(overlayPos);
            }
            event->accept();
            return true;
        }
        if (m_callbacks.updateSelectionCursor) {
            m_callbacks.updateSelectionCursor(overlayPos);
        }
        event->accept();
        return true;
    }

    if (m_annotationController.hasActivePreview()) {
        if (m_callbacks.updateAnnotation) {
            m_callbacks.updateAnnotation(selectionPosFor(event->position()));
        }
        event->accept();
        return true;
    }

    return false;
}

bool CaptureInputController::handleMouseRelease(QMouseEvent* event)
{
    if (event == nullptr || event->button() != Qt::LeftButton) {
        return false;
    }

    if (m_annotationController.currentTool() == CaptureTool::Select) {
        if (m_callbacks.finishSelectionInteraction) {
            m_callbacks.finishSelectionInteraction();
        }
        event->accept();
        return true;
    }

    if (m_annotationController.hasActivePreview()) {
        if (m_callbacks.finishAnnotation) {
            m_callbacks.finishAnnotation(selectionPosFor(event->position()));
        }
        event->accept();
        return true;
    }

    return false;
}

bool CaptureInputController::handleWheel(QWheelEvent* event)
{
    if (event == nullptr || m_annotationController.currentTool() != CaptureTool::Select) {
        return false;
    }

    const QPoint overlayPos = overlayPosFor(event->position().toPoint());
    if (!canAdjustSelectionAt(overlayPos)) {
        return false;
    }

    if (event->angleDelta().y() > 0) {
        if (m_callbacks.resizeSelectionByWheel) {
            m_callbacks.resizeSelectionByWheel(1);
        }
    } else if (event->angleDelta().y() < 0) {
        if (m_callbacks.resizeSelectionByWheel) {
            m_callbacks.resizeSelectionByWheel(-1);
        }
    }

    event->accept();
    return true;
}

QPoint CaptureInputController::overlayPosFor(const QPoint& viewportPos) const
{
    if (m_callbacks.viewportToOverlayPos) {
        return m_callbacks.viewportToOverlayPos(viewportPos);
    }
    return viewportPos;
}

QPointF CaptureInputController::selectionPosFor(const QPointF& viewportPos) const
{
    if (m_callbacks.viewportToSelectionPos) {
        return m_callbacks.viewportToSelectionPos(viewportPos);
    }
    return viewportPos;
}

SelectionHandle CaptureInputController::hitTestHandle(const QPoint& overlayPos) const
{
    if (m_callbacks.hitTestHandle) {
        return m_callbacks.hitTestHandle(overlayPos);
    }
    return SelectionHandle::None;
}

bool CaptureInputController::isInsideSelection(const QPoint& overlayPos) const
{
    return m_callbacks.isInsideSelection && m_callbacks.isInsideSelection(overlayPos);
}

bool CaptureInputController::canAdjustSelectionAt(const QPoint& overlayPos) const
{
    return m_callbacks.canAdjustSelectionAt && m_callbacks.canAdjustSelectionAt(overlayPos);
}

bool CaptureInputController::activeTextItemContainsViewportPos(const QPoint& viewportPos) const
{
    if (m_callbacks.activeTextItemContainsViewportPos) {
        return m_callbacks.activeTextItemContainsViewportPos(viewportPos);
    }
    return false;
}
