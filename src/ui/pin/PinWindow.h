#ifndef PINWINDOW_H
#define PINWINDOW_H

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QWidget>

class QToolButton;

class PinWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PinWindow(const QImage& image, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    enum class ResizeEdge
    {
        None,
        Top,
        Bottom,
        Left,
        Right,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    enum class DragState
    {
        None,
        Moving,
        Resizing
    };

    QSize initialWindowSize() const;
    QSize boundedImageSize(const QSize& requested, const QRect& available) const;
    QRect availableGeometryFor(const QPoint& globalPos) const;
    QRect clampedVisibleGeometry(const QRect& geometry) const;
    ResizeEdge hitTestResizeEdge(const QPoint& localPos) const;
    QCursor cursorForResizeEdge(ResizeEdge edge) const;
    void updateHoverState(const QPoint& localPos);
    void updateCloseButtonGeometry();
    void updateCloseButtonVisibility();
    void resizeByMouseMove(const QPoint& globalPos);
    QRect aspectResizeGeometry(const QPoint& globalPos) const;
    QSize constrainedSize(QSize size, const QRect& available) const;

    QImage m_originalImage;
    QPixmap m_displayPixmap;
    QToolButton* m_closeButton = nullptr;
    QPoint m_pressGlobalPos;
    QPoint m_initialWindowTopLeft;
    QRect m_initialGeometry;
    ResizeEdge m_activeResizeEdge = ResizeEdge::None;
    ResizeEdge m_hoverResizeEdge = ResizeEdge::None;
    DragState m_dragState = DragState::None;
    qreal m_aspectRatio = 1.0;
    bool m_hovered = false;
    bool m_closing = false;
};

#endif // PINWINDOW_H
