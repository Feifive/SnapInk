#include "GlobalMouseDragTracker.h"

bool GlobalMouseDragTracker::isDragging() const
{
    return m_dragging;
}

GlobalMouseDragTransition GlobalMouseDragTracker::handleLeftButtonPress(const QPoint& position,
                                                                        bool modifierDown)
{
    if (!modifierDown || m_dragging) {
        return none();
    }

    return start(position);
}

GlobalMouseDragTransition GlobalMouseDragTracker::handleMouseMove(const QPoint& position,
                                                                  bool modifierDown)
{
    if (!m_dragging) {
        return none();
    }

    if (!modifierDown) {
        return finish(position);
    }

    return move(position);
}

GlobalMouseDragTransition GlobalMouseDragTracker::handleLeftButtonRelease(const QPoint& position)
{
    if (!m_dragging) {
        return none();
    }

    return finish(position);
}

GlobalMouseDragTransition GlobalMouseDragTracker::handleModifierChanged(const QPoint& position,
                                                                        bool modifierDown)
{
    if (!m_dragging || modifierDown) {
        return none();
    }

    return finish(position);
}

GlobalMouseDragTransition GlobalMouseDragTracker::cancel(const QPoint& position)
{
    if (!m_dragging) {
        return none();
    }

    return cancelDragging(position);
}

GlobalMouseDragTransition GlobalMouseDragTracker::none()
{
    return {};
}

GlobalMouseDragTransition GlobalMouseDragTracker::start(const QPoint& position)
{
    m_dragging = true;
    return {GlobalMouseDragTransitionType::Started, position};
}

GlobalMouseDragTransition GlobalMouseDragTracker::move(const QPoint& position) const
{
    return {GlobalMouseDragTransitionType::Moved, position};
}

GlobalMouseDragTransition GlobalMouseDragTracker::finish(const QPoint& position)
{
    m_dragging = false;
    return {GlobalMouseDragTransitionType::Finished, position};
}

GlobalMouseDragTransition GlobalMouseDragTracker::cancelDragging(const QPoint& position)
{
    m_dragging = false;
    return {GlobalMouseDragTransitionType::Canceled, position};
}
