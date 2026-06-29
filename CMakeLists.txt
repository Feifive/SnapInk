cmake_minimum_required(VERSION 3.19)
project(SnapInk LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Test)

qt_standard_project_setup()

set(SNAPINK_CORE_SOURCES
    src/core/capture/CapturedScreen.h
    src/core/capture/ScreenCaptureService.cpp
    src/core/capture/ScreenCaptureService.h
    src/core/clipboard/ClipboardService.cpp
    src/core/clipboard/ClipboardService.h
)

set(SNAPINK_CAPTURE_SOURCES
    src/ui/capture/AddAnnotationCommand.cpp
    src/ui/capture/AddAnnotationCommand.h
    src/ui/capture/CaptureAnnotation.cpp
    src/ui/capture/CaptureAnnotation.h
    src/ui/capture/CaptureOverlay.cpp
    src/ui/capture/CaptureOverlay.h
    src/ui/capture/CaptureToolbar.cpp
    src/ui/capture/CaptureToolbar.h
    src/ui/capture/CaptureTool.h
    src/ui/pin/PinWindow.cpp
    src/ui/pin/PinWindow.h
)

set(SNAPINK_MACOS_SOURCES)
if(APPLE)
    enable_language(OBJCXX)
    set(SNAPINK_MACOS_SOURCES
        src/ui/pin/MacWindowHelper.h
        src/ui/pin/MacWindowHelper.mm
    )
endif()

qt_add_executable(SnapInk
    WIN32 MACOSX_BUNDLE
    main.cpp
    src/app/mainwindow.cpp
    src/app/mainwindow.h
    src/core/hotkey/Hotkey.cpp
    src/core/hotkey/Hotkey.h
    src/core/hotkey/Hotkey_p.h
    src/core/hotkey/HotkeyPlatform.h
    ${SNAPINK_CORE_SOURCES}
    ${SNAPINK_CAPTURE_SOURCES}
    ${SNAPINK_MACOS_SOURCES}
)

target_link_libraries(SnapInk
    PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE
        src/core/hotkey/HotkeyPlatform_win.cpp
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        user32
        d3d11
        dxgi
        windowsapp
        runtimeobject
    )
elseif(APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
        src/core/hotkey/HotkeyPlatform_mac.cpp
    )

    find_library(CARBON_LIBRARY Carbon REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${CARBON_LIBRARY}
        "-framework AppKit"
    )
elseif(UNIX)
    target_sources(${PROJECT_NAME} PRIVATE
        src/core/hotkey/HotkeyPlatform_linux.cpp
    )
endif()

enable_testing()

qt_add_executable(SnapInkTests
    tests/capture_overlay_tests.cpp
    ${SNAPINK_CORE_SOURCES}
    ${SNAPINK_CAPTURE_SOURCES}
    ${SNAPINK_MACOS_SOURCES}
)

target_link_libraries(SnapInkTests
    PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Test
)

target_compile_features(SnapInkTests PRIVATE cxx_std_20)

add_test(NAME SnapInkTests COMMAND SnapInkTests)

if(APPLE)
    target_link_libraries(SnapInkTests PRIVATE "-framework AppKit")

    qt_add_executable(HotkeyMacTests
        tests/hotkey_mac_tests.cpp
        src/core/hotkey/Hotkey.cpp
        src/core/hotkey/Hotkey.h
        src/core/hotkey/Hotkey_p.h
        src/core/hotkey/HotkeyPlatform.h
        src/core/hotkey/HotkeyPlatform_mac.cpp
    )

    target_link_libraries(HotkeyMacTests
        PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Test
            ${CARBON_LIBRARY}
    )

    target_compile_features(HotkeyMacTests PRIVATE cxx_std_20)

    add_test(NAME HotkeyMacTests COMMAND HotkeyMacTests)
endif()

include(GNUInstallDirs)

install(TARGETS SnapInk
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET SnapInk
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
