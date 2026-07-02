include(${PROJECT_SOURCE_DIR}/cmake/SnapInkPlatform.cmake)

add_library(snapink_core STATIC)

target_sources(snapink_core PRIVATE
    core/capture/CaptureImageComposer.cpp
    core/capture/CaptureImageComposer.h
    core/capture/CapturedScreen.h
    core/capture/ScreenCaptureService.cpp
    core/capture/ScreenCaptureService.h
    core/clipboard/ClipboardService.cpp
    core/clipboard/ClipboardService.h
    core/globalmouse/GlobalMouseDragMonitor.cpp
    core/globalmouse/GlobalMouseDragMonitor.h
    core/globalmouse/GlobalMouseDragPlatform.cpp
    core/globalmouse/GlobalMouseDragPlatform.h
    core/globalmouse/GlobalMouseDragTracker.cpp
    core/globalmouse/GlobalMouseDragTracker.h
    core/hotkey/Hotkey.cpp
    core/hotkey/Hotkey.h
    core/hotkey/HotkeyConfig.h
    core/hotkey/Hotkey_p.h
    core/hotkey/HotkeyPlatform.h
)

target_include_directories(snapink_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(snapink_core PUBLIC
    Qt::Core
    Qt::Gui
)

target_compile_features(snapink_core PUBLIC cxx_std_20)

add_library(snapink_platform STATIC)

target_include_directories(snapink_platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(snapink_platform PUBLIC
    snapink_core
    Qt::Core
    Qt::Gui
    Qt::Widgets
)

target_compile_features(snapink_platform PUBLIC cxx_std_20)

snapink_configure_platform_target(snapink_platform)

add_library(snapink_capture_ui STATIC)

target_sources(snapink_capture_ui PRIVATE
    ui/capture/AddAnnotationCommand.cpp
    ui/capture/AddAnnotationCommand.h
    ui/capture/AnnotationSceneController.cpp
    ui/capture/AnnotationSceneController.h
    ui/capture/CaptureAnnotation.cpp
    ui/capture/CaptureAnnotation.h
    ui/capture/CaptureInputController.cpp
    ui/capture/CaptureInputController.h
    ui/capture/CaptureOverlay.cpp
    ui/capture/CaptureOverlay.h
    ui/capture/CaptureSelectionModel.cpp
    ui/capture/CaptureSelectionModel.h
    ui/capture/CaptureToolbar.cpp
    ui/capture/CaptureToolbar.h
    ui/capture/CaptureTool.h
    ui/pin/PinWindow.cpp
    ui/pin/PinWindow.h
    ui/pin/PinWindowManager.cpp
    ui/pin/PinWindowManager.h
    ui/tray/TrayController.cpp
    ui/tray/TrayController.h
)

target_include_directories(snapink_capture_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(snapink_capture_ui PUBLIC
    snapink_core
    snapink_platform
    Qt::Core
    Qt::Gui
    Qt::Widgets
)

target_compile_features(snapink_capture_ui PUBLIC cxx_std_20)

add_library(snapink_app STATIC)

target_sources(snapink_app PRIVATE
    app/CaptureController.cpp
    app/CaptureController.h
    app/GlobalMouseController.cpp
    app/GlobalMouseController.h
    app/HotkeyController.cpp
    app/HotkeyController.h
    app/mainwindow.cpp
    app/mainwindow.h
)

target_include_directories(snapink_app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(snapink_app PUBLIC
    snapink_capture_ui
    snapink_platform
    Qt::Core
    Qt::Gui
    Qt::Widgets
)

target_compile_features(snapink_app PUBLIC cxx_std_20)

qt_add_executable(SnapInk
    WIN32 MACOSX_BUNDLE
    ${PROJECT_SOURCE_DIR}/main.cpp
)

target_link_libraries(SnapInk PRIVATE
    snapink_app
)

target_compile_features(SnapInk PRIVATE cxx_std_20)

if(APPLE)
    target_sources(SnapInk PRIVATE
        ${PROJECT_SOURCE_DIR}/icons/app.icns
        ${PROJECT_SOURCE_DIR}/icons/trayTemplate.png
    )

    set_source_files_properties(
        ${PROJECT_SOURCE_DIR}/icons/app.icns
        ${PROJECT_SOURCE_DIR}/icons/trayTemplate.png
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources
    )

    set_target_properties(SnapInk PROPERTIES
        MACOSX_BUNDLE_ICON_FILE app.icns
        MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in"
    )
endif()
