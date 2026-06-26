# SnapInk MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the requested SnapInk screenshot MVP while preserving the existing hotkey module.

**Architecture:** Keep `MainWindow` as an orchestrator. Put virtual desktop capture in `src/core/capture`, clipboard export in `src/core/clipboard`, and selection/editing/annotation logic directly in `src/ui/capture`.

**Tech Stack:** C++20, Qt 6.8.3 Widgets/Gui/Test, CMake, Windows first, no third-party libraries.

---

### Task 1: Project Wiring And Tests

**Files:**
- Modify: `CMakeLists.txt`
- Create: `tests/capture_overlay_tests.cpp`

- [ ] Add Qt Test to CMake and define a `SnapInkTests` target.
- [ ] Write failing tests that reference the planned annotation/editor APIs.
- [ ] Run the test target and confirm it fails because the APIs do not exist yet.

### Task 2: Capture And Clipboard Services

**Files:**
- Create: `src/core/capture/ScreenCaptureService.h`
- Create: `src/core/capture/ScreenCaptureService.cpp`
- Create: `src/core/clipboard/ClipboardService.h`
- Create: `src/core/clipboard/ClipboardService.cpp`
- Modify: `CMakeLists.txt`

- [ ] Implement virtual desktop union calculation from all screens.
- [ ] Capture each screen with `QScreen::grabWindow(0)` and paint successes into one ARGB32 image.
- [ ] Return a null image only when every screen capture fails.
- [ ] Add clipboard image copy helper.
- [ ] Build after this task.

### Task 3: Overlay Annotation Model And Undo Command

**Files:**
- Create: `src/ui/capture/CaptureTool.h`
- Create: `src/ui/capture/CaptureAnnotation.h`
- Create: `src/ui/capture/CaptureAnnotation.cpp`
- Create: `src/ui/capture/AddAnnotationCommand.h/.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/capture_overlay_tests.cpp`

- [ ] Implement independent annotation classes with red default style support and selection-relative coordinates.
- [ ] Implement `AddAnnotationCommand` using `QUndoCommand`.
- [ ] Run tests and build after this task.

### Task 4: Overlay Toolbar And Editing State

**Files:**
- Create: `src/ui/capture/CaptureToolbar.h`
- Create: `src/ui/capture/CaptureToolbar.cpp`
- Modify: `src/ui/capture/CaptureOverlay.h`
- Modify: `src/ui/capture/CaptureOverlay.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/capture_overlay_tests.cpp`

- [ ] Implement floating toolbar actions for rectangle, arrow, pen, text, undo, redo, copy, save, cancel, and confirm.
- [ ] Create temporary preview annotations during drags and push only completed annotations to `QUndoStack`.
- [ ] Implement `renderResultImage()` from the selected image region plus annotations only.
- [ ] Run tests and build after this task.

### Task 5: Region Capture Overlay

**Files:**
- Create: `src/ui/capture/CaptureOverlay.h`
- Create: `src/ui/capture/CaptureOverlay.cpp`
- Modify: `CMakeLists.txt`

- [ ] Implement frameless top-level overlay over the virtual desktop geometry.
- [ ] Paint the captured image, dim non-selected areas, keep selected area bright, draw border and dimensions.
- [ ] Convert global/overlay mouse positions to image-relative coordinates and enter Editing on valid selections.
- [ ] Support Esc and right-click cancellation.
- [ ] Build after this task.

### Task 6: MainWindow Integration

**Files:**
- Modify: `src/app/mainwindow.h`
- Modify: `src/app/mainwindow.cpp`
- Modify: `main.cpp` only if required

- [ ] Replace the Ctrl+1 temporary capture with Ctrl+Alt+A and Ctrl+Alt+S `Hotkey` objects.
- [ ] Show one registration warning per failed hotkey and log the reason.
- [ ] Ignore new capture requests while an overlay is active.
- [ ] Implement full-screen and region capture flows into `CaptureOverlay`; do not create an editor window.
- [ ] Run tests and build after this task.

### Task 7: Final Verification

**Files:**
- Inspect all modified files.

- [ ] Run `cmake --build build/Desktop_Qt_6_8_3_MSVC2022_64bit-Release --config Release`.
- [ ] Run `ctest --test-dir build/Desktop_Qt_6_8_3_MSVC2022_64bit-Release --output-on-failure`.
- [ ] Re-read the original requirements and check support or limitations.
- [ ] Report changed files, supported features, implementation notes, known limitations, and manual verification steps.

## Self-Review

The plan covers the requested capture service, overlay, editor, annotations, undo/redo, export, hotkey integration, CMake updates, and verification. No intentionally deferred MVP requirement remains except native/manual validation areas that cannot be reliably automated in this workspace.
