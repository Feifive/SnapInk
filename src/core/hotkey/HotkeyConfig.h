#ifndef HOTKEYCONFIG_H
#define HOTKEYCONFIG_H

#include <QKeySequence>
#include <QString>

/// Centralized hotkey configuration for SnapInk.
/// All global and local shortcut definitions live here so that
/// they can be discovered, changed, or persisted in one place.
namespace HotkeyConfig {

/// Global hotkey: open the region-capture overlay.
inline QKeySequence regionCapture() { return QKeySequence(QStringLiteral("Ctrl+1")); }

/// Overlay-local hotkey: pin the current selection.
inline QKeySequence pinCapture() { return QKeySequence(QStringLiteral("Ctrl+2")); }

/// Human-readable label for a given hotkey (used in UI hints / error messages).
inline QString regionCaptureLabel() { return regionCapture().toString(QKeySequence::NativeText); }
inline QString pinCaptureLabel()    { return pinCapture().toString(QKeySequence::NativeText); }

} // namespace HotkeyConfig

#endif // HOTKEYCONFIG_H
