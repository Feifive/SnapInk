#include "CaptureSelectionModel.h"

#include <QtGlobal>

#include <array>
#include <utility>

namespace
{
constexpr int kMinimumSelectionSize = 2;
constexpr int kHandleHitRadius = 8;

int rightExclusive(const QRect& rect)
{
    return rect.left() + rect.width();
}

int bottomExclusive(const QRect& rect)
{
    return rect.top() + rect.height();
}

QRect rectFromExclusiveEdges(int left, int top, int rightEdge, int bottomEdge)
{
    return QRect(QPoint(left, top), QSize(rightEdge - left, bottomEdge - top));
}

QRect rectFromDragPoints(const QPoint& first, const QPoint& second)
{
    const int left = qMin(first.x(), second.x());
    const int top = qMin(first.y(), second.y());
    const int right = qMax(first.x(), second.x());
    const int bottom = qMax(first.y(), second.y());
    return rectFromExclusiveEdges(left, top, right, bottom);
}

bool adjustsLeft(SelectionHandle handle)
{
    return handle == SelectionHandle::TopLeft
           || handle == SelectionHandle::BottomLeft
           || handle == SelectionHandle::Left;
}

bool adjustsRight(SelectionHandle handle)
{
    return handle == SelectionHandle::TopRight
           || handle == SelectionHandle::Right
           || handle == SelectionHandle::BottomRight;
}

bool adjustsTop(SelectionHandle handle)
{
    return handle == SelectionHandle::TopLeft
           || handle == SelectionHandle::Top
           || handle == SelectionHandle::TopRight;
}

bool adjustsBottom(SelectionHandle handle)
{
    return handle == SelectionHandle::BottomLeft
           || handle == SelectionHandle::Bottom
           || handle == SelectionHandle::BottomRight;
}
}

CaptureSelectionModel::CaptureSelectionModel(const QRect& bounds)
    : m_bounds(bounds.normalized())
{
}

void CaptureSelectionModel::setBounds(const QRect& bounds)
{
    m_bounds = bounds.normalized();
    if (!m_selectionRect.isEmpty()) {
        m_selectionRect = boundedRect(m_selectionRect);
        if (m_selectionRect.isEmpty()) {
            finishInteraction();
        }
    }
}

QRect CaptureSelectionModel::bounds() const
{
    return m_bounds;
}

void CaptureSelectionModel::clear()
{
    m_selectionRect = {};
    m_selectionStart = {};
    m_selectionEnd = {};
    m_initialSelectionRect = {};
    m_pressPoint = {};
    m_activeHandle = SelectionHandle::None;
    m_interaction = SelectionInteraction::None;
    m_selecting = false;
}

bool CaptureSelectionModel::hasSelection() const
{
    return m_selecting || !m_selectionRect.isEmpty();
}

void CaptureSelectionModel::beginSelection(const QPoint& point)
{
    finishInteraction();
    m_selectionRect = {};
    m_selectionStart = point;
    m_selectionEnd = point;
    m_selecting = true;
}

void CaptureSelectionModel::updateSelection(const QPoint& point)
{
    if (!m_selecting) {
        return;
    }

    m_selectionEnd = point;
}

QRect CaptureSelectionModel::currentSelection() const
{
    if (m_selecting) {
        return clippedRect(rectFromDragPoints(m_selectionStart, m_selectionEnd));
    }

    return m_selectionRect;
}

bool CaptureSelectionModel::commitSelection()
{
    if (!m_selecting) {
        return !m_selectionRect.isEmpty();
    }

    const QRect committed = boundedRect(currentSelection());
    m_selecting = false;
    m_selectionRect = committed;
    return !m_selectionRect.isEmpty();
}

QRect CaptureSelectionModel::selectionRect() const
{
    return m_selectionRect;
}

void CaptureSelectionModel::setSelectionRect(const QRect& rect)
{
    m_selecting = false;
    finishInteraction();
    m_selectionRect = boundedRect(rect);
}

bool CaptureSelectionModel::contains(const QPoint& point) const
{
    return m_selectionRect.contains(point);
}

SelectionHandle CaptureSelectionModel::hitTestHandle(const QPoint& point) const
{
    if (m_selectionRect.isEmpty()) {
        return SelectionHandle::None;
    }

    const QRect r = m_selectionRect;
    const std::array<std::pair<SelectionHandle, QPoint>, 8> handles = {{
        {SelectionHandle::TopLeft, r.topLeft()},
        {SelectionHandle::Top, QPoint(r.center().x(), r.top())},
        {SelectionHandle::TopRight, r.topRight()},
        {SelectionHandle::Right, QPoint(r.right(), r.center().y())},
        {SelectionHandle::BottomRight, r.bottomRight()},
        {SelectionHandle::Bottom, QPoint(r.center().x(), r.bottom())},
        {SelectionHandle::BottomLeft, r.bottomLeft()},
        {SelectionHandle::Left, QPoint(r.left(), r.center().y())},
    }};

    for (const auto& handle : handles) {
        const QRect hitRect(handle.second - QPoint(kHandleHitRadius, kHandleHitRadius),
                            QSize(kHandleHitRadius * 2 + 1, kHandleHitRadius * 2 + 1));
        if (hitRect.contains(point)) {
            return handle.first;
        }
    }

    return SelectionHandle::None;
}

void CaptureSelectionModel::beginMove(const QPoint& point)
{
    if (m_selectionRect.isEmpty()) {
        finishInteraction();
        return;
    }

    m_selecting = false;
    m_interaction = SelectionInteraction::Moving;
    m_initialSelectionRect = m_selectionRect;
    m_pressPoint = point;
    m_activeHandle = SelectionHandle::None;
}

bool CaptureSelectionModel::updateMove(const QPoint& point)
{
    if (m_interaction != SelectionInteraction::Moving || m_initialSelectionRect.isEmpty()) {
        return false;
    }

    const QPoint delta = point - m_pressPoint;
    QPoint topLeft = m_initialSelectionRect.topLeft() + delta;
    if (hasBounds()) {
        topLeft.setX(qBound(m_bounds.left(),
                            topLeft.x(),
                            rightExclusive(m_bounds) - m_initialSelectionRect.width()));
        topLeft.setY(qBound(m_bounds.top(),
                            topLeft.y(),
                            bottomExclusive(m_bounds) - m_initialSelectionRect.height()));
    }

    const QRect moved(topLeft, m_initialSelectionRect.size());
    if (moved == m_selectionRect) {
        return false;
    }

    m_selectionRect = moved;
    return true;
}

void CaptureSelectionModel::beginResize(SelectionHandle handle, const QPoint& point)
{
    if (m_selectionRect.isEmpty() || handle == SelectionHandle::None) {
        finishInteraction();
        return;
    }

    m_selecting = false;
    m_interaction = SelectionInteraction::Resizing;
    m_initialSelectionRect = m_selectionRect;
    m_pressPoint = point;
    m_activeHandle = handle;
}

bool CaptureSelectionModel::updateResize(const QPoint& point)
{
    if (m_interaction != SelectionInteraction::Resizing
        || m_activeHandle == SelectionHandle::None
        || m_initialSelectionRect.isEmpty()) {
        return false;
    }

    const QPoint delta = point - m_pressPoint;
    int left = m_initialSelectionRect.left();
    int top = m_initialSelectionRect.top();
    int rightEdge = rightExclusive(m_initialSelectionRect);
    int bottomEdge = bottomExclusive(m_initialSelectionRect);

    if (adjustsLeft(m_activeHandle)) {
        const int proposedLeft = left + delta.x();
        left = hasBounds()
            ? qBound(m_bounds.left(), proposedLeft, rightEdge - kMinimumSelectionSize)
            : qMin(proposedLeft, rightEdge - kMinimumSelectionSize);
    }
    if (adjustsRight(m_activeHandle)) {
        const int proposedRight = rightEdge + delta.x();
        rightEdge = hasBounds()
            ? qBound(left + kMinimumSelectionSize, proposedRight, rightExclusive(m_bounds))
            : qMax(proposedRight, left + kMinimumSelectionSize);
    }
    if (adjustsTop(m_activeHandle)) {
        const int proposedTop = top + delta.y();
        top = hasBounds()
            ? qBound(m_bounds.top(), proposedTop, bottomEdge - kMinimumSelectionSize)
            : qMin(proposedTop, bottomEdge - kMinimumSelectionSize);
    }
    if (adjustsBottom(m_activeHandle)) {
        const int proposedBottom = bottomEdge + delta.y();
        bottomEdge = hasBounds()
            ? qBound(top + kMinimumSelectionSize, proposedBottom, bottomExclusive(m_bounds))
            : qMax(proposedBottom, top + kMinimumSelectionSize);
    }

    const QRect resized = rectFromExclusiveEdges(left, top, rightEdge, bottomEdge);
    if (resized == m_selectionRect) {
        return false;
    }

    m_selectionRect = resized;
    return true;
}

void CaptureSelectionModel::finishInteraction()
{
    m_interaction = SelectionInteraction::None;
    m_activeHandle = SelectionHandle::None;
    m_initialSelectionRect = {};
    m_pressPoint = {};
}

SelectionInteraction CaptureSelectionModel::interaction() const
{
    return m_interaction;
}

SelectionHandle CaptureSelectionModel::activeHandle() const
{
    return m_activeHandle;
}

bool CaptureSelectionModel::resizeByWheel(int delta)
{
    if (delta == 0 || m_selectionRect.isEmpty()) {
        return false;
    }

    int left = m_selectionRect.left();
    int top = m_selectionRect.top();
    int rightEdge = rightExclusive(m_selectionRect);
    int bottomEdge = bottomExclusive(m_selectionRect);

    if (delta > 0) {
        left = hasBounds() ? qMax(m_bounds.left(), left - 1) : left - 1;
        top = hasBounds() ? qMax(m_bounds.top(), top - 1) : top - 1;
        rightEdge = hasBounds() ? qMin(rightExclusive(m_bounds), rightEdge + 1) : rightEdge + 1;
        bottomEdge = hasBounds() ? qMin(bottomExclusive(m_bounds), bottomEdge + 1) : bottomEdge + 1;
    } else {
        if (rightEdge - left <= kMinimumSelectionSize || bottomEdge - top <= kMinimumSelectionSize) {
            return false;
        }
        left = qMin(left + 1, rightEdge - kMinimumSelectionSize);
        top = qMin(top + 1, bottomEdge - kMinimumSelectionSize);
        rightEdge = qMax(rightEdge - 1, left + kMinimumSelectionSize);
        bottomEdge = qMax(bottomEdge - 1, top + kMinimumSelectionSize);
    }

    const QRect resized = rectFromExclusiveEdges(left, top, rightEdge, bottomEdge);
    if (resized == m_selectionRect) {
        return false;
    }

    m_selectionRect = resized;
    return true;
}

QRect CaptureSelectionModel::clippedRect(const QRect& rect) const
{
    QRect clipped = rect.normalized();
    if (hasBounds()) {
        clipped = clipped.intersected(m_bounds);
    }
    return clipped;
}

QRect CaptureSelectionModel::boundedRect(const QRect& rect) const
{
    const QRect bounded = clippedRect(rect);
    if (bounded.width() < kMinimumSelectionSize || bounded.height() < kMinimumSelectionSize) {
        return {};
    }
    return bounded;
}

bool CaptureSelectionModel::hasBounds() const
{
    return !m_bounds.isNull();
}
