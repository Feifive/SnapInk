#include "Hotkey.h"

#include "HotkeyPlatform.h"
#include "Hotkey_p.h"

#include <QMetaObject>

HotkeyPrivate::HotkeyPrivate(Hotkey* publicHotkey)
    : q(publicHotkey)
{
}

void HotkeyPrivate::emitActivated()
{
    Q_EMIT q->activated();
}

void HotkeyPrivate::emitRegistrationFailed(const QString& reason)
{
    Q_EMIT q->registrationFailed(reason);
}

Hotkey::Hotkey(QObject* parent)
    : QObject(parent)
    , d(new HotkeyPrivate(this))
{
}

Hotkey::Hotkey(const QKeySequence& shortcut, bool autoRegister, QObject* parent)
    : Hotkey(parent)
{
    d->shortcut = shortcut;

    if (autoRegister) {
        QMetaObject::invokeMethod(this, [this]() {
            registerShortcut();
        }, Qt::QueuedConnection);
    }
}

Hotkey::~Hotkey()
{
    unregisterShortcut();
    delete d;
}

QKeySequence Hotkey::shortcut() const
{
    return d->shortcut;
}

void Hotkey::setShortcut(const QKeySequence& shortcut)
{
    if (d->shortcut == shortcut) {
        return;
    }

    const bool wasRegistered = d->registered;
    if (wasRegistered) {
        unregisterShortcut();
    }

    d->shortcut = shortcut;

    if (wasRegistered) {
        registerShortcut();
    }
}

bool Hotkey::isRegistered() const
{
    return d->registered;
}

bool Hotkey::registerShortcut()
{
    if (d->registered) {
        return true;
    }

    if (d->shortcut.isEmpty()) {
        d->emitRegistrationFailed(QStringLiteral("Cannot register an empty global hotkey."));
        return false;
    }

    QString reason;
    if (HotkeyPlatform::instance().registerHotkey(d, d->shortcut, &reason)) {
        d->registered = true;
        return true;
    }

    if (reason.isEmpty()) {
        reason = QStringLiteral("Failed to register global hotkey.");
    }

    d->emitRegistrationFailed(reason);
    return false;
}

void Hotkey::unregisterShortcut()
{
    if (!d->registered) {
        return;
    }

    HotkeyPlatform::instance().unregisterHotkey(d);
    d->registered = false;
}
