#include "GlobalMouseDragMonitor.h"

#include "GlobalMouseDragPlatform.h"

GlobalMouseDragMonitor::GlobalMouseDragMonitor(QObject* parent)
    : QObject(parent)
    , m_platform(createGlobalMouseDragPlatform(this))
{
}

GlobalMouseDragMonitor::~GlobalMouseDragMonitor()
{
    stop();
}

bool GlobalMouseDragMonitor::start()
{
    if (m_running) {
        return true;
    }

    if (!m_platform) {
        const QString reason = QStringLiteral("Global mouse drag is not supported on this platform.");
        Q_EMIT registrationFailed(reason);
        return false;
    }

    QString reason;
    if (!m_platform->start(&reason)) {
        Q_EMIT registrationFailed(reason);
        return false;
    }

    m_running = true;
    return true;
}

void GlobalMouseDragMonitor::stop()
{
    if (!m_running) {
        return;
    }

    if (m_platform) {
        m_platform->stop();
    }
    m_running = false;
}

bool GlobalMouseDragMonitor::isRunning() const
{
    return m_running;
}

void GlobalMouseDragMonitor::processNativeLeftButtonPress(const QPoint& globalPos,
                                                          bool modifierDown)
{
    dispatchTransition(m_tracker.handleLeftButtonPress(globalPos, modifierDown));
}

void GlobalMouseDragMonitor::processNativeMouseMove(const QPoint& globalPos, bool modifierDown)
{
    dispatchTransition(m_tracker.handleMouseMove(globalPos, modifierDown));
}

void GlobalMouseDragMonitor::processNativeLeftButtonRelease(const QPoint& globalPos)
{
    dispatchTransition(m_tracker.handleLeftButtonRelease(globalPos));
}

void GlobalMouseDragMonitor::processNativeModifierChanged(const QPoint& globalPos,
                                                          bool modifierDown)
{
    dispatchTransition(m_tracker.handleModifierChanged(globalPos, modifierDown));
}

void GlobalMouseDragMonitor::processNativeCancel(const QPoint& globalPos)
{
    dispatchTransition(m_tracker.cancel(globalPos));
}

void GlobalMouseDragMonitor::dispatchTransition(const GlobalMouseDragTransition& transition)
{
    switch (transition.type) {
    case GlobalMouseDragTransitionType::Started:
        Q_EMIT dragStarted(transition.position);
        break;
    case GlobalMouseDragTransitionType::Moved:
        Q_EMIT dragMoved(transition.position);
        break;
    case GlobalMouseDragTransitionType::Finished:
        Q_EMIT dragFinished(transition.position);
        break;
    case GlobalMouseDragTransitionType::Canceled:
        Q_EMIT dragCanceled(transition.position);
        break;
    case GlobalMouseDragTransitionType::None:
        break;
    }
}
