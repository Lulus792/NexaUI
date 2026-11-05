if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(NexaUISettingsConsumer LANGUAGES C)
    option(NEXAUI_SETTINGS_SOFTWARE "Use the Linux Wayland software renderer" OFF)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "The Settings runner requires Linux")
    endif()
    find_package(NexaUI CONFIG REQUIRED)
    if(NOT NEXAUI_SETTINGS_SOFTWARE AND NexaUI_Vulkan_FOUND)
        find_package(NexaUI CONFIG REQUIRED COMPONENTS Vulkan)
    endif()
    find_package(Threads REQUIRED)
    add_library(nexaui_settings_scene STATIC scene.c)
    target_compile_features(nexaui_settings_scene PUBLIC c_std_17)
    target_link_libraries(nexaui_settings_scene PUBLIC NexaUI::NexaUI)
    add_executable(nexaui_settings main_linux.c renderer.c)
    set_target_properties(nexaui_settings PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    target_link_libraries(nexaui_settings PRIVATE nexaui_settings_scene Threads::Threads m)
    if(NOT NEXAUI_SETTINGS_SOFTWARE AND NexaUI_Vulkan_FOUND)
        target_compile_definitions(nexaui_settings PRIVATE NEXAUI_SETTINGS_HAVE_VULKAN)
        target_link_libraries(nexaui_settings PRIVATE NexaUI::Vulkan)
    endif()
    install(TARGETS nexaui_settings RUNTIME DESTINATION bin)
    return()
endif()

add_library(nexaui_settings_scene STATIC scene.c)
target_link_libraries(nexaui_settings_scene PUBLIC NexaUI::NexaUI)
target_compile_features(nexaui_settings_scene PUBLIC c_std_17)
option(NEXAUI_SETTINGS_SOFTWARE "Use the Linux Wayland software renderer" OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND (NEXAUI_SETTINGS_SOFTWARE OR NOT TARGET nexaui_renderer_vulkan))
    add_executable(nexaui_settings main_linux.c renderer.c)
    target_link_libraries(nexaui_settings PRIVATE nexaui_settings_scene Threads::Threads m)
    target_compile_options(nexaui_settings PRIVATE -Wall -Wextra -Wpedantic)
elseif(TARGET nexaui_renderer_vulkan)
    add_executable(nexaui_settings main_linux.c renderer.c)
    target_compile_definitions(nexaui_settings PRIVATE NEXAUI_SETTINGS_HAVE_VULKAN)
    target_link_libraries(nexaui_settings PRIVATE nexaui_settings_scene NexaUI::Vulkan Threads::Threads m)
    target_compile_options(nexaui_settings PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(NEXAUI_BUILD_TESTS)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_executable(nexaui_settings_renderer_tests test_renderer.c renderer.c)
        target_include_directories(nexaui_settings_renderer_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
        target_compile_definitions(nexaui_settings_renderer_tests PRIVATE NEXAUI_SETTINGS_HAVE_VULKAN)
        target_compile_features(nexaui_settings_renderer_tests PRIVATE c_std_17)
        add_test(NAME nexaui_settings_renderer COMMAND nexaui_settings_renderer_tests)
    endif()
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
                ${PROJECT_SOURCE_DIR}/tests ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/platform/linux ${NEXAUI_WAYLAND_PROTOCOL_DIR})
            target_link_libraries(nexaui_settings_ime_v3_tests PRIVATE nexaui_settings_scene
                Threads::Threads PkgConfig::WAYLAND_SERVER PkgConfig::WAYLAND_CLIENT)
            add_test(NAME nexaui_settings_ime_v3 COMMAND nexaui_settings_ime_v3_tests "${_settings_font}")
            set_tests_properties(nexaui_settings_ime_v3 PROPERTIES TIMEOUT 20)
            add_test(NAME nexaui_settings_ime_v3_external COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --external)
            set_tests_properties(nexaui_settings_ime_v3_external PROPERTIES TIMEOUT 20)
            add_test(NAME nexaui_settings_ime_v3_same_value COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --same)
            set_tests_properties(nexaui_settings_ime_v3_same_value PROPERTIES TIMEOUT 20)
            add_test(NAME nexaui_settings_clipboard_single_read COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --clipboard-once)
            set_tests_properties(nexaui_settings_clipboard_single_read PROPERTIES TIMEOUT 20)
            add_test(NAME nexaui_settings_keyboard_clipboard_single_read COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --keyboard-clipboard-once)
            set_tests_properties(nexaui_settings_keyboard_clipboard_single_read PROPERTIES TIMEOUT 20)
            add_test(NAME nexaui_settings_clipboard_stall COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --clipboard-stall)
            add_test(NAME nexaui_settings_keyboard_clipboard_stall COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --keyboard-clipboard-stall)
            add_test(NAME nexaui_settings_clipboard_slow_allocator COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --clipboard-slow-allocator)
            set_tests_properties(nexaui_settings_clipboard_stall nexaui_settings_keyboard_clipboard_stall nexaui_settings_clipboard_slow_allocator PROPERTIES TIMEOUT 20)
            add_test(NAME nexaui_settings_keyboard_clipboard_focus COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --keyboard-clipboard-focus)
            add_test(NAME nexaui_settings_keyboard_clipboard_model COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --keyboard-clipboard-model)
            add_test(NAME nexaui_settings_keyboard_clipboard_selection COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --keyboard-clipboard-selection)
            add_test(NAME nexaui_settings_keyboard_clipboard_timeout COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --keyboard-clipboard-timeout)
            add_test(NAME nexaui_settings_keyboard_clipboard_command COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --keyboard-clipboard-command)
            add_test(NAME nexaui_settings_keyboard_clipboard_abandoned COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --keyboard-clipboard-abandoned)
            add_test(NAME nexaui_settings_keyboard_clipboard_ime COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --keyboard-clipboard-ime)
            add_test(NAME nexaui_settings_keyboard_clipboard_revert COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --keyboard-clipboard-revert)
            add_test(NAME nexaui_settings_keyboard_clipboard_repeat COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --keyboard-clipboard-repeat)
            add_test(NAME nexaui_settings_keyboard_clipboard_window_focus COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --keyboard-clipboard-window-focus)
            add_test(NAME nexaui_settings_ax_async_once COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --ax-async-once)
            add_test(NAME nexaui_settings_ax_async_stall COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --ax-async-stall)
            add_test(NAME nexaui_settings_ax_async_short COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --ax-async-short)
            add_test(NAME nexaui_settings_ax_async_short_delayed_focus
                COMMAND ${CMAKE_COMMAND} -E env NEXAUI_TEST_DELAY_SHORT_FOCUS=1
                    $<TARGET_FILE:nexaui_settings_ime_v3_tests> "${_settings_font}" --ax-async-short)
            add_test(NAME nexaui_settings_ax_async_input COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --ax-async-input)
            add_test(NAME nexaui_settings_ax_async_model COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --ax-async-model)
            add_test(NAME nexaui_settings_ax_async_detach COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --ax-async-detach)
            add_test(NAME nexaui_settings_ax_async_omit COMMAND nexaui_settings_ime_v3_tests "${_settings_font}" --ax-async-omit)
            set_tests_properties(nexaui_settings_ax_async_once nexaui_settings_ax_async_stall nexaui_settings_ax_async_short nexaui_settings_ax_async_short_delayed_focus nexaui_settings_ax_async_input nexaui_settings_ax_async_model nexaui_settings_ax_async_detach nexaui_settings_ax_async_omit PROPERTIES TIMEOUT 20)
            set_tests_properties(nexaui_settings_keyboard_clipboard_focus nexaui_settings_keyboard_clipboard_model nexaui_settings_keyboard_clipboard_selection nexaui_settings_keyboard_clipboard_timeout nexaui_settings_keyboard_clipboard_command nexaui_settings_keyboard_clipboard_abandoned nexaui_settings_keyboard_clipboard_ime nexaui_settings_keyboard_clipboard_revert nexaui_settings_keyboard_clipboard_repeat nexaui_settings_keyboard_clipboard_window_focus PROPERTIES TIMEOUT 20)
        endif()
        if(TARGET nexaui_settings)
            if(Python3_Interpreter_FOUND)
                add_test(NAME nexaui_settings_wayland_idle
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_idle.py
                        $<TARGET_FILE:nexaui_settings> "${_settings_font}")
                set_tests_properties(nexaui_settings_wayland_idle PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77
                    RESOURCE_LOCK native_display)
            endif()
            find_program(NEXAUI_XDOTOOL_EXECUTABLE xdotool)
            if(Python3_Interpreter_FOUND AND NEXAUI_XDOTOOL_EXECUTABLE)
                add_test(NAME nexaui_settings_wayland_simple_ime
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_simple_ime.py
                        $<TARGET_FILE:nexaui_settings> "${_settings_font}" ${NEXAUI_XDOTOOL_EXECUTABLE})
                set_tests_properties(nexaui_settings_wayland_simple_ime PROPERTIES TIMEOUT 45 SKIP_RETURN_CODE 77
                    RESOURCE_LOCK native_display)
                add_test(NAME nexaui_settings_wayland_simple_ime_focus_cancel
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_simple_ime.py
                        $<TARGET_FILE:nexaui_settings> "${_settings_font}" ${NEXAUI_XDOTOOL_EXECUTABLE} --cancel)
                set_tests_properties(nexaui_settings_wayland_simple_ime_focus_cancel PROPERTIES TIMEOUT 45 SKIP_RETURN_CODE 77
                    RESOURCE_LOCK native_display)
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
                add_test(NAME nexaui_settings_wayland_keyboard_minimum
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_keyboard.py
                        $<TARGET_FILE:nexaui_settings> "${_settings_font}" ${NEXAUI_XDOTOOL_EXECUTABLE} --minimum)
                set_tests_properties(nexaui_settings_wayland_keyboard_minimum PROPERTIES TIMEOUT 40 SKIP_RETURN_CODE 77
                    RESOURCE_LOCK native_display)
                add_test(NAME nexaui_settings_wayland_keyboard_german
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_keyboard.py
                        $<TARGET_FILE:nexaui_settings> "${_settings_font}" ${NEXAUI_XDOTOOL_EXECUTABLE} --small)
                set_tests_properties(nexaui_settings_wayland_keyboard_german PROPERTIES TIMEOUT 40 SKIP_RETURN_CODE 77
                    ENVIRONMENT "NEXAUI_SETTINGS_TEST_GERMAN=1" RESOURCE_LOCK native_display)
                add_test(NAME nexaui_settings_wayland_keyboard_german_minimum
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_keyboard.py
                        $<TARGET_FILE:nexaui_settings> "${_settings_font}" ${NEXAUI_XDOTOOL_EXECUTABLE} --minimum)
                set_tests_properties(nexaui_settings_wayland_keyboard_german_minimum PROPERTIES TIMEOUT 40 SKIP_RETURN_CODE 77
                    ENVIRONMENT "NEXAUI_SETTINGS_TEST_GERMAN=1" RESOURCE_LOCK native_display)
                add_test(NAME nexaui_settings_wayland_keyboard_clipboard
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_keyboard.py
                        $<TARGET_FILE:nexaui_settings> "${_settings_font}" ${NEXAUI_XDOTOOL_EXECUTABLE} --clipboard)
                set_tests_properties(nexaui_settings_wayland_keyboard_clipboard PROPERTIES TIMEOUT 40 SKIP_RETURN_CODE 77
                    RESOURCE_LOCK native_display)
                add_test(NAME nexaui_settings_wayland_keyboard_close_pending
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_keyboard.py
                        $<TARGET_FILE:nexaui_settings> "${_settings_font}" ${NEXAUI_XDOTOOL_EXECUTABLE} --close-pending)
                set_tests_properties(nexaui_settings_wayland_keyboard_close_pending PROPERTIES TIMEOUT 40 SKIP_RETURN_CODE 77
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
                if(NEXAUI_XDOTOOL_EXECUTABLE)
                    add_executable(nexaui_settings_atspi_focus_client atspi_focus_client.c)
                    target_link_libraries(nexaui_settings_atspi_focus_client PRIVATE PkgConfig::NEXAUI_ATSPI)
                    target_compile_features(nexaui_settings_atspi_focus_client PRIVATE c_std_17)
                    add_test(NAME nexaui_settings_wayland_simple_ime_control_cancel
                        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_simple_ime.py
                            $<TARGET_FILE:nexaui_settings> "${_settings_font}" ${NEXAUI_XDOTOOL_EXECUTABLE}
                            --control-cancel $<TARGET_FILE:nexaui_settings_atspi_focus_client>)
                    set_tests_properties(nexaui_settings_wayland_simple_ime_control_cancel PROPERTIES
                        TIMEOUT 45 SKIP_RETURN_CODE 77 RESOURCE_LOCK native_display)
                    add_test(NAME nexaui_settings_wayland_simple_ime_replace
                        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_simple_ime.py
                            $<TARGET_FILE:nexaui_settings> "${_settings_font}" ${NEXAUI_XDOTOOL_EXECUTABLE}
                            --replace $<TARGET_FILE:nexaui_settings_atspi_focus_client>)
                    set_tests_properties(nexaui_settings_wayland_simple_ime_replace PROPERTIES
                        TIMEOUT 45 SKIP_RETURN_CODE 77 RESOURCE_LOCK native_display)
                endif()
                add_executable(nexaui_settings_atspi_client atspi_client.c)
                target_link_libraries(nexaui_settings_atspi_client PRIVATE PkgConfig::NEXAUI_ATSPI)
                target_compile_features(nexaui_settings_atspi_client PRIVATE c_std_17)
                if(TARGET nexaui_wayland_test_protocols)
                    add_executable(nexaui_settings_atspi_async_host atspi_async_host.c
                        ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-shell-protocol.c
                        ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v3-protocol.c)
                    add_dependencies(nexaui_settings_atspi_async_host nexaui_wayland_test_protocols)
                    target_include_directories(nexaui_settings_atspi_async_host PRIVATE
                        ${PROJECT_SOURCE_DIR}/platform/linux ${NEXAUI_WAYLAND_PROTOCOL_DIR})
                    target_link_libraries(nexaui_settings_atspi_async_host PRIVATE nexaui_settings_scene
                        Threads::Threads PkgConfig::WAYLAND_SERVER PkgConfig::WAYLAND_CLIENT)
                    add_executable(nexaui_settings_atspi_async_client atspi_async_client.c)
                    target_link_libraries(nexaui_settings_atspi_async_client PRIVATE PkgConfig::NEXAUI_ATSPI)
                    foreach(_mode IN ITEMS input timeout complete)
                        add_test(NAME nexaui_settings_atspi_async_native_${_mode}
                            COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/run_atspi_native.py
                                ${NEXAUI_ATSPI_REGISTRY} $<TARGET_FILE:nexaui_settings_atspi_async_client>
                                $<TARGET_FILE:nexaui_settings_atspi_async_host>)
                        set_tests_properties(nexaui_settings_atspi_async_native_${_mode} PROPERTIES TIMEOUT 35
                            ENVIRONMENT "NEXAUI_SETTINGS_FONT=${_settings_font};NEXAUI_AX_ASYNC_MODE=${_mode}")
                    endforeach()
                endif()
                add_test(NAME nexaui_settings_wayland_atspi
                    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/run_atspi_multiselection_wayland.py
                        ${NEXAUI_ATSPI_REGISTRY} $<TARGET_FILE:nexaui_settings_atspi_client> $<TARGET_FILE:nexaui_settings>)
                set_tests_properties(nexaui_settings_wayland_atspi PROPERTIES TIMEOUT 70 SKIP_RETURN_CODE 77
                    ENVIRONMENT "NEXAUI_SETTINGS_FONT=${_settings_font}"
                    RESOURCE_LOCK native_display)
                add_test(NAME nexaui_settings_wayland_atspi_apply_error
                    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/run_atspi_multiselection_wayland.py
                        ${NEXAUI_ATSPI_REGISTRY} $<TARGET_FILE:nexaui_settings_atspi_client> $<TARGET_FILE:nexaui_settings>)
                set_tests_properties(nexaui_settings_wayland_atspi_apply_error PROPERTIES TIMEOUT 70 SKIP_RETURN_CODE 77
                    ENVIRONMENT "NEXAUI_SETTINGS_FONT=${_settings_font};NEXAUI_SETTINGS_TEST_APPLY_ERROR=1"
                    RESOURCE_LOCK native_display)
                add_test(NAME nexaui_settings_wayland_atspi_german
                    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/run_atspi_multiselection_wayland.py
                        ${NEXAUI_ATSPI_REGISTRY} $<TARGET_FILE:nexaui_settings_atspi_client> $<TARGET_FILE:nexaui_settings>)
                set_tests_properties(nexaui_settings_wayland_atspi_german PROPERTIES TIMEOUT 70 SKIP_RETURN_CODE 77
                    ENVIRONMENT "NEXAUI_SETTINGS_FONT=${_settings_font};NEXAUI_SETTINGS_TEST_GERMAN=1;NEXAUI_SETTINGS_TEST_APPLY_ERROR=1"
                    RESOURCE_LOCK native_display)
                if(NEXAUI_XDOTOOL_EXECUTABLE)
                    add_test(NAME nexaui_settings_wayland_atspi_clipboard
                        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/run_atspi_multiselection_wayland.py
                            ${NEXAUI_ATSPI_REGISTRY} $<TARGET_FILE:nexaui_settings_atspi_client> $<TARGET_FILE:nexaui_settings>)
                    set_tests_properties(nexaui_settings_wayland_atspi_clipboard PROPERTIES TIMEOUT 70 SKIP_RETURN_CODE 77
                        ENVIRONMENT "NEXAUI_SETTINGS_FONT=${_settings_font};NEXAUI_SETTINGS_CLIPBOARD_SERIAL=1;NEXAUI_SETTINGS_XDOTOOL=${NEXAUI_XDOTOOL_EXECUTABLE}"
                        RESOURCE_LOCK native_display)
                    find_program(NEXAUI_WL_COPY_EXECUTABLE wl-copy)
                    find_program(NEXAUI_WL_PASTE_EXECUTABLE wl-paste)
                    if(NEXAUI_WL_COPY_EXECUTABLE AND NEXAUI_WL_PASTE_EXECUTABLE)
                        add_test(NAME nexaui_settings_wayland_atspi_clipboard_peer
                            COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/run_atspi_multiselection_wayland.py
                                ${NEXAUI_ATSPI_REGISTRY} $<TARGET_FILE:nexaui_settings_atspi_client> $<TARGET_FILE:nexaui_settings>)
                        set_tests_properties(nexaui_settings_wayland_atspi_clipboard_peer PROPERTIES TIMEOUT 70 SKIP_RETURN_CODE 77
                            ENVIRONMENT "NEXAUI_SETTINGS_FONT=${_settings_font};NEXAUI_SETTINGS_CLIPBOARD_SERIAL=1;NEXAUI_SETTINGS_CLIPBOARD_PEER=1;NEXAUI_SETTINGS_XDOTOOL=${NEXAUI_XDOTOOL_EXECUTABLE};NEXAUI_TEST_WL_COPY=${NEXAUI_WL_COPY_EXECUTABLE};NEXAUI_TEST_WL_PASTE=${NEXAUI_WL_PASTE_EXECUTABLE}"
                            RESOURCE_LOCK native_display)
                        add_test(NAME nexaui_settings_wayland_atspi_clipboard_once
                            COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/run_atspi_multiselection_wayland.py
                                ${NEXAUI_ATSPI_REGISTRY} $<TARGET_FILE:nexaui_settings_atspi_client> $<TARGET_FILE:nexaui_settings>)
                        set_tests_properties(nexaui_settings_wayland_atspi_clipboard_once PROPERTIES TIMEOUT 70 SKIP_RETURN_CODE 77
                            ENVIRONMENT "NEXAUI_SETTINGS_FONT=${_settings_font};NEXAUI_SETTINGS_CLIPBOARD_SERIAL=1;NEXAUI_SETTINGS_CLIPBOARD_PEER=1;NEXAUI_SETTINGS_CLIPBOARD_ONCE=1;NEXAUI_SETTINGS_XDOTOOL=${NEXAUI_XDOTOOL_EXECUTABLE};NEXAUI_TEST_WL_COPY=${NEXAUI_WL_COPY_EXECUTABLE};NEXAUI_TEST_WL_PASTE=${NEXAUI_WL_PASTE_EXECUTABLE}"
                            RESOURCE_LOCK native_display)
                    endif()
                endif()
            endif()
        endif()
    endif()
endif()
