#include "MacWindowHelper.h"

#include <QGuiApplication>
#include <QWidget>
#include <QWindow>

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

namespace MacWindowHelper
{

namespace
{

void configureOverlayWindow(QWidget* widget, NSWindowLevel level, bool makePanelNonactivating)
{
    if (widget == nullptr) {
        return;
    }

    if (qGuiApp == nullptr || QGuiApplication::platformName() != QStringLiteral("cocoa")) {
        return;
    }

    QWindow* window = widget->windowHandle();
    if (window == nullptr) {
        return;
    }

    NSView* nativeView = reinterpret_cast<NSView*>(window->winId());
    if (nativeView == nil) {
        return;
    }

    NSWindow* nsWindow = nativeView.window;
    if (nsWindow == nil) {
        return;
    }

    if (makePanelNonactivating && [nsWindow isKindOfClass:NSPanel.class]) {
        // Qt::Tool maps to QNSPanel on macOS. Marking the panel as
        // non-activating lets it remain an overlay for another app's active
        // fullscreen Space instead of behaving like a normal app window.
        nsWindow.styleMask = nsWindow.styleMask | NSWindowStyleMaskNonactivatingPanel;
    }

    [nsWindow setLevel:level];

    NSWindowCollectionBehavior collectionBehavior = nsWindow.collectionBehavior;
    collectionBehavior &= ~NSWindowCollectionBehaviorMoveToActiveSpace;
    collectionBehavior |= NSWindowCollectionBehaviorCanJoinAllSpaces
        | NSWindowCollectionBehaviorStationary;

    if (@available(macOS 14.0, *)) {
        // macOS 14+ validates CanJoinAllApplications and FullScreenAuxiliary as
        // mutually exclusive. CanJoinAllApplications lets the pin join other
        // apps' fullscreen Spaces without using a system-reserved window level.
        collectionBehavior |= NSWindowCollectionBehaviorCanJoinAllApplications;
    } else {
        // Older macOS releases use FullScreenAuxiliary for fullscreen Space
        // participation. Do not combine it with CanJoinAllApplications.
        collectionBehavior |= NSWindowCollectionBehaviorFullScreenAuxiliary;
    }

    // Join every Space, stay visually fixed during Space transitions, and use
    // the per-OS fullscreen participation behavior selected above.
    [nsWindow setCollectionBehavior:collectionBehavior];

    // Keep the Qt tool window visible after SnapInk loses application focus.
    [nsWindow setHidesOnDeactivate:NO];
    [nsWindow setCanHide:NO];
}

} // namespace

void configureOverlayWindow(QWidget* widget)
{
    // Float pin windows above normal and fullscreen application windows without
    // covering system UI such as the menu bar.
    configureOverlayWindow(widget, NSModalPanelWindowLevel, true);
}

void configureCaptureOverlayWindow(QWidget* widget)
{
    // Screenshot capture is a temporary modal surface and must cover the whole
    // captured desktop, including the macOS menu bar and Dock.
    const NSWindowLevel captureLevel =
        static_cast<NSWindowLevel>(CGWindowLevelForKey(kCGScreenSaverWindowLevelKey));
    configureOverlayWindow(widget, captureLevel, false);
}

} // namespace MacWindowHelper
