# Select Tool Selection Adjust Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a default Select tool that can move and resize the capture selection in Editing state while preserving existing annotations and undo/redo history.

**Architecture:** `CaptureToolbar` exposes a checkable Select tool. `CaptureOverlay` handles select-tool mouse and wheel input through the existing overlay/viewport event paths, and all selection changes flow through `applySelectionRect(const QRect&)`, which updates the selection background, scene rect, view geometry, chrome, and toolbar without clearing annotation items or the undo stack.

**Tech Stack:** C++20, Qt 6 Widgets, Qt Test, CMake/CTest.

---

### Task 1: Tests For Select Tool Behavior

**Files:**
- Modify: `tests/capture_overlay_tests.cpp`

- [ ] **Step 1: Write failing tests**

Add tests for default Select activation, moving the selection while preserving undo, resizing from the left/top while shifting annotations, resizing from right/bottom without shifting annotations, wheel resizing by one pixel, and annotation tools disabling selection adjustment.

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target SnapInkTests` and `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`.
Expected: build or tests fail because Select tool behavior is not implemented.

### Task 2: Toolbar And Tool Enum

**Files:**
- Modify: `src/ui/capture/CaptureTool.h`
- Modify: `src/ui/capture/CaptureToolbar.h`
- Modify: `src/ui/capture/CaptureToolbar.cpp`

- [ ] **Step 1: Implement minimal toolbar support**

Add `CaptureTool::Select`, add a checkable Select button, include it in the exclusive button group, emit `toolSelected(CaptureTool::Select)`, and make `setCurrentTool` check it.

- [ ] **Step 2: Run tests**

Run: `cmake --build build --target SnapInkTests` and the focused Qt tests.

### Task 3: Selection Interaction In Overlay

**Files:**
- Modify: `src/ui/capture/CaptureOverlay.h`
- Modify: `src/ui/capture/CaptureOverlay.cpp`

- [ ] **Step 1: Add state and helpers**

Add `SelectionHandle`, `SelectionInteraction`, hit testing, move/resize begin/update functions, wheel resizing, `applySelectionRect`, `updateSelectionBackground`, `updateAnnotationViewGeometry`, `updateToolbarPosition`, and `shiftAllAnnotations`.

- [ ] **Step 2: Route events**

In Select mode, process viewport/overlay mouse and wheel events for selection adjustment. In annotation modes, preserve the existing annotation drawing behavior.

- [ ] **Step 3: Run tests**

Run: `cmake --build build --target SnapInkTests`, `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`, and `cmake --build build --target SnapInk`.

