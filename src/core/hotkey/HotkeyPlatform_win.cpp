#include "HotkeyPlatform.h"

#include "Hotkey_p.h"

#include <QCoreApplication>
#include <QDebug>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace
{
constexpr int kMinNativeId = 1;
constexpr int kMaxNativeId = 0xBFFF;

QString sequenceName(const QKeySequence& sequence)
{
    const QString text = sequence.toString(QKeySequence::NativeText);
    return text.isEmpty() ? QStringLiteral("<empty>") : text;
}

bool convertKey(const int key, UINT* virtualKey, QString* errorReason, const QKeySequence& sequence)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        *virtualKey = static_cast<UINT>('A' + (key - Qt::Key_A));
        return true;
    }

    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        *virtualKey = static_cast<UINT>('0' + (key - Qt::Key_0));
        return true;
    }

    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        *virtualKey = static_cast<UINT>(VK_F1 + (key - Qt::Key_F1));
        return true;
    }

    switch (key) {
    case Qt::Key_Escape:
        *virtualKey = VK_ESCAPE;
        return true;
    case Qt::Key_Space:
        *virtualKey = VK_SPACE;
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        *virtualKey = VK_RETURN;
        return true;
    case Qt::Key_Print:
        *virtualKey = VK_SNAPSHOT;
        return true;
    default:
        if (errorReason != nullptr) {
            *errorReason = QStringLiteral("Unsupported global hotkey key: %1.")
                               .arg(sequenceName(sequence));
        }
        return false;
    }
}

bool convertSequence(const QKeySequence& sequence,
                     UINT* nativeModifiers,
                     UINT* virtualKey,
                     QString* errorReason)
{
    if (sequence.isEmpty()) {
        if (errorReason != nullptr) {
            *errorReason = QStringLiteral("Cannot register an empty global hotkey.");
        }
        return false;
    }

    if (sequence.count() != 1) {
        if (errorReason != nullptr) {
            *errorReason = QStringLiteral("Global hotkeys support only one key sequence, but got %1.")
                               .arg(sequenceName(sequence));
        }
        return false;
    }

    const QKeyCombination combination = sequence[0];
    const Qt::KeyboardModifiers modifiers = combination.keyboardModifiers();

    *nativeModifiers = 0;
    if (modifiers.testFlag(Qt::ControlModifier)) {
        *nativeModifiers |= MOD_CONTROL;
    }
    if (modifiers.testFlag(Qt::AltModifier)) {
        *nativeModifiers |= MOD_ALT;
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        *nativeModifiers |= MOD_SHIFT;
    }
    if (modifiers.testFlag(Qt::MetaModifier)) {
        *nativeModifiers |= MOD_WIN;
    }

    return convertKey(combination.key(), virtualKey, errorReason, sequence);
}

QString registerErrorMessage(const QKeySequence& sequence, DWORD errorCode)
{
    return QStringLiteral("Failed to register global hotkey %1 (Win32 error %2). It may already be in use.")
        .arg(sequenceName(sequence))
        .arg(errorCode);
}
}

HotkeyPlatform& HotkeyPlatform::instance()
{
    static HotkeyPlatform platform;
    return platform;
}

HotkeyPlatform::HotkeyPlatform()
{
    ensureNativeEventFilterInstalled();
}

HotkeyPlatform::~HotkeyPlatform()
{
    const auto registeredIds = m_hotkeysByNativeId.keys();
    for (const int nativeId : registeredIds) {
        if (!UnregisterHotKey(nullptr, nativeId)) {
            qWarning() << "Failed to unregister global hotkey during shutdown. Native id:"
                       << nativeId
                       << "Win32 error:"
                       << GetLastError();
        }
    }

    m_hotkeysByNativeId.clear();
    m_nativeIdsByHotkey.clear();

    if (m_eventFilterInstalled && QCoreApplication::instance() != nullptr) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
}

bool HotkeyPlatform::registerHotkey(HotkeyPrivate* hotkey,
                                    const QKeySequence& sequence,
                                    QString* errorReason)
{
    if (hotkey == nullptr) {
        if (errorReason != nullptr) {
            *errorReason = QStringLiteral("Cannot register a null global hotkey.");
        }
        return false;
    }

    ensureNativeEventFilterInstalled();

    if (m_nativeIdsByHotkey.contains(hotkey)) {
        if (errorReason != nullptr) {
            *errorReason = QStringLiteral("This Hotkey object is already registered.");
        }
        return false;
    }

    UINT nativeModifiers = 0;
    UINT virtualKey = 0;
    if (!convertSequence(sequence, &nativeModifiers, &virtualKey, errorReason)) {
        return false;
    }

    const int nativeId = allocateNativeId();
    if (nativeId == 0) {
        if (errorReason != nullptr) {
            *errorReason = QStringLiteral("No native global hotkey id is available.");
        }
        return false;
    }

    if (!RegisterHotKey(nullptr, nativeId, nativeModifiers, virtualKey)) {
        if (errorReason != nullptr) {
            *errorReason = registerErrorMessage(sequence, GetLastError());
        }
        return false;
    }

    m_hotkeysByNativeId.insert(nativeId, hotkey);
    m_nativeIdsByHotkey.insert(hotkey, nativeId);

    qInfo() << "Registered global hotkey" << sequenceName(sequence) << "with native id" << nativeId;
    return true;
}

void HotkeyPlatform::unregisterHotkey(HotkeyPrivate* hotkey)
{
    const auto it = m_nativeIdsByHotkey.find(hotkey);
    if (it == m_nativeIdsByHotkey.end()) {
        return;
    }

    const int nativeId = it.value();
    m_nativeIdsByHotkey.erase(it);
    m_hotkeysByNativeId.remove(nativeId);

    if (!UnregisterHotKey(nullptr, nativeId)) {
        qWarning() << "Failed to unregister global hotkey. Native id:"
                   << nativeId
                   << "Win32 error:"
                   << GetLastError();
    }
}

bool HotkeyPlatform::nativeEventFilter(const QByteArray& eventType,
                                       void* message,
                                       qintptr* result)
{
    Q_UNUSED(eventType)
    Q_UNUSED(result)

    const MSG* nativeMessage = static_cast<MSG*>(message);
    if (nativeMessage == nullptr || nativeMessage->message != WM_HOTKEY) {
        return false;
    }

    const int nativeId = static_cast<int>(nativeMessage->wParam);
    const auto it = m_hotkeysByNativeId.constFind(nativeId);
    if (it == m_hotkeysByNativeId.constEnd() || it.value() == nullptr) {
        return false;
    }

    it.value()->emitActivated();
    return true;
}

void HotkeyPlatform::ensureNativeEventFilterInstalled()
{
    if (m_eventFilterInstalled) {
        return;
    }

    QCoreApplication* app = QCoreApplication::instance();
    if (app == nullptr) {
        qWarning() << "Cannot install global hotkey native event filter before QCoreApplication exists.";
        return;
    }

    app->installNativeEventFilter(this);
    m_eventFilterInstalled = true;
}

int HotkeyPlatform::allocateNativeId()
{
    for (int attempts = 0; attempts <= kMaxNativeId - kMinNativeId; ++attempts) {
        const int candidate = m_nextNativeId;
        ++m_nextNativeId;
        if (m_nextNativeId > kMaxNativeId) {
            m_nextNativeId = kMinNativeId;
        }

        if (!m_hotkeysByNativeId.contains(candidate)) {
            return candidate;
        }
    }

    return 0;
}
