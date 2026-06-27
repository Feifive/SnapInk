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
#include <QScreen>
#include <QToolButton>
#include <QtMath>

namespace
{
constexpr int kResizeHitWidth = 8;
constexpr int kMinimumExtent = 120;
constexpr qreal kMaximumScreenRatio = 0.9;
constexpr int kVisibleMargin = 40;
constexpr int kCloseButtonSize = 22;
constexpr int kCloseButtonMargin = 5;

QSize deviceIndependentImageSize(const QImage& image)
{
    const qreal dpr = image.devicePixelRatio();
    if (dpr <= 0.0) {
        return image.size();
    }

    return QSize(qMax(1, qRound(image.width() / dpr)),
                 qMax(1, qRound(image.height() / dpr)));
}
}

PinWindow::PinWindow(const QImage& image, QWidget* parent)
    : QWidget(nullptr)
    , m_originalImage(image)
{
    Q_UNUSED(parent)

    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    const QSize imageSize = deviceIndependentImageSize(m_originalImage);
    m_aspectRatio = imageSize.height() > 0
        ? qreal(imageSize.width()) / qreal(imageSize.height())
        : 1.0;

    resize(initialWindowSize());

    m_closeButton = new QToolButton(this);
    m_closeButton->setText(QStringLiteral("x"));
    m_closeButton->setToolTip(QStringLiteral("Close pin"));
    m_closeButton->setAutoRaise(true);
    m_closeButton->setCursor(Qt::ArrowCursor);
    m_closeButton->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  background: rgba(32, 42, 54, 185);"
        "  border: 1px solid rgba(255, 255, 255, 155);"
        "  border-radius: 11px;"
        "  color: white;"
        "  font: 700 13px Arial;"
        "}"
        "QToolButton:hover { background: rgba(224, 54, 54, 220); }"));
    m_closeButton->hide();
    connect(m_closeButton, &QToolButton::clicked, this, &PinWindow::close);
    updateCloseButtonGeometry();

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(14.0);
    shadow->setOffset(0.0, 2.0);
    shadow->setColor(QColor(0, 0, 0, 95));
    setGraphicsEffect(shadow);
}

QSize PinWindow::initialWindowSize() const
{
    const QRect available = availableGeometryFor(QCursor::pos());
    return boundedImageSize(deviceIndependentImageSize(m_originalImage), available);
}

QSize PinWindow::boundedImageSize(const QSize& requested, const QRect& available) const
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
    if (!rect().contains(localPos)) {
        return ResizeEdge::None;
    }

    const bool left = localPos.x() <= kResizeHitWidth;
    const bool right = localPos.x() >= width() - kResizeHitWidth - 1;
    const bool top = localPos.y() <= kResizeHitWidth;
    const bool bottom = localPos.y() >= height() - kResizeHitWidth - 1;

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

void PinWindow::updateCloseButtonGeometry()
{
    if (m_closeButton == nullptr) {
        return;
    }

    m_closeButton->setGeometry(width() - kCloseButtonSize - kCloseButtonMargin,
                               kCloseButtonMargin,
                               kCloseButtonSize,
                               kCloseButtonSize);
}

void PinWindow::updateCloseButtonVisibility()
{
    if (m_closeButton == nullptr) {
        return;
    }

    m_closeButton->setVisible(m_hovered || hasFocus());
}

void PinWindow::resizeByMouseMove(const QPoint& globalPos)
{
    setGeometry(aspectResizeGeometry(globalPos));
}

QRect PinWindow::aspectResizeGeometry(const QPoint& globalPos) const
{
    const QPoint delta = globalPos - m_pressGlobalPos;
    const QRect available = availableGeometryFor(globalPos);
    QSize targetSize = m_initialGeometry.size();

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
        targetSize.setWidth(m_initialGeometry.width() + widthDelta);
        targetSize.setHeight(qRound(targetSize.width() / m_aspectRatio));
    } else if (!horizontal && vertical) {
        const int heightDelta = adjustsTop ? -delta.y() : delta.y();
        targetSize.setHeight(m_initialGeometry.height() + heightDelta);
        targetSize.setWidth(qRound(targetSize.height() * m_aspectRatio));
    } else {
        const int widthDelta = adjustsLeft ? -delta.x() : delta.x();
        const int heightDelta = adjustsTop ? -delta.y() : delta.y();
        const int widthCandidate = m_initialGeometry.width() + widthDelta;
        const int heightFromWidth = qRound(widthCandidate / m_aspectRatio);
        const int heightCandidate = m_initialGeometry.height() + heightDelta;
        const int widthFromHeight = qRound(heightCandidate * m_aspectRatio);
        if (qAbs(widthDelta) >= qAbs(qRound(heightDelta * m_aspectRatio))) {
            targetSize = QSize(widthCandidate, heightFromWidth);
        } else {
            targetSize = QSize(widthFromHeight, heightCandidate);
        }
    }

    targetSize = constrainedSize(targetSize, available);

    QRect geometry(QPoint(0, 0), targetSize);
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
    if (size.isEmpty()) {
        size = QSize(kMinimumExtent, qRound(kMinimumExtent / m_aspectRatio));
    }

    const QSize maxSize(qMax(1, qFloor(available.width() * kMaximumScreenRatio)),
                        qMax(1, qFloor(available.height() * kMaximumScreenRatio)));
    size.scale(maxSize, Qt::KeepAspectRatio);

    int minWidth = kMinimumExtent;
    int minHeight = qRound(kMinimumExtent / m_aspectRatio);
    if (m_aspectRatio < 1.0) {
        minHeight = kMinimumExtent;
        minWidth = qRound(kMinimumExtent * m_aspectRatio);
    }

    if (size.width() < minWidth || size.height() < minHeight) {
        size = QSize(minWidth, minHeight);
    }
    if (size.width() > maxSize.width() || size.height() > maxSize.height()) {
        size.scale(maxSize, Qt::KeepAspectRatio);
    }
    return size.expandedTo(QSize(1, 1));
}

void PinWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (!m_originalImage.isNull()) {
        painter.drawImage(rect(), m_originalImage);
    } else {
        painter.fillRect(rect(), Qt::white);
    }

    const QColor border = (m_hovered || hasFocus())
        ? QColor(64, 145, 255)
        : QColor(214, 220, 228);
    painter.setPen(QPen(border, (m_hovered || hasFocus()) ? 2 : 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void PinWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateCloseButtonGeometry();
}

void PinWindow::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    updateHoverState(event->position().toPoint());
    updateCloseButtonVisibility();
    update();
}

void PinWindow::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    m_hovered = false;
    if (m_dragState == DragState::None) {
        m_hoverResizeEdge = ResizeEdge::None;
        setCursor(Qt::ArrowCursor);
    }
    updateCloseButtonVisibility();
    update();
}

void PinWindow::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    updateCloseButtonVisibility();
    update();
}

void PinWindow::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    updateCloseButtonVisibility();
    update();
}

void PinWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_pressGlobalPos = event->globalPosition().toPoint();
    m_initialWindowTopLeft = frameGeometry().topLeft();
    m_initialGeometry = geometry();
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
