qt_add_executable(SnapInkTests
    capture_overlay_tests.cpp
)

target_link_libraries(SnapInkTests PRIVATE
    snapink_app
    Qt::Test
)

target_compile_features(SnapInkTests PRIVATE cxx_std_20)

add_custom_command(TARGET SnapInkTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:SnapInkTests>/icons"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/icons/app.icns"
        "$<TARGET_FILE_DIR:SnapInkTests>/icons/app.icns"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/icons/trayTemplate.png"
        "$<TARGET_FILE_DIR:SnapInkTests>/icons/trayTemplate.png"
)

add_test(NAME SnapInkTests COMMAND SnapInkTests)

qt_add_executable(CaptureSelectionModelTests
    capture_selection_model_tests.cpp
)

target_link_libraries(CaptureSelectionModelTests PRIVATE
    snapink_capture_ui
    Qt::Test
)

target_compile_features(CaptureSelectionModelTests PRIVATE cxx_std_20)

add_test(NAME CaptureSelectionModelTests COMMAND CaptureSelectionModelTests)

qt_add_executable(CaptureImageComposerTests
    capture_image_composer_tests.cpp
)

target_link_libraries(CaptureImageComposerTests PRIVATE
    snapink_core
    Qt::Test
)

target_compile_features(CaptureImageComposerTests PRIVATE cxx_std_20)

add_test(NAME CaptureImageComposerTests COMMAND CaptureImageComposerTests)

qt_add_executable(AnnotationSceneControllerTests
    annotation_scene_controller_tests.cpp
)

target_link_libraries(AnnotationSceneControllerTests PRIVATE
    snapink_capture_ui
    Qt::Test
)

target_compile_features(AnnotationSceneControllerTests PRIVATE cxx_std_20)

add_test(NAME AnnotationSceneControllerTests COMMAND AnnotationSceneControllerTests)

qt_add_executable(CaptureInputControllerTests
    capture_input_controller_tests.cpp
)

target_link_libraries(CaptureInputControllerTests PRIVATE
    snapink_capture_ui
    Qt::Test
)

target_compile_features(CaptureInputControllerTests PRIVATE cxx_std_20)

add_test(NAME CaptureInputControllerTests COMMAND CaptureInputControllerTests)

qt_add_executable(GlobalMouseDragTrackerTests
    global_mouse_drag_tracker_tests.cpp
)

target_link_libraries(GlobalMouseDragTrackerTests PRIVATE
    snapink_core
    Qt::Test
)

target_compile_features(GlobalMouseDragTrackerTests PRIVATE cxx_std_20)

add_test(NAME GlobalMouseDragTrackerTests COMMAND GlobalMouseDragTrackerTests)

qt_add_executable(GlobalMouseDragMonitorTests
    global_mouse_drag_monitor_tests.cpp
    fakes/FakeGlobalMouseDragPlatform.cpp
)

target_link_libraries(GlobalMouseDragMonitorTests PRIVATE
    snapink_core
    Qt::Test
)

target_compile_features(GlobalMouseDragMonitorTests PRIVATE cxx_std_20)

add_test(NAME GlobalMouseDragMonitorTests COMMAND GlobalMouseDragMonitorTests)

if(APPLE)
    qt_add_executable(HotkeyMacTests
        hotkey_mac_tests.cpp
    )

    target_link_libraries(HotkeyMacTests PRIVATE
        snapink_platform
        Qt::Test
    )

    target_compile_features(HotkeyMacTests PRIVATE cxx_std_20)

    add_test(NAME HotkeyMacTests COMMAND HotkeyMacTests)
endif()
