#include "CaptureOverlay.h"

#include "AddAnnotationCommand.h"
#include "CaptureToolbar.h"
#include "../../core/clipboard/ClipboardService.h"
#include "../../core/hotkey/HotkeyConfig.h"

#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QWheelEvent>

namespace
{
constexpr int kDimAlpha = 120;
constexpr int kMinimumAnnotationSize = 2;
constexpr qreal kMinimumArrowLength = 4.0;
constexpr int kHandleRadius = 4;
constexpr int kToolbarGap = 8;

QString defaultSavePath()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (directory.isEmpty()) {
        directory = QDir::homePath();
    }

    const QString fileName = QStringLiteral("SnapInk_%1.png")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    return QDir(directory).filePath(fileName);
}
}

class SelectionChromeLayer final : public QWidget
{
public:
    explicit SelectionChromeLayer(CaptureOverlay* overlay)
        : QWidget(overlay)
        , m_overlay(overlay)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAutoFillBackground(false);
        hide();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event)

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        m_overlay->paintCurrentSelectionChrome(painter);
    }

private:
    CaptureOverlay* m_overlay = nullptr;
};

CaptureOverlay::CaptureOverlay(CaptureResult captureResult,
                               const QRect& virtualGeometry,
                               QWidget* parent)
    : QWidget(parent)
    , m_captureResult(std::move(captureResult))
    , m_virtualGeometry(virtualGeometry)
    , m_selectionModel(QRect(QPoint(0, 0), virtualGeometry.size()))
    , m_imageComposer(m_captureResult, m_virtualGeometry)
    , m_toolbar(new CaptureToolbar(this))
    , m_selectionChromeLayer(new SelectionChromeLayer(this))
{
    m_annotationPen.setCapStyle(Qt::RoundCap);
    m_annotationPen.setJoinStyle(Qt::RoundJoin);

    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::ArrowCursor);
    setGeometry(m_virtualGeometry);

    setupAnnotationView();

    m_toolbar->hide();
    m_toolbar->setCurrentTool(m_currentTool);

    connect(m_toolbar, &CaptureToolbar::toolSelected, this, &CaptureOverlay::setCurrentTool);
    connect(m_toolbar, &CaptureToolbar::undoRequested, this, &CaptureOverlay::undo);
    connect(m_toolbar, &CaptureToolbar::redoRequested, this, &CaptureOverlay::redo);
    connect(m_toolbar, &CaptureToolbar::reselectRequested, this, &CaptureOverlay::reselect);
    connect(m_toolbar, &CaptureToolbar::copyRequested, this, &CaptureOverlay::copyAndClose);
    connect(m_toolbar, &CaptureToolbar::saveRequested, this, &CaptureOverlay::saveAndClose);
    connect(m_toolbar, &CaptureToolbar::pinRequested, this, &CaptureOverlay::pinAndClose);
    connect(m_toolbar, &CaptureToolbar::cancelRequested, this, &CaptureOverlay::cancelAndClose);
    connect(m_toolbar, &CaptureToolbar::confirmRequested, this, &CaptureOverlay::copyAndClose);
    connect(&m_undoStack, &QUndoStack::canUndoChanged, m_toolbar, &CaptureToolbar::setUndoAvailable);
    connect(&m_undoStack, &QUndoStack::canRedoChanged, m_toolbar, &CaptureToolbar::setRedoAvailable);
}

CaptureState CaptureOverlay::state() const
{
    return m_state;
}

void CaptureOverlay::enterEditing(const QRect& imageRect)
{
    if (m_captureResult.screens().isEmpty()) {
        return;
    }

    CaptureSelectionModel candidate(m_selectionModel.bounds());
    candidate.setSelectionRect(imageRect);
    const QRect bounded = candidate.selectionRect();
    if (bounded.isEmpty()) {
        return;
    }

    clearAnnotationState();
    m_selectionModel.setSelectionRect(bounded);
    m_state = CaptureState::Editing;

    prepareAnnotationScene();
    syncAnnotationViewGeometry();

    setCurrentTool(CaptureTool::Select);
    setCursor(Qt::ArrowCursor);
    m_annotationView->show();
    m_annotationView->raise();
    m_selectionChromeLayer->setGeometry(rect());
    m_selectionChromeLayer->show();
    m_selectionChromeLayer->raise();
    m_toolbar->show();
    updateToolbarGeometry();
    update();
}

void CaptureOverlay::addAnnotationItem(QGraphicsItem* item)
{
    if (item == nullptr || m_state != CaptureState::Editing || m_annotationScene == nullptr) {
        delete item;
        return;
    }

    m_undoStack.push(new AddAnnotationCommand(m_annotationScene, item, [this]() {
        if (m_annotationScene != nullptr) {
            m_annotationScene->update();
        }
        updateToolbarGeometry();
    }));
}

bool CaptureOverlay::canUndo() const
{
    return m_undoStack.canUndo();
}

bool CaptureOverlay::canRedo() const
{
    return m_undoStack.canRedo();
}

void CaptureOverlay::undo()
{
    commitActiveTextEditing();
    m_undoStack.undo();
}

void CaptureOverlay::redo()
{
    commitActiveTextEditing();
    m_undoStack.redo();
}

QImage CaptureOverlay::renderResultImage() const
{
    const QRect selection = m_selectionModel.selectionRect();
    if (m_state != CaptureState::Editing || selection.isEmpty() || m_annotationScene == nullptr) {
        return {};
    }

    const_cast<CaptureOverlay*>(this)->commitActiveTextEditing();

    QImage result = m_imageComposer.createTransparentCanvas(selection);
    if (result.isNull()) {
        return {};
    }

    const qreal canvasDpr = result.devicePixelRatio();
    result.setDevicePixelRatio(1.0);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(canvasDpr, canvasDpr);

    if (m_previewItem != nullptr && m_previewItem->scene() == m_annotationScene) {
        const_cast<QGraphicsScene*>(m_annotationScene)->removeItem(m_previewItem);
        m_annotationScene->render(&painter, selectionSceneRect(), selectionSceneRect());
        const_cast<QGraphicsScene*>(m_annotationScene)->addItem(m_previewItem);
    } else {
        m_annotationScene->render(&painter, selectionSceneRect(), selectionSceneRect());
    }

    painter.end();
    result.setDevicePixelRatio(canvasDpr);
    return result;
}

void CaptureOverlay::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    paintBackground(painter);
    painter.fillRect(rect(), QColor(0, 0, 0, kDimAlpha));

    const QRect selection = normalizedImageSelection();
    if (selection.isEmpty()) {
        return;
    }

    const QRect widgetSelection = imageToWidgetRect(selection);
    painter.save();
    painter.setClipRect(widgetSelection);
    paintBackground(painter);
    painter.restore();

    if (m_annotationView == nullptr || !m_annotationView->isVisible()) {
        paintSelectionChrome(painter, widgetSelection, selection);
    }
}

void CaptureOverlay::paintBackground(QPainter& painter) const
{
    for (const CapturedScreen& screen : m_captureResult.screens()) {
        const QRect widgetRect(screen.logicalGeometry.topLeft() - m_virtualGeometry.topLeft(),
                               screen.logicalGeometry.size());
        painter.drawImage(widgetRect, screen.image);
    }
}

void CaptureOverlay::paintCurrentSelectionChrome(QPainter& painter) const
{
    const QRect selection = normalizedImageSelection();
    if (selection.isEmpty()) {
        return;
    }

    paintSelectionChrome(painter, imageToWidgetRect(selection), selection);
}

void CaptureOverlay::paintSelectionChrome(QPainter& painter,
                                          const QRect& widgetSelection,
                                          const QRect& selection) const
{
    painter.save();
    painter.setPen(QPen(QColor(47, 128, 237), 3.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(widgetSelection);

    // Only draw resize handles when selection is confirmed (Editing state),
    // not during the initial drag-selection process.
    if (m_state == CaptureState::Editing) {
        painter.setPen(QPen(QColor(47, 128, 237), 1.0));
        painter.setBrush(Qt::white);
        const QList<QPoint> handles = {
            widgetSelection.topLeft(),
            QPoint(widgetSelection.center().x(), widgetSelection.top()),
            widgetSelection.topRight(),
            QPoint(widgetSelection.right(), widgetSelection.center().y()),
            widgetSelection.bottomRight(),
            QPoint(widgetSelection.center().x(), widgetSelection.bottom()),
            widgetSelection.bottomLeft(),
            QPoint(widgetSelection.left(), widgetSelection.center().y()),
        };
        for (const QPoint& handle : handles) {
            painter.drawEllipse(handle, kHandleRadius, kHandleRadius);
        }
    }

    const QString sizeText = QStringLiteral("%1,%2  %3 x %4 px")
                                 .arg(selection.left())
                                 .arg(selection.top())
                                 .arg(selection.width())
                                 .arg(selection.height());

    QFont labelFont = painter.font();
    labelFont.setPixelSize(14);
    labelFont.setWeight(QFont::Medium);
    painter.setFont(labelFont);

    const QFontMetrics metrics(painter.font());
    const QSize textSize = metrics.size(Qt::TextSingleLine, sizeText) + QSize(16, 10);
    QPoint labelTopLeft = widgetSelection.topLeft() + QPoint(0, -textSize.height() - 4);
    if (labelTopLeft.y() < 8) {
        labelTopLeft = widgetSelection.bottomLeft() + QPoint(0, 8);
    }
    if (labelTopLeft.x() + textSize.width() > width()) {
        labelTopLeft.setX(width() - textSize.width() - 8);
    }
    labelTopLeft.setX(qMax(8, labelTopLeft.x()));

    const QRect labelRect(labelTopLeft, textSize);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 150));
    painter.drawRoundedRect(labelRect, 4.0, 4.0);
    painter.setPen(Qt::white);
    painter.drawText(labelRect, Qt::AlignCenter, sizeText);
    painter.restore();
}

QPoint CaptureOverlay::widgetToImage(const QPointF& widgetPos) const
{
    const QSize bounds = m_virtualGeometry.size();
    if (bounds.isEmpty()) {
        return {};
    }

    const int x = qBound(0, qRound(widgetPos.x()), bounds.width());
    const int y = qBound(0, qRound(widgetPos.y()), bounds.height());
    return QPoint(x, y);
}

QPointF CaptureOverlay::widgetToSelection(const QPointF& widgetPos) const
{
    const QPoint imagePoint = widgetToImage(widgetPos);
    return clampSelectionPoint(QPointF(imagePoint - m_selectionModel.selectionRect().topLeft()));
}

QRect CaptureOverlay::imageToWidgetRect(const QRect& imageRect) const
{
    return imageRect;
}

QRect CaptureOverlay::normalizedImageSelection() const
{
    return m_selectionModel.currentSelection();
}

bool CaptureOverlay::isInsideSelection(const QPoint& pos) const
{
    return m_selectionModel.contains(pos);
}

SelectionHandle CaptureOverlay::hitTestSelectionHandle(const QPoint& pos) const
{
    return m_selectionModel.hitTestHandle(pos);
}

void CaptureOverlay::updateSelectionCursor(const QPoint& pos)
{
    if (m_currentTool != CaptureTool::Select || m_state != CaptureState::Editing) {
        setCursor(Qt::CrossCursor);
        return;
    }

    const SelectionHandle handle = hitTestSelectionHandle(pos);
    if (handle != SelectionHandle::None) {
        setCursor(cursorForSelectionHandle(handle));
    } else if (isInsideSelection(pos)) {
        setCursor(Qt::SizeAllCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

QCursor CaptureOverlay::cursorForSelectionHandle(SelectionHandle handle) const
{
    switch (handle) {
    case SelectionHandle::TopLeft:
    case SelectionHandle::BottomRight:
        return Qt::SizeFDiagCursor;
    case SelectionHandle::TopRight:
    case SelectionHandle::BottomLeft:
        return Qt::SizeBDiagCursor;
    case SelectionHandle::Top:
    case SelectionHandle::Bottom:
        return Qt::SizeVerCursor;
    case SelectionHandle::Left:
    case SelectionHandle::Right:
        return Qt::SizeHorCursor;
    case SelectionHandle::None:
        return Qt::ArrowCursor;
    }
    return Qt::ArrowCursor;
}

QPointF CaptureOverlay::clampSelectionPoint(const QPointF& point) const
{
    const QRect selection = m_selectionModel.selectionRect();
    return QPointF(qBound<qreal>(0.0, point.x(), selection.width()),
                   qBound<qreal>(0.0, point.y(), selection.height()));
}

QRectF CaptureOverlay::selectionSceneRect() const
{
    return QRectF(QPointF(0, 0), QSizeF(m_selectionModel.selectionRect().size()));
}

void CaptureOverlay::beginMoveSelection(const QPoint& globalPos)
{
    commitActiveTextEditing();
    clearAnnotationState();
    m_selectionModel.beginMove(globalPos);
    setCursor(Qt::SizeAllCursor);
}

void CaptureOverlay::updateMoveSelection(const QPoint& globalPos)
{
    const QRect oldRect = m_selectionModel.selectionRect();
    if (m_selectionModel.updateMove(globalPos)) {
        applySelectionModelChange(oldRect);
    }
}

void CaptureOverlay::beginResizeSelection(SelectionHandle handle, const QPoint& globalPos)
{
    commitActiveTextEditing();
    clearAnnotationState();
    m_selectionModel.beginResize(handle, globalPos);
    setCursor(cursorForSelectionHandle(handle));
}

void CaptureOverlay::updateResizeSelection(const QPoint& globalPos)
{
    const QRect oldRect = m_selectionModel.selectionRect();
    if (m_selectionModel.updateResize(globalPos)) {
        applySelectionModelChange(oldRect);
    }
}

void CaptureOverlay::finishSelectionInteraction()
{
    m_selectionModel.finishInteraction();
}

void CaptureOverlay::resizeSelectionByWheel(int pixelDelta)
{
    const QRect oldRect = m_selectionModel.selectionRect();
    if (m_selectionModel.resizeByWheel(pixelDelta)) {
        applySelectionModelChange(oldRect);
    }
}

bool CaptureOverlay::canAdjustSelectionAt(const QPoint& pos) const
{
    return hitTestSelectionHandle(pos) != SelectionHandle::None || isInsideSelection(pos);
}

void CaptureOverlay::applySelectionModelChange(const QRect& oldRect)
{
    const QRect selection = m_selectionModel.selectionRect();
    if (selection.isEmpty() || selection == oldRect) {
        return;
    }

    const qreal dx = oldRect.left() - selection.left();
    const qreal dy = oldRect.top() - selection.top();
    if (!qFuzzyIsNull(dx) || !qFuzzyIsNull(dy)) {
        shiftAllAnnotations(dx, dy);
    }

    if (m_annotationScene != nullptr) {
        m_annotationScene->setSceneRect(selectionSceneRect());
    }
    updateSelectionBackground();
    updateAnnotationViewGeometry();
    updateToolbarPosition();
    update();
    if (m_selectionChromeLayer != nullptr) {
        m_selectionChromeLayer->update();
    }
}

void CaptureOverlay::updateSelectionBackground()
{
    if (m_annotationScene == nullptr) {
        return;
    }

    const QPixmap selectionPixmap =
        m_imageComposer.composeSelectionPixmap(m_selectionModel.selectionRect());
    if (m_selectionPixmapItem == nullptr || m_selectionPixmapItem->scene() != m_annotationScene) {
        m_selectionPixmapItem = m_annotationScene->addPixmap(selectionPixmap);
        m_selectionPixmapItem->setZValue(-1000.0);
    } else {
        m_selectionPixmapItem->setPixmap(selectionPixmap);
    }
    m_selectionPixmapItem->setPos(0, 0);
}

void CaptureOverlay::updateAnnotationViewGeometry()
{
    syncAnnotationViewGeometry();
}

void CaptureOverlay::updateToolbarPosition()
{
    updateToolbarGeometry();
}

void CaptureOverlay::shiftAllAnnotations(qreal dx, qreal dy)
{
    if (m_annotationScene == nullptr) {
        return;
    }

    for (QGraphicsItem* item : m_annotationScene->items()) {
        if (item == nullptr || item == m_selectionPixmapItem || item == m_previewItem) {
            continue;
        }
        item->moveBy(dx, dy);
    }
}

void CaptureOverlay::expandSelectionToPoint(const QPoint& imagePoint)
{
    if (m_state != CaptureState::Editing || m_selectionModel.selectionRect().isEmpty()) {
        return;
    }

    // Commit any active text editing before modifying the selection.
    commitActiveTextEditing();

    // Compute the minimal bounding rect that contains both the old selection
    // and the click point (union of old rect + click point).
    const QRect oldRect = m_selectionModel.selectionRect();
    QRect expanded = oldRect;

    if (imagePoint.x() < oldRect.left()) {
        expanded.setLeft(imagePoint.x());
    } else if (imagePoint.x() > oldRect.right()) {
        expanded.setRight(imagePoint.x());
    }

    if (imagePoint.y() < oldRect.top()) {
        expanded.setTop(imagePoint.y());
    } else if (imagePoint.y() > oldRect.bottom()) {
        expanded.setBottom(imagePoint.y());
    }

    m_selectionModel.setSelectionRect(expanded);

    // Apply the model change, which handles:
    // - shifting existing annotations to compensate for origin changes
    // - updating scene rect, selection background, annotation view geometry
    // - repositioning toolbar and refreshing chrome
    applySelectionModelChange(oldRect);
}

void CaptureOverlay::setupAnnotationView()
{
    m_annotationScene = new QGraphicsScene(this);
    m_annotationView = new QGraphicsView(m_annotationScene, this);
    m_annotationView->setObjectName(QStringLiteral("AnnotationGraphicsView"));
    m_annotationView->setFrameShape(QFrame::NoFrame);
    m_annotationView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_annotationView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_annotationView->setTransformationAnchor(QGraphicsView::NoAnchor);
    m_annotationView->setResizeAnchor(QGraphicsView::NoAnchor);
    m_annotationView->setDragMode(QGraphicsView::NoDrag);
    m_annotationView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    m_annotationView->setInteractive(true);
    m_annotationView->setMouseTracking(true);
    m_annotationView->setAutoFillBackground(false);
    m_annotationView->setStyleSheet(QStringLiteral("background: transparent"));
    m_annotationView->viewport()->setAutoFillBackground(false);
    m_annotationView->viewport()->setAttribute(Qt::WA_TranslucentBackground, true);
    m_annotationView->viewport()->installEventFilter(this);
    m_annotationView->hide();
}

void CaptureOverlay::prepareAnnotationScene()
{
    m_undoStack.clear();
    m_annotationScene->clear();
    m_annotationScene->setSceneRect(selectionSceneRect());
    const QPixmap selectionPixmap =
        m_imageComposer.composeSelectionPixmap(m_selectionModel.selectionRect());
    m_selectionPixmapItem = m_annotationScene->addPixmap(selectionPixmap);
    m_selectionPixmapItem->setPos(0, 0);
    m_selectionPixmapItem->setZValue(-1000.0);
    m_previewItem = nullptr;
    m_activeTextItem = nullptr;
}

void CaptureOverlay::syncAnnotationViewGeometry()
{
    const QRect selection = m_selectionModel.selectionRect();
    if (m_annotationView == nullptr || selection.isEmpty()) {
        return;
    }

    m_annotationView->resetTransform();
    m_annotationView->setSceneRect(selectionSceneRect());
    m_annotationView->setGeometry(imageToWidgetRect(selection));
    if (m_selectionChromeLayer != nullptr) {
        m_selectionChromeLayer->setGeometry(rect());
        if (m_state == CaptureState::Editing) {
            m_selectionChromeLayer->show();
            m_selectionChromeLayer->raise();
        }
    }
}

void CaptureOverlay::beginAnnotation(const QPointF& selectionPos)
{
    commitActiveTextEditing();
    clearAnnotationState();
    m_drawingAnnotation = true;
    m_annotationStart = clampSelectionPoint(selectionPos);

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
        m_annotationScene->addItem(m_previewItem);
    }
}

void CaptureOverlay::updateAnnotation(const QPointF& selectionPos)
{
    if (!m_drawingAnnotation || m_annotationScene == nullptr) {
        return;
    }

    if (m_previewItem != nullptr) {
        m_annotationScene->removeItem(m_previewItem);
        delete m_previewItem;
        m_previewItem = nullptr;
    }

    const QPointF current = clampSelectionPoint(selectionPos);
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
        m_annotationScene->addItem(m_previewItem);
    }
}

void CaptureOverlay::finishAnnotation(const QPointF& selectionPos)
{
    updateAnnotation(selectionPos);
    m_drawingAnnotation = false;

    const bool valid = previewIsLargeEnough();
    QGraphicsItem* finishedItem = m_previewItem;
    m_previewItem = nullptr;

    if (finishedItem != nullptr && valid) {
        m_annotationScene->removeItem(finishedItem);
        addAnnotationItem(finishedItem);
    } else {
        delete finishedItem;
    }

    clearAnnotationState();
}

void CaptureOverlay::createTextAnnotation(const QPointF& selectionPos)
{
    commitActiveTextEditing();

    auto* item = new TextAnnotationItem(clampSelectionPoint(selectionPos),
                                       m_annotationPen.color());
    const qreal remainingWidth = qMax<qreal>(1.0, m_selectionModel.selectionRect().width() - item->pos().x());
    item->setMaximumTextWidth(remainingWidth);
    item->setFinishedHandler([this](TextAnnotationItem* textItem, TextAnnotationItem::FinishAction action) {
        if (textItem == m_activeTextItem) {
            finishActiveTextEditing(action);
        }
    });

    m_activeTextItem = item;
    m_annotationScene->addItem(item);
    item->setFocus(Qt::MouseFocusReason);
}

void CaptureOverlay::finishActiveTextEditing(TextAnnotationItem::FinishAction action)
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
        return;
    }

    item->setTextInteractionFlags(Qt::NoTextInteraction);
    item->clearFocus();
    if (item->scene() != nullptr) {
        item->scene()->removeItem(item);
    }
    addAnnotationItem(item);
}

void CaptureOverlay::commitActiveTextEditing()
{
    finishActiveTextEditing(TextAnnotationItem::FinishAction::Commit);
}

bool CaptureOverlay::previewIsLargeEnough() const
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

void CaptureOverlay::clearAnnotationState()
{
    if (m_previewItem != nullptr) {
        if (m_previewItem->scene() != nullptr) {
            m_previewItem->scene()->removeItem(m_previewItem);
        }
        delete m_previewItem;
        m_previewItem = nullptr;
    }
    m_activePath = QPainterPath();
    m_penPointCount = 0;
    m_drawingAnnotation = false;
}

void CaptureOverlay::clearAnnotationsAndHistory()
{
    if (m_activeTextItem != nullptr) {
        if (m_activeTextItem->scene() != nullptr) {
            m_activeTextItem->scene()->removeItem(m_activeTextItem);
        }
        delete m_activeTextItem;
        m_activeTextItem = nullptr;
    }
    clearAnnotationState();
    m_undoStack.clear();
    if (m_annotationScene != nullptr) {
        m_annotationScene->clear();
    }
    m_selectionPixmapItem = nullptr;
    if (m_annotationView != nullptr) {
        m_annotationView->hide();
    }
    if (m_selectionChromeLayer != nullptr) {
        m_selectionChromeLayer->hide();
    }
    m_toolbar->setUndoAvailable(false);
    m_toolbar->setRedoAvailable(false);
}

void CaptureOverlay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_selectionChromeLayer != nullptr) {
        m_selectionChromeLayer->setGeometry(rect());
    }
    syncAnnotationViewGeometry();
    updateToolbarGeometry();
}

void CaptureOverlay::mousePressEvent(QMouseEvent* event)
{
    // --- Right button: context-dependent behavior ---
    if (event->button() == Qt::RightButton) {
        event->accept();
        if (m_state == CaptureState::Editing) {
            reselect();
        } else {
            cancelAndClose();
        }
        return;
    }

    if (event->button() != Qt::LeftButton) {
        return;
    }

    const QPoint imagePoint = widgetToImage(event->position());
    if (m_state == CaptureState::Selecting) {
        m_selectionModel.beginSelection(imagePoint);
        if (m_annotationView != nullptr) {
            m_annotationView->hide();
        }
        if (m_selectionChromeLayer != nullptr) {
            m_selectionChromeLayer->hide();
        }
        update();
        return;
    }

    if (m_state == CaptureState::Editing && m_currentTool == CaptureTool::Select) {
        const SelectionHandle handle = hitTestSelectionHandle(imagePoint);
        if (handle != SelectionHandle::None) {
            beginResizeSelection(handle, imagePoint);
            return;
        }
        if (isInsideSelection(imagePoint)) {
            beginMoveSelection(imagePoint);
        } else {
            // Left click outside selection → expand selection to include click point
            expandSelectionToPoint(imagePoint);
        }
    }
}

void CaptureOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (m_state == CaptureState::Selecting && m_selectionModel.hasSelection()) {
        m_selectionModel.updateSelection(widgetToImage(event->position()));
        update();
        return;
    }

    if (m_state == CaptureState::Editing && m_currentTool == CaptureTool::Select) {
        const QPoint imagePoint = widgetToImage(event->position());
        if (m_selectionModel.interaction() == SelectionInteraction::Moving) {
            updateMoveSelection(imagePoint);
            return;
        }
        if (m_selectionModel.interaction() == SelectionInteraction::Resizing) {
            updateResizeSelection(imagePoint);
            return;
        }
        updateSelectionCursor(imagePoint);
    }
}

void CaptureOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }

    if (m_state == CaptureState::Selecting && m_selectionModel.hasSelection()) {
        m_selectionModel.updateSelection(widgetToImage(event->position()));
        if (!m_selectionModel.commitSelection()) {
            update();
            return;
        }

        enterEditing(m_selectionModel.selectionRect());
        return;
    }

    if (m_state == CaptureState::Editing && m_currentTool == CaptureTool::Select) {
        finishSelectionInteraction();
    }
}

void CaptureOverlay::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        if (m_activeTextItem != nullptr) {
            finishActiveTextEditing(m_activeTextItem->toPlainText().trimmed().isEmpty()
                                        ? TextAnnotationItem::FinishAction::Cancel
                                        : TextAnnotationItem::FinishAction::Commit);
            return;
        }
        cancelAndClose();
        return;
    }

    const bool control = event->modifiers().testFlag(Qt::ControlModifier);
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    if (control && event->key() == Qt::Key_Z && !shift) {
        undo();
        return;
    }
    if ((control && event->key() == Qt::Key_Y) || (control && shift && event->key() == Qt::Key_Z)) {
        redo();
        return;
    }

    // Pin shortcut: Ctrl+2.  On macOS, Qt::MetaModifier maps to the physical
    // Control key, matching the Carbon global-hotkey backend where
    // Qt::ControlModifier → physical Control.
#ifdef Q_OS_MACOS
    const bool pinModifier = event->modifiers().testFlag(Qt::MetaModifier);
#else
    const bool pinModifier = control;
#endif
    if (pinModifier && event->key() == Qt::Key_2) {
        if (m_selectionModel.hasSelection() && m_state == CaptureState::Editing) {
            pinAndClose();
        }
        return;
    }

    QWidget::keyPressEvent(event);
}

void CaptureOverlay::contextMenuEvent(QContextMenuEvent* event)
{
    // Suppress default context menu; right-click is handled in mousePressEvent.
    event->accept();
}

bool CaptureOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_annotationView->viewport() || m_state != CaptureState::Editing) {
        return QWidget::eventFilter(watched, event);
    }

    // Forward key presses from the annotation view to the overlay so that
    // keyboard shortcuts (Ctrl+Z, Escape, Ctrl+2 pin, …) work even when the
    // graphics view or one of its items has keyboard focus.
    // Only intercept shortcut combinations; let plain character keys pass
    // through so that text editing still receives typed input.
    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        const bool hasModifier = keyEvent->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
        const bool isEscape = keyEvent->key() == Qt::Key_Escape;
        if (hasModifier || isEscape) {
            keyPressEvent(keyEvent);
            return keyEvent->isAccepted();
        }
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::RightButton) {
            // In Editing state, right-click on annotation view → reselect
            reselect();
            return true;
        }
        if (mouseEvent->button() != Qt::LeftButton) {
            return true;
        }

        if (m_activeTextItem != nullptr && m_currentTool == CaptureTool::Select) {
            QGraphicsItem* hitItem = m_annotationView->itemAt(mouseEvent->pos());
            if (hitItem == m_activeTextItem) {
                return QWidget::eventFilter(watched, event);
            }
            commitActiveTextEditing();
        } else if (m_activeTextItem != nullptr) {
            QGraphicsItem* hitItem = m_annotationView->itemAt(mouseEvent->pos());
            if (hitItem == m_activeTextItem) {
                return QWidget::eventFilter(watched, event);
            }
            commitActiveTextEditing();
            return true;
        }

        if (m_currentTool == CaptureTool::Select) {
            const QPoint overlayPos = m_annotationView->viewport()->mapTo(this, mouseEvent->pos());
            const SelectionHandle handle = hitTestSelectionHandle(overlayPos);
            if (handle != SelectionHandle::None) {
                beginResizeSelection(handle, overlayPos);
            } else if (isInsideSelection(overlayPos)) {
                beginMoveSelection(overlayPos);
            }
            return true;
        }

        const QPointF selectionPos = clampSelectionPoint(mouseEvent->position());
        if (m_currentTool == CaptureTool::Text) {
            createTextAnnotation(selectionPos);
        } else {
            beginAnnotation(selectionPos);
        }
        return true;
    }

    if (event->type() == QEvent::MouseMove) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (m_currentTool == CaptureTool::Select) {
            const QPoint overlayPos = m_annotationView->viewport()->mapTo(this, mouseEvent->pos());
            if (m_selectionModel.interaction() == SelectionInteraction::Moving) {
                updateMoveSelection(overlayPos);
                return true;
            }
            if (m_selectionModel.interaction() == SelectionInteraction::Resizing) {
                updateResizeSelection(overlayPos);
                return true;
            }
            updateSelectionCursor(overlayPos);
            return true;
        }
        if (m_drawingAnnotation) {
            updateAnnotation(mouseEvent->position());
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && m_currentTool == CaptureTool::Select) {
            finishSelectionInteraction();
            return true;
        }
        if (mouseEvent->button() == Qt::LeftButton && m_drawingAnnotation) {
            finishAnnotation(mouseEvent->position());
            return true;
        }
    }

    if (event->type() == QEvent::Wheel && m_currentTool == CaptureTool::Select) {
        auto* wheelEvent = static_cast<QWheelEvent*>(event);
        const QPoint overlayPos = m_annotationView->viewport()->mapTo(this, wheelEvent->position().toPoint());
        if (canAdjustSelectionAt(overlayPos)) {
            if (wheelEvent->angleDelta().y() > 0) {
                resizeSelectionByWheel(1);
            } else if (wheelEvent->angleDelta().y() < 0) {
                resizeSelectionByWheel(-1);
            }
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void CaptureOverlay::updateToolbarGeometry()
{
    const QRect selection = m_selectionModel.selectionRect();
    if (m_toolbar == nullptr || m_state != CaptureState::Editing || selection.isEmpty()) {
        if (m_toolbar != nullptr) {
            m_toolbar->hide();
        }
        return;
    }

    m_toolbar->adjustSize();
    const QSize toolbarSize = m_toolbar->sizeHint();
    const QRect widgetSelection = imageToWidgetRect(selection);
    int x = widgetSelection.x() + widgetSelection.width() - toolbarSize.width();
    int y = widgetSelection.bottom() + kToolbarGap;

    if (y + toolbarSize.height() > height()) {
        y = widgetSelection.top() - toolbarSize.height() - kToolbarGap;
    }

    x = qBound(8, x, qMax(8, width() - toolbarSize.width() - 8));
    y = qBound(8, y, qMax(8, height() - toolbarSize.height() - 8));

    m_toolbar->setGeometry(QRect(QPoint(x, y), toolbarSize));
    m_toolbar->show();
    if (m_selectionChromeLayer != nullptr) {
        m_selectionChromeLayer->raise();
    }
    m_toolbar->raise();
}

void CaptureOverlay::setCurrentTool(CaptureTool tool)
{
    commitActiveTextEditing();
    clearAnnotationState();
    finishSelectionInteraction();
    m_currentTool = tool;
    m_toolbar->setCurrentTool(tool);
    setCursor(tool == CaptureTool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
}

void CaptureOverlay::copyAndClose()
{
    const QImage result = renderResultImage();
    if (!result.isNull()) {
        ClipboardService::copyImage(result);
    }

    m_state = CaptureState::Finished;
    close();
}

void CaptureOverlay::saveAndClose()
{
    const QImage result = renderResultImage();
    if (result.isNull()) {
        return;
    }

    const QString fileName = QFileDialog::getSaveFileName(this,
                                                          QStringLiteral("Save Screenshot"),
                                                          defaultSavePath(),
                                                          QStringLiteral("PNG Images (*.png)"));
    if (fileName.isEmpty()) {
        return;
    }

    if (!result.save(fileName, "PNG")) {
        QMessageBox::critical(this,
                              QStringLiteral("Save Failed"),
                              QStringLiteral("Could not save the PNG file."));
        return;
    }

    m_state = CaptureState::Finished;
    close();
}

void CaptureOverlay::cancelAndClose()
{
    m_state = CaptureState::Canceled;
    Q_EMIT canceled();
    close();
}

void CaptureOverlay::reselect()
{
    clearAnnotationsAndHistory();
    m_state = CaptureState::Selecting;
    m_selectionModel.clear();
    m_toolbar->hide();
    update();
}

void CaptureOverlay::pinAndClose()
{
    const QImage result = renderResultImage();
    if (result.isNull()) {
        QMessageBox::critical(this,
                              QStringLiteral("Pin Failed"),
                              QStringLiteral("Could not create a pin from the current selection."));
        return;
    }

    const QRect selectionGlobal = m_imageComposer.toGlobalLogical(m_selectionModel.selectionRect());

    Q_EMIT pinRequested(result, selectionGlobal);

    m_state = CaptureState::Finished;
    close();
}
