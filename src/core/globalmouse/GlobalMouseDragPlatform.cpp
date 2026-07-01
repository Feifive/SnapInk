#include "GlobalMouseDragPlatform.h"

#include "GlobalMouseDragMonitor.h"

GlobalMouseDragPlatform::GlobalMouseDragPlatform(GlobalMouseDragMonitor* monitor)
    : QObject(monitor)
    , m_monitor(monitor)
{
}

GlobalMouseDragPlatform::~GlobalMouseDragPlatform() = default;

GlobalMouseDragMonitor* GlobalMouseDragPlatform::monitor() const
{
    return m_monitor;
}
