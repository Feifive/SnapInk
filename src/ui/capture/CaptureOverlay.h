#ifndef CAPTUREOVERLAY_H
#define CAPTUREOVERLAY_H

#include "AnnotationSceneController.h"
#include "CaptureSelectionModel.h"
#include "CaptureTool.h"
#include "../../core/capture/CaptureImageComposer.h"
#include "../../core/capture/CapturedScreen.h"

#include <QCursor>
#include <QImage>
#include <QRect>
#include <QWidget>

class CaptureToolbar;
class QGraphicsItem;
class QGraphicsView;
class QPainter;
class QWidget;

enum class CaptureState
{
    Selecting,
    Editing,
    Finished,
    Canceled
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
    ~CaptureOverlay() override;

    CaptureState state() const;
    void enterEditing(const QRect& imageRect);
    void addAnnotationItem(QGraphicsItem* item);
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();

    QImage renderResultImage();

signals:
    void canceled();
    void pinRequested(const QImage& image, const QRect& sourceGlobalRect);

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

    // ----- selection ----------------------------------------------------------
    QRect normalizedImageSelection() const;
    bool isInsideSelection(const QPoint& pos) const;
    SelectionHandle hitTestSelectionHandle(const QPoint& pos) const;
    void updateSelectionCursor(const QPoint& pos);
    QCursor cursorForSelectionHandle(SelectionHandle handle) const;
    QPointF clampSelectionPoint(const QPointF& point) const;
    QRectF selectionSceneRect() const;
    void beginMoveSelection(const QPoint& globalPos);
    void updateMoveSelection(const QPoint& globalPos);
    void beginResizeSelection(SelectionHandle handle, const QPoint& globalPos);
    void updateResizeSelection(const QPoint& globalPos);
    void finishSelectionInteraction();
    void resizeSelectionByWheel(int pixelDelta);
    bool canAdjustSelectionAt(const QPoint& pos) const;
    void applySelectionModelChange(const QRect& oldRect);
    void updateSelectionBackground();
    void updateAnnotationViewGeometry();
    void updateToolbarPosition();
    void shiftAllAnnotations(qreal dx, qreal dy);
    void expandSelectionToPoint(const QPoint& imagePoint);

    // ----- annotation drawing -------------------------------------------------
    void setupAnnotationView();
    void prepareAnnotationScene();
    void syncAnnotationViewGeometry();
    void clearAnnotationsAndHistory();
    void syncUndoRedoAvailability();

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
    CaptureSelectionModel m_selectionModel;
    CaptureImageComposer m_imageComposer;
    CaptureState m_state = CaptureState::Selecting;
    CaptureToolbar* m_toolbar = nullptr;
    AnnotationSceneController m_annotationController;
    QWidget* m_selectionChromeLayer = nullptr;
};

#endif // CAPTUREOVERLAY_H
