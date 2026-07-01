#include "GlobalMouseDragPlatform.h"

#include "GlobalMouseDragMonitor.h"

#include <QPoint>
#include <QString>
#include <QtGlobal>

#include <ApplicationServices/ApplicationServices.h>

namespace
{
QPoint pointFromEvent(CGEventRef event)
{
    const CGPoint location = CGEventGetLocation(event);
    return QPoint(qRound(location.x), qRound(location.y));
}

bool optionModifierDown(CGEventRef event)
{
    return (CGEventGetFlags(event) & kCGEventFlagMaskAlternate) != 0;
}

class MacGlobalMouseDragPlatform final : public GlobalMouseDragPlatform
{
public:
    explicit MacGlobalMouseDragPlatform(GlobalMouseDragMonitor* monitor)
        : GlobalMouseDragPlatform(monitor)
    {
    }

    ~MacGlobalMouseDragPlatform() override
    {
        stop();
    }

    bool start(QString* errorReason) override
    {
        if (m_eventTap != nullptr) {
            return true;
        }

        const CGEventMask mask =
            CGEventMaskBit(kCGEventLeftMouseDown)
            | CGEventMaskBit(kCGEventLeftMouseDragged)
            | CGEventMaskBit(kCGEventLeftMouseUp)
            | CGEventMaskBit(kCGEventFlagsChanged);

        m_eventTap = CGEventTapCreate(kCGSessionEventTap,
                                      kCGHeadInsertEventTap,
                                      kCGEventTapOptionDefault,
                                      mask,
                                      &MacGlobalMouseDragPlatform::eventCallback,
                                      this);
        if (m_eventTap == nullptr) {
            if (errorReason != nullptr) {
                *errorReason = QStringLiteral(
                    "Could not install global mouse listener. Grant SnapInk Accessibility permission in System Settings.");
            }
            return false;
        }

        m_runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, m_eventTap, 0);
        if (m_runLoopSource == nullptr) {
            stop();
            if (errorReason != nullptr) {
                *errorReason = QStringLiteral("Could not create the global mouse listener run loop source.");
            }
            return false;
        }

        CFRunLoopAddSource(CFRunLoopGetMain(), m_runLoopSource, kCFRunLoopCommonModes);
        CGEventTapEnable(m_eventTap, true);
        return true;
    }

    void stop() override
    {
        m_dragging = false;
        if (m_eventTap != nullptr) {
            CGEventTapEnable(m_eventTap, false);
        }
        if (m_runLoopSource != nullptr) {
            CFRunLoopRemoveSource(CFRunLoopGetMain(), m_runLoopSource, kCFRunLoopCommonModes);
            CFRelease(m_runLoopSource);
            m_runLoopSource = nullptr;
        }
        if (m_eventTap != nullptr) {
            CFRelease(m_eventTap);
            m_eventTap = nullptr;
        }
    }

private:
    static CGEventRef eventCallback(CGEventTapProxy proxy,
                                    CGEventType type,
                                    CGEventRef event,
                                    void* userInfo)
    {
        Q_UNUSED(proxy)

        auto* self = static_cast<MacGlobalMouseDragPlatform*>(userInfo);
        if (self == nullptr || event == nullptr) {
            return event;
        }

        return self->handleEvent(type, event);
    }

    CGEventRef handleEvent(CGEventType type, CGEventRef event)
    {
        if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
            if (m_eventTap != nullptr) {
                CGEventTapEnable(m_eventTap, true);
            }
            return event;
        }

        GlobalMouseDragMonitor* dragMonitor = monitor();
        if (dragMonitor == nullptr) {
            return event;
        }

        const QPoint position = pointFromEvent(event);
        switch (type) {
        case kCGEventLeftMouseDown:
            if (optionModifierDown(event)) {
                m_dragging = true;
                dragMonitor->processNativeLeftButtonPress(position, true);
                return nullptr;
            }
            break;
        case kCGEventLeftMouseDragged:
            if (m_dragging) {
                const bool optionDown = optionModifierDown(event);
                dragMonitor->processNativeMouseMove(position, optionDown);
                if (!optionDown) {
                    m_dragging = false;
                }
                return nullptr;
            }
            break;
        case kCGEventLeftMouseUp:
            if (m_dragging) {
                m_dragging = false;
                dragMonitor->processNativeLeftButtonRelease(position);
                return nullptr;
            }
            break;
        case kCGEventFlagsChanged:
            if (m_dragging && !optionModifierDown(event)) {
                m_dragging = false;
                dragMonitor->processNativeModifierChanged(position, false);
            }
            break;
        default:
            break;
        }

        return event;
    }

    CFMachPortRef m_eventTap = nullptr;
    CFRunLoopSourceRef m_runLoopSource = nullptr;
    bool m_dragging = false;
};
}

GlobalMouseDragPlatform* createGlobalMouseDragPlatform(GlobalMouseDragMonitor* monitor)
{
    return new MacGlobalMouseDragPlatform(monitor);
}
