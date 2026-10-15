add_library(nexacloud_scene STATIC scene.c)
target_link_libraries(nexacloud_scene PUBLIC NexaUI::NexaUI)
target_compile_features(nexacloud_scene PRIVATE c_std_17)
if(MSVC)
    target_compile_options(nexacloud_scene PRIVATE /W4 /utf-8)
else()
    target_compile_options(nexacloud_scene PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(nexacloud_scene PRIVATE m)
endif()
add_executable(nexaui_nexacloud main.c)
target_link_libraries(nexaui_nexacloud PRIVATE nexacloud_scene)
if(WIN32)
    add_executable(nexaui_nexacloud_native native_windows.c)
    target_include_directories(nexaui_nexacloud_native PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/platform/windows
        ${PROJECT_SOURCE_DIR}/renderers/d3d12)
    target_link_libraries(nexaui_nexacloud_native PRIVATE nexacloud_scene nexaui_renderer_d3d12)
    if(MSVC)
        target_compile_options(nexaui_nexacloud_native PRIVATE /W4 /utf-8)
    else()
        target_compile_options(nexaui_nexacloud_native PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(NEXAUI_BUILD_TESTS)
        target_compile_definitions(nexaui_nexacloud_native PRIVATE NEXAUI_D3D12_TEST_HOOKS)
    endif()
    if(BUILD_SHARED_LIBS)
        add_custom_command(TARGET nexaui_nexacloud_native POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui> $<TARGET_FILE_DIR:nexaui_nexacloud_native> VERBATIM)
    endif()
endif()
if(TARGET nexaui_renderer_vulkan)
    add_executable(nexaui_nexacloud_native native_linux.c)
    target_include_directories(nexaui_nexacloud_native PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/renderers/vulkan)
    target_link_libraries(nexaui_nexacloud_native PRIVATE nexacloud_scene nexaui_renderer_vulkan)
    target_compile_features(nexaui_nexacloud_native PRIVATE c_std_17)
    target_compile_options(nexaui_nexacloud_native PRIVATE -Wall -Wextra -Wpedantic)
    if(NEXAUI_BUILD_TESTS)
        target_compile_definitions(nexaui_nexacloud_native PRIVATE NEXAUI_VULKAN_TEST_HOOKS)
    endif()
endif()
if(TARGET nexaui_renderer_metal)
    add_executable(nexaui_nexacloud_native native_macos.c)
    target_include_directories(nexaui_nexacloud_native PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/renderers/metal)
    target_link_libraries(nexaui_nexacloud_native PRIVATE nexacloud_scene nexaui_renderer_metal)
    target_compile_features(nexaui_nexacloud_native PRIVATE c_std_17)
    target_compile_options(nexaui_nexacloud_native PRIVATE -Wall -Wextra -Wpedantic)
    if(NEXAUI_BUILD_TESTS)
        target_compile_definitions(nexaui_nexacloud_native PRIVATE NEXAUI_METAL_TEST_HOOKS)
    endif()
endif()
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET nexaui_nexacloud POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:nexaui> $<TARGET_FILE_DIR:nexaui_nexacloud> VERBATIM)
endif()
if(NEXAUI_BUILD_TESTS)
    include("${PROJECT_SOURCE_DIR}/cmake/NexaUIExampleFont.cmake")
    nexaui_find_example_font(_cloud_font)
    if(EXISTS "${_cloud_font}")
        add_test(NAME nexaui_nexacloud_example COMMAND nexaui_nexacloud --smoke
            "${_cloud_font}" "${CMAKE_CURRENT_BINARY_DIR}/nexacloud")
        set_tests_properties(nexaui_nexacloud_example PROPERTIES TIMEOUT 60)
        if(WIN32 AND TARGET nexaui_nexacloud_native)
            add_test(NAME nexaui_nexacloud_native_example COMMAND nexaui_nexacloud_native
                --smoke "${_cloud_font}" "${CMAKE_CURRENT_BINARY_DIR}/nexacloud")
            set_tests_properties(nexaui_nexacloud_native_example PROPERTIES TIMEOUT 120
                RESOURCE_LOCK native_display)
        endif()
        if(TARGET nexaui_renderer_metal)
            add_test(NAME nexaui_nexacloud_metal_native_example COMMAND nexaui_nexacloud_native
                --smoke "${_cloud_font}" "${CMAKE_CURRENT_BINARY_DIR}/nexacloud")
            set_tests_properties(nexaui_nexacloud_metal_native_example PROPERTIES
                TIMEOUT 180 RESOURCE_LOCK native_display SKIP_RETURN_CODE 77
                ENVIRONMENT "MTL_DEBUG_LAYER=1")
        endif()
        if(TARGET nexaui_renderer_vulkan)
            foreach(_cloud_platform IN ITEMS x11 wayland)
                add_test(NAME nexaui_nexacloud_${_cloud_platform}_native_example COMMAND nexaui_nexacloud_native
                    --smoke "${_cloud_font}" "${CMAKE_CURRENT_BINARY_DIR}/nexacloud-${_cloud_platform}")
                set_tests_properties(nexaui_nexacloud_${_cloud_platform}_native_example PROPERTIES
                    TIMEOUT 180 RESOURCE_LOCK native_display SKIP_RETURN_CODE 77
                    ENVIRONMENT "NEXAUI_PLATFORM=${_cloud_platform}"
                    FAIL_REGULAR_EXPRESSION "Validation Error;VUID-;SYNC-HAZARD")
            endforeach()
        endif()
    endif()
endif()
