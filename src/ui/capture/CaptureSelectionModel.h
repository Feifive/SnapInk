#ifndef CAPTURESELECTIONMODEL_H
#define CAPTURESELECTIONMODEL_H

#include <QPoint>
#include <QRect>

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

class CaptureSelectionModel
{
public:
    explicit CaptureSelectionModel(const QRect& bounds = {});

    static constexpr int minimumSelectionSize() { return 2; }

    void setBounds(const QRect& bounds);
    QRect bounds() const;

    void clear();
    bool hasSelection() const;

    void beginSelection(const QPoint& point);
    void updateSelection(const QPoint& point);
    QRect currentSelection() const;
    bool commitSelection();

    QRect selectionRect() const;
    void setSelectionRect(const QRect& rect);

    bool contains(const QPoint& point) const;
    SelectionHandle hitTestHandle(const QPoint& point) const;

    void beginMove(const QPoint& point);
    bool updateMove(const QPoint& point);

    void beginResize(SelectionHandle handle, const QPoint& point);
    bool updateResize(const QPoint& point);

    void finishInteraction();
    SelectionInteraction interaction() const;
    SelectionHandle activeHandle() const;

    bool resizeByWheel(int delta);

private:
    QRect clippedRect(const QRect& rect) const;
    QRect boundedRect(const QRect& rect) const;
    bool hasBounds() const;

    QRect m_bounds;
    QRect m_selectionRect;
    QPoint m_selectionStart;
    QPoint m_selectionEnd;
    QRect m_initialSelectionRect;
    QPoint m_pressPoint;
    SelectionHandle m_activeHandle = SelectionHandle::None;
    SelectionInteraction m_interaction = SelectionInteraction::None;
    bool m_selecting = false;
};

#endif // CAPTURESELECTIONMODEL_H
