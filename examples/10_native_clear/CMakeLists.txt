if(WIN32)
    add_executable(nexaui_native_clear main.c)
    target_include_directories(nexaui_native_clear PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/platform/windows
        ${CMAKE_SOURCE_DIR}/renderers/d3d12)
    target_link_libraries(nexaui_native_clear PRIVATE
        NexaUI::NexaUI nexaui_renderer_d3d12)
elseif(TARGET nexaui_renderer_vulkan)
    add_executable(nexaui_native_clear main_linux.c)
    target_include_directories(nexaui_native_clear PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/renderers/vulkan)
    target_link_libraries(nexaui_native_clear PRIVATE
        NexaUI::NexaUI nexaui_renderer_vulkan)
    target_compile_features(nexaui_native_clear PRIVATE c_std_17)
    target_compile_options(nexaui_native_clear PRIVATE -Wall -Wextra -Wpedantic)
    if(NEXAUI_BUILD_TESTS)
        foreach(_platform x11 wayland)
            add_test(NAME nexaui_vulkan_${_platform}_example COMMAND nexaui_native_clear --smoke)
            set_tests_properties(nexaui_vulkan_${_platform}_example PROPERTIES
                TIMEOUT 30 SKIP_RETURN_CODE 77 RESOURCE_LOCK native_display
                ENVIRONMENT "NEXAUI_PLATFORM=${_platform}"
                FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
        endforeach()
    endif()
endif()
