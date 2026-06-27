#ifndef CAPTUREDSCREEN_H
#define CAPTUREDSCREEN_H

#include <QImage>
#include <QList>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QScreen>

#include <QtMath>

/// Per-screen capture result.
///
/// The image is stored at the original physical pixel resolution returned by
/// QScreen::grabWindow(0).  It must never be prescaled — scaling, if needed,
/// only happens during final export compositing.
struct CapturedScreen
{
    /// The QScreen that produced this capture (may be null after screen removal).
    QPointer<QScreen> screen;

    /// Logical geometry in virtual-desktop coordinates (Qt device-independent pixels).
    QRect logicalGeometry;

    /// Physical-pixel image captured from the screen.
    QImage image;

    /// Device pixel ratio: image width / logicalGeometry width.
    qreal devicePixelRatio = 1.0;
};

/// Owns a set of per-screen captures and provides logical ↔ physical coordinate
/// mapping.
class CaptureResult
{
public:
    CaptureResult() = default;

    explicit CaptureResult(QList<CapturedScreen> screens);

    /// Union of all CapturedScreen::logicalGeometry rects.
    QRect virtualLogicalGeometry() const;

    /// The captured screens (never prescaled).
    const QList<CapturedScreen> &screens() const;

    // ---- Coordinate conversion (global logical ↔ per-screen physical) --------

    /// Map a global logical point to physical pixel coordinates within @p screen.
    /// The point is NOT clamped — the caller must ensure it lies within the screen.
    QPoint logicalToPhysical(const CapturedScreen &screen,
                             const QPoint &globalLogicalPoint) const;

    /// Map a global logical rectangle to a physical pixel rectangle within
    /// @p screen.  The rectangle is intersected with the screen's logical
    /// geometry and then scaled by devicePixelRatio.  Returns a null QRect
    /// when there is no intersection.
    QRect logicalToPhysical(const CapturedScreen &screen,
                            const QRect &globalLogicalRect) const;

private:
    QList<CapturedScreen> m_screens;
    QRect m_virtualLogicalGeometry;
};

// ---------------------------------------------------------------------------
// Inline implementations
// ---------------------------------------------------------------------------

inline CaptureResult::CaptureResult(QList<CapturedScreen> screens)
    : m_screens(std::move(screens))
{
    for (const auto &s : m_screens) {
        m_virtualLogicalGeometry = m_virtualLogicalGeometry.isNull()
                                       ? s.logicalGeometry
                                       : m_virtualLogicalGeometry.united(s.logicalGeometry);
    }
}

inline QRect CaptureResult::virtualLogicalGeometry() const
{
    return m_virtualLogicalGeometry;
}

inline const QList<CapturedScreen> &CaptureResult::screens() const
{
    return m_screens;
}

inline QPoint CaptureResult::logicalToPhysical(const CapturedScreen &screen,
                                               const QPoint &globalLogicalPoint) const
{
    const QPoint localLogical = globalLogicalPoint - screen.logicalGeometry.topLeft();
    return QPoint(qRound(localLogical.x() * screen.devicePixelRatio),
                  qRound(localLogical.y() * screen.devicePixelRatio));
}

inline QRect CaptureResult::logicalToPhysical(const CapturedScreen &screen,
                                              const QRect &globalLogicalRect) const
{
    const QRect intersection = globalLogicalRect.intersected(screen.logicalGeometry);
    if (intersection.isEmpty()) {
        return {};
    }

    const QPoint localLogicalTL = intersection.topLeft() - screen.logicalGeometry.topLeft();
    const int physX = qRound(localLogicalTL.x() * screen.devicePixelRatio);
    const int physY = qRound(localLogicalTL.y() * screen.devicePixelRatio);
    const int physW = qRound(intersection.width() * screen.devicePixelRatio);
    const int physH = qRound(intersection.height() * screen.devicePixelRatio);

    QRect physicalRect(physX, physY, physW, physH);

    // Clamp to the image bounds to guard against rounding errors at edges.
    return physicalRect.intersected(screen.image.rect());
}

#endif // CAPTUREDSCREEN_H
