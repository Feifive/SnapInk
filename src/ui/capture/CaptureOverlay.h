#ifndef CAPTUREOVERLAY_H
#define CAPTUREOVERLAY_H

#include "CaptureAnnotation.h"
#include "CaptureTool.h"
#include "../../core/capture/CapturedScreen.h"

#include <QImage>
#include <QPainterPath>
#include <QRect>
#include <QUndoStack>
#include <QWidget>

class CaptureToolbar;
class QPainter;

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
    void addAnnotation(std::unique_ptr<Annotation> annotation);
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();

    /// Export the current selection at original physical resolution.
    /// Annotations are rendered at the effective device pixel ratio so
    /// everything is sharp on high-DPI displays.
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

private:
    // ----- coordinate helpers -------------------------------------------------
    /// Widget position → logical image coordinate (identity on this overlay).
    QPoint widgetToImage(const QPointF& widgetPos) const;

    /// Widget position → selection-relative logical coordinate.
    QPointF widgetToSelection(const QPointF& widgetPos) const;

    /// Logical image rect → widget rect (identity on this overlay).
    QRect imageToWidgetRect(const QRect& imageRect) const;

    /// The effective DPR for a logical rectangle: max DPR of intersecting screens.
    qreal effectiveDevicePixelRatio(const QRect& logicalRect) const;

    // ----- selection ----------------------------------------------------------
    QRect normalizedImageSelection() const;
    QRect rectFromImagePoints(const QPoint& first, const QPoint& second) const;
    bool imagePointInSelection(const QPoint& imagePoint) const;
    QPointF clampSelectionPoint(const QPointF& point) const;

    // ----- annotation drawing -------------------------------------------------
    void beginAnnotation(const QPointF& selectionPos);
    void updateAnnotation(const QPointF& selectionPos);
    void finishAnnotation(const QPointF& selectionPos);
    void createTextAnnotation(const QPointF& selectionPos);
    bool previewIsLargeEnough() const;
    void clearAnnotationState();
    void clearAnnotationsAndHistory();

    // ----- painting -----------------------------------------------------------
    void paintBackground(QPainter& painter) const;
    void paintSelection(QPainter& painter, const QRect& selection) const;
    void paintAnnotations(QPainter& painter,
                          const QRect& widgetSelection,
                          bool includePreview) const;
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
    void showPinPlaceholder();

    // ----- data members -------------------------------------------------------
    CaptureResult m_captureResult;
    QRect m_virtualGeometry;
    CaptureState m_state = CaptureState::Selecting;
    QPoint m_selectionStart;
    QPoint m_selectionEnd;
    QRect m_selectionImageRect;
    AnnotationList m_annotations;
    std::unique_ptr<Annotation> m_previewAnnotation;
    QUndoStack m_undoStack;
    CaptureToolbar* m_toolbar = nullptr;
    CaptureTool m_currentTool = CaptureTool::Rect;
    QPen m_annotationPen = QPen(Qt::red, 3.0);
    QPointF m_annotationStart;
    QPainterPath m_activePath;
    int m_penPointCount = 0;
    bool m_selecting = false;
    bool m_hasSelection = false;
    bool m_drawingAnnotation = false;
};

#endif // CAPTUREOVERLAY_H
