if(APPLE)
    enable_language(OBJCXX)
endif()

function(snapink_configure_platform_target target_name)
    if(WIN32)
        target_sources(${target_name} PRIVATE
            ${PROJECT_SOURCE_DIR}/src/core/hotkey/HotkeyPlatform_win.cpp
            ${PROJECT_SOURCE_DIR}/src/core/globalmouse/GlobalMouseDragPlatform_win.cpp
        )

        target_link_libraries(${target_name} PRIVATE
            user32
            d3d11
            dxgi
            windowsapp
            runtimeobject
        )
    elseif(APPLE)
        target_sources(${target_name} PRIVATE
            ${PROJECT_SOURCE_DIR}/src/core/hotkey/HotkeyPlatform_mac.cpp
            ${PROJECT_SOURCE_DIR}/src/core/globalmouse/GlobalMouseDragPlatform_mac.mm
            ${PROJECT_SOURCE_DIR}/src/platform/macos/MacTrayIconHelper.h
            ${PROJECT_SOURCE_DIR}/src/platform/macos/MacTrayIconHelper.mm
            ${PROJECT_SOURCE_DIR}/src/ui/pin/MacWindowHelper.h
            ${PROJECT_SOURCE_DIR}/src/ui/pin/MacWindowHelper.mm
        )

        find_library(CARBON_LIBRARY Carbon REQUIRED)
        find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices REQUIRED)
        find_library(APPKIT_LIBRARY AppKit REQUIRED)

        target_link_libraries(${target_name} PRIVATE
            ${CARBON_LIBRARY}
            ${APPLICATIONSERVICES_LIBRARY}
            ${APPKIT_LIBRARY}
        )
    elseif(UNIX)
        target_sources(${target_name} PRIVATE
            ${PROJECT_SOURCE_DIR}/src/core/hotkey/HotkeyPlatform_linux.cpp
            ${PROJECT_SOURCE_DIR}/src/core/globalmouse/GlobalMouseDragPlatform_linux.cpp
        )
    endif()
endfunction()
