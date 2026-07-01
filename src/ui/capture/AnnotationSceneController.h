#ifndef ANNOTATIONSCENECONTROLLER_H
#define ANNOTATIONSCENECONTROLLER_H

#include "CaptureAnnotation.h"
#include "CaptureTool.h"

#include <QPainterPath>
#include <QPen>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QUndoStack>

#include <functional>
#include <memory>

class QGraphicsItem;
class QGraphicsPixmapItem;
class QGraphicsScene;
class QGraphicsView;
class QPainter;
class QPixmap;

class AnnotationSceneController
{
public:
    AnnotationSceneController();
    ~AnnotationSceneController();

    QGraphicsScene* scene() const;
    QGraphicsView* view() const;

    void setChangedHandler(std::function<void()> handler);

    void setSceneRect(const QRectF& rect);
    QRectF sceneRect() const;

    void setBackgroundPixmap(const QPixmap& pixmap);

    void clearAll();
    void clearInteractionState();

    void setCurrentTool(CaptureTool tool);
    CaptureTool currentTool() const;

    bool hasActivePreview() const;
    bool hasActiveTextEditing() const;
    bool activeTextItemContainsViewportPos(const QPoint& viewportPos) const;

    void beginAnnotation(const QPointF& selectionPos);
    void updateAnnotation(const QPointF& selectionPos);
    void finishAnnotation(const QPointF& selectionPos);
    void addAnnotationItem(QGraphicsItem* item);

    void beginTextEditing(const QPointF& selectionPos);
    void commitActiveTextEditing();
    void cancelActiveTextEditing();

    void shiftAllAnnotations(const QPointF& delta);

    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    void renderScene(QPainter* painter,
                     const QRectF& targetSceneRect);

private:
    QPointF clampToScene(const QPointF& point) const;
    void finishActiveTextEditing(TextAnnotationItem::FinishAction action);
    bool previewIsLargeEnough() const;
    void removePreviewItem();
    void notifyChanged();

    std::unique_ptr<QGraphicsScene> m_scene;
    std::unique_ptr<QGraphicsView> m_view;
    QUndoStack m_undoStack;
    QGraphicsPixmapItem* m_selectionPixmapItem = nullptr;
    QGraphicsItem* m_previewItem = nullptr;
    TextAnnotationItem* m_activeTextItem = nullptr;
    CaptureTool m_currentTool = CaptureTool::Select;
    QPen m_annotationPen = QPen(Qt::red, 3.0);
    QPointF m_annotationStart;
    QPainterPath m_activePath;
    int m_penPointCount = 0;
    bool m_drawingAnnotation = false;
    std::function<void()> m_changedHandler;
};

#endif // ANNOTATIONSCENECONTROLLER_H
