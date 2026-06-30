#include "CaptureOverlay.h"

#include "CaptureToolbar.h"
#include "../../core/clipboard/ClipboardService.h"
#include "../../core/hotkey/HotkeyConfig.h"

#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QStandardPaths>

namespace
{
constexpr int kDimAlpha = 120;
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
    , m_inputController(m_annotationController, m_selectionModel, makeInputCallbacks())
    , m_toolbar(new CaptureToolbar(this))
    , m_selectionChromeLayer(new SelectionChromeLayer(this))
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::ArrowCursor);
    setGeometry(m_virtualGeometry);

    setupAnnotationView();

    m_toolbar->hide();
    m_toolbar->setCurrentTool(m_annotationController.currentTool());

    connect(m_toolbar, &CaptureToolbar::toolSelected, this, &CaptureOverlay::setCurrentTool);
    connect(m_toolbar, &CaptureToolbar::undoRequested, this, &CaptureOverlay::undo);
    connect(m_toolbar, &CaptureToolbar::redoRequested, this, &CaptureOverlay::redo);
    connect(m_toolbar, &CaptureToolbar::reselectRequested, this, &CaptureOverlay::reselect);
    connect(m_toolbar, &CaptureToolbar::copyRequested, this, &CaptureOverlay::copyAndClose);
    connect(m_toolbar, &CaptureToolbar::saveRequested, this, &CaptureOverlay::saveAndClose);
    connect(m_toolbar, &CaptureToolbar::pinRequested, this, &CaptureOverlay::pinAndClose);
    connect(m_toolbar, &CaptureToolbar::cancelRequested, this, &CaptureOverlay::cancelAndClose);
    connect(m_toolbar, &CaptureToolbar::confirmRequested, this, &CaptureOverlay::copyAndClose);

    m_annotationController.setChangedHandler([this]() {
        syncUndoRedoAvailability();
        updateToolbarGeometry();
    });
    syncUndoRedoAvailability();
}

CaptureOverlay::~CaptureOverlay()
{
    if (QGraphicsView* annotationView = m_annotationController.view()) {
        if (QWidget* viewport = annotationView->viewport()) {
            viewport->removeEventFilter(this);
        }
    }
    m_annotationController.setChangedHandler({});
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

    m_annotationController.clearInteractionState();
    m_selectionModel.setSelectionRect(bounded);
    m_state = CaptureState::Editing;

    prepareAnnotationScene();
    syncAnnotationViewGeometry();

    setCurrentTool(CaptureTool::Select);
    setCursor(Qt::ArrowCursor);
    m_annotationController.view()->show();
    m_annotationController.view()->raise();
    m_selectionChromeLayer->setGeometry(rect());
    m_selectionChromeLayer->show();
    m_selectionChromeLayer->raise();
    m_toolbar->show();
    updateToolbarGeometry();
    update();
}

void CaptureOverlay::addAnnotationItem(QGraphicsItem* item)
{
    if (item == nullptr || m_state != CaptureState::Editing) {
        delete item;
        return;
    }

    m_annotationController.addAnnotationItem(item);
}

bool CaptureOverlay::canUndo() const
{
    return m_annotationController.canUndo();
}

bool CaptureOverlay::canRedo() const
{
    return m_annotationController.canRedo();
}

void CaptureOverlay::undo()
{
    m_annotationController.undo();
}

void CaptureOverlay::redo()
{
    m_annotationController.redo();
}

QImage CaptureOverlay::renderResultImage()
{
    const QRect selection = m_selectionModel.selectionRect();
    if (m_state != CaptureState::Editing || selection.isEmpty()) {
        return {};
    }

    QImage result = m_imageComposer.createTransparentCanvas(selection);
    if (result.isNull()) {
        return {};
    }

    const qreal canvasDpr = result.devicePixelRatio();
    result.setDevicePixelRatio(1.0);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(canvasDpr, canvasDpr);

    m_annotationController.renderScene(&painter, selectionSceneRect());

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

    if (m_annotationController.view() == nullptr || !m_annotationController.view()->isVisible()) {
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
    if (m_annotationController.currentTool() != CaptureTool::Select || m_state != CaptureState::Editing) {
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
    m_annotationController.commitActiveTextEditing();
    m_annotationController.clearInteractionState();
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
    m_annotationController.commitActiveTextEditing();
    m_annotationController.clearInteractionState();
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

    m_annotationController.setSceneRect(selectionSceneRect());
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
    const QPixmap selectionPixmap =
        QPixmap::fromImage(m_imageComposer.composeSelectionImage(m_selectionModel.selectionRect()));
    m_annotationController.setBackgroundPixmap(selectionPixmap);
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
    m_annotationController.shiftAllAnnotations(QPointF(dx, dy));
}

void CaptureOverlay::expandSelectionToPoint(const QPoint& imagePoint)
{
    if (m_state != CaptureState::Editing || m_selectionModel.selectionRect().isEmpty()) {
        return;
    }

    // Commit any active text editing before modifying the selection.
    m_annotationController.commitActiveTextEditing();

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

CaptureInputController::Callbacks CaptureOverlay::makeInputCallbacks()
{
    CaptureInputController::Callbacks callbacks;
    callbacks.viewportToOverlayPos = [this](const QPoint& viewportPos) {
        QGraphicsView* annotationView = m_annotationController.view();
        if (annotationView == nullptr || annotationView->viewport() == nullptr) {
            return QPoint();
        }
        return annotationView->viewport()->mapTo(this, viewportPos);
    };
    callbacks.viewportToSelectionPos = [this](const QPointF& viewportPos) {
        return clampSelectionPoint(viewportPos);
    };
    callbacks.hitTestHandle = [this](const QPoint& overlayPos) {
        return hitTestSelectionHandle(overlayPos);
    };
    callbacks.isInsideSelection = [this](const QPoint& overlayPos) {
        return isInsideSelection(overlayPos);
    };
    callbacks.canAdjustSelectionAt = [this](const QPoint& overlayPos) {
        return canAdjustSelectionAt(overlayPos);
    };
    callbacks.beginResize = [this](SelectionHandle handle, const QPoint& overlayPos) {
        beginResizeSelection(handle, overlayPos);
    };
    callbacks.updateResize = [this](const QPoint& overlayPos) {
        updateResizeSelection(overlayPos);
    };
    callbacks.beginMove = [this](const QPoint& overlayPos) {
        beginMoveSelection(overlayPos);
    };
    callbacks.updateMove = [this](const QPoint& overlayPos) {
        updateMoveSelection(overlayPos);
    };
    callbacks.finishSelectionInteraction = [this]() {
        finishSelectionInteraction();
    };
    callbacks.resizeSelectionByWheel = [this](int pixelDelta) {
        resizeSelectionByWheel(pixelDelta);
    };
    callbacks.updateSelectionCursor = [this](const QPoint& overlayPos) {
        updateSelectionCursor(overlayPos);
    };
    callbacks.beginAnnotation = [this](const QPointF& selectionPos) {
        m_annotationController.beginAnnotation(selectionPos);
    };
    callbacks.updateAnnotation = [this](const QPointF& selectionPos) {
        m_annotationController.updateAnnotation(selectionPos);
    };
    callbacks.finishAnnotation = [this](const QPointF& selectionPos) {
        m_annotationController.finishAnnotation(selectionPos);
    };
    callbacks.beginTextEditing = [this](const QPointF& selectionPos) {
        m_annotationController.beginTextEditing(selectionPos);
    };
    callbacks.commitActiveTextEditing = [this]() {
        m_annotationController.commitActiveTextEditing();
    };
    callbacks.activeTextItemContainsViewPos = [this](const QPoint& viewportPos) {
        return m_annotationController.activeTextItemContainsViewPos(viewportPos);
    };
    callbacks.forwardShortcut = [this](QKeyEvent* event) {
        keyPressEvent(event);
    };
    callbacks.reselect = [this]() {
        reselect();
    };
    return callbacks;
}

void CaptureOverlay::setupAnnotationView()
{
    QGraphicsView* annotationView = m_annotationController.view();
    annotationView->setParent(this);
    annotationView->viewport()->installEventFilter(this);
    annotationView->hide();
}

void CaptureOverlay::prepareAnnotationScene()
{
    m_annotationController.clearAll();
    m_annotationController.setSceneRect(selectionSceneRect());
    const QPixmap selectionPixmap =
        QPixmap::fromImage(m_imageComposer.composeSelectionImage(m_selectionModel.selectionRect()));
    m_annotationController.setBackgroundPixmap(selectionPixmap);
}

void CaptureOverlay::syncAnnotationViewGeometry()
{
    const QRect selection = m_selectionModel.selectionRect();
    QGraphicsView* annotationView = m_annotationController.view();
    if (annotationView == nullptr || selection.isEmpty()) {
        return;
    }

    annotationView->resetTransform();
    m_annotationController.setSceneRect(selectionSceneRect());
    annotationView->setGeometry(imageToWidgetRect(selection));
    if (m_selectionChromeLayer != nullptr) {
        m_selectionChromeLayer->setGeometry(rect());
        if (m_state == CaptureState::Editing) {
            m_selectionChromeLayer->show();
            m_selectionChromeLayer->raise();
        }
    }
}

void CaptureOverlay::clearAnnotationsAndHistory()
{
    m_annotationController.clearAll();
    m_annotationController.view()->hide();
    if (m_selectionChromeLayer != nullptr) {
        m_selectionChromeLayer->hide();
    }
    syncUndoRedoAvailability();
}

void CaptureOverlay::syncUndoRedoAvailability()
{
    if (m_toolbar == nullptr) {
        return;
    }

    m_toolbar->setUndoAvailable(m_annotationController.canUndo());
    m_toolbar->setRedoAvailable(m_annotationController.canRedo());
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
        m_annotationController.view()->hide();
        if (m_selectionChromeLayer != nullptr) {
            m_selectionChromeLayer->hide();
        }
        update();
        return;
    }

    if (m_state == CaptureState::Editing && m_annotationController.currentTool() == CaptureTool::Select) {
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

    if (m_state == CaptureState::Editing && m_annotationController.currentTool() == CaptureTool::Select) {
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

    if (m_state == CaptureState::Editing && m_annotationController.currentTool() == CaptureTool::Select) {
        finishSelectionInteraction();
    }
}

void CaptureOverlay::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        if (m_annotationController.hasActiveTextEditing()) {
            m_annotationController.commitActiveTextEditing();
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
    if (m_inputController.handleViewportEvent(watched,
                                              event,
                                              m_annotationController.view(),
                                              m_state == CaptureState::Editing)) {
        return true;
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
    m_annotationController.setCurrentTool(tool);
    finishSelectionInteraction();
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

    const QRect selectionGlobal =
        m_imageComposer.overlayLocalToGlobalLogical(m_selectionModel.selectionRect());

    Q_EMIT pinRequested(result, selectionGlobal);

    m_state = CaptureState::Finished;
    close();
}
