#include "CaptureOverlay.h"

#include "AddAnnotationCommand.h"
#include "CaptureToolbar.h"
#include "../../core/clipboard/ClipboardService.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QStandardPaths>

namespace
{
constexpr int kDimAlpha = 120;
constexpr int kMinimumSelectionSize = 2;
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

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

CaptureOverlay::CaptureOverlay(CaptureResult captureResult,
                               const QRect& virtualGeometry,
                               QWidget* parent)
    : QWidget(parent)
    , m_captureResult(std::move(captureResult))
    , m_virtualGeometry(virtualGeometry)
    , m_toolbar(new CaptureToolbar(this))
{
    m_annotationPen.setCapStyle(Qt::RoundCap);
    m_annotationPen.setJoinStyle(Qt::RoundJoin);

    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    setGeometry(m_virtualGeometry);

    m_toolbar->hide();
    m_toolbar->setCurrentTool(m_currentTool);

    connect(m_toolbar, &CaptureToolbar::toolSelected, this, &CaptureOverlay::setCurrentTool);
    connect(m_toolbar, &CaptureToolbar::undoRequested, this, &CaptureOverlay::undo);
    connect(m_toolbar, &CaptureToolbar::redoRequested, this, &CaptureOverlay::redo);
    connect(m_toolbar, &CaptureToolbar::reselectRequested, this, &CaptureOverlay::reselect);
    connect(m_toolbar, &CaptureToolbar::copyRequested, this, &CaptureOverlay::copyAndClose);
    connect(m_toolbar, &CaptureToolbar::saveRequested, this, &CaptureOverlay::saveAndClose);
    connect(m_toolbar, &CaptureToolbar::pinRequested, this, &CaptureOverlay::showPinPlaceholder);
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
    const QRect bounds = QRect(QPoint(0, 0), m_virtualGeometry.size());
    const QRect bounded = imageRect.normalized().intersected(bounds);
    if (m_captureResult.screens().isEmpty()
        || bounded.width() < kMinimumSelectionSize
        || bounded.height() < kMinimumSelectionSize) {
        return;
    }

    clearAnnotationState();
    m_selectionImageRect = bounded;
    m_selectionStart = bounded.topLeft();
    m_selectionEnd = bounded.bottomRight();
    m_hasSelection = true;
    m_selecting = false;
    m_state = CaptureState::Editing;
    setCursor(Qt::CrossCursor);
    m_toolbar->show();
    updateToolbarGeometry();
    update();
}

// ---------------------------------------------------------------------------
// Annotation management
// ---------------------------------------------------------------------------

void CaptureOverlay::addAnnotation(std::unique_ptr<Annotation> annotation)
{
    if (annotation == nullptr || m_state != CaptureState::Editing) {
        return;
    }

    m_undoStack.push(new AddAnnotationCommand(&m_annotations, std::move(annotation), [this]() {
        update();
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
    m_undoStack.undo();
}

void CaptureOverlay::redo()
{
    m_undoStack.redo();
}

// ---------------------------------------------------------------------------
// Export (DPR-aware — preserves original physical pixels)
// ---------------------------------------------------------------------------

QImage CaptureOverlay::renderResultImage() const
{
    if (m_state != CaptureState::Editing || m_selectionImageRect.isEmpty()) {
        return {};
    }

    const QRect selectionGlobal = overlayToGlobalLogical(m_selectionImageRect);
    const qreal effectiveDpr = effectiveDevicePixelRatio(selectionGlobal);
    const int physW = qRound(m_selectionImageRect.width() * effectiveDpr);
    const int physH = qRound(m_selectionImageRect.height() * effectiveDpr);

    QImage result(physW, physH, QImage::Format_ARGB32);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Scale the logical selection into physical output pixels so that
    // annotations and target rects are automatically mapped.
    painter.scale(effectiveDpr, effectiveDpr);

    // Composite each screen's original physical pixels into the output.
    //
    // When the screen's DPR equals the effective DPR this is a 1:1 pixel
    // copy — zero quality loss.  When a screen has a lower DPR, QPainter
    // performs the single necessary upscale during this final composite.
    //
    // Strategy: single-screen export is always lossless.  Multi-screen
    // export with mixed DPR prioritises the highest DPR so most pixels
    // stay 1:1; the lower-DPR fragment is upscaled once.
    for (const CapturedScreen &screen : m_captureResult.screens()) {
        const QRect overlapGlobal = selectionGlobal.intersected(screen.logicalGeometry);
        if (overlapGlobal.isEmpty()) {
            continue;
        }

        const QRect physicalSrc = m_captureResult.logicalToPhysical(screen, overlapGlobal);
        if (physicalSrc.isEmpty()) {
            continue;
        }

        // Target rect in selection-relative logical coordinates.
        const QRect overlapLocal = globalToOverlayLogical(overlapGlobal);
        const QPointF targetTopLeft(overlapLocal.topLeft() - m_selectionImageRect.topLeft());
        const QRectF targetRect(targetTopLeft, QSizeF(overlapLocal.size()));

        QImage sourceImage = screen.image;
        sourceImage.setDevicePixelRatio(1.0);
        painter.drawImage(targetRect, sourceImage, physicalSrc);
    }

    // Paint annotations in selection-relative logical coordinates.
    // Because the painter is scaled by effectiveDpr, annotations are
    // rendered at full physical resolution — no blurry downscale.
    painter.setClipRect(QRectF(QPointF(0, 0), QSizeF(m_selectionImageRect.size())));

    for (const std::unique_ptr<Annotation> &annotation : m_annotations) {
        annotation->paint(painter);
    }

    painter.end();
    result.setDevicePixelRatio(effectiveDpr);
    return result;
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void CaptureOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    paintBackground(painter);

    // Dim everything …
    painter.fillRect(rect(), QColor(0, 0, 0, kDimAlpha));

    const QRect selection = normalizedImageSelection();
    if (selection.isEmpty()) {
        return;
    }

    // … then undim the selection area by redrawing it at full brightness.
    const QRect widgetSelection = imageToWidgetRect(selection);
    painter.save();
    painter.setClipRect(widgetSelection);
    paintBackground(painter);
    painter.restore();

    paintAnnotations(painter, widgetSelection, true);
    paintSelectionChrome(painter, widgetSelection, selection);
}

void CaptureOverlay::paintBackground(QPainter &painter) const
{
    // Draw each screen's physical image into its logical geometry rect within
    // the widget.  Qt scales the physical pixels into the logical target
    // rectangle — the widget backing store may have a higher device-pixel
    // density and Qt will use it, so the preview looks sharp.
    for (const CapturedScreen &screen : m_captureResult.screens()) {
        const QRect widgetRect(screen.logicalGeometry.topLeft() - m_virtualGeometry.topLeft(),
                               screen.logicalGeometry.size());
        painter.drawImage(widgetRect, screen.image);
    }
}

void CaptureOverlay::paintAnnotations(QPainter &painter,
                                      const QRect &widgetSelection,
                                      bool includePreview) const
{
    if (m_state != CaptureState::Editing || m_selectionImageRect.isEmpty()) {
        return;
    }

    painter.save();
    painter.setClipRect(widgetSelection);

    // Widget coordinates ≡ logical image coordinates, so the widget
    // selection rect has the same size as m_selectionImageRect.
    // The scale factor is nominally 1.0 but we compute it explicitly
    // to stay robust against any future coordinate change.
    painter.translate(widgetSelection.topLeft());
    const qreal sx = static_cast<qreal>(widgetSelection.width())
                     / static_cast<qreal>(m_selectionImageRect.width());
    const qreal sy = static_cast<qreal>(widgetSelection.height())
                     / static_cast<qreal>(m_selectionImageRect.height());
    painter.scale(sx, sy);

    for (const std::unique_ptr<Annotation> &annotation : m_annotations) {
        annotation->paint(painter);
    }
    if (includePreview && m_previewAnnotation != nullptr) {
        m_previewAnnotation->paint(painter);
    }

    painter.restore();
}

void CaptureOverlay::paintSelectionChrome(QPainter &painter,
                                          const QRect &widgetSelection,
                                          const QRect &selection) const
{
    painter.save();
    painter.setPen(QPen(QColor(47, 128, 237), 2.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(widgetSelection.adjusted(0, 0, -1, -1));

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
    for (const QPoint &handle : handles) {
        painter.drawEllipse(handle, kHandleRadius, kHandleRadius);
    }

    const QString sizeText = QStringLiteral("%1,%2  %3 x %4")
                                 .arg(selection.left())
                                 .arg(selection.top())
                                 .arg(selection.width())
                                 .arg(selection.height());
    const QFontMetrics metrics(painter.font());
    const QSize textSize = metrics.size(Qt::TextSingleLine, sizeText) + QSize(12, 8);
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
    painter.setBrush(QColor(0, 0, 0, 210));
    painter.drawRoundedRect(labelRect, 4.0, 4.0);
    painter.setPen(Qt::white);
    painter.drawText(labelRect, Qt::AlignCenter, sizeText);
    painter.restore();
}

// ---------------------------------------------------------------------------
// Coordinate helpers
// ---------------------------------------------------------------------------

QPoint CaptureOverlay::widgetToImage(const QPointF &widgetPos) const
{
    // The overlay widget occupies the exact virtual-desktop logical geometry,
    // so widget coordinates equal logical image coordinates (identity).
    const QSize bounds = m_virtualGeometry.size();
    if (bounds.isEmpty()) {
        return {};
    }

    const int x = qBound(0, qRound(widgetPos.x()), bounds.width());
    const int y = qBound(0, qRound(widgetPos.y()), bounds.height());
    return QPoint(x, y);
}

QPointF CaptureOverlay::widgetToSelection(const QPointF &widgetPos) const
{
    const QPoint imagePoint = widgetToImage(widgetPos);
    return clampSelectionPoint(QPointF(imagePoint - m_selectionImageRect.topLeft()));
}

QRect CaptureOverlay::imageToWidgetRect(const QRect &imageRect) const
{
    // Identity: widget coords ≡ logical image coords.
    return imageRect;
}

QRect CaptureOverlay::overlayToGlobalLogical(const QRect &localRect) const
{
    return localRect.translated(m_virtualGeometry.topLeft());
}

QRect CaptureOverlay::globalToOverlayLogical(const QRect &globalRect) const
{
    return globalRect.translated(-m_virtualGeometry.topLeft());
}

qreal CaptureOverlay::effectiveDevicePixelRatio(const QRect &globalLogicalRect) const
{
    qreal dpr = 1.0;
    for (const CapturedScreen &screen : m_captureResult.screens()) {
        if (screen.logicalGeometry.intersects(globalLogicalRect)) {
            dpr = qMax(dpr, screen.devicePixelRatio);
        }
    }
    return dpr;
}

QRect CaptureOverlay::normalizedImageSelection() const
{
    if (!m_hasSelection) {
        return {};
    }

    if (m_state == CaptureState::Editing) {
        return m_selectionImageRect;
    }

    const QSize bounds = m_virtualGeometry.size();
    return rectFromImagePoints(m_selectionStart, m_selectionEnd)
        .intersected(QRect(QPoint(0, 0), bounds));
}

QRect CaptureOverlay::rectFromImagePoints(const QPoint &first, const QPoint &second) const
{
    const int left = qMin(first.x(), second.x());
    const int top = qMin(first.y(), second.y());
    const int right = qMax(first.x(), second.x());
    const int bottom = qMax(first.y(), second.y());
    return QRect(left, top, right - left, bottom - top);
}

bool CaptureOverlay::imagePointInSelection(const QPoint &imagePoint) const
{
    return m_selectionImageRect.contains(imagePoint);
}

QPointF CaptureOverlay::clampSelectionPoint(const QPointF &point) const
{
    return QPointF(qBound<qreal>(0.0, point.x(), m_selectionImageRect.width()),
                   qBound<qreal>(0.0, point.y(), m_selectionImageRect.height()));
}

// ---------------------------------------------------------------------------
// Annotation drawing
// ---------------------------------------------------------------------------

void CaptureOverlay::beginAnnotation(const QPointF &selectionPos)
{
    clearAnnotationState();
    m_drawingAnnotation = true;
    m_annotationStart = clampSelectionPoint(selectionPos);

    switch (m_currentTool) {
    case CaptureTool::Rect:
        m_previewAnnotation = std::make_unique<RectAnnotation>(QRectF(m_annotationStart, m_annotationStart),
                                                               m_annotationPen);
        break;
    case CaptureTool::Arrow:
        m_previewAnnotation = std::make_unique<ArrowAnnotation>(QLineF(m_annotationStart, m_annotationStart),
                                                                m_annotationPen);
        break;
    case CaptureTool::Pen:
        m_activePath = QPainterPath(m_annotationStart);
        m_penPointCount = 1;
        m_previewAnnotation = std::make_unique<PenAnnotation>(m_activePath, m_annotationPen);
        break;
    case CaptureTool::None:
    case CaptureTool::Text:
        m_drawingAnnotation = false;
        break;
    }
}

void CaptureOverlay::updateAnnotation(const QPointF &selectionPos)
{
    if (!m_drawingAnnotation) {
        return;
    }

    const QPointF current = clampSelectionPoint(selectionPos);
    switch (m_currentTool) {
    case CaptureTool::Rect:
        m_previewAnnotation = std::make_unique<RectAnnotation>(QRectF(m_annotationStart, current),
                                                               m_annotationPen);
        break;
    case CaptureTool::Arrow:
        m_previewAnnotation = std::make_unique<ArrowAnnotation>(QLineF(m_annotationStart, current),
                                                                m_annotationPen);
        break;
    case CaptureTool::Pen:
        m_activePath.lineTo(current);
        ++m_penPointCount;
        m_previewAnnotation = std::make_unique<PenAnnotation>(m_activePath, m_annotationPen);
        break;
    case CaptureTool::None:
    case CaptureTool::Text:
        break;
    }
}

void CaptureOverlay::finishAnnotation(const QPointF &selectionPos)
{
    updateAnnotation(selectionPos);
    m_drawingAnnotation = false;

    if (m_previewAnnotation != nullptr && previewIsLargeEnough()) {
        addAnnotation(std::move(m_previewAnnotation));
    }

    clearAnnotationState();
    update();
}

void CaptureOverlay::createTextAnnotation(const QPointF &selectionPos)
{
    bool accepted = false;
    const QString text = QInputDialog::getText(this,
                                               QStringLiteral("Text"),
                                               QStringLiteral("Text:"),
                                               QLineEdit::Normal,
                                               QString(),
                                               &accepted)
                             .trimmed();
    if (!accepted || text.isEmpty()) {
        return;
    }

    addAnnotation(std::make_unique<TextAnnotation>(clampSelectionPoint(selectionPos),
                                                  text,
                                                  m_annotationPen.color()));
}

bool CaptureOverlay::previewIsLargeEnough() const
{
    if (m_previewAnnotation == nullptr) {
        return false;
    }

    if (const auto *rect = dynamic_cast<const RectAnnotation *>(m_previewAnnotation.get())) {
        return rect->rect().width() >= kMinimumSelectionSize
               && rect->rect().height() >= kMinimumSelectionSize;
    }
    if (const auto *arrow = dynamic_cast<const ArrowAnnotation *>(m_previewAnnotation.get())) {
        return arrow->line().length() >= kMinimumArrowLength;
    }
    if (const auto *pen = dynamic_cast<const PenAnnotation *>(m_previewAnnotation.get())) {
        const QRectF bounds = pen->path().controlPointRect();
        return m_penPointCount >= 2
               && (bounds.width() >= kMinimumSelectionSize || bounds.height() >= kMinimumSelectionSize);
    }

    return true;
}

void CaptureOverlay::clearAnnotationState()
{
    m_previewAnnotation.reset();
    m_activePath = QPainterPath();
    m_penPointCount = 0;
    m_drawingAnnotation = false;
}

void CaptureOverlay::clearAnnotationsAndHistory()
{
    clearAnnotationState();
    m_undoStack.clear();
    m_annotations.clear();
    m_toolbar->setUndoAvailable(false);
    m_toolbar->setRedoAvailable(false);
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void CaptureOverlay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateToolbarGeometry();
}

void CaptureOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        cancelAndClose();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        return;
    }

    const QPoint imagePoint = widgetToImage(event->position());
    if (m_state == CaptureState::Selecting) {
        m_selectionStart = imagePoint;
        m_selectionEnd = imagePoint;
        m_selecting = true;
        m_hasSelection = true;
        update();
        return;
    }

    if (m_state != CaptureState::Editing || !imagePointInSelection(imagePoint)) {
        return;
    }

    const QPointF selectionPos = widgetToSelection(event->position());
    if (m_currentTool == CaptureTool::Text) {
        createTextAnnotation(selectionPos);
        return;
    }

    beginAnnotation(selectionPos);
}

void CaptureOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (m_state == CaptureState::Selecting && m_selecting) {
        m_selectionEnd = widgetToImage(event->position());
        update();
        return;
    }

    if (m_state == CaptureState::Editing && m_drawingAnnotation) {
        updateAnnotation(widgetToSelection(event->position()));
        update();
    }
}

void CaptureOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }

    if (m_state == CaptureState::Selecting && m_selecting) {
        m_selectionEnd = widgetToImage(event->position());
        m_selecting = false;

        const QRect selection = normalizedImageSelection();
        if (selection.width() < kMinimumSelectionSize || selection.height() < kMinimumSelectionSize) {
            m_hasSelection = false;
            update();
            return;
        }

        enterEditing(selection);
        return;
    }

    if (m_state == CaptureState::Editing && m_drawingAnnotation) {
        finishAnnotation(widgetToSelection(event->position()));
    }
}

void CaptureOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
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

    QWidget::keyPressEvent(event);
}

// ---------------------------------------------------------------------------
// Toolbar
// ---------------------------------------------------------------------------

void CaptureOverlay::updateToolbarGeometry()
{
    if (m_toolbar == nullptr || m_state != CaptureState::Editing || m_selectionImageRect.isEmpty()) {
        if (m_toolbar != nullptr) {
            m_toolbar->hide();
        }
        return;
    }

    m_toolbar->adjustSize();
    const QSize toolbarSize = m_toolbar->sizeHint();
    const QRect widgetSelection = imageToWidgetRect(m_selectionImageRect);
    int x = widgetSelection.x() + widgetSelection.width() - toolbarSize.width();
    int y = widgetSelection.bottom() + kToolbarGap;

    if (y + toolbarSize.height() > height()) {
        y = widgetSelection.top() - toolbarSize.height() - kToolbarGap;
    }

    x = qBound(8, x, qMax(8, width() - toolbarSize.width() - 8));
    y = qBound(8, y, qMax(8, height() - toolbarSize.height() - 8));

    m_toolbar->setGeometry(QRect(QPoint(x, y), toolbarSize));
    m_toolbar->show();
    m_toolbar->raise();
}

void CaptureOverlay::setCurrentTool(CaptureTool tool)
{
    m_currentTool = tool;
    m_toolbar->setCurrentTool(tool);
    clearAnnotationState();
    update();
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

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
    m_hasSelection = false;
    m_selecting = false;
    m_selectionImageRect = {};
    m_toolbar->hide();
    update();
}

void CaptureOverlay::showPinPlaceholder()
{
    QMessageBox::information(this,
                             QStringLiteral("Pin"),
                             QStringLiteral("Pinning will be added in a later version."));
}
