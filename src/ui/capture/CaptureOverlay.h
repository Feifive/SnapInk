#ifndef CAPTUREOVERLAY_H
#define CAPTUREOVERLAY_H

#include "CaptureAnnotation.h"
#include "CaptureTool.h"
#include "../../core/capture/CapturedScreen.h"

#include <QCursor>
#include <QImage>
#include <QPainterPath>
#include <QRect>
#include <QUndoStack>
#include <QWidget>

class CaptureToolbar;
class QGraphicsItem;
class QGraphicsPixmapItem;
class QGraphicsScene;
class QGraphicsView;
class QPainter;
class TextAnnotationItem;
class QWidget;

enum class CaptureState
{
    Selecting,
    Editing,
    Finished,
    Canceled
};

enum class SelectionHandle
{
    None,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left
};

enum class SelectionInteraction
{
    None,
    Moving,
    Resizing
};

class CaptureOverlay : public QWidget
{
    Q_OBJECT
    friend class SelectionChromeLayer;

public:
    /// @param captureResult  Per-screen physical captures with DPR metadata.
    ///                       The overlay takes ownership of the capture data.
    /// @param virtualGeometry Union of all screen logical geometries.
    /// @param parent          Optional parent widget.
    CaptureOverlay(CaptureResult captureResult,
                   const QRect& virtualGeometry,
                   QWidget* parent = nullptr);

    CaptureState state() const;
    void enterEditing(const QRect& imageRect);
    void addAnnotationItem(QGraphicsItem* item);
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();

    QImage renderResultImage() const;

signals:
    void canceled();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // ----- coordinate helpers -------------------------------------------------
    /// Widget position → logical image coordinate (identity on this overlay).
    QPoint widgetToImage(const QPointF& widgetPos) const;

    /// Widget position → selection-relative logical coordinate.
    QPointF widgetToSelection(const QPointF& widgetPos) const;

    /// Logical image rect → widget rect (identity on this overlay).
    QRect imageToWidgetRect(const QRect& imageRect) const;

    /// Overlay-local logical rect → virtual-desktop global logical rect.
    QRect overlayToGlobalLogical(const QRect& localRect) const;

    /// Virtual-desktop global logical rect → overlay-local logical rect.
    QRect globalToOverlayLogical(const QRect& globalRect) const;

    /// The effective DPR for a global logical rectangle: max DPR of intersecting screens.
    qreal effectiveDevicePixelRatio(const QRect& globalLogicalRect) const;

    // ----- selection ----------------------------------------------------------
    QRect normalizedImageSelection() const;
    QRect rectFromImagePoints(const QPoint& first, const QPoint& second) const;
    bool imagePointInSelection(const QPoint& imagePoint) const;
    bool isInsideSelection(const QPoint& pos) const;
    SelectionHandle hitTestSelectionHandle(const QPoint& pos) const;
    void updateSelectionCursor(const QPoint& pos);
    QCursor cursorForSelectionHandle(SelectionHandle handle) const;
    QPointF clampSelectionPoint(const QPointF& point) const;
    QRectF selectionSceneRect() const;
    QRect boundedSelectionRect(const QRect& rect) const;
    void beginMoveSelection(const QPoint& globalPos);
    void updateMoveSelection(const QPoint& globalPos);
    void beginResizeSelection(SelectionHandle handle, const QPoint& globalPos);
    void updateResizeSelection(const QPoint& globalPos);
    void finishSelectionInteraction();
    void resizeSelectionByWheel(int pixelDelta);
    bool canAdjustSelectionAt(const QPoint& pos) const;
    void applySelectionRect(const QRect& newRect);
    void updateSelectionBackground();
    void updateAnnotationViewGeometry();
    void updateToolbarPosition();
    void shiftAllAnnotations(qreal dx, qreal dy);
    void expandSelectionToPoint(const QPoint& imagePoint);

    // ----- annotation drawing -------------------------------------------------
    void setupAnnotationView();
    void prepareAnnotationScene();
    QPixmap selectionPixmap() const;
    void syncAnnotationViewGeometry();
    void beginAnnotation(const QPointF& selectionPos);
    void updateAnnotation(const QPointF& selectionPos);
    void finishAnnotation(const QPointF& selectionPos);
    void createTextAnnotation(const QPointF& selectionPos);
    void finishActiveTextEditing(TextAnnotationItem::FinishAction action);
    void commitActiveTextEditing();
    bool previewIsLargeEnough() const;
    void clearAnnotationState();
    void clearAnnotationsAndHistory();

    // ----- painting -----------------------------------------------------------
    void paintBackground(QPainter& painter) const;
    void paintCurrentSelectionChrome(QPainter& painter) const;
    void paintSelectionChrome(QPainter& painter,
                              const QRect& widgetSelection,
                              const QRect& selection) const;

    // ----- toolbar ------------------------------------------------------------
    void updateToolbarGeometry();
    void setCurrentTool(CaptureTool tool);

    // ----- actions ------------------------------------------------------------
    void copyAndClose();
    void saveAndClose();
    void cancelAndClose();
    void reselect();
    void pinAndClose();

    // ----- data members -------------------------------------------------------
    CaptureResult m_captureResult;
    QRect m_virtualGeometry;
    CaptureState m_state = CaptureState::Selecting;
    QPoint m_selectionStart;
    QPoint m_selectionEnd;
    QRect m_selectionImageRect;
    QUndoStack m_undoStack;
    CaptureToolbar* m_toolbar = nullptr;
    QGraphicsView* m_annotationView = nullptr;
    QGraphicsScene* m_annotationScene = nullptr;
    QGraphicsPixmapItem* m_selectionPixmapItem = nullptr;
    QGraphicsItem* m_previewItem = nullptr;
    TextAnnotationItem* m_activeTextItem = nullptr;
    QWidget* m_selectionChromeLayer = nullptr;
    CaptureTool m_currentTool = CaptureTool::Select;
    QRect m_initialSelectionRect;
    QPoint m_pressGlobalPos;
    SelectionHandle m_activeHandle = SelectionHandle::None;
    SelectionInteraction m_selectionInteraction = SelectionInteraction::None;
    QPen m_annotationPen = QPen(Qt::red, 3.0);
    QPointF m_annotationStart;
    QPainterPath m_activePath;
    int m_penPointCount = 0;
    bool m_selecting = false;
    bool m_hasSelection = false;
    bool m_drawingAnnotation = false;
};

#endif // CAPTUREOVERLAY_H
