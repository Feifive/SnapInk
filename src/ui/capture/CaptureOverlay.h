#ifndef CAPTUREOVERLAY_H
#define CAPTUREOVERLAY_H

#include "AnnotationSceneController.h"
#include "CaptureInputController.h"
#include "CaptureSelectionModel.h"
#include "CaptureTool.h"
#include "../../core/capture/CaptureImageComposer.h"
#include "../../core/capture/CapturedScreen.h"

#include <QCursor>
#include <QImage>
#include <QRect>
#include <QString>
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

enum class CaptureOverlayMode
{
    InteractiveEdit,
    GlobalDragPin
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
                   CaptureOverlayMode mode = CaptureOverlayMode::InteractiveEdit,
                   QWidget* parent = nullptr);
    ~CaptureOverlay() override;

    CaptureState state() const;
    void enterEditing(const QRect& imageRect);
    void beginExternalSelection(const QPoint& imagePos);
    void updateExternalSelection(const QPoint& imagePos);
    void finishExternalSelectionAndPin(const QPoint& imagePos);
    void cancelExternalSelection();
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
    virtual QImage buildResultImageForAction();
    virtual void copyResultImage(const QImage& image);
    virtual QString requestSavePath();
    virtual bool saveResultImage(const QImage& image, const QString& fileName);
    virtual void showSaveFailedMessage();
    virtual void showPinFailedMessage();

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
    QPoint widgetToImagePos(const QPointF& widgetPos) const;

    /// Logical image rect → widget rect (identity on this overlay).
    QRect imageToWidgetRect(const QRect& imageRect) const;

    // ----- selection ----------------------------------------------------------
    QRect normalizedImageSelection() const;
    bool isInsideSelection(const QPoint& overlayPos) const;
    SelectionHandle hitTestSelectionHandle(const QPoint& overlayPos) const;
    void updateSelectionCursor(const QPoint& overlayPos);
    QCursor cursorForSelectionHandle(SelectionHandle handle) const;
    QPointF clampSelectionPoint(const QPointF& selectionPos) const;
    QRectF selectionSceneRect() const;
    void beginMoveSelection(const QPoint& overlayPos);
    void updateMoveSelection(const QPoint& overlayPos);
    void beginResizeSelection(SelectionHandle handle, const QPoint& overlayPos);
    void updateResizeSelection(const QPoint& overlayPos);
    void finishSelectionInteraction();
    void resizeSelectionByWheel(int pixelDelta);
    bool canAdjustSelectionAt(const QPoint& overlayPos) const;
    void applySelectionModelChange(const QRect& oldRect);
    void updateSelectionBackground();
    void expandSelectionToImagePos(const QPoint& imagePos);
    QRect normalizeSelectionForEditing(const QRect& imageRect) const;

    // ----- annotation drawing -------------------------------------------------
    CaptureInputController::Callbacks makeInputCallbacks();
    void setupAnnotationView();
    bool prepareEditingSession(const QRect& selection);
    void prepareAnnotationScene();
    void syncAnnotationViewGeometry();
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
    void transitionToSelecting();
    void transitionToEditing();
    void transitionToFinished();
    void transitionToCanceled();
    void resetEditingUi();
    void resetEditingSession();
    bool terminalActionHasStarted() const;
    bool beginTerminalAction();

    // ----- data members -------------------------------------------------------
    CaptureResult m_captureResult;
    QRect m_virtualGeometry;
    CaptureSelectionModel m_selectionModel;
    CaptureImageComposer m_imageComposer;
    AnnotationSceneController m_annotationController;
    CaptureInputController m_inputController;
    CaptureState m_state = CaptureState::Selecting;
    CaptureOverlayMode m_mode = CaptureOverlayMode::InteractiveEdit;
    CaptureToolbar* m_toolbar = nullptr;
    QWidget* m_selectionChromeLayer = nullptr;
    bool m_canceledSignalEmitted = false;
    bool m_terminalActionStarted = false;
};

#endif // CAPTUREOVERLAY_H
