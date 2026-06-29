#ifndef HOTKEY_P_H
#define HOTKEY_P_H

#include "Hotkey.h"

class HotkeyPrivate
{
public:
    explicit HotkeyPrivate(Hotkey* publicHotkey);

    void emitActivated();
    void emitRegistrationFailed(const QString& reason);

    Hotkey* q = nullptr;
    QKeySequence shortcut;
    bool registered = false;
    // Platform-specific opaque handle. macOS stores EventHotKeyRef here so the
    // platform backend can unregister it without an extra side map.
    void* platformHandle = nullptr;
};

#endif // HOTKEY_P_H
