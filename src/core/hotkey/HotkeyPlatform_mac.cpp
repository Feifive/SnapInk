#include "HotkeyPlatform.h"

#include <QDebug>

HotkeyPlatform& HotkeyPlatform::instance()
{
    static HotkeyPlatform platform;
    return platform;
}

HotkeyPlatform::HotkeyPlatform() = default;

HotkeyPlatform::~HotkeyPlatform() = default;

bool HotkeyPlatform::registerHotkey(HotkeyPrivate* hotkey,
                                    const QKeySequence& sequence,
                                    QString* errorReason)
{
    Q_UNUSED(hotkey)
    Q_UNUSED(sequence)

    const QString reason = QStringLiteral("Global hotkeys are not implemented on macOS yet.");
    if (errorReason != nullptr) {
        *errorReason = reason;
    }

    qWarning() << reason;
    return false;
}

void HotkeyPlatform::unregisterHotkey(HotkeyPrivate* hotkey)
{
    Q_UNUSED(hotkey)
}

bool HotkeyPlatform::nativeEventFilter(const QByteArray& eventType,
                                       void* message,
                                       qintptr* result)
{
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
    return false;
}

void HotkeyPlatform::ensureNativeEventFilterInstalled()
{
}

int HotkeyPlatform::allocateNativeId()
{
    return 0;
}
