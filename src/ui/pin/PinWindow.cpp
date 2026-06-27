#include "PinWindow.h"

#include <QApplication>
#include <QAction>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QtMath>

// ============================================================================
// PinContentWidget Implementation
// ============================================================================

PinContentWidget::PinContentWidget(const QImage& image, qreal aspectRatio, QWidget* parent)
    : QWidget(parent)
    , m_image(image)
    , m_aspectRatio(aspectRatio)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
}

void PinContentWidget::setImage(const QImage& image)
{
    m_image = image;
    update();
}

void PinContentWidget::setActive(bool active)
{
    if (m_active == active) {
        return;
    }

    m_active = active;
    updateShadowEffect();
    update(); // 边框颜色也要刷新
}

void PinContentWidget::updateShadowEffect()
{
    // Get existing shadow effect or create new one
    auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(graphicsEffect());
    
    if (!shadow) {
        // Create new shadow effect if it doesn't exist
        shadow = new QGraphicsDropShadowEffect(this);
        setGraphicsEffect(shadow);
    }

    // Update shadow properties based on active state
    if (m_active) {
        // Blue glow when focused
        shadow->setBlurRadius(20.0);
        shadow->setOffset(0.0, 0.0);  // No offset for uniform shadow in all directions
        shadow->setColor(QColor(80, 150, 255, 200));  // Blue with transparency
    } else {
        // Black shadow when inactive - darker and more visible
        shadow->setBlurRadius(20.0);
        shadow->setOffset(0.0, 0.0);  // No offset for uniform shadow in all directions
        shadow->setColor(QColor(0, 0, 0, 200));  // Darker black for better visibility
    }
}

void PinContentWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    constexpr qreal kCornerRadius = 0.0;

    const QRectF contentRect = rect().adjusted(0.5, 0.5, -0.5, -0.5);

    QPainterPath path;
    path.addRoundedRect(contentRect, kCornerRadius, kCornerRadius);

    painter.save();
    painter.setClipPath(path);

    if (!m_image.isNull()) {
        painter.drawImage(rect(), m_image);
    } else {
        painter.fillRect(rect(), Qt::white);
    }

    painter.restore();

    // 清晰边框：不再依赖阴影“碰巧看起来像边框”
    const QColor borderColor = m_active
                                   ? QColor(82, 155, 255, 235)
                                   : QColor(135, 135, 135, 185);

    painter.setPen(QPen(borderColor, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(contentRect, kCornerRadius, kCornerRadius);
}

namespace
{
constexpr int kResizeHitWidth = 8;
constexpr int kMinimumExtent = 120;
constexpr qreal kMaximumScreenRatio = 0.9;
constexpr int kVisibleMargin = 40;

QSize deviceIndependentImageSize(const QImage& image)
{
    const qreal dpr = image.devicePixelRatio();
    if (dpr <= 0.0) {
        return image.size();
    }

    return QSize(qMax(1, qRound(image.width() / dpr)),
                 qMax(1, qRound(image.height() / dpr)));
}

QSize constrainPinSize(QSize size, qreal aspectRatio, const QRect& available)
{
    if (size.isEmpty()) {
        size = QSize(kMinimumExtent, qRound(kMinimumExtent / aspectRatio));
    }

    int minWidth = kMinimumExtent;
    int minHeight = qRound(kMinimumExtent / aspectRatio);
    if (aspectRatio < 1.0) {
        minHeight = kMinimumExtent;
        minWidth = qRound(kMinimumExtent * aspectRatio);
    }

    if (size.width() < minWidth || size.height() < minHeight) {
        size = QSize(minWidth, minHeight);
    }

    const QSize maxSize(qMax(1, qFloor(available.width() * kMaximumScreenRatio)),
                        qMax(1, qFloor(available.height() * kMaximumScreenRatio)));
    if (size.width() > maxSize.width() || size.height() > maxSize.height()) {
        size.scale(maxSize, Qt::KeepAspectRatio);
    }
    return size.expandedTo(QSize(1, 1));
}
}

PinWindow::PinWindow(const QImage& image, const QRect& sourceGlobalRect, QWidget* parent)
    : QWidget(nullptr)
    , m_originalImage(image)
{
    Q_UNUSED(parent)

    // Set up transparent window for shadow effect
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Calculate content size (actual image display area)
    const QSize imageSize = deviceIndependentImageSize(m_originalImage);
    m_aspectRatio = imageSize.height() > 0
        ? qreal(imageSize.width()) / qreal(imageSize.height())
        : 1.0;

    const QRect available = availableGeometryFor(sourceGlobalRect.center());
    const QSize contentSize = boundedImageSize(imageSize, available);

    // Window size includes shadow margins on all sides
    const QSize windowSize(contentSize.width() + kShadowMargin * 2,
                           contentSize.height() + kShadowMargin * 2);
    resize(windowSize);

    // Position window so that content top-left aligns with sourceGlobalRect.topLeft()
    const QPoint windowTopLeft = sourceGlobalRect.topLeft() - QPoint(kShadowMargin, kShadowMargin);
    const QRect initialGeometry(windowTopLeft, windowSize);
    move(clampedVisibleGeometry(initialGeometry).topLeft());

    // Create content widget with shadow effect
    m_contentWidget = new PinContentWidget(m_originalImage, m_aspectRatio, this);
    m_contentWidget->setGeometry(kShadowMargin, kShadowMargin,
                                  contentSize.width(), contentSize.height());

    // Initialize with black shadow (inactive state)
    m_contentWidget->updateShadowEffect();
}

QSize PinWindow::boundedImageSize(const QSize& requested, const QRect& available)
{
    if (requested.isEmpty() || available.isEmpty()) {
        return QSize(kMinimumExtent, kMinimumExtent);
    }

    const QSize maxSize(qMax(1, qFloor(available.width() * kMaximumScreenRatio)),
                        qMax(1, qFloor(available.height() * kMaximumScreenRatio)));
    QSize size = requested;
    if (size.width() > maxSize.width() || size.height() > maxSize.height()) {
        size.scale(maxSize, Qt::KeepAspectRatio);
    }
    return size.expandedTo(QSize(1, 1));
}

QRect PinWindow::availableGeometryFor(const QPoint& globalPos) const
{
    QScreen* targetScreen = QGuiApplication::screenAt(globalPos);
    if (targetScreen == nullptr) {
        targetScreen = screen();
    }
    if (targetScreen == nullptr) {
        targetScreen = QGuiApplication::primaryScreen();
    }
    return targetScreen != nullptr ? targetScreen->availableGeometry() : QRect(0, 0, 800, 600);
}

QRect PinWindow::clampedVisibleGeometry(const QRect& geometry) const
{
    const QRect available = availableGeometryFor(geometry.center());
    QRect clamped = geometry;
    const int minLeft = available.left() - clamped.width() + kVisibleMargin;
    const int maxLeft = available.right() - kVisibleMargin + 1;
    const int minTop = available.top() - clamped.height() + kVisibleMargin;
    const int maxTop = available.bottom() - kVisibleMargin + 1;
    clamped.moveLeft(qBound(minLeft, clamped.left(), maxLeft));
    clamped.moveTop(qBound(minTop, clamped.top(), maxTop));
    return clamped;
}

PinWindow::ResizeEdge PinWindow::hitTestResizeEdge(const QPoint& localPos) const
{
    // Hit test should work on the entire window area (including shadow margins)
    // so users can resize by dragging near the edges of the visible content.
    if (!rect().contains(localPos)) {
        return ResizeEdge::None;
    }

    // Expand hit test area to include shadow margin regions
    // This allows resizing when mouse is over the shadow area near edges
    constexpr int kExtendedHitWidth = kResizeHitWidth + kShadowMargin / 2;
    
    const bool left = localPos.x() <= kExtendedHitWidth;
    const bool right = localPos.x() >= width() - kExtendedHitWidth - 1;
    const bool top = localPos.y() <= kExtendedHitWidth;
    const bool bottom = localPos.y() >= height() - kExtendedHitWidth - 1;

    if (top && left) {
        return ResizeEdge::TopLeft;
    }
    if (top && right) {
        return ResizeEdge::TopRight;
    }
    if (bottom && left) {
        return ResizeEdge::BottomLeft;
    }
    if (bottom && right) {
        return ResizeEdge::BottomRight;
    }
    if (top) {
        return ResizeEdge::Top;
    }
    if (bottom) {
        return ResizeEdge::Bottom;
    }
    if (left) {
        return ResizeEdge::Left;
    }
    if (right) {
        return ResizeEdge::Right;
    }
    return ResizeEdge::None;
}

QCursor PinWindow::cursorForResizeEdge(ResizeEdge edge) const
{
    switch (edge) {
    case ResizeEdge::Top:
    case ResizeEdge::Bottom:
        return Qt::SizeVerCursor;
    case ResizeEdge::Left:
    case ResizeEdge::Right:
        return Qt::SizeHorCursor;
    case ResizeEdge::TopLeft:
    case ResizeEdge::BottomRight:
        return Qt::SizeFDiagCursor;
    case ResizeEdge::TopRight:
    case ResizeEdge::BottomLeft:
        return Qt::SizeBDiagCursor;
    case ResizeEdge::None:
        return Qt::ArrowCursor;
    }
    return Qt::ArrowCursor;
}

void PinWindow::updateHoverState(const QPoint& localPos)
{
    if (m_dragState != DragState::None) {
        return;
    }

    m_hoverResizeEdge = hitTestResizeEdge(localPos);
    setCursor(cursorForResizeEdge(m_hoverResizeEdge));
}

void PinWindow::resizeByMouseMove(const QPoint& globalPos)
{
    setGeometry(aspectResizeGeometry(globalPos));
}

QRect PinWindow::aspectResizeGeometry(const QPoint& globalPos) const
{
    const QPoint delta = globalPos - m_pressGlobalPos;
    const QRect available = availableGeometryFor(globalPos);
    
    // Work with content size (excluding shadow margins)
    QSize targetContentSize = m_initialGeometry.size() - QSize(kShadowMargin * 2, kShadowMargin * 2);

    const bool adjustsLeft = m_activeResizeEdge == ResizeEdge::Left
                             || m_activeResizeEdge == ResizeEdge::TopLeft
                             || m_activeResizeEdge == ResizeEdge::BottomLeft;
    const bool adjustsRight = m_activeResizeEdge == ResizeEdge::Right
                              || m_activeResizeEdge == ResizeEdge::TopRight
                              || m_activeResizeEdge == ResizeEdge::BottomRight;
    const bool adjustsTop = m_activeResizeEdge == ResizeEdge::Top
                            || m_activeResizeEdge == ResizeEdge::TopLeft
                            || m_activeResizeEdge == ResizeEdge::TopRight;
    const bool adjustsBottom = m_activeResizeEdge == ResizeEdge::Bottom
                               || m_activeResizeEdge == ResizeEdge::BottomLeft
                               || m_activeResizeEdge == ResizeEdge::BottomRight;

    const bool horizontal = adjustsLeft || adjustsRight;
    const bool vertical = adjustsTop || adjustsBottom;
    if (horizontal && !vertical) {
        const int widthDelta = adjustsLeft ? -delta.x() : delta.x();
        targetContentSize.setWidth(targetContentSize.width() + widthDelta);
        targetContentSize.setHeight(qRound(targetContentSize.width() / m_aspectRatio));
    } else if (!horizontal && vertical) {
        const int heightDelta = adjustsTop ? -delta.y() : delta.y();
        targetContentSize.setHeight(targetContentSize.height() + heightDelta);
        targetContentSize.setWidth(qRound(targetContentSize.height() * m_aspectRatio));
    } else {
        const int widthDelta = adjustsLeft ? -delta.x() : delta.x();
        const int heightDelta = adjustsTop ? -delta.y() : delta.y();
        const int widthCandidate = targetContentSize.width() + widthDelta;
        const int heightFromWidth = qRound(widthCandidate / m_aspectRatio);
        const int heightCandidate = targetContentSize.height() + heightDelta;
        const int widthFromHeight = qRound(heightCandidate * m_aspectRatio);
        if (qAbs(widthDelta) >= qAbs(qRound(heightDelta * m_aspectRatio))) {
            targetContentSize = QSize(widthCandidate, heightFromWidth);
        } else {
            targetContentSize = QSize(widthFromHeight, heightCandidate);
        }
    }

    targetContentSize = constrainedSize(targetContentSize, available);
    
    // Convert content size to window size by adding shadow margins
    const QSize targetWindowSize(targetContentSize.width() + kShadowMargin * 2,
                                  targetContentSize.height() + kShadowMargin * 2);

    QRect geometry(QPoint(0, 0), targetWindowSize);
    if (adjustsLeft) {
        geometry.moveRight(m_initialGeometry.right());
    } else {
        geometry.moveLeft(m_initialGeometry.left());
    }
    if (!horizontal && vertical) {
        geometry.moveLeft(m_initialGeometry.center().x() - geometry.width() / 2);
    }

    if (adjustsTop) {
        geometry.moveBottom(m_initialGeometry.bottom());
    } else {
        geometry.moveTop(m_initialGeometry.top());
    }
    if (horizontal && !vertical) {
        geometry.moveTop(m_initialGeometry.center().y() - geometry.height() / 2);
    }

    return clampedVisibleGeometry(geometry);
}

QSize PinWindow::constrainedSize(QSize size, const QRect& available) const
{
    // Size passed here is content size (without shadow margins)
    return constrainPinSize(size, m_aspectRatio, available);
}

void PinWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    // Content is rendered by m_contentWidget, no need to paint here
    // The window background is transparent to allow shadow effect
}

void PinWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    
    // Update content widget geometry to maintain shadow margins
    if (m_contentWidget) {
        const QSize contentSize = size() - QSize(kShadowMargin * 2, kShadowMargin * 2);
        m_contentWidget->setGeometry(kShadowMargin, kShadowMargin,
                                      contentSize.width(), contentSize.height());
    }
}

void PinWindow::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    // Don't activate on hover anymore - only on focus
    updateHoverState(event->position().toPoint());
}

void PinWindow::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    m_hovered = false;
    // No need to deactivate on leave since we don't activate on hover
    if (m_dragState == DragState::None) {
        m_hoverResizeEdge = ResizeEdge::None;
        setCursor(Qt::ArrowCursor);
    }
}

void PinWindow::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    if (m_contentWidget) {
        m_contentWidget->setActive(true);
    }
}

void PinWindow::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    // Keep active if mouse is still hovering, even when focus is lost
    if (m_contentWidget && !m_hovered) {
        m_contentWidget->setActive(false);
    }
}

void PinWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_pressGlobalPos = event->globalPosition().toPoint();
    m_initialGeometry = geometry();
    m_initialWindowTopLeft = m_initialGeometry.topLeft();
    m_activeResizeEdge = hitTestResizeEdge(event->position().toPoint());
    if (m_activeResizeEdge != ResizeEdge::None) {
        m_dragState = DragState::Resizing;
        setCursor(cursorForResizeEdge(m_activeResizeEdge));
    } else {
        m_dragState = DragState::Moving;
        setCursor(Qt::SizeAllCursor);
    }
    event->accept();
}

void PinWindow::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint globalPos = event->globalPosition().toPoint();
    if (m_dragState == DragState::Moving) {
        const QPoint delta = globalPos - m_pressGlobalPos;
        move(clampedVisibleGeometry(QRect(m_initialWindowTopLeft + delta, size())).topLeft());
        event->accept();
        return;
    }

    if (m_dragState == DragState::Resizing) {
        resizeByMouseMove(globalPos);
        event->accept();
        return;
    }

    updateHoverState(event->position().toPoint());
    QWidget::mouseMoveEvent(event);
}

void PinWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragState = DragState::None;
        m_activeResizeEdge = ResizeEdge::None;
        updateHoverState(event->position().toPoint());
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void PinWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Delete) {
        close();
        return;
    }

    QWidget::keyPressEvent(event);
}

void PinWindow::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    QAction* closeAction = menu.addAction(QStringLiteral("Close pin"));
    connect(closeAction, &QAction::triggered, this, &PinWindow::close);
    menu.exec(event->globalPos());
}

void PinWindow::closeEvent(QCloseEvent* event)
{
    QWidget::closeEvent(event);
    if (!m_closing) {
        m_closing = true;
        deleteLater();
    }
}
