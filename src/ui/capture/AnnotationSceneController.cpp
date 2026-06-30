#include "AnnotationSceneController.h"

#include "AddAnnotationCommand.h"

#include <QFrame>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPainter>
#include <QPixmap>
#include <QWidget>

namespace
{
constexpr int kMinimumAnnotationSize = 2;
constexpr qreal kMinimumArrowLength = 4.0;
constexpr qreal kBackgroundZValue = -1000.0;
}

AnnotationSceneController::AnnotationSceneController()
    : m_scene(std::make_unique<QGraphicsScene>())
    , m_view(std::make_unique<QGraphicsView>(m_scene.get()))
{
    m_annotationPen.setCapStyle(Qt::RoundCap);
    m_annotationPen.setJoinStyle(Qt::RoundJoin);

    m_view->setObjectName(QStringLiteral("AnnotationGraphicsView"));
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setTransformationAnchor(QGraphicsView::NoAnchor);
    m_view->setResizeAnchor(QGraphicsView::NoAnchor);
    m_view->setDragMode(QGraphicsView::NoDrag);
    m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    m_view->setInteractive(true);
    m_view->setMouseTracking(true);
    m_view->setAutoFillBackground(false);
    m_view->setStyleSheet(QStringLiteral("background: transparent"));
    m_view->viewport()->setAutoFillBackground(false);
    m_view->viewport()->setAttribute(Qt::WA_TranslucentBackground, true);
    m_view->hide();

    QObject::connect(&m_undoStack, &QUndoStack::canUndoChanged, &m_undoStack, [this]() {
        notifyChanged();
    });
    QObject::connect(&m_undoStack, &QUndoStack::canRedoChanged, &m_undoStack, [this]() {
        notifyChanged();
    });
}

AnnotationSceneController::~AnnotationSceneController()
{
    clearAll();
}

QGraphicsScene* AnnotationSceneController::scene() const
{
    return m_scene.get();
}

QGraphicsView* AnnotationSceneController::view() const
{
    return m_view.get();
}

void AnnotationSceneController::setChangedHandler(std::function<void()> handler)
{
    m_changedHandler = std::move(handler);
}

void AnnotationSceneController::setSceneRect(const QRectF& rect)
{
    m_scene->setSceneRect(rect);
    m_view->setSceneRect(rect);
}

QRectF AnnotationSceneController::sceneRect() const
{
    return m_scene->sceneRect();
}

void AnnotationSceneController::setBackgroundPixmap(const QPixmap& pixmap)
{
    if (m_selectionPixmapItem == nullptr || m_selectionPixmapItem->scene() != m_scene.get()) {
        m_selectionPixmapItem = m_scene->addPixmap(pixmap);
        m_selectionPixmapItem->setZValue(kBackgroundZValue);
    } else {
        m_selectionPixmapItem->setPixmap(pixmap);
    }
    m_selectionPixmapItem->setPos(0.0, 0.0);
    notifyChanged();
}

void AnnotationSceneController::clearAll()
{
    if (m_activeTextItem != nullptr) {
        m_activeTextItem->setFinishedHandler({});
    }

    m_activeTextItem = nullptr;
    m_previewItem = nullptr;
    m_selectionPixmapItem = nullptr;
    m_activePath = QPainterPath();
    m_penPointCount = 0;
    m_drawingAnnotation = false;

    m_undoStack.clear();
    m_scene->clear();
    notifyChanged();
}

void AnnotationSceneController::clearInteractionState()
{
    removePreviewItem();
    m_activePath = QPainterPath();
    m_penPointCount = 0;
    m_drawingAnnotation = false;
    notifyChanged();
}

void AnnotationSceneController::setCurrentTool(CaptureTool tool)
{
    commitActiveTextEditing();
    clearInteractionState();
    m_currentTool = tool;
}

CaptureTool AnnotationSceneController::currentTool() const
{
    return m_currentTool;
}

bool AnnotationSceneController::hasActivePreview() const
{
    return m_previewItem != nullptr;
}

bool AnnotationSceneController::hasActiveTextEditing() const
{
    return m_activeTextItem != nullptr;
}

bool AnnotationSceneController::activeTextItemContainsViewPos(const QPoint& viewPos) const
{
    return m_activeTextItem != nullptr && m_view->itemAt(viewPos) == m_activeTextItem;
}

void AnnotationSceneController::beginAnnotation(const QPointF& selectionPos)
{
    commitActiveTextEditing();
    clearInteractionState();
    m_drawingAnnotation = true;
    m_annotationStart = clampToScene(selectionPos);

    switch (m_currentTool) {
    case CaptureTool::Rect:
        m_previewItem = new RectAnnotationItem(QRectF(m_annotationStart, m_annotationStart), m_annotationPen);
        break;
    case CaptureTool::Arrow:
        m_previewItem = new ArrowAnnotationItem(QLineF(m_annotationStart, m_annotationStart), m_annotationPen);
        break;
    case CaptureTool::Pen:
        m_activePath = QPainterPath(m_annotationStart);
        m_penPointCount = 1;
        m_previewItem = new PenAnnotationItem(m_activePath, m_annotationPen);
        break;
    case CaptureTool::Select:
    case CaptureTool::None:
    case CaptureTool::Text:
        m_drawingAnnotation = false;
        break;
    }

    if (m_previewItem != nullptr) {
        m_scene->addItem(m_previewItem);
        notifyChanged();
    }
}

void AnnotationSceneController::updateAnnotation(const QPointF& selectionPos)
{
    if (!m_drawingAnnotation) {
        return;
    }

    removePreviewItem();

    const QPointF current = clampToScene(selectionPos);
    switch (m_currentTool) {
    case CaptureTool::Rect:
        m_previewItem = new RectAnnotationItem(QRectF(m_annotationStart, current), m_annotationPen);
        break;
    case CaptureTool::Arrow:
        m_previewItem = new ArrowAnnotationItem(QLineF(m_annotationStart, current), m_annotationPen);
        break;
    case CaptureTool::Pen:
        m_activePath.lineTo(current);
        ++m_penPointCount;
        m_previewItem = new PenAnnotationItem(m_activePath, m_annotationPen);
        break;
    case CaptureTool::Select:
    case CaptureTool::None:
    case CaptureTool::Text:
        break;
    }

    if (m_previewItem != nullptr) {
        m_scene->addItem(m_previewItem);
        notifyChanged();
    }
}

void AnnotationSceneController::finishAnnotation(const QPointF& selectionPos)
{
    updateAnnotation(selectionPos);
    m_drawingAnnotation = false;

    const bool valid = previewIsLargeEnough();
    QGraphicsItem* finishedItem = m_previewItem;
    m_previewItem = nullptr;

    if (finishedItem != nullptr && valid) {
        if (finishedItem->scene() != nullptr) {
            finishedItem->scene()->removeItem(finishedItem);
        }
        addAnnotationItem(finishedItem);
    } else if (finishedItem != nullptr) {
        if (finishedItem->scene() != nullptr) {
            finishedItem->scene()->removeItem(finishedItem);
        }
        delete finishedItem;
    }

    clearInteractionState();
}

void AnnotationSceneController::addAnnotationItem(QGraphicsItem* item)
{
    if (item == nullptr) {
        delete item;
        return;
    }

    m_undoStack.push(new AddAnnotationCommand(m_scene.get(), item, [this]() {
        m_scene->update();
        notifyChanged();
    }));
}

void AnnotationSceneController::beginTextEditing(const QPointF& selectionPos)
{
    commitActiveTextEditing();
    clearInteractionState();

    auto* item = new TextAnnotationItem(clampToScene(selectionPos), m_annotationPen.color());
    const qreal remainingWidth = qMax<qreal>(1.0, m_scene->sceneRect().width() - item->pos().x());
    item->setMaximumTextWidth(remainingWidth);
    item->setFinishedHandler([this](TextAnnotationItem* textItem, TextAnnotationItem::FinishAction action) {
        if (textItem == m_activeTextItem) {
            finishActiveTextEditing(action);
        }
    });

    m_activeTextItem = item;
    m_scene->addItem(item);
    item->setFocus(Qt::MouseFocusReason);
    notifyChanged();
}

void AnnotationSceneController::commitActiveTextEditing()
{
    finishActiveTextEditing(TextAnnotationItem::FinishAction::Commit);
}

void AnnotationSceneController::cancelActiveTextEditing()
{
    finishActiveTextEditing(TextAnnotationItem::FinishAction::Cancel);
}

void AnnotationSceneController::shiftAllAnnotations(const QPointF& delta)
{
    for (QGraphicsItem* item : m_scene->items()) {
        if (item == nullptr || item == m_selectionPixmapItem || item == m_previewItem) {
            continue;
        }
        item->moveBy(delta.x(), delta.y());
    }
    notifyChanged();
}

void AnnotationSceneController::undo()
{
    commitActiveTextEditing();
    m_undoStack.undo();
}

void AnnotationSceneController::redo()
{
    commitActiveTextEditing();
    m_undoStack.redo();
}

bool AnnotationSceneController::canUndo() const
{
    return m_undoStack.canUndo();
}

bool AnnotationSceneController::canRedo() const
{
    return m_undoStack.canRedo();
}

void AnnotationSceneController::renderAnnotations(QPainter* painter,
                                                  const QRectF& targetSceneRect)
{
    if (painter == nullptr) {
        return;
    }

    commitActiveTextEditing();

    const bool hadPreview = m_previewItem != nullptr && m_previewItem->scene() == m_scene.get();
    QGraphicsItem* previewItem = m_previewItem;
    if (hadPreview) {
        m_scene->removeItem(previewItem);
    }

    m_scene->render(painter, targetSceneRect, targetSceneRect);

    if (hadPreview) {
        m_scene->addItem(previewItem);
    }
}

QPointF AnnotationSceneController::clampToScene(const QPointF& point) const
{
    const QRectF rect = m_scene->sceneRect();
    if (rect.isEmpty()) {
        return {};
    }

    return QPointF(qBound<qreal>(rect.left(), point.x(), rect.left() + rect.width()),
                   qBound<qreal>(rect.top(), point.y(), rect.top() + rect.height()));
}

void AnnotationSceneController::finishActiveTextEditing(TextAnnotationItem::FinishAction action)
{
    if (m_activeTextItem == nullptr) {
        return;
    }

    TextAnnotationItem* item = m_activeTextItem;
    m_activeTextItem = nullptr;
    item->setFinishedHandler({});

    const bool empty = item->toPlainText().trimmed().isEmpty();
    if (action == TextAnnotationItem::FinishAction::Cancel || empty) {
        if (item->scene() != nullptr) {
            item->scene()->removeItem(item);
        }
        delete item;
        notifyChanged();
        return;
    }

    item->setTextInteractionFlags(Qt::NoTextInteraction);
    item->clearFocus();
    if (item->scene() != nullptr) {
        item->scene()->removeItem(item);
    }
    addAnnotationItem(item);
}

bool AnnotationSceneController::previewIsLargeEnough() const
{
    if (m_previewItem == nullptr) {
        return false;
    }

    if (const auto* rect = dynamic_cast<const RectAnnotationItem*>(m_previewItem)) {
        const QRectF r = rect->rect();
        return r.width() >= kMinimumAnnotationSize && r.height() >= kMinimumAnnotationSize;
    }
    if (const auto* arrow = dynamic_cast<const ArrowAnnotationItem*>(m_previewItem)) {
        return arrow->line().length() >= kMinimumArrowLength;
    }
    if (const auto* pen = dynamic_cast<const PenAnnotationItem*>(m_previewItem)) {
        const QRectF bounds = pen->path().controlPointRect();
        return m_penPointCount >= 2
               && (bounds.width() >= kMinimumAnnotationSize || bounds.height() >= kMinimumAnnotationSize);
    }

    return true;
}

void AnnotationSceneController::removePreviewItem()
{
    if (m_previewItem == nullptr) {
        return;
    }

    if (m_previewItem->scene() != nullptr) {
        m_previewItem->scene()->removeItem(m_previewItem);
    }
    delete m_previewItem;
    m_previewItem = nullptr;
}

void AnnotationSceneController::notifyChanged()
{
    if (m_changedHandler) {
        m_changedHandler();
    }
}
