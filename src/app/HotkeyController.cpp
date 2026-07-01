#include "HotkeyController.h"

#include "../core/hotkey/Hotkey.h"
#include "../core/hotkey/HotkeyConfig.h"

#include <QDebug>
#include <QGuiApplication>
#include <QKeySequence>
#include <QMessageBox>

HotkeyController::HotkeyController(QWidget* dialogParent, QObject* parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
{
}

HotkeyController::~HotkeyController()
{
    unregisterShortcuts();
}

void HotkeyController::registerShortcuts()
{
    if (m_registered) {
        return;
    }

    const QKeySequence regionShortcut = HotkeyConfig::regionCapture();
    m_regionHotkey = new Hotkey(regionShortcut, false, this);
    const QKeySequence restorePinShortcut = HotkeyConfig::restorePin();
    m_restorePinHotkey = new Hotkey(restorePinShortcut, false, this);

    connect(m_regionHotkey,
            &Hotkey::activated,
            this,
            &HotkeyController::regionCaptureRequested);
    connect(m_restorePinHotkey,
            &Hotkey::activated,
            this,
            &HotkeyController::restorePinRequested);

    connect(m_regionHotkey,
            &Hotkey::registrationFailed,
            this,
            [this, regionShortcut](const QString& reason) {
                handleRegistrationFailure(
                    regionShortcut.toString(QKeySequence::NativeText),
                    reason);
            });
    connect(m_restorePinHotkey,
            &Hotkey::registrationFailed,
            this,
            [this, restorePinShortcut](const QString& reason) {
                handleRegistrationFailure(
                    restorePinShortcut.toString(QKeySequence::NativeText),
                    reason);
            });

    m_regionHotkey->registerShortcut();
    m_restorePinHotkey->registerShortcut();
    m_registered = true;
}

void HotkeyController::unregisterShortcuts()
{
    if (m_regionHotkey != nullptr) {
        m_regionHotkey->unregisterShortcut();
        delete m_regionHotkey;
        m_regionHotkey = nullptr;
    }
    if (m_restorePinHotkey != nullptr) {
        m_restorePinHotkey->unregisterShortcut();
        delete m_restorePinHotkey;
        m_restorePinHotkey = nullptr;
    }
    m_registered = false;
}

void HotkeyController::handleRegistrationFailure(const QString& shortcut,
                                                  const QString& reason)
{
    qWarning().noquote()
        << QStringLiteral("Failed to register %1: %2")
               .arg(shortcut, reason);

    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        QMessageBox::warning(
            m_dialogParent,
            QStringLiteral("Global Hotkey Failed"),
            QStringLiteral("Could not register %1.\n%2").arg(shortcut, reason));
    }

    Q_EMIT shortcutRegistrationFailed(shortcut, reason);
}
