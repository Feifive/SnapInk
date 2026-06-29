# SnapInk Agent Guide

This file is for future coding agents working in this repository. Keep it portable:
do not add machine-specific absolute paths.

## Project Overview

SnapInk is a Qt 6 desktop screenshot annotation tool written in C++20. The app
captures screen images, opens a full-screen `CaptureOverlay`, lets the user select
an area, annotates that selection, and exports the annotated selection image.

Main technologies:

- C++20
- Qt 6 Core, Gui, Widgets, Test
- CMake
- CTest / Qt Test

## Repository Layout

- `CMakeLists.txt`
  - Defines the `SnapInk` app target and `SnapInkTests` test target.
  - Shared source groups are `SNAPINK_CORE_SOURCES` and `SNAPINK_CAPTURE_SOURCES`.
- `main.cpp`
  - Application entry point.
- `src/app/`
  - Main window and top-level app wiring.
- `src/core/capture/`
  - Screen capture data structures and capture service.
  - `CapturedScreen.h` contains per-screen capture metadata, logical geometry,
    physical image data, and DPR conversion helpers.
- `src/core/clipboard/`
  - Clipboard image write support.
- `src/core/hotkey/`
  - Cross-platform hotkey abstraction plus platform-specific backends.
  - `Hotkey.*` is the public QObject API; `Hotkey_p.h` holds per-instance
    state including a `void* platformHandle` used by backends to keep their
    native handle next to the public object (e.g. `EventHotKeyRef` on macOS).
  - `HotkeyPlatform_*.{cpp}` provide the per-OS backend. The Windows backend
    uses Win32 `RegisterHotKey` and dispatches `WM_HOTKEY` through Qt's native
    event filter. The macOS backend uses Carbon `RegisterEventHotKey` /
    `UnregisterEventHotKey` and dispatches activations through a Carbon
    `EventHandlerRef` callback installed on `GetApplicationEventTarget()`. The
    macOS backend does NOT need Accessibility permission.
  - On macOS, Qt::Key -> virtual key code is not an ASCII arithmetic mapping:
    macOS virtual codes follow the physical QWERTY layout, so the conversion
    uses an explicit table in the `SnapInk::HotkeyMac` namespace inside
    `HotkeyPlatform_mac.cpp`. That namespace is reused by `HotkeyMacTests`.
- `src/ui/capture/`
  - Screenshot selection and annotation UI.
  - `CaptureOverlay.*` owns the full-screen overlay, selection chrome, embedded
    annotation view/scene, toolbar, undo stack, and final rendering.
  - `CaptureAnnotation.*` defines graphics items for rectangle, arrow, pen, and text annotations.
  - `AddAnnotationCommand.*` contains undo/redo support for scene annotation items.
  - `CaptureToolbar.*` contains the capture/annotation toolbar.
  - `CaptureTool.h` defines available annotation tools.
- `tests/`
  - Qt Test based automated tests. The main test file is `tests/capture_overlay_tests.cpp`.
  - `tests/hotkey_mac_tests.cpp` covers macOS-only key/modifier conversion
    helpers; it is built as the separate `HotkeyMacTests` target only on
    `APPLE`.
- `docs/superpowers/`
  - Planning/spec notes from previous development work.
- `build/`
  - Local build outputs. Treat as generated.

## File Search

Use fast repository searches before editing:

- List files: `rg --files`
- Search text: `rg "pattern"`
- Search capture UI code: `rg "pattern" src/ui/capture tests`
- Search CMake/test targets: `rg "SnapInkTests|qt_add_executable|add_test" CMakeLists.txt tests`

Prefer `rg` over slower recursive shell searches.

## Build Targets

CMake defines two primary targets:

- `SnapInk`
  - Main GUI application.
- `SnapInkTests`
  - Qt Test executable registered with CTest as `SnapInkTests`.
- `HotkeyMacTests` (macOS only)
  - Qt Test executable registered with CTest as `HotkeyMacTests`. Covers the
    pure key/modifier conversion helpers used by the macOS hotkey backend.

Typical build commands, from the repository root:

```sh
cmake --build <build-dir> --target SnapInk
cmake --build <build-dir> --target SnapInkTests
```

`<build-dir>` is the local CMake build directory under `build/`. Do not hard-code
machine-specific generator or Qt installation paths in project files.

On Windows/MSVC, run builds from a developer environment where the compiler and Qt
runtime are on `PATH`. If linking `SnapInk.exe` fails with an access/open error,
check whether the app is currently running and locking the executable.

## Test Framework

Tests use Qt Test and are registered through CTest:

- Test source: `tests/capture_overlay_tests.cpp`
- Test executable target: `SnapInkTests`
- CTest test name: `SnapInkTests`

Run all tests:

```sh
ctest --test-dir <build-dir> --output-on-failure
```

Run one Qt Test function directly:

```sh
<build-dir>/SnapInkTests <testFunctionName>
```

For headless test runs, especially on CI or agent environments, set:

```sh
QT_QPA_PLATFORM=offscreen
```

Example intent, without local absolute paths:

```sh
cmake --build <build-dir> --target SnapInkTests
QT_QPA_PLATFORM=offscreen ctest --test-dir <build-dir> --output-on-failure
```

## Capture Overlay Architecture

`CaptureOverlay` has two major responsibilities:

- Overlay chrome drawn by the overlay itself:
  - full-screen screenshot background
  - dim mask outside the selected area
  - blue selection border and handles
  - size hint
  - toolbar placement
- Selection-local annotation editing through an embedded graphics scene:
  - `QGraphicsView` child named `AnnotationGraphicsView`
  - `QGraphicsScene` with origin at selection top-left
  - bottom `QGraphicsPixmapItem` for the original selected image
  - annotation items in selection-local coordinates

Important rules:

- The annotation view is a child widget of `CaptureOverlay`, not a separate window.
- The view geometry must match the selected rectangle.
- Scene coordinates are selection-relative, with `(0, 0)` at the selection top-left.
- Scene size must match the selected image size.
- Selection chrome remains overlay-owned and should not be exported.
- Final export should render the annotation scene, not the full overlay.

## Annotation Items

Annotation graphics items live in `src/ui/capture/CaptureAnnotation.*`:

- `RectAnnotationItem`
- `ArrowAnnotationItem`
- `PenAnnotationItem`
- `TextAnnotationItem`

Text annotations use `QGraphicsTextItem` editing in place. Do not use modal input
dialogs for text. Text editing is committed only after edit completion, so undo/redo
can treat confirmed text as an annotation command.

Current text behavior:

- Text starts with natural width.
- Text only gets a maximum width constraint when needed.
- Maximum text width is based on remaining selection width from the click position.
- Wrapping uses `QTextOption::WrapAnywhere` so mixed Chinese, numbers, and English
do not leave large gaps at line ends.

## Undo/Redo

Undo/redo is handled with `QUndoStack` in `CaptureOverlay` and commands in
`AddAnnotationCommand.*`.

Guidelines:

- Temporary preview items can be placed in the scene during drag/edit.
- Only confirmed, valid annotation items should be pushed to the undo stack.
- Empty or canceled text items must be deleted and must not remain in the scene.
- Redo should restore item content, style, position, and geometry.

## Export Behavior

`CaptureOverlay::renderResultImage()` should produce only the selected image plus
confirmed annotations.

Expected output:

- Size equals the selected image's physical pixel size.
- Includes selection background pixmap.
- Includes confirmed scene annotation items.

Must not include:

- dim mask outside selection
- selection border or handles
- size hint
- toolbar
- canceled or unconfirmed temporary text

## Coding Guidelines

- Prefer local project patterns over new abstractions.
- Keep coordinates explicit: overlay/global logical coordinates and selection-local
  scene coordinates are different.
- Do not revert unrelated user changes in a dirty worktree.
- Use `apply_patch` for manual edits.
- Add focused tests for capture/annotation behavior changes.
- Keep generated build output out of source edits.
- Avoid adding machine-specific absolute paths to scripts, docs, or source.

## Common Verification Flow

From the repository root:

```sh
cmake --build <build-dir> --target SnapInkTests
cmake --build <build-dir> --target HotkeyMacTests   # macOS only
QT_QPA_PLATFORM=offscreen ctest --test-dir <build-dir> --output-on-failure
cmake --build <build-dir> --target SnapInk
```

The macOS hotkey unit tests in `HotkeyMacTests` only exercise the pure
`SnapInk::HotkeyMac` conversion helpers — they do not register any
system-wide hotkey, so they are safe under `QT_QPA_PLATFORM=offscreen`. The
integration verification (pressing `Control+Option+A` and observing the
overlay open) is manual and must be performed with a real display session.

If the final app build fails because the executable cannot be opened, close or stop
the running `SnapInk` process and rerun the app build.
