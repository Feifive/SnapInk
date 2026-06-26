# SnapInk MVP Design

## Goal

Build a runnable Windows Qt Widgets screenshot MVP that captures the virtual desktop, supports region or full-screen capture, edits directly inside the capture overlay, adds basic annotations, and exports the final image to the clipboard or PNG.

## Boundaries

The existing `src/core/hotkey` module remains unchanged. `MainWindow` owns application flow only: global hotkey registration, duplicate overlay prevention, capture requests, and overlay lifetime. Screen capture, overlay selection, editing, annotation rendering, undo/redo, clipboard, and saving live in separate modules.

Out of scope: window capture, scrolling capture, OCR, recording, mosaic, pinning, cloud sync, complex theming, QML, and non-Qt third-party dependencies.

## Architecture

`ScreenCaptureService` computes the union of all `QScreen::geometry()` values and paints every successfully captured screen into one `QImage::Format_ARGB32` image. The union rectangle can start at negative coordinates, so screen content is translated by `screen.geometry().topLeft() - virtualGeometry.topLeft()`.

`CaptureOverlay` receives the pre-captured desktop image plus virtual desktop geometry. It displays a top-level frameless overlay at the virtual desktop geometry, paints the image, darkens non-selected areas, and owns a `Selecting -> Editing -> Finished/Canceled` state machine. It never captures the screen after becoming visible.

In `Editing`, `CaptureOverlay` keeps the selected region fixed, shows a floating `CaptureToolbar`, stores annotation coordinates relative to the selection top-left, and paints annotations directly in `paintEvent()` with `QPainter`. Completed annotations enter a `QUndoStack` through `AddAnnotationCommand`; temporary drag previews do not.

`ClipboardService` copies the rendered result image through `QGuiApplication::clipboard()`. Save uses `QFileDialog::getSaveFileName()` and `QImage::save()` with timestamped PNG defaults.

## Files

- Modify `CMakeLists.txt` to add Qt Gui/Test usage, app sources, and a small automated test target.
- Modify `src/app/mainwindow.h/.cpp` to replace the temporary Ctrl+1 test capture with formal MVP flow.
- Create `src/core/capture/ScreenCaptureService.h/.cpp`.
- Create `src/core/clipboard/ClipboardService.h/.cpp`.
- Create `src/ui/capture/CaptureOverlay.h/.cpp`.
- Create `src/ui/capture/CaptureToolbar.h/.cpp`.
- Create `src/ui/capture/CaptureAnnotation.h/.cpp`, `CaptureTool.h`, and `AddAnnotationCommand.h/.cpp`.
- Create `tests/capture_overlay_tests.cpp` for stable non-native automated checks.

## Verification

Automated tests cover overlay state, selection-sized rendering, selection-relative annotation coordinates, and undo/redo. Full native screen capture and global hotkeys are verified by building the Windows target and using the manual validation steps from the request.

## Known Constraints

The current workspace has an empty `.git` directory, so this spec cannot be committed here. The project has no existing test framework; the plan adds a Qt Test target for focused regression coverage.
