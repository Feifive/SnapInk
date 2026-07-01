#include "GlobalMouseDragPlatform.h"

#include "GlobalMouseDragMonitor.h"

#include <QMetaObject>
#include <QPoint>
#include <QString>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace
{
class WinGlobalMouseDragPlatform final : public GlobalMouseDragPlatform
{
public:
    explicit WinGlobalMouseDragPlatform(GlobalMouseDragMonitor* monitor)
        : GlobalMouseDragPlatform(monitor)
    {
    }

    ~WinGlobalMouseDragPlatform() override
    {
        stop();
    }

    bool start(QString* errorReason) override
    {
        if (m_mouseHook != nullptr && m_keyboardHook != nullptr) {
            return true;
        }

        s_instance = this;
        m_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, &WinGlobalMouseDragPlatform::mouseHookProc, nullptr, 0);
        if (m_mouseHook == nullptr) {
            s_instance = nullptr;
            if (errorReason != nullptr) {
                *errorReason = QStringLiteral("Could not install global mouse hook (Win32 error %1).")
                                   .arg(GetLastError());
            }
            return false;
        }
        m_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL,
                                           &WinGlobalMouseDragPlatform::keyboardHookProc,
                                           nullptr,
                                           0);
        if (m_keyboardHook == nullptr) {
            const DWORD error = GetLastError();
            stop();
            if (errorReason != nullptr) {
                *errorReason = QStringLiteral("Could not install global keyboard hook (Win32 error %1).")
                                   .arg(error);
            }
            return false;
        }
        return true;
    }

    void stop() override
    {
        m_dragging = false;
        if (m_keyboardHook != nullptr) {
            UnhookWindowsHookEx(m_keyboardHook);
            m_keyboardHook = nullptr;
        }
        if (m_mouseHook != nullptr) {
            UnhookWindowsHookEx(m_mouseHook);
            m_mouseHook = nullptr;
        }
        if (s_instance == this) {
            s_instance = nullptr;
        }
    }

private:
    static LRESULT CALLBACK mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode < 0 || s_instance == nullptr) {
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        const auto* mouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        if (mouse == nullptr) {
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        if (s_instance->handleMouseMessage(wParam, *mouse)) {
            return 1;
        }

        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    static LRESULT CALLBACK keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode < 0 || s_instance == nullptr) {
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        const auto* keyboard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (keyboard == nullptr) {
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        if (s_instance->handleKeyboardMessage(wParam, *keyboard)) {
            return 1;
        }

        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    bool handleMouseMessage(WPARAM message, const MSLLHOOKSTRUCT& mouse)
    {
        GlobalMouseDragMonitor* dragMonitor = monitor();
        if (dragMonitor == nullptr) {
            return false;
        }

        const QPoint position(mouse.pt.x, mouse.pt.y);
        m_lastPosition = position;
        switch (message) {
        case WM_LBUTTONDOWN:
            if (winModifierDown()) {
                m_dragging = true;
                invokeMonitor([dragMonitor, position]() {
                    dragMonitor->processNativeLeftButtonPress(position, true);
                });
                return true;
            }
            break;
        case WM_MOUSEMOVE:
            if (m_dragging) {
                const bool modifierDown = winModifierDown();
                invokeMonitor([dragMonitor, position, modifierDown]() {
                    dragMonitor->processNativeMouseMove(position, modifierDown);
                });
                if (!modifierDown) {
                    m_dragging = false;
                }
                return true;
            }
            break;
        case WM_LBUTTONUP:
            if (m_dragging) {
                m_dragging = false;
                invokeMonitor([dragMonitor, position]() {
                    dragMonitor->processNativeLeftButtonRelease(position);
                });
                return true;
            }
            break;
        default:
            break;
        }

        return false;
    }

    bool handleKeyboardMessage(WPARAM message, const KBDLLHOOKSTRUCT& keyboard)
    {
        if (!m_dragging || (message != WM_KEYUP && message != WM_SYSKEYUP)) {
            return false;
        }

        if (keyboard.vkCode != VK_LWIN && keyboard.vkCode != VK_RWIN) {
            return false;
        }

        GlobalMouseDragMonitor* dragMonitor = monitor();
        if (dragMonitor == nullptr) {
            return false;
        }

        const QPoint position = m_lastPosition;
        m_dragging = false;
        invokeMonitor([dragMonitor, position]() {
            dragMonitor->processNativeModifierChanged(position, false);
        });
        return true;
    }

    static bool winModifierDown()
    {
        return (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0
               || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;
    }

    template <typename Callback>
    void invokeMonitor(Callback callback)
    {
        GlobalMouseDragMonitor* dragMonitor = monitor();
        if (dragMonitor == nullptr) {
            return;
        }

        QMetaObject::invokeMethod(dragMonitor, callback, Qt::QueuedConnection);
    }

    HHOOK m_mouseHook = nullptr;
    HHOOK m_keyboardHook = nullptr;
    QPoint m_lastPosition;
    bool m_dragging = false;
    static inline WinGlobalMouseDragPlatform* s_instance = nullptr;
};
}

GlobalMouseDragPlatform* createGlobalMouseDragPlatform(GlobalMouseDragMonitor* monitor)
{
    return new WinGlobalMouseDragPlatform(monitor);
}
