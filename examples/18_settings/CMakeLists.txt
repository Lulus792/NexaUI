add_library(nexaui_settings_scene STATIC scene.c)
target_link_libraries(nexaui_settings_scene PUBLIC NexaUI::NexaUI)
target_compile_features(nexaui_settings_scene PUBLIC c_std_17)
if(TARGET nexaui_renderer_vulkan)
    add_executable(nexaui_settings main_linux.c)
    target_include_directories(nexaui_settings PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/renderers/vulkan)
    target_link_libraries(nexaui_settings PRIVATE nexaui_settings_scene nexaui_renderer_vulkan m)
    target_compile_options(nexaui_settings PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(NEXAUI_BUILD_TESTS)
    include("${PROJECT_SOURCE_DIR}/cmake/NexaUIExampleFont.cmake")
    nexaui_find_example_font(_settings_font)
    if(EXISTS "${_settings_font}")
        add_executable(nexaui_settings_scene_tests test_scene.c)
        target_link_libraries(nexaui_settings_scene_tests PRIVATE nexaui_settings_scene)
        add_test(NAME nexaui_settings_scene COMMAND nexaui_settings_scene_tests "${_settings_font}")
        set_tests_properties(nexaui_settings_scene PROPERTIES TIMEOUT 20)
        if(TARGET nexaui_wayland_test_protocols)
            add_executable(nexaui_settings_ime_v3_tests test_ime_v3.c
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-shell-protocol.c
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v3-protocol.c)
            add_dependencies(nexaui_settings_ime_v3_tests nexaui_wayland_test_protocols)
            target_include_directories(nexaui_settings_ime_v3_tests PRIVATE
                ${PROJECT_SOURCE_DIR}/tests ${PROJECT_SOURCE_DIR}/platform/linux ${NEXAUI_WAYLAND_PROTOCOL_DIR})
            target_link_libraries(nexaui_settings_ime_v3_tests PRIVATE nexaui_settings_scene
                Threads::Threads PkgConfig::WAYLAND_SERVER PkgConfig::WAYLAND_CLIENT)
            add_test(NAME nexaui_settings_ime_v3 COMMAND nexaui_settings_ime_v3_tests "${_settings_font}")
            set_tests_properties(nexaui_settings_ime_v3 PROPERTIES TIMEOUT 20)
            add_test(NAME nexaui_settings_ime_v3_external COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --external)
            set_tests_properties(nexaui_settings_ime_v3_external PROPERTIES TIMEOUT 20)
            add_test(NAME nexaui_settings_ime_v3_same_value COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --same)
            set_tests_properties(nexaui_settings_ime_v3_same_value PROPERTIES TIMEOUT 20)
        endif()
        if(TARGET nexaui_settings)
            find_program(NEXAUI_XDOTOOL_EXECUTABLE xdotool)
            if(Python3_Interpreter_FOUND AND NEXAUI_XDOTOOL_EXECUTABLE)
                add_test(NAME nexaui_settings_wayland_keyboard
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_keyboard.py
                        $<TARGET_FILE:nexaui_settings> "${_settings_font}" ${NEXAUI_XDOTOOL_EXECUTABLE})
                set_tests_properties(nexaui_settings_wayland_keyboard PROPERTIES TIMEOUT 40 SKIP_RETURN_CODE 77
                    RESOURCE_LOCK native_display)
                add_test(NAME nexaui_settings_wayland_keyboard_controls
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_keyboard.py
                        $<TARGET_FILE:nexaui_settings> "${_settings_font}" ${NEXAUI_XDOTOOL_EXECUTABLE} --controls)
                set_tests_properties(nexaui_settings_wayland_keyboard_controls PROPERTIES TIMEOUT 40 SKIP_RETURN_CODE 77
                    RESOURCE_LOCK native_display)
                add_test(NAME nexaui_settings_wayland_keyboard_small
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_keyboard.py
                        $<TARGET_FILE:nexaui_settings> "${_settings_font}" ${NEXAUI_XDOTOOL_EXECUTABLE} --small)
                set_tests_properties(nexaui_settings_wayland_keyboard_small PROPERTIES TIMEOUT 40 SKIP_RETURN_CODE 77
                    RESOURCE_LOCK native_display)
            endif()
            foreach(_platform IN LISTS _nexaui_test_platforms)
                add_test(NAME nexaui_settings_${_platform}_native COMMAND nexaui_settings "${_settings_font}" --smoke)
                set_tests_properties(nexaui_settings_${_platform}_native PROPERTIES TIMEOUT 40
                    RESOURCE_LOCK native_display ENVIRONMENT "NEXAUI_PLATFORM=${_platform}"
                    FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
            endforeach()
            if(NEXAUI_DBUS_FOUND AND NEXAUI_ATSPI_FOUND AND NEXAUI_ATSPI_REGISTRY
                    AND NEXAUI_DBUS_DAEMON AND NEXAUI_DBUS_SEND AND Python3_Interpreter_FOUND)
                add_executable(nexaui_settings_atspi_client atspi_client.c)
                target_link_libraries(nexaui_settings_atspi_client PRIVATE PkgConfig::NEXAUI_ATSPI)
                target_compile_features(nexaui_settings_atspi_client PRIVATE c_std_17)
                add_test(NAME nexaui_settings_wayland_atspi
                    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/run_atspi_multiselection_wayland.py
                        ${NEXAUI_ATSPI_REGISTRY} $<TARGET_FILE:nexaui_settings_atspi_client> $<TARGET_FILE:nexaui_settings>)
                set_tests_properties(nexaui_settings_wayland_atspi PROPERTIES TIMEOUT 70 SKIP_RETURN_CODE 77
                    ENVIRONMENT "NEXAUI_SETTINGS_FONT=${_settings_font}")
            endif()
        endif()
    endif()
endif()
