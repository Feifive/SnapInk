#include "HotkeyPlatform.h"

#include "Hotkey_p.h"

#include <QCoreApplication>
#include <QDebug>
#include <QHash>
#include <QKeyCombination>
#include <QThread>

#include <Carbon/Carbon.h>

// ---------------------------------------------------------------------------
// Test entry points
// ---------------------------------------------------------------------------
//
// Key and modifier conversion helpers live in the SnapInk::HotkeyMac namespace
// (rather than an anonymous namespace) so the dedicated unit test file
// tests/hotkey_mac_tests.cpp can call them directly by qualified name. They
// remain internal to the SnapInk shared library / test executables and are not
// exported.

namespace SnapInk::HotkeyMac {

/// Map a Qt::Key to a macOS virtual key code.
///
/// macOS virtual key codes follow the physical keyboard layout, not ASCII
/// order, so an explicit table is required. The table covers the keys most
/// likely to be used as global hotkeys: letters, digits, F1-F12 and the common
/// special keys. Unknown keys return false and set *errorReason.
bool qtKeyToMacVirtualKey(Qt::Key key,
                          UInt32* virtualKey,
                          QString* errorReason,
                          const QString& sequenceText)
{
    const auto assign = [&](UInt32 value) {
        *virtualKey = value;
        return true;
    };

    // Letters A-Z. macOS virtual key codes follow the QWERTY layout, not ASCII.
    switch (key) {
    case Qt::Key_A: return assign(0);
    case Qt::Key_B: return assign(11);
    case Qt::Key_C: return assign(8);
    case Qt::Key_D: return assign(2);
    case Qt::Key_E: return assign(14);
    case Qt::Key_F: return assign(3);
    case Qt::Key_G: return assign(5);
    case Qt::Key_H: return assign(4);
    case Qt::Key_I: return assign(34);
    case Qt::Key_J: return assign(38);
    case Qt::Key_K: return assign(40);
    case Qt::Key_L: return assign(37);
    case Qt::Key_M: return assign(46);
    case Qt::Key_N: return assign(45);
    case Qt::Key_O: return assign(31);
    case Qt::Key_P: return assign(35);
    case Qt::Key_Q: return assign(12);
    case Qt::Key_R: return assign(15);
    case Qt::Key_S: return assign(1);
    case Qt::Key_T: return assign(17);
    case Qt::Key_U: return assign(32);
    case Qt::Key_V: return assign(9);
    case Qt::Key_W: return assign(13);
    case Qt::Key_X: return assign(7);
    case Qt::Key_Y: return assign(16);
    case Qt::Key_Z: return assign(6);
    default: break;
    }

    // Digits 0-9 on the top row. Numpad digits are intentionally not mapped.
    switch (key) {
    case Qt::Key_0: return assign(29);
    case Qt::Key_1: return assign(18);
    case Qt::Key_2: return assign(19);
    case Qt::Key_3: return assign(20);
    case Qt::Key_4: return assign(21);
    case Qt::Key_5: return assign(23);
    case Qt::Key_6: return assign(22);
    case Qt::Key_7: return assign(26);
    case Qt::Key_8: return assign(28);
    case Qt::Key_9: return assign(25);
    default: break;
    }

    // Function keys F1-F12.
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        static constexpr UInt32 kFunctionKeyCodes[] = {
            122, 120, 99, 118, 96, 97, 98, 100, 101, 109, 103, 111
        };
        return assign(kFunctionKeyCodes[key - Qt::Key_F1]);
    }

    // Common special keys.
    switch (key) {
    case Qt::Key_Escape:    return assign(53);
    case Qt::Key_Space:     return assign(49);
    case Qt::Key_Return:    return assign(36);   // main Return key
    case Qt::Key_Enter:     return assign(76);   // keypad Enter
    case Qt::Key_Tab:       return assign(48);
    case Qt::Key_Backspace: return assign(51);   // Mac "Delete" key (backspace)
    case Qt::Key_Delete:    return assign(117);  // Mac "Forward Delete" (fn+delete)
    case Qt::Key_Left:      return assign(123);
    case Qt::Key_Right:     return assign(124);
    case Qt::Key_Down:      return assign(125);
    case Qt::Key_Up:        return assign(126);
    case Qt::Key_Home:      return assign(115);
    case Qt::Key_End:       return assign(119);
    case Qt::Key_PageUp:    return assign(116);
    case Qt::Key_PageDown:  return assign(121);
    case Qt::Key_Insert:    return assign(114);  // Mac "Help" key
    default: break;
    }

    if (errorReason != nullptr) {
        *errorReason = QStringLiteral("Unsupported global hotkey key: %1.").arg(sequenceText);
    }
    return false;
}

/// Map Qt keyboard modifiers to Carbon modifier flags.
UInt32 qtModifiersToCarbonModifiers(Qt::KeyboardModifiers modifiers)
{
    UInt32 carbon = 0;
    if (modifiers & Qt::ShiftModifier)   carbon |= shiftKey;
    if (modifiers & Qt::ControlModifier) carbon |= controlKey;
    if (modifiers & Qt::AltModifier)     carbon |= optionKey;
    if (modifiers & Qt::MetaModifier)    carbon |= cmdKey;
    return carbon;
}

} // namespace SnapInk::HotkeyMac

// ---------------------------------------------------------------------------
// Internal helpers and state
// ---------------------------------------------------------------------------

namespace {

// Four-character code used as EventHotKeyID.signature. 'snik' = SnapInk.
constexpr FourCharCode kHotkeySignature = 'snik';

QString sequenceName(const QKeySequence& sequence)
{
    const QString text = sequence.toString(QKeySequence::NativeText);
    return text.isEmpty() ? QStringLiteral("<empty>") : text;
}

bool convertSequence(const QKeySequence& sequence,
                     UInt32* carbonModifiers,
                     UInt32* virtualKey,
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
    *carbonModifiers = SnapInk::HotkeyMac::qtModifiersToCarbonModifiers(modifiers);

    return SnapInk::HotkeyMac::qtKeyToMacVirtualKey(
        Qt::Key(combination.key()),
        virtualKey,
        errorReason,
        sequenceName(sequence));
}

QString osStatusToMessage(OSStatus status, const QKeySequence& sequence)
{
    QString base;
    switch (status) {
    case eventHotKeyExistsErr:
        base = QStringLiteral("Hotkey already in use by another application.");
        break;
    case paramErr:
        base = QStringLiteral("Invalid hotkey parameters.");
        break;
    case memFullErr:
        base = QStringLiteral("Out of memory while registering hotkey.");
        break;
    default:
        base = QStringLiteral("Carbon RegisterEventHotKey failed (OSStatus %1).")
                   .arg(static_cast<int>(status));
        break;
    }
    return QStringLiteral("Failed to register global hotkey %1: %2")
        .arg(sequenceName(sequence), base);
}

// ---------------------------------------------------------------------------
// Carbon event handler state (file-scope POD statics)
//
// Why file-scope statics instead of a struct returned from a function?
//
// C++ destroys function-local statics in reverse construction order. The
// previous design had two function-local statics:
//   1. HotkeyPlatform::instance() -> static HotkeyPlatform platform;
//   2. macState()                 -> static MacState state;
// state was constructed after platform (during the first registerHotkey call),
// so it was destroyed BEFORE platform. When ~HotkeyPlatform() then called
// macState(), it got back an already-destroyed MacState whose QHash members
// were gone -> UB -> crash on app exit (HotkeyPlatform::~HotkeyPlatform + 80).
//
// Fix: complex state (the QHash maps + id counter) is stored as members of
// HotkeyPlatform itself (m_hotkeysByNativeId / m_nativeIdsByHotkey /
// m_nextNativeId), so its lifetime is identical to HotkeyPlatform. The
// remaining Carbon-specific state (EventHandlerRef + installed flag) is plain
// old data with trivial destructors, so file-scope statics are safe here
// regardless of destruction order.
// ---------------------------------------------------------------------------

namespace {

EventHandlerRef g_carbonEventHandler = nullptr;
bool g_carbonEventHandlerInstalled = false;

} // anonymous namespace

OSStatus hotkeyEventHandler(EventHandlerCallRef /*callRef*/,
                            EventRef event,
                            void* /*userData*/)
{
    if (event == nullptr) {
        return noErr;
    }

    EventHotKeyID hotKeyId{};
    const OSStatus status = GetEventParameter(event,
                                              kEventParamDirectObject,
                                              typeEventHotKeyID,
                                              nullptr,
                                              sizeof(hotKeyId),
                                              nullptr,
                                              &hotKeyId);
    if (status != noErr) {
        return status;
    }

    const int nativeId = static_cast<int>(hotKeyId.id);
    // Dispatch through the HotkeyPlatform singleton. The singleton is alive for
    // the entire duration of main(), so this access is safe from the Carbon
    // event handler callback.
    HotkeyPlatform::instance().dispatchActivationByNativeId(nativeId);
    return noErr;
}

bool installCarbonEventHandler()
{
    if (g_carbonEventHandlerInstalled) {
        return true;
    }

    const EventTypeSpec eventTypes[] = {
        { kEventClassKeyboard, kEventHotKeyPressed }
    };

    const OSStatus status = InstallEventHandler(
        GetApplicationEventTarget(),
        hotkeyEventHandler,
        GetEventTypeCount(eventTypes),
        eventTypes,
        nullptr,
        &g_carbonEventHandler);

    if (status != noErr) {
        qWarning() << "Failed to install Carbon hotkey event handler. OSStatus:"
                   << static_cast<int>(status);
        return false;
    }

    g_carbonEventHandlerInstalled = true;
    return true;
}

void removeCarbonEventHandler()
{
    if (!g_carbonEventHandlerInstalled || g_carbonEventHandler == nullptr) {
        return;
    }
    RemoveEventHandler(g_carbonEventHandler);
    g_carbonEventHandler = nullptr;
    g_carbonEventHandlerInstalled = false;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// HotkeyPlatform implementation (macOS)
// ---------------------------------------------------------------------------

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
    // Unregister every still-registered hotkey. The Hotkey destructor calls
    // unregisterShortcut() already (Hotkey is a QObject child of MainWindow,
    // which is a stack object in main(), so it is destroyed before the
    // HotkeyPlatform singleton). But we do a defensive sweep here in case any
    // Hotkey outlived the platform teardown.
    //
    // We use HotkeyPlatform's own member maps (not a function-local static),
    // so this access is always safe — these maps share HotkeyPlatform's
    // lifetime and are still valid here.
    const auto ids = m_hotkeysByNativeId.keys();
    for (const int id : ids) {
        HotkeyPrivate* hotkey = m_hotkeysByNativeId.value(id);
        if (hotkey != nullptr && hotkey->platformHandle != nullptr) {
            const EventHotKeyRef ref = static_cast<EventHotKeyRef>(hotkey->platformHandle);
            UnregisterEventHotKey(ref);
            hotkey->platformHandle = nullptr;
        }
    }
    m_hotkeysByNativeId.clear();
    m_nativeIdsByHotkey.clear();

    removeCarbonEventHandler();
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

    // Carbon APIs must be called on the main thread.
    if (QCoreApplication::instance() != nullptr) {
        Q_ASSERT_X(QThread::currentThread() == QCoreApplication::instance()->thread(),
                   "HotkeyPlatform::registerHotkey",
                   "Carbon RegisterEventHotKey must be called on the main thread.");
    }

    ensureNativeEventFilterInstalled();

    if (m_nativeIdsByHotkey.contains(hotkey)) {
        if (errorReason != nullptr) {
            *errorReason = QStringLiteral("This Hotkey object is already registered.");
        }
        return false;
    }

    UInt32 carbonModifiers = 0;
    UInt32 virtualKey = 0;
    if (!convertSequence(sequence, &carbonModifiers, &virtualKey, errorReason)) {
        return false;
    }

    if (!installCarbonEventHandler()) {
        if (errorReason != nullptr) {
            *errorReason = QStringLiteral("Could not install Carbon event handler for global hotkeys.");
        }
        return false;
    }

    // Allocate a native id. m_nextNativeId is a HotkeyPlatform member, so its
    // lifetime is identical to the singleton — no static-destruction hazard.
    const UInt32 nativeId = static_cast<UInt32>(allocateNativeId());
    if (nativeId == 0) {
        if (errorReason != nullptr) {
            *errorReason = QStringLiteral("No native global hotkey id is available.");
        }
        return false;
    }

    EventHotKeyID hotKeyId{};
    hotKeyId.signature = kHotkeySignature;
    hotKeyId.id = nativeId;

    EventHotKeyRef ref = nullptr;
    const OSStatus status = RegisterEventHotKey(
        virtualKey,
        carbonModifiers,
        hotKeyId,
        GetApplicationEventTarget(),
        0,
        &ref);

    if (status != noErr) {
        if (errorReason != nullptr) {
            *errorReason = osStatusToMessage(status, sequence);
        }
        return false;
    }

    hotkey->platformHandle = ref;
    m_hotkeysByNativeId.insert(static_cast<int>(nativeId), hotkey);
    m_nativeIdsByHotkey.insert(hotkey, static_cast<int>(nativeId));

    qInfo().noquote() << "Registered global hotkey" << sequenceName(sequence)
                      << "with native id" << nativeId;
    return true;
}

void HotkeyPlatform::unregisterHotkey(HotkeyPrivate* hotkey)
{
    if (hotkey == nullptr) {
        return;
    }

    const auto it = m_nativeIdsByHotkey.find(hotkey);
    if (it == m_nativeIdsByHotkey.end()) {
        return;
    }

    const int nativeId = it.value();
    m_nativeIdsByHotkey.erase(it);
    m_hotkeysByNativeId.remove(nativeId);

    if (hotkey->platformHandle != nullptr) {
        const EventHotKeyRef ref = static_cast<EventHotKeyRef>(hotkey->platformHandle);
        const OSStatus status = UnregisterEventHotKey(ref);
        if (status != noErr) {
            qWarning() << "Failed to unregister global hotkey. Native id:"
                       << nativeId << "OSStatus:" << static_cast<int>(status);
        }
        hotkey->platformHandle = nullptr;
    }
}

bool HotkeyPlatform::dispatchActivationByNativeId(int nativeId)
{
    const auto it = m_hotkeysByNativeId.constFind(nativeId);
    if (it == m_hotkeysByNativeId.constEnd() || it.value() == nullptr) {
        return false;
    }
    it.value()->emitActivated();
    return true;
}

bool HotkeyPlatform::nativeEventFilter(const QByteArray& eventType,
                                       void* message,
                                       qintptr* result)
{
    // Carbon dispatches hotkey events through its own event handler callback
    // (hotkeyEventHandler), not through Qt's native event filter. Keep this
    // override as a no-op so the base class contract is satisfied.
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
    return false;
}

void HotkeyPlatform::ensureNativeEventFilterInstalled()
{
    // On macOS we install a Carbon EventHandler instead of a Qt
    // QAbstractNativeEventFilter. The Carbon handler is created lazily inside
    // registerHotkey() via installCarbonEventHandler() so it is only created
    // when actually needed (and so it is not installed during the unit tests
    // that exercise only the pure conversion functions).
}

int HotkeyPlatform::allocateNativeId()
{
    // Reuse the base-class id allocator (m_nextNativeId + m_hotkeysByNativeId
    // collision check). This keeps macOS and Windows using the same id
    // allocation logic, and crucially stores the id in HotkeyPlatform's own
    // member (m_nextNativeId) rather than a function-local static — avoiding
    // the static-destruction-order crash that bit us with MacState.
    constexpr int kMinNativeId = 1;
    constexpr int kMaxNativeId = 0x7FFFFFFF; // UInt32 max is fine for Carbon ids

    for (int attempts = 0; attempts <= 1000; ++attempts) {
        const int candidate = m_nextNativeId;
        ++m_nextNativeId;
        if (m_nextNativeId > kMaxNativeId) {
            m_nextNativeId = kMinNativeId;
        }

        if (!m_hotkeysByNativeId.contains(candidate)) {
            return candidate;
        }
    }

    return 0; // exhausted
}
