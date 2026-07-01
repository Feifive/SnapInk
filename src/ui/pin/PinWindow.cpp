#include "PinWindow.h"

#ifdef Q_OS_MACOS
#include "MacWindowHelper.h"
#endif

#include <QApplication>
#include <QAction>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>
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

    const QRectF contentRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

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
constexpr int kMinimumInteractiveExtent = 20;
constexpr qreal kMaximumScreenRatio = 0.9;
constexpr int kVisibleMargin = 40;
constexpr int kWheelDeltaPerStep = 120;
constexpr qreal kNormalWheelScaleIncrement = 0.10;
constexpr qreal kFineWheelScaleIncrement = 0.01;
constexpr int kOpacityWheelIncrementPercent = 5;
constexpr int kMinimumWindowOpacityPercent = 5;
constexpr int kScaleIndicatorHideDelayMs = 1000;

bool isOpacityWheelModifier(Qt::KeyboardModifiers modifiers)
{
#ifdef Q_OS_MACOS
    return modifiers.testFlag(Qt::ControlModifier);
#else
    return modifiers.testFlag(Qt::MetaModifier);
#endif
}

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
        size = QSize(kMinimumInteractiveExtent, qRound(kMinimumInteractiveExtent / aspectRatio));
    }

    int minWidth = kMinimumInteractiveExtent;
    int minHeight = qRound(kMinimumInteractiveExtent / aspectRatio);
    if (aspectRatio < 1.0) {
        minHeight = kMinimumInteractiveExtent;
        minWidth = qRound(kMinimumInteractiveExtent * aspectRatio);
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
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_MacAlwaysShowToolWindow, true);
#endif
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Calculate content size (actual image display area)
    const QSize imageSize = deviceIndependentImageSize(m_originalImage);
    m_originalContentSize = imageSize;
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

    m_scaleIndicator = new QLabel(this);
    m_scaleIndicator->setObjectName(QStringLiteral("PinScaleIndicator"));
    m_scaleIndicator->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_scaleIndicator->setAutoFillBackground(false);
    m_scaleIndicator->setStyleSheet(QStringLiteral(
        "QLabel#PinScaleIndicator {"
        "background-color: rgba(55, 55, 55, 220);"
        "color: white;"
        "font-weight: 600;"
        "padding: 3px 3px;"
        "}"));
    m_scaleIndicator->hide();

    m_scaleIndicatorHideTimer = new QTimer(this);
    m_scaleIndicatorHideTimer->setSingleShot(true);
    connect(m_scaleIndicatorHideTimer, &QTimer::timeout, m_scaleIndicator, &QLabel::hide);
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

void PinWindow::setActive(bool active)
{
    if (m_contentWidget) {
        m_contentWidget->setActive(active);
    }
}

bool PinWindow::isActiveVisual() const
{
    return m_contentWidget != nullptr && m_contentWidget->isActive();
}

PinWindowSnapshot PinWindow::snapshot() const
{
    PinWindowSnapshot result;
    result.image = m_originalImage;
    result.contentSize = currentContentSize();
    result.contentGlobalRect = QRect(geometry().topLeft() + QPoint(kShadowMargin, kShadowMargin),
                                     result.contentSize);
    result.opacityPercent = m_opacityPercent;
    return result;
}

void PinWindow::applySnapshotPresentation(const PinWindowSnapshot& snapshot)
{
    const QSize contentSize = snapshot.contentSize.isValid()
        ? snapshot.contentSize
        : snapshot.contentGlobalRect.size();
    const QSize windowSize(contentSize.width() + kShadowMargin * 2,
                           contentSize.height() + kShadowMargin * 2);
    const QPoint windowTopLeft = snapshot.contentGlobalRect.topLeft()
                                 - QPoint(kShadowMargin, kShadowMargin);
    setGeometry(clampedVisibleGeometry(QRect(windowTopLeft, windowSize)));

    m_opacityPercent = qBound(kMinimumWindowOpacityPercent,
                              snapshot.opacityPercent,
                              100);
    setWindowOpacity(qreal(m_opacityPercent) / 100.0);
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

QSize PinWindow::currentContentSize() const
{
    return size() - QSize(kShadowMargin * 2, kShadowMargin * 2);
}

QSize PinWindow::constrainedWheelSize(QSize size, const QRect& available) const
{
    if (size.isEmpty()) {
        return QSize(kMinimumInteractiveExtent, qRound(kMinimumInteractiveExtent / m_aspectRatio));
    }

    int minWidth = kMinimumInteractiveExtent;
    int minHeight = qRound(kMinimumInteractiveExtent / m_aspectRatio);
    if (m_aspectRatio < 1.0) {
        minHeight = kMinimumInteractiveExtent;
        minWidth = qRound(kMinimumInteractiveExtent * m_aspectRatio);
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

void PinWindow::scaleByWheelEvent(QWheelEvent* event)
{
    if (event == nullptr) {
        return;
    }

    const int deltaY = !event->angleDelta().isNull()
        ? event->angleDelta().y()
        : event->pixelDelta().y();
    if (deltaY == 0) {
        return;
    }

    const qreal step = event->modifiers().testFlag(Qt::AltModifier)
        ? kFineWheelScaleIncrement
        : kNormalWheelScaleIncrement;
    const qreal wheelSteps = event->angleDelta().isNull()
        ? 1.0
        : qMax(1.0, qAbs(qreal(deltaY)) / qreal(kWheelDeltaPerStep));
    const qreal scaleDelta = step * wheelSteps * (deltaY > 0 ? 1.0 : -1.0);

    const qreal currentScale = m_originalContentSize.isEmpty()
        ? 1.0
        : qreal(currentContentSize().width()) / qreal(m_originalContentSize.width());
    const qreal targetScale = currentScale + scaleDelta;
    QSize targetContentSize(qRound(m_originalContentSize.width() * targetScale),
                            qRound(m_originalContentSize.height() * targetScale));
    targetContentSize = constrainedWheelSize(targetContentSize, availableGeometryFor(geometry().center()));

    const QSize targetWindowSize(targetContentSize.width() + kShadowMargin * 2,
                                 targetContentSize.height() + kShadowMargin * 2);
    QRect targetGeometry(QPoint(0, 0), targetWindowSize);
    targetGeometry.moveCenter(geometry().center());
    setGeometry(clampedVisibleGeometry(targetGeometry));

    showScaleIndicator();
}

void PinWindow::adjustOpacityByWheelEvent(QWheelEvent* event)
{
    if (event == nullptr) {
        return;
    }

    const int deltaY = !event->angleDelta().isNull()
        ? event->angleDelta().y()
        : event->pixelDelta().y();
    if (deltaY == 0) {
        return;
    }

    const int opacityDelta = deltaY > 0 ? kOpacityWheelIncrementPercent : -kOpacityWheelIncrementPercent;
    m_opacityPercent = qBound(kMinimumWindowOpacityPercent,
                              m_opacityPercent + opacityDelta,
                              100);
    setWindowOpacity(qreal(m_opacityPercent) / 100.0);
    showOpacityIndicator();
}

int PinWindow::currentScalePercent() const
{
    if (m_originalContentSize.isEmpty()) {
        return 100;
    }

    return qRound((qreal(currentContentSize().width()) / qreal(m_originalContentSize.width())) * 100.0);
}

int PinWindow::currentOpacityPercent() const
{
    return m_opacityPercent;
}

void PinWindow::showIndicatorText(const QString& text)
{
    if (m_scaleIndicator == nullptr || m_scaleIndicatorHideTimer == nullptr) {
        return;
    }

    m_scaleIndicator->setText(text);
    updateScaleIndicatorGeometry();
    m_scaleIndicator->show();
    m_scaleIndicator->raise();
    m_scaleIndicatorHideTimer->start(kScaleIndicatorHideDelayMs);
}

void PinWindow::showScaleIndicator()
{
    showIndicatorText(QStringLiteral("缩放：%1%").arg(currentScalePercent()));
}

void PinWindow::showOpacityIndicator()
{
    showIndicatorText(QStringLiteral("不透明度：%1%").arg(currentOpacityPercent()));
}

void PinWindow::updateScaleIndicatorGeometry()
{
    if (m_scaleIndicator == nullptr) {
        return;
    }

    m_scaleIndicator->adjustSize();
    m_scaleIndicator->move(kShadowMargin, kShadowMargin);
}

void PinWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    // Content is rendered by m_contentWidget, no need to paint here
    // The window background is transparent to allow shadow effect
}

void PinWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

#ifdef Q_OS_MACOS
    if (!m_nativeOverlayConfigured) {
        MacWindowHelper::configureOverlayWindow(this);
        m_nativeOverlayConfigured = true;
    }
#endif
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
    updateScaleIndicatorGeometry();
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
    Q_EMIT focusActivated(this);
}

void PinWindow::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    Q_EMIT focusDeactivated(this);
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

void PinWindow::wheelEvent(QWheelEvent* event)
{
    if (isOpacityWheelModifier(event->modifiers())) {
        adjustOpacityByWheelEvent(event);
    } else {
        scaleByWheelEvent(event);
    }
    event->accept();
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
    Q_EMIT aboutToClose(this);

    QWidget::closeEvent(event);

    if (!m_closing) {
        m_closing = true;
        deleteLater();
    }
}
