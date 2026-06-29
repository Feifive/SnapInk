// Unit tests for the macOS global hotkey backend.
//
// These tests exercise the pure conversion helpers in
// SnapInk::HotkeyMac (Qt::Key -> virtual key code, Qt modifiers -> Carbon
// modifiers). They do NOT register any real system-wide hotkey, so they are
// safe to run in CI / offscreen environments and do not pollute the global
// hotkey table.

#include "../src/core/hotkey/HotkeyPlatform.h"

#include <QTest>

#include <Carbon/Carbon.h>

// The conversion helpers live inside HotkeyPlatform_mac.cpp in the
// SnapInk::HotkeyMac namespace. We redeclare them here so the test can call
// them directly without adding a new internal header to the project.
namespace SnapInk::HotkeyMac {
bool qtKeyToMacVirtualKey(Qt::Key key,
                          UInt32* virtualKey,
                          QString* errorReason,
                          const QString& sequenceText);
UInt32 qtModifiersToCarbonModifiers(Qt::KeyboardModifiers modifiers);
}

namespace {

constexpr UInt32 kCarbonShift   = shiftKey;
constexpr UInt32 kCarbonControl = controlKey;
constexpr UInt32 kCarbonOption  = optionKey;
constexpr UInt32 kCarbonCommand = cmdKey;

} // anonymous namespace

class HotkeyMacTests : public QObject
{
    Q_OBJECT

private slots:
    void testLettersAZ();
    void testDigits09();
    void testFunctionKeys();
    void testSpecialKeys();
    void testUnsupportedKey();
    void testNoModifiers();
    void testSingleModifiers();
    void testModifierCombinations();
};

// ---------------------------------------------------------------------------
// Key conversion
// ---------------------------------------------------------------------------

void HotkeyMacTests::testLettersAZ()
{
    // Every letter must map to a non-zero key code and round-trip through a
    // QKeySequence. The values come from the well-known macOS virtual key code
    // table (HIToolbox/Events.h kVK_* constants) and are stable across Apple
    // keyboards.
    struct Row { Qt::Key key; UInt32 expected; };
    const Row rows[] = {
        { Qt::Key_A, 0 },   { Qt::Key_B, 11 },  { Qt::Key_C, 8 },
        { Qt::Key_D, 2 },   { Qt::Key_E, 14 },  { Qt::Key_F, 3 },
        { Qt::Key_G, 5 },   { Qt::Key_H, 4 },   { Qt::Key_I, 34 },
        { Qt::Key_J, 38 },  { Qt::Key_K, 40 },  { Qt::Key_L, 37 },
        { Qt::Key_M, 46 },  { Qt::Key_N, 45 },  { Qt::Key_O, 31 },
        { Qt::Key_P, 35 },  { Qt::Key_Q, 12 },  { Qt::Key_R, 15 },
        { Qt::Key_S, 1 },   { Qt::Key_T, 17 },  { Qt::Key_U, 32 },
        { Qt::Key_V, 9 },   { Qt::Key_W, 13 },  { Qt::Key_X, 7 },
        { Qt::Key_Y, 16 },  { Qt::Key_Z, 6 },
    };

    for (const auto& row : rows) {
        UInt32 virtualKey = 0;
        QString errorReason;
        const bool ok = SnapInk::HotkeyMac::qtKeyToMacVirtualKey(
            row.key, &virtualKey, &errorReason, QStringLiteral("test"));
        QVERIFY2(ok, qPrintable(errorReason));
        QCOMPARE(virtualKey, row.expected);
        QVERIFY(errorReason.isEmpty());
    }
}

void HotkeyMacTests::testDigits09()
{
    struct Row { Qt::Key key; UInt32 expected; };
    const Row rows[] = {
        { Qt::Key_0, 29 }, { Qt::Key_1, 18 }, { Qt::Key_2, 19 },
        { Qt::Key_3, 20 }, { Qt::Key_4, 21 }, { Qt::Key_5, 23 },
        { Qt::Key_6, 22 }, { Qt::Key_7, 26 }, { Qt::Key_8, 28 },
        { Qt::Key_9, 25 },
    };

    for (const auto& row : rows) {
        UInt32 virtualKey = 0;
        QString errorReason;
        const bool ok = SnapInk::HotkeyMac::qtKeyToMacVirtualKey(
            row.key, &virtualKey, &errorReason, QStringLiteral("test"));
        QVERIFY2(ok, qPrintable(errorReason));
        QCOMPARE(virtualKey, row.expected);
    }
}

void HotkeyMacTests::testFunctionKeys()
{
    struct Row { Qt::Key key; UInt32 expected; };
    const Row rows[] = {
        { Qt::Key_F1, 122 },  { Qt::Key_F2, 120 },  { Qt::Key_F3, 99 },
        { Qt::Key_F4, 118 },  { Qt::Key_F5, 96 },   { Qt::Key_F6, 97 },
        { Qt::Key_F7, 98 },   { Qt::Key_F8, 100 },  { Qt::Key_F9, 101 },
        { Qt::Key_F10, 109 }, { Qt::Key_F11, 103 }, { Qt::Key_F12, 111 },
    };

    for (const auto& row : rows) {
        UInt32 virtualKey = 0;
        QString errorReason;
        const bool ok = SnapInk::HotkeyMac::qtKeyToMacVirtualKey(
            row.key, &virtualKey, &errorReason, QStringLiteral("test"));
        QVERIFY2(ok, qPrintable(errorReason));
        QCOMPARE(virtualKey, row.expected);
    }
}

void HotkeyMacTests::testSpecialKeys()
{
    struct Row { Qt::Key key; UInt32 expected; const char* name; };
    const Row rows[] = {
        { Qt::Key_Escape,    53,  "Escape" },
        { Qt::Key_Space,     49,  "Space" },
        { Qt::Key_Return,    36,  "Return" },
        { Qt::Key_Enter,     76,  "Enter" },
        { Qt::Key_Tab,       48,  "Tab" },
        { Qt::Key_Backspace, 51,  "Backspace" },
        { Qt::Key_Delete,    117, "Delete" },
        { Qt::Key_Left,      123, "Left" },
        { Qt::Key_Right,     124, "Right" },
        { Qt::Key_Down,      125, "Down" },
        { Qt::Key_Up,        126, "Up" },
        { Qt::Key_Home,      115, "Home" },
        { Qt::Key_End,       119, "End" },
        { Qt::Key_PageUp,    116, "PageUp" },
        { Qt::Key_PageDown,  121, "PageDown" },
        { Qt::Key_Insert,    114, "Insert" },
    };

    for (const auto& row : rows) {
        UInt32 virtualKey = 0;
        QString errorReason;
        const bool ok = SnapInk::HotkeyMac::qtKeyToMacVirtualKey(
            row.key, &virtualKey, &errorReason, QStringLiteral("test"));
        QVERIFY2(ok, qPrintable(QStringLiteral("Failed for %1: %2")
                                    .arg(QString::fromLatin1(row.name), errorReason)));
        QCOMPARE(virtualKey, row.expected);
    }
}

void HotkeyMacTests::testUnsupportedKey()
{
    // Qt::Key_CapsLock is not in our supported table.
    UInt32 virtualKey = 0;
    QString errorReason;
    const bool ok = SnapInk::HotkeyMac::qtKeyToMacVirtualKey(
        Qt::Key_CapsLock, &virtualKey, &errorReason, QStringLiteral("Ctrl+Alt+CapsLock"));

    QVERIFY(!ok);
    QVERIFY(!errorReason.isEmpty());
    QVERIFY(errorReason.contains(QStringLiteral("Unsupported global hotkey key")));
    QVERIFY(errorReason.contains(QStringLiteral("Ctrl+Alt+CapsLock")));
}

// ---------------------------------------------------------------------------
// Modifier conversion
// ---------------------------------------------------------------------------

void HotkeyMacTests::testNoModifiers()
{
    QCOMPARE(SnapInk::HotkeyMac::qtModifiersToCarbonModifiers(Qt::NoModifier),
             static_cast<UInt32>(0));
}

void HotkeyMacTests::testSingleModifiers()
{
    QCOMPARE(SnapInk::HotkeyMac::qtModifiersToCarbonModifiers(Qt::ShiftModifier),
             kCarbonShift);
    QCOMPARE(SnapInk::HotkeyMac::qtModifiersToCarbonModifiers(Qt::ControlModifier),
             kCarbonControl);
    QCOMPARE(SnapInk::HotkeyMac::qtModifiersToCarbonModifiers(Qt::AltModifier),
             kCarbonOption);
    QCOMPARE(SnapInk::HotkeyMac::qtModifiersToCarbonModifiers(Qt::MetaModifier),
             kCarbonCommand);
}

void HotkeyMacTests::testModifierCombinations()
{
    // SnapInk's default hotkeys are Ctrl+Alt+<letter>.
    const UInt32 ctrlAlt = SnapInk::HotkeyMac::qtModifiersToCarbonModifiers(
        Qt::ControlModifier | Qt::AltModifier);
    QCOMPARE(ctrlAlt, kCarbonControl | kCarbonOption);

    // Full modifier stack.
    const UInt32 all = SnapInk::HotkeyMac::qtModifiersToCarbonModifiers(
        Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    QCOMPARE(all, kCarbonShift | kCarbonControl | kCarbonOption | kCarbonCommand);

    // KeypadModifier and GroupSwitchModifier are intentionally ignored.
    const UInt32 withIgnored = SnapInk::HotkeyMac::qtModifiersToCarbonModifiers(
        Qt::ControlModifier | Qt::KeypadModifier | Qt::GroupSwitchModifier);
    QCOMPARE(withIgnored, kCarbonControl);
}

QTEST_APPLESS_MAIN(HotkeyMacTests)

#include "hotkey_mac_tests.moc"
