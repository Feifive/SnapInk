#ifndef GLOBALMOUSEDRAGTRACKER_H
#define GLOBALMOUSEDRAGTRACKER_H

#include <QPoint>

enum class GlobalMouseDragTransitionType
{
    None,
    Started,
    Moved,
    Finished,
    Canceled
};

struct GlobalMouseDragTransition
{
    GlobalMouseDragTransitionType type = GlobalMouseDragTransitionType::None;
    QPoint position;
};

class GlobalMouseDragTracker
{
public:
    [[nodiscard]] bool isDragging() const;

    GlobalMouseDragTransition handleLeftButtonPress(const QPoint& position, bool modifierDown);
    GlobalMouseDragTransition handleMouseMove(const QPoint& position, bool modifierDown);
    GlobalMouseDragTransition handleLeftButtonRelease(const QPoint& position);
    GlobalMouseDragTransition handleModifierChanged(const QPoint& position, bool modifierDown);
    GlobalMouseDragTransition cancel(const QPoint& position);

private:
    static GlobalMouseDragTransition none();
    GlobalMouseDragTransition start(const QPoint& position);
    GlobalMouseDragTransition move(const QPoint& position) const;
    GlobalMouseDragTransition finish(const QPoint& position);
    GlobalMouseDragTransition cancelDragging(const QPoint& position);

    bool m_dragging = false;
};

#endif // GLOBALMOUSEDRAGTRACKER_H
