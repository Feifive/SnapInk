#ifndef PINWINDOW_H
#define PINWINDOW_H

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QWidget>

/// Internal content widget that renders the pinned image with rounded corners and border.
/// This widget is transparent to mouse events so PinWindow handles all interactions.
class PinContentWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PinContentWidget(const QImage& image, qreal aspectRatio, QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void setActive(bool active);
    bool isActive() const { return m_active; }
    void updateShadowEffect();
protected:
    void paintEvent(QPaintEvent* event) override;

    QImage m_image;
    qreal m_aspectRatio = 1.0;
    bool m_active = false;
};

class PinWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PinWindow(const QImage& image,
                       const QRect& sourceGlobalRect,
                       QWidget* parent = nullptr);

    /// Pure calculation helper: bound a requested size to fit within available area
    /// while preserving aspect ratio. Does not upscale sizes smaller than max.
    static QSize boundedImageSize(const QSize& requested, const QRect& available);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
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

    QRect availableGeometryFor(const QPoint& globalPos) const;
    QRect clampedVisibleGeometry(const QRect& geometry) const;
    ResizeEdge hitTestResizeEdge(const QPoint& localPos) const;
    QCursor cursorForResizeEdge(ResizeEdge edge) const;
    void updateHoverState(const QPoint& localPos);
    void resizeByMouseMove(const QPoint& globalPos);
    QRect aspectResizeGeometry(const QPoint& globalPos) const;
    QSize constrainedSize(QSize size, const QRect& available) const;

    QImage m_originalImage;
    QPixmap m_displayPixmap;
    QPoint m_pressGlobalPos;
    QPoint m_initialWindowTopLeft;
    QRect m_initialGeometry;
    ResizeEdge m_activeResizeEdge = ResizeEdge::None;
    ResizeEdge m_hoverResizeEdge = ResizeEdge::None;
    DragState m_dragState = DragState::None;
    qreal m_aspectRatio = 1.0;
    bool m_hovered = false;
    bool m_closing = false;
    bool m_nativeOverlayConfigured = false;

    // Content widget for rendering the image with shadow effect
    PinContentWidget* m_contentWidget = nullptr;
    static constexpr int kShadowMargin = 18;
};

#endif // PINWINDOW_H
