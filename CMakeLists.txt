cmake_minimum_required(VERSION 3.21)
project(NexaUI VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(NEXAUI_BUILD_TESTS "Build NexaUI tests" ON)
option(NEXAUI_BUILD_EXAMPLES "Build NexaUI examples" ON)
option(NEXAUI_BUILD_BENCHMARKS "Build NexaUI benchmarks" OFF)
option(NEXAUI_BUILD_FUZZERS "Build Clang/libFuzzer targets" OFF)

add_library(nexaui src/nexaui.c src/headless.c src/renderer_backend.c src/theme.c src/color.c src/utf8.c src/grapheme.c src/word.c src/line.c src/bidi.c src/shape.c src/gsub.c src/gpos.c src/gpos_marks.c src/ot_layout.c src/text_layout.c src/text_buffer.c src/virtual_list.c src/table.c src/tree.c src/docking.c src/search.c src/snapshot.c src/sim_channel.c src/series.c src/arena.c src/layout.c src/font.c src/glyf.c src/raster.c src/glyph_cache.c src/font_collection.c src/unicode_data.c platform/common/event_queue.c platform/common/drop_queue.c platform/common/uri_list.c)
target_include_directories(nexaui PRIVATE platform/common src)
target_sources(nexaui PRIVATE src/text_stamp.c)
target_sources(nexaui PRIVATE src/paint.c)
target_sources(nexaui PRIVATE src/gsub_context.c)
target_sources(nexaui PRIVATE src/selection_model.c)
if(WIN32)
    target_sources(nexaui PRIVATE platform/windows/window.c platform/windows/snapshot_lock.c platform/windows/accessibility.c)
    target_link_libraries(nexaui PRIVATE user32 shell32 imm32 ole32 oleaut32 uiautomationcore uuid)
elseif(APPLE)
    enable_language(OBJC)
    find_package(Threads REQUIRED)
    target_sources(nexaui PRIVATE platform/macos/window.m platform/macos/accessibility.m platform/posix/snapshot_lock.c)
    target_compile_options(nexaui PRIVATE $<$<COMPILE_LANGUAGE:OBJC>:-fno-objc-arc>)
    set_target_properties(nexaui PROPERTIES OBJC_STANDARD 17 OBJC_STANDARD_REQUIRED YES OBJC_EXTENSIONS NO)
    target_link_libraries(nexaui PRIVATE "-framework AppKit" objc Threads::Threads)
elseif(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    find_package(Threads REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
    pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
    option(NEXAUI_ENABLE_ATSPI "Build the runtime-optional Linux AT-SPI transport" ON)
    if(NEXAUI_ENABLE_ATSPI)
        pkg_check_modules(NEXAUI_DBUS QUIET dbus-1)
    else()
        set(NEXAUI_DBUS_FOUND FALSE)
    endif()
    if(NEXAUI_DBUS_FOUND)
        target_compile_definitions(nexaui PRIVATE NEXAUI_HAVE_DBUS_SDK=1)
        target_include_directories(nexaui PRIVATE ${NEXAUI_DBUS_INCLUDE_DIRS})
    endif()
    target_link_libraries(nexaui PRIVATE ${CMAKE_DL_LIBS})
    pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
    pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
    find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
    set(NEXAUI_WAYLAND_PROTOCOL_DIR
        ${CMAKE_CURRENT_BINARY_DIR}/generated/wayland)
    set(NEXAUI_XDG_SHELL_XML
        ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml)
    set(NEXAUI_TEXT_INPUT_V1_XML
        ${WAYLAND_PROTOCOLS_DIR}/unstable/text-input/text-input-unstable-v1.xml)
    set(NEXAUI_TEXT_INPUT_V3_XML
        ${WAYLAND_PROTOCOLS_DIR}/unstable/text-input/text-input-unstable-v3.xml)
    set(NEXAUI_DECORATION_XML
        ${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml)
    add_custom_command(
        OUTPUT ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-decoration-client-protocol.h
               ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-decoration-protocol.c
        COMMAND ${CMAKE_COMMAND} -E make_directory ${NEXAUI_WAYLAND_PROTOCOL_DIR}
        COMMAND ${WAYLAND_SCANNER} client-header ${NEXAUI_DECORATION_XML}
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-decoration-client-protocol.h
        COMMAND ${WAYLAND_SCANNER} private-code ${NEXAUI_DECORATION_XML}
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-decoration-protocol.c
        DEPENDS ${NEXAUI_DECORATION_XML}
        VERBATIM)
    add_custom_command(
        OUTPUT ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-shell-client-protocol.h
               ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-shell-protocol.c
        COMMAND ${CMAKE_COMMAND} -E make_directory
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}
        COMMAND ${WAYLAND_SCANNER} client-header ${NEXAUI_XDG_SHELL_XML}
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-shell-client-protocol.h
        COMMAND ${WAYLAND_SCANNER} private-code ${NEXAUI_XDG_SHELL_XML}
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-shell-protocol.c
        DEPENDS ${NEXAUI_XDG_SHELL_XML}
        VERBATIM)
    add_custom_command(
        OUTPUT ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v1-client-protocol.h
               ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v1-protocol.c
        COMMAND ${CMAKE_COMMAND} -E make_directory
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}
        COMMAND ${WAYLAND_SCANNER} client-header ${NEXAUI_TEXT_INPUT_V1_XML}
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v1-client-protocol.h
        COMMAND ${WAYLAND_SCANNER} private-code ${NEXAUI_TEXT_INPUT_V1_XML}
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v1-protocol.c
        DEPENDS ${NEXAUI_TEXT_INPUT_V1_XML}
        VERBATIM)
    add_custom_command(
        OUTPUT ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v3-client-protocol.h
               ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v3-protocol.c
        COMMAND ${CMAKE_COMMAND} -E make_directory
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}
        COMMAND ${WAYLAND_SCANNER} client-header ${NEXAUI_TEXT_INPUT_V3_XML}
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v3-client-protocol.h
        COMMAND ${WAYLAND_SCANNER} private-code ${NEXAUI_TEXT_INPUT_V3_XML}
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v3-protocol.c
        DEPENDS ${NEXAUI_TEXT_INPUT_V3_XML}
        VERBATIM)
    target_sources(nexaui PRIVATE platform/linux/window.c platform/linux/accessibility.c
        platform/linux/x11/window.c platform/linux/wayland/window.c
        platform/linux/wayland/repeat.c platform/linux/wayland/cursor.c
        ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-shell-protocol.c
        ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-decoration-protocol.c
        ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v1-protocol.c
        ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v3-protocol.c
        platform/posix/snapshot_lock.c)
    target_include_directories(nexaui PRIVATE platform/linux/x11
        ${NEXAUI_WAYLAND_PROTOCOL_DIR})
    target_link_libraries(nexaui PRIVATE X11::X11 Threads::Threads
        PkgConfig::WAYLAND_CLIENT PkgConfig::XKBCOMMON)
else()
    target_sources(nexaui PRIVATE platform/unsupported/window.c platform/posix/snapshot_lock.c)
    find_package(Threads REQUIRED)
    target_link_libraries(nexaui PRIVATE Threads::Threads)
endif()

if(NEXAUI_BUILD_FUZZERS)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "NEXAUI_BUILD_FUZZERS requires Clang with libFuzzer")
    endif()
    add_executable(nexaui_sfnt_fuzz fuzz/sfnt_fuzz.c src/font.c src/glyf.c src/raster.c src/glyph_cache.c src/font_collection.c)
    target_include_directories(nexaui_sfnt_fuzz PRIVATE include)
    target_compile_features(nexaui_sfnt_fuzz PRIVATE c_std_17)
    target_compile_options(nexaui_sfnt_fuzz PRIVATE -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer)
    target_link_options(nexaui_sfnt_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
    if(UNIX AND NOT APPLE)
        target_link_libraries(nexaui_sfnt_fuzz PRIVATE m)
    endif()
    add_executable(nexaui_gsub_fuzz fuzz/gsub_fuzz.c src/font.c src/gsub.c src/gsub_context.c src/ot_layout.c src/unicode_data.c)
    target_include_directories(nexaui_gsub_fuzz PRIVATE include src)
    target_compile_features(nexaui_gsub_fuzz PRIVATE c_std_17)
    target_compile_options(nexaui_gsub_fuzz PRIVATE -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer)
    target_link_options(nexaui_gsub_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(nexaui_gpos_fuzz fuzz/gpos_fuzz.c src/font.c src/gsub.c src/gsub_context.c src/gpos.c src/gpos_marks.c src/ot_layout.c src/unicode_data.c)
    target_include_directories(nexaui_gpos_fuzz PRIVATE include src)
    target_compile_features(nexaui_gpos_fuzz PRIVATE c_std_17)
    target_compile_options(nexaui_gpos_fuzz PRIVATE -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer)
    target_link_options(nexaui_gpos_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(nexaui_text_fuzz fuzz/text_fuzz.c src/utf8.c src/grapheme.c
        src/word.c src/line.c src/text_buffer.c src/unicode_data.c)
    target_include_directories(nexaui_text_fuzz PRIVATE include src)
    target_compile_features(nexaui_text_fuzz PRIVATE c_std_17)
    target_compile_options(nexaui_text_fuzz PRIVATE -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer)
    target_link_options(nexaui_text_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(nexaui_layout_fuzz fuzz/layout_fuzz.c src/font.c
        src/font_collection.c src/shape.c src/gsub.c src/gsub_context.c src/gpos.c src/gpos_marks.c src/ot_layout.c src/bidi.c
        src/grapheme.c src/word.c src/line.c src/text_layout.c src/utf8.c
        src/unicode_data.c)
    target_include_directories(nexaui_layout_fuzz PRIVATE include src)
    target_compile_features(nexaui_layout_fuzz PRIVATE c_std_17)
    target_compile_options(nexaui_layout_fuzz PRIVATE -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer)
    target_link_options(nexaui_layout_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
    if(UNIX AND NOT APPLE)
        target_link_libraries(nexaui_gsub_fuzz PRIVATE m)
        target_link_libraries(nexaui_gpos_fuzz PRIVATE m)
        target_link_libraries(nexaui_layout_fuzz PRIVATE m)
    endif()
endif()
add_library(NexaUI::NexaUI ALIAS nexaui)
target_compile_features(nexaui PUBLIC c_std_17)
set_target_properties(nexaui PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO
    VERSION ${PROJECT_VERSION} SOVERSION 0 EXPORT_NAME NexaUI)
target_include_directories(nexaui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if(UNIX AND NOT APPLE)
    target_link_libraries(nexaui PRIVATE m)
endif()
if(BUILD_SHARED_LIBS)
    target_compile_definitions(nexaui PRIVATE NEXAUI_BUILDING_SHARED PUBLIC NEXAUI_SHARED)
endif()
if(MSVC)
    target_compile_options(nexaui PRIVATE /W4)
else()
    target_compile_options(nexaui PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    add_library(nexaui_renderer_d3d12 STATIC renderers/d3d12/renderer.c)
    target_include_directories(nexaui_renderer_d3d12 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(nexaui_renderer_d3d12 PRIVATE
        NexaUI::NexaUI d3d12 dxgi dxguid)
    target_compile_features(nexaui_renderer_d3d12 PRIVATE c_std_17)
    if(MSVC)
        target_compile_options(nexaui_renderer_d3d12 PRIVATE /W4)
    endif()
endif()

if(APPLE)
    option(NEXAUI_BUILD_METAL "Build the private macOS Metal backend" ON)
    if(NEXAUI_BUILD_METAL)
        add_library(nexaui_renderer_metal STATIC renderers/metal/renderer.m
            renderers/metal/draw_plan.c renderers/metal/bitmap_plan.c renderers/metal/batch_plan.c
            renderers/metal/timing_history.c)
        target_include_directories(nexaui_renderer_metal PRIVATE
            ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/platform/macos)
        target_link_libraries(nexaui_renderer_metal PRIVATE NexaUI::NexaUI
            "-framework Metal" "-framework QuartzCore" "-framework CoreGraphics"
            "-framework Foundation" objc)
        target_compile_options(nexaui_renderer_metal PRIVATE
            $<$<COMPILE_LANGUAGE:OBJC>:-fno-objc-arc> -Wall -Wextra -Wpedantic)
        set_target_properties(nexaui_renderer_metal PROPERTIES
            OBJC_STANDARD 17 OBJC_STANDARD_REQUIRED YES OBJC_EXTENSIONS NO)
    endif()
endif()

if(UNIX AND NOT APPLE)
    option(NEXAUI_BUILD_VULKAN "Build the private Linux Vulkan backend" ON)
    if(NEXAUI_BUILD_VULKAN)
        find_package(Vulkan 1.3.250 REQUIRED)
        add_library(nexaui_renderer_vulkan STATIC renderers/vulkan/device.c
            renderers/vulkan/renderer.c renderers/vulkan/pipeline.c renderers/vulkan/timing_history.c)
        target_include_directories(nexaui_renderer_vulkan PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/platform/linux)
        target_link_libraries(nexaui_renderer_vulkan PRIVATE
            NexaUI::NexaUI Vulkan::Vulkan Threads::Threads X11::X11
            PkgConfig::WAYLAND_CLIENT m)
        set_target_properties(nexaui_renderer_vulkan PROPERTIES
            C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        target_compile_options(nexaui_renderer_vulkan PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
endif()

include(cmake/NexaUIDependencyAudit.cmake)
nexaui_audit_core_target(nexaui)

if(NEXAUI_BUILD_TESTS)
    enable_testing()
    if(TARGET nexaui_renderer_vulkan)
        add_executable(nexaui_vulkan_validation_tests tests/test_vulkan_validation.c)
        target_link_libraries(nexaui_vulkan_validation_tests PRIVATE Vulkan::Vulkan)
        target_compile_features(nexaui_vulkan_validation_tests PRIVATE c_std_17)
        target_compile_options(nexaui_vulkan_validation_tests PRIVATE -Wall -Wextra -Wpedantic)
        add_executable(nexaui_vulkan_lifetime_control tools/vulkan_lifetime_control.c)
        target_link_libraries(nexaui_vulkan_lifetime_control PRIVATE Vulkan::Vulkan ${CMAKE_DL_LIBS})
        target_compile_features(nexaui_vulkan_lifetime_control PRIVATE c_std_17)
        set_target_properties(nexaui_vulkan_lifetime_control PROPERTIES C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        target_compile_options(nexaui_vulkan_lifetime_control PRIVATE -Wall -Wextra -Wpedantic)
        add_test(NAME nexaui_vulkan_validation COMMAND nexaui_vulkan_validation_tests)
        set_tests_properties(nexaui_vulkan_validation PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
        target_compile_definitions(nexaui_renderer_vulkan PRIVATE
            NEXAUI_VULKAN_TEST_HOOKS)
        foreach(_vk_test device native)
            add_executable(nexaui_vulkan_${_vk_test}_tests
                tests/test_vulkan_${_vk_test}.c)
            target_include_directories(nexaui_vulkan_${_vk_test}_tests PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/include
                ${CMAKE_CURRENT_SOURCE_DIR}/renderers/vulkan)
            target_compile_definitions(nexaui_vulkan_${_vk_test}_tests PRIVATE
                NEXAUI_VULKAN_TEST_HOOKS)
            target_compile_options(nexaui_vulkan_${_vk_test}_tests PRIVATE
                -Wall -Wextra -Wpedantic)
            set_target_properties(nexaui_vulkan_${_vk_test}_tests PROPERTIES
                C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
            target_link_libraries(nexaui_vulkan_${_vk_test}_tests PRIVATE
                nexaui_renderer_vulkan Vulkan::Vulkan Threads::Threads)
            add_test(NAME nexaui_vulkan_${_vk_test}
                COMMAND nexaui_vulkan_${_vk_test}_tests)
            set_tests_properties(nexaui_vulkan_${_vk_test} PROPERTIES TIMEOUT 30)
        endforeach()
        set_tests_properties(nexaui_vulkan_native PROPERTIES SKIP_RETURN_CODE 77
            FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
        add_executable(nexaui_vulkan_present_tests tests/test_vulkan_present.c)
        target_include_directories(nexaui_vulkan_present_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/renderers/vulkan
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/platform/linux)
        target_compile_definitions(nexaui_vulkan_present_tests PRIVATE
            NEXAUI_VULKAN_TEST_HOOKS NEXAUI_WAYLAND_TEST_HOOKS)
        target_compile_options(nexaui_vulkan_present_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_features(nexaui_vulkan_present_tests PRIVATE c_std_17)
        target_link_libraries(nexaui_vulkan_present_tests PRIVATE
            nexaui_renderer_vulkan NexaUI::NexaUI X11::X11 Threads::Threads
            PkgConfig::WAYLAND_CLIENT)
        foreach(_vk_platform x11 wayland)
            add_test(NAME nexaui_vulkan_${_vk_platform}_present COMMAND nexaui_vulkan_present_tests)
            set_tests_properties(nexaui_vulkan_${_vk_platform}_present PROPERTIES
                TIMEOUT 90 SKIP_RETURN_CODE 77 RESOURCE_LOCK native_display
                ENVIRONMENT "NEXAUI_PLATFORM=${_vk_platform}"
                FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
        endforeach()
        add_executable(nexaui_vulkan_draw_tests tests/test_vulkan_draw.c)
        target_include_directories(nexaui_vulkan_draw_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/renderers/vulkan
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/platform/linux)
        target_compile_definitions(nexaui_vulkan_draw_tests PRIVATE
            NEXAUI_VULKAN_TEST_HOOKS NEXAUI_WAYLAND_TEST_HOOKS)
        target_compile_options(nexaui_vulkan_draw_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_features(nexaui_vulkan_draw_tests PRIVATE c_std_17)
        target_link_libraries(nexaui_vulkan_draw_tests PRIVATE
            nexaui_renderer_vulkan NexaUI::NexaUI PkgConfig::WAYLAND_CLIENT)
        foreach(_vk_platform x11 wayland)
            add_test(NAME nexaui_vulkan_${_vk_platform}_draw COMMAND nexaui_vulkan_draw_tests)
            set_tests_properties(nexaui_vulkan_${_vk_platform}_draw PROPERTIES
                TIMEOUT 90 SKIP_RETURN_CODE 77 RESOURCE_LOCK native_display
                ENVIRONMENT "NEXAUI_PLATFORM=${_vk_platform}"
                FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
        endforeach()
    endif()
    if(TARGET nexaui_renderer_vulkan)
        add_executable(nexaui_vulkan_timing_tests tests/test_vulkan_timing.c)
        target_include_directories(nexaui_vulkan_timing_tests PRIVATE ${PROJECT_SOURCE_DIR}/renderers/vulkan ${PROJECT_SOURCE_DIR}/src)
        target_compile_definitions(nexaui_vulkan_timing_tests PRIVATE NEXAUI_VULKAN_TEST_HOOKS)
        target_compile_features(nexaui_vulkan_timing_tests PRIVATE c_std_17)
        target_compile_options(nexaui_vulkan_timing_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_link_libraries(nexaui_vulkan_timing_tests PRIVATE nexaui_renderer_vulkan NexaUI::NexaUI)
        foreach(_vk_platform x11 wayland)
            add_test(NAME nexaui_vulkan_${_vk_platform}_timing COMMAND nexaui_vulkan_timing_tests)
            set_tests_properties(nexaui_vulkan_${_vk_platform}_timing PROPERTIES
                TIMEOUT 45 SKIP_RETURN_CODE 77 RESOURCE_LOCK native_display
                ENVIRONMENT "NEXAUI_PLATFORM=${_vk_platform}"
                FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
        endforeach()
        add_executable(nexaui_vulkan_ring_tests tests/test_vulkan_ring.c)
        target_include_directories(nexaui_vulkan_ring_tests PRIVATE
            ${PROJECT_SOURCE_DIR}/renderers/vulkan ${PROJECT_SOURCE_DIR}/src)
        target_compile_definitions(nexaui_vulkan_ring_tests PRIVATE NEXAUI_VULKAN_TEST_HOOKS)
        target_compile_options(nexaui_vulkan_ring_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_features(nexaui_vulkan_ring_tests PRIVATE c_std_17)
        target_link_libraries(nexaui_vulkan_ring_tests PRIVATE nexaui_renderer_vulkan NexaUI::NexaUI)
        foreach(_vk_platform x11 wayland)
            add_test(NAME nexaui_vulkan_${_vk_platform}_ring COMMAND nexaui_vulkan_ring_tests)
            set_tests_properties(nexaui_vulkan_${_vk_platform}_ring PROPERTIES
                TIMEOUT 45 SKIP_RETURN_CODE 77 RESOURCE_LOCK native_display
                ENVIRONMENT "NEXAUI_PLATFORM=${_vk_platform};NEXAUI_VULKAN_TEST_TIMELINE=1"
                FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
        endforeach()
    endif()
    add_executable(nexaui_vulkan_timing_math_tests tests/test_vulkan_timing_math.c renderers/vulkan/timing_history.c)
    target_include_directories(nexaui_vulkan_timing_math_tests PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/renderers/vulkan)
    target_link_libraries(nexaui_vulkan_timing_math_tests PRIVATE NexaUI::NexaUI)
    if(UNIX AND NOT APPLE)
        target_link_libraries(nexaui_vulkan_timing_math_tests PRIVATE m)
    endif()
    target_compile_features(nexaui_vulkan_timing_math_tests PRIVATE c_std_17)
    add_test(NAME nexaui_vulkan_timing_math COMMAND nexaui_vulkan_timing_math_tests)
    set_tests_properties(nexaui_vulkan_timing_math PROPERTIES TIMEOUT 20)
    add_executable(nexaui_metal_plan_tests tests/test_metal_plan.c renderers/metal/draw_plan.c)
    target_include_directories(nexaui_metal_plan_tests PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/renderers/metal)
    target_link_libraries(nexaui_metal_plan_tests PRIVATE NexaUI::NexaUI)
    target_compile_features(nexaui_metal_plan_tests PRIVATE c_std_17)
    add_test(NAME nexaui_metal_plan COMMAND nexaui_metal_plan_tests)
    set_tests_properties(nexaui_metal_plan PROPERTIES TIMEOUT 20)
    add_executable(nexaui_metal_bitmap_tests tests/test_metal_bitmaps.c renderers/metal/bitmap_plan.c)
    target_include_directories(nexaui_metal_bitmap_tests PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/renderers/metal)
    target_link_libraries(nexaui_metal_bitmap_tests PRIVATE NexaUI::NexaUI)
    target_compile_features(nexaui_metal_bitmap_tests PRIVATE c_std_17)
    add_test(NAME nexaui_metal_bitmaps COMMAND nexaui_metal_bitmap_tests)
    set_tests_properties(nexaui_metal_bitmaps PROPERTIES TIMEOUT 20)
    add_executable(nexaui_metal_batch_tests tests/test_metal_batches.c renderers/metal/batch_plan.c
        renderers/metal/draw_plan.c renderers/metal/bitmap_plan.c)
    target_include_directories(nexaui_metal_batch_tests PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/renderers/metal)
    target_link_libraries(nexaui_metal_batch_tests PRIVATE NexaUI::NexaUI)
    target_compile_features(nexaui_metal_batch_tests PRIVATE c_std_17)
    add_test(NAME nexaui_metal_batches COMMAND nexaui_metal_batch_tests)
    set_tests_properties(nexaui_metal_batches PROPERTIES TIMEOUT 30)
    add_executable(nexaui_metal_timing_tests tests/test_metal_timing.c renderers/metal/timing_history.c)
    target_include_directories(nexaui_metal_timing_tests PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/renderers/metal)
    target_link_libraries(nexaui_metal_timing_tests PRIVATE NexaUI::NexaUI)
    target_compile_features(nexaui_metal_timing_tests PRIVATE c_std_17)
    add_test(NAME nexaui_metal_timing COMMAND nexaui_metal_timing_tests)
    set_tests_properties(nexaui_metal_timing PROPERTIES TIMEOUT 20)
    add_executable(nexaui_platform_event_queue_tests
        tests/test_platform_event_queue.c platform/common/event_queue.c)
    target_include_directories(nexaui_platform_event_queue_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/common)
    target_link_libraries(nexaui_platform_event_queue_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_platform_event_queue
        COMMAND nexaui_platform_event_queue_tests)
    set_tests_properties(nexaui_platform_event_queue PROPERTIES TIMEOUT 15)
    add_executable(nexaui_platform_drop_queue_tests
        tests/test_platform_drop_queue.c platform/common/drop_queue.c)
    target_include_directories(nexaui_platform_drop_queue_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/common)
    target_link_libraries(nexaui_platform_drop_queue_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_platform_drop_queue
        COMMAND nexaui_platform_drop_queue_tests)
    set_tests_properties(nexaui_platform_drop_queue PROPERTIES TIMEOUT 15)
    add_executable(nexaui_uri_list_tests tests/test_uri_list.c
        platform/common/uri_list.c platform/common/drop_queue.c)
    target_include_directories(nexaui_uri_list_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/common)
    target_link_libraries(nexaui_uri_list_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_uri_list COMMAND nexaui_uri_list_tests)
    set_tests_properties(nexaui_uri_list PROPERTIES TIMEOUT 15)
    if(UNIX AND NOT APPLE)
        target_compile_definitions(nexaui PRIVATE
            NEXAUI_WAYLAND_TEST_HOOKS NEXAUI_X11_TEST_HOOKS)
        add_executable(nexaui_wayland_cursor_tests
            tests/test_wayland_cursor.c platform/linux/wayland/cursor.c)
        target_include_directories(nexaui_wayland_cursor_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/wayland
            ${CMAKE_CURRENT_SOURCE_DIR}/include)
        add_test(NAME nexaui_wayland_cursor
            COMMAND nexaui_wayland_cursor_tests)
        set_tests_properties(nexaui_wayland_cursor PROPERTIES TIMEOUT 15)
        add_executable(nexaui_wayland_repeat_tests
            tests/test_wayland_repeat.c platform/linux/wayland/repeat.c)
        target_include_directories(nexaui_wayland_repeat_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/wayland)
        add_test(NAME nexaui_wayland_repeat
            COMMAND nexaui_wayland_repeat_tests)
        set_tests_properties(nexaui_wayland_repeat PROPERTIES TIMEOUT 15)
        add_executable(nexaui_wayland_window_tests
            tests/test_wayland_window.c)
        target_include_directories(nexaui_wayland_window_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/platform/linux)
        target_compile_definitions(nexaui_wayland_window_tests PRIVATE
            NEXAUI_WAYLAND_TEST_HOOKS)
        target_link_libraries(nexaui_wayland_window_tests PRIVATE
            NexaUI::NexaUI X11::X11 Threads::Threads
            PkgConfig::WAYLAND_CLIENT)
        add_test(NAME nexaui_wayland_window
            COMMAND nexaui_wayland_window_tests)
        set_tests_properties(nexaui_wayland_window PROPERTIES TIMEOUT 15
            SKIP_RETURN_CODE 77 RESOURCE_LOCK native_display)
        add_executable(nexaui_native_management_tests tests/test_native_management.c)
        target_link_libraries(nexaui_native_management_tests PRIVATE NexaUI::NexaUI X11::X11)
        add_test(NAME nexaui_wayland_management_native COMMAND nexaui_native_management_tests)
        set_tests_properties(nexaui_wayland_management_native PROPERTIES TIMEOUT 20
            SKIP_RETURN_CODE 77 ENVIRONMENT "NEXAUI_PLATFORM=wayland" RESOURCE_LOCK native_display)
        pkg_check_modules(WAYLAND_SERVER QUIET IMPORTED_TARGET wayland-server)
        if(WAYLAND_SERVER_FOUND)
            add_custom_command(
                OUTPUT ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-shell-server-protocol.h
                       ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-decoration-unstable-v1-server-protocol.h
                       ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v1-server-protocol.h
                       ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v3-server-protocol.h
                COMMAND ${CMAKE_COMMAND} -E make_directory
                        ${NEXAUI_WAYLAND_PROTOCOL_DIR}
                COMMAND ${WAYLAND_SCANNER} server-header ${NEXAUI_XDG_SHELL_XML}
                        ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-shell-server-protocol.h
                COMMAND ${WAYLAND_SCANNER} server-header ${NEXAUI_DECORATION_XML}
                        ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-decoration-unstable-v1-server-protocol.h
                COMMAND ${WAYLAND_SCANNER} server-header ${NEXAUI_TEXT_INPUT_V3_XML}
                        ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v3-server-protocol.h
                COMMAND ${WAYLAND_SCANNER} server-header ${NEXAUI_TEXT_INPUT_V1_XML}
                        ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v1-server-protocol.h
                DEPENDS ${NEXAUI_XDG_SHELL_XML} ${NEXAUI_DECORATION_XML} ${NEXAUI_TEXT_INPUT_V1_XML}
                        ${NEXAUI_TEXT_INPUT_V3_XML}
                VERBATIM)
            # One producer owns shared outputs. Independent test targets must
            # not race to run the same scanner command in a parallel build.
            add_custom_target(nexaui_wayland_test_protocols DEPENDS
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-shell-server-protocol.h
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-decoration-unstable-v1-server-protocol.h
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v1-server-protocol.h
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v3-server-protocol.h)
            add_executable(nexaui_wayland_management_tests tests/test_wayland_management.c
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-shell-protocol.c
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-decoration-protocol.c)
            add_dependencies(nexaui_wayland_management_tests nexaui_wayland_test_protocols)
            target_include_directories(nexaui_wayland_management_tests PRIVATE ${NEXAUI_WAYLAND_PROTOCOL_DIR} platform/linux)
            target_link_libraries(nexaui_wayland_management_tests PRIVATE NexaUI::NexaUI Threads::Threads PkgConfig::WAYLAND_SERVER PkgConfig::WAYLAND_CLIENT)
            add_test(NAME nexaui_wayland_management COMMAND nexaui_wayland_management_tests)
            set_tests_properties(nexaui_wayland_management PROPERTIES TIMEOUT 20)
            add_executable(nexaui_wayland_text_input_v1_tests
                tests/test_wayland_text_input_v1.c
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-shell-protocol.c
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v1-protocol.c)
            add_dependencies(nexaui_wayland_text_input_v1_tests
                nexaui_wayland_test_protocols)
            target_include_directories(nexaui_wayland_text_input_v1_tests PRIVATE
                ${NEXAUI_WAYLAND_PROTOCOL_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/platform/linux)
            target_link_libraries(nexaui_wayland_text_input_v1_tests PRIVATE
                NexaUI::NexaUI Threads::Threads PkgConfig::WAYLAND_SERVER
                PkgConfig::WAYLAND_CLIENT)
            add_test(NAME nexaui_wayland_text_input_v1
                COMMAND nexaui_wayland_text_input_v1_tests)
            set_tests_properties(nexaui_wayland_text_input_v1 PROPERTIES TIMEOUT 15)
            add_executable(nexaui_wayland_text_input_v3_tests
                tests/test_wayland_text_input_v3.c
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/xdg-shell-protocol.c
                ${NEXAUI_WAYLAND_PROTOCOL_DIR}/text-input-unstable-v3-protocol.c)
            add_dependencies(nexaui_wayland_text_input_v3_tests
                nexaui_wayland_test_protocols)
            target_include_directories(nexaui_wayland_text_input_v3_tests PRIVATE
                ${NEXAUI_WAYLAND_PROTOCOL_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/platform/linux)
            target_link_libraries(nexaui_wayland_text_input_v3_tests PRIVATE
                NexaUI::NexaUI Threads::Threads PkgConfig::WAYLAND_SERVER
                PkgConfig::WAYLAND_CLIENT)
            add_test(NAME nexaui_wayland_text_input_v3
                COMMAND nexaui_wayland_text_input_v3_tests)
            set_tests_properties(nexaui_wayland_text_input_v3 PROPERTIES TIMEOUT 15)
        endif()
        add_executable(nexaui_x11_window_tests tests/test_x11_window.c)
        target_include_directories(nexaui_x11_window_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/x11)
        target_compile_definitions(nexaui_x11_window_tests PRIVATE
            NEXAUI_X11_TEST_HOOKS)
        target_link_libraries(nexaui_x11_window_tests PRIVATE
            NexaUI::NexaUI X11::X11 Threads::Threads)
        add_test(NAME nexaui_x11_window COMMAND nexaui_x11_window_tests)
        set_tests_properties(nexaui_x11_window PROPERTIES TIMEOUT 15
            SKIP_RETURN_CODE 77 ENVIRONMENT "NEXAUI_PLATFORM=x11"
            RESOURCE_LOCK native_display)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        find_program(NEXAUI_XVFB_EXECUTABLE Xvfb)
        find_program(NEXAUI_XAUTH_EXECUTABLE xauth)
        add_executable(nexaui_linux_atspi_host tests/linux_atspi_host.c)
        target_include_directories(nexaui_linux_atspi_host PRIVATE platform/linux)
        target_link_libraries(nexaui_linux_atspi_host PRIVATE NexaUI::NexaUI X11::X11)
        target_compile_features(nexaui_linux_atspi_host PRIVATE c_std_17)
        set_target_properties(nexaui_linux_atspi_host PROPERTIES
            C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        target_compile_options(nexaui_linux_atspi_host PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_definitions(nexaui_linux_atspi_host PRIVATE NEXAUI_X11_TEST_HOOKS)
        pkg_check_modules(NEXAUI_ATSPI QUIET IMPORTED_TARGET atspi-2 gobject-2.0 dbus-1)
        find_program(NEXAUI_ATSPI_REGISTRY at-spi2-registryd
            PATHS /usr/libexec /usr/lib/at-spi2-core)
        find_program(NEXAUI_DBUS_DAEMON dbus-daemon)
        find_program(NEXAUI_DBUS_SEND dbus-send)
        find_program(NEXAUI_ATSPI_LAUNCHER at-spi-bus-launcher
            PATHS /usr/libexec /usr/lib/at-spi2-core)
        if(NEXAUI_DBUS_FOUND AND NEXAUI_ATSPI_FOUND AND NEXAUI_ATSPI_REGISTRY
                AND NEXAUI_DBUS_DAEMON AND NEXAUI_DBUS_SEND AND Python3_Interpreter_FOUND)
            add_executable(nexaui_linux_atspi_client tests/linux_atspi_client.c)
            target_link_libraries(nexaui_linux_atspi_client PRIVATE PkgConfig::NEXAUI_ATSPI)
            target_compile_features(nexaui_linux_atspi_client PRIVATE c_std_17)
            set_target_properties(nexaui_linux_atspi_client PROPERTIES
                C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
            target_compile_options(nexaui_linux_atspi_client PRIVATE -Wall -Wextra -Wpedantic)
            add_library(nexaui_dbus_missing_abi SHARED tests/linux_dbus_missing.c)
            set_target_properties(nexaui_dbus_missing_abi PROPERTIES
                OUTPUT_NAME dbus-1 SOVERSION 3 VERSION 3
                C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO
                LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/missing-dbus)
            if(NEXAUI_XVFB_EXECUTABLE AND NEXAUI_XAUTH_EXECUTABLE)
                add_test(NAME nexaui_atspi_x11_private
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_xvfb_native.py
                        ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_atspi_native.py
                        ${NEXAUI_ATSPI_REGISTRY} $<TARGET_FILE:nexaui_linux_atspi_client>
                        $<TARGET_FILE:nexaui_linux_atspi_host>)
                set_tests_properties(nexaui_atspi_x11_private PROPERTIES TIMEOUT 70
                    ENVIRONMENT "NEXAUI_XVFB=${NEXAUI_XVFB_EXECUTABLE};NEXAUI_XAUTH=${NEXAUI_XAUTH_EXECUTABLE}")
                if(NEXAUI_ATSPI_LAUNCHER)
                    add_test(NAME nexaui_atspi_session_discovery_private
                        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_xvfb_native.py
                            ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_atspi_native.py
                            ${NEXAUI_ATSPI_REGISTRY} $<TARGET_FILE:nexaui_linux_atspi_client>
                            $<TARGET_FILE:nexaui_linux_atspi_host> ${NEXAUI_ATSPI_LAUNCHER})
                    set_tests_properties(nexaui_atspi_session_discovery_private PROPERTIES TIMEOUT 70
                        ENVIRONMENT "NEXAUI_XVFB=${NEXAUI_XVFB_EXECUTABLE};NEXAUI_XAUTH=${NEXAUI_XAUTH_EXECUTABLE}")
                endif()
                add_test(NAME nexaui_atspi_disabled
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_xvfb_native.py
                        $<TARGET_FILE:nexaui_linux_atspi_host> --optional)
                set_tests_properties(nexaui_atspi_disabled PROPERTIES TIMEOUT 30
                    ENVIRONMENT "NO_AT_BRIDGE=1;NEXAUI_XVFB=${NEXAUI_XVFB_EXECUTABLE};NEXAUI_XAUTH=${NEXAUI_XAUTH_EXECUTABLE}")
                add_test(NAME nexaui_atspi_missing_runtime_abi
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_xvfb_native.py
                        ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_atspi_native.py
                        ${NEXAUI_ATSPI_REGISTRY} --optional
                        $<TARGET_FILE:nexaui_linux_atspi_host>)
                # Only the host must see the incomplete SDK. registryd and the
                # fixture's dbus-send must keep their real system dependencies.
                set_tests_properties(nexaui_atspi_missing_runtime_abi PROPERTIES TIMEOUT 70
                    ENVIRONMENT "NEXAUI_TEST_DBUS_LIBRARY_DIR=$<TARGET_FILE_DIR:nexaui_dbus_missing_abi>;NEXAUI_XVFB=${NEXAUI_XVFB_EXECUTABLE};NEXAUI_XAUTH=${NEXAUI_XAUTH_EXECUTABLE}")
                add_test(NAME nexaui_atspi_no_bus
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_xvfb_native.py
                        ${CMAKE_COMMAND} -E env --unset=AT_SPI_BUS_ADDRESS
                        --unset=DBUS_SESSION_BUS_ADDRESS --unset=NO_AT_BRIDGE
                        $<TARGET_FILE:nexaui_linux_atspi_host> --optional)
                set_tests_properties(nexaui_atspi_no_bus PROPERTIES TIMEOUT 30
                    ENVIRONMENT "NEXAUI_XVFB=${NEXAUI_XVFB_EXECUTABLE};NEXAUI_XAUTH=${NEXAUI_XAUTH_EXECUTABLE}")
            endif()
        endif()
        if(Python3_Interpreter_FOUND AND NEXAUI_XVFB_EXECUTABLE
                AND NEXAUI_XAUTH_EXECUTABLE)
            add_executable(nexaui_x11_management_tests tests/test_x11_management.c)
            target_include_directories(nexaui_x11_management_tests PRIVATE platform/linux/x11)
            target_link_libraries(nexaui_x11_management_tests PRIVATE NexaUI::NexaUI X11::X11 Threads::Threads)
            add_test(NAME nexaui_x11_management
                COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_xvfb_native.py
                    $<TARGET_FILE:nexaui_x11_management_tests> --private-xvfb)
            set_tests_properties(nexaui_x11_management PROPERTIES TIMEOUT 30
                ENVIRONMENT "NEXAUI_XVFB=${NEXAUI_XVFB_EXECUTABLE};NEXAUI_XAUTH=${NEXAUI_XAUTH_EXECUTABLE}")
            find_program(NEXAUI_OPENBOX_EXECUTABLE openbox)
            if(NEXAUI_OPENBOX_EXECUTABLE)
                add_test(NAME nexaui_x11_management_native
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_xvfb_native.py
                        ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_openbox_native.py
                        ${NEXAUI_OPENBOX_EXECUTABLE} $<TARGET_FILE:nexaui_native_management_tests>)
                set_tests_properties(nexaui_x11_management_native PROPERTIES TIMEOUT 45
                    ENVIRONMENT "NEXAUI_XVFB=${NEXAUI_XVFB_EXECUTABLE};NEXAUI_XAUTH=${NEXAUI_XAUTH_EXECUTABLE}")
            endif()
            add_test(NAME nexaui_x11_private
                COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_xvfb_native.py
                    $<TARGET_FILE:nexaui_x11_window_tests>)
            set_tests_properties(nexaui_x11_private PROPERTIES TIMEOUT 80
                ENVIRONMENT "NEXAUI_XVFB=${NEXAUI_XVFB_EXECUTABLE};NEXAUI_XAUTH=${NEXAUI_XAUTH_EXECUTABLE}")
        endif()
    endif()
    if(WIN32)
        add_executable(nexaui_d3d12_smoke_tests tests/test_d3d12_smoke.c)
        target_compile_definitions(nexaui_renderer_d3d12 PRIVATE
            NEXAUI_D3D12_TEST_HOOKS)
        target_compile_definitions(nexaui_d3d12_smoke_tests PRIVATE
            NEXAUI_D3D12_TEST_HOOKS)
        target_include_directories(nexaui_d3d12_smoke_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/renderers/d3d12
            ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(nexaui_d3d12_smoke_tests PRIVATE
            NexaUI::NexaUI nexaui_renderer_d3d12)
        add_test(NAME nexaui_d3d12_smoke COMMAND nexaui_d3d12_smoke_tests)
        set_tests_properties(nexaui_d3d12_smoke PROPERTIES TIMEOUT 30)
        add_test(NAME nexaui_d3d12_warp_smoke COMMAND nexaui_d3d12_smoke_tests)
        set_tests_properties(nexaui_d3d12_warp_smoke PROPERTIES TIMEOUT 60
            ENVIRONMENT "NEXAUI_TEST_FORCE_WARP=1" RESOURCE_LOCK native_display)
        add_executable(nexaui_d3d12_widget_tests tests/test_d3d12_widgets.c)
        target_compile_definitions(nexaui_d3d12_widget_tests PRIVATE
            NEXAUI_D3D12_TEST_HOOKS)
        target_include_directories(nexaui_d3d12_widget_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/renderers/d3d12
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(nexaui_d3d12_widget_tests PRIVATE
            NexaUI::NexaUI nexaui_renderer_d3d12)
        add_test(NAME nexaui_d3d12_widgets COMMAND nexaui_d3d12_widget_tests)
        add_test(NAME nexaui_d3d12_widgets_warp COMMAND nexaui_d3d12_widget_tests)
        set_tests_properties(nexaui_d3d12_widgets nexaui_d3d12_widgets_warp
            PROPERTIES TIMEOUT 30 RESOURCE_LOCK native_display)
        set_tests_properties(nexaui_d3d12_widgets_warp PROPERTIES
            ENVIRONMENT "NEXAUI_TEST_FORCE_WARP=1")
        add_executable(nexaui_d3d12_pipeline_tests tests/test_d3d12_pipelines.c)
        target_compile_definitions(nexaui_d3d12_pipeline_tests PRIVATE NEXAUI_D3D12_TEST_HOOKS)
        target_include_directories(nexaui_d3d12_pipeline_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/renderers/d3d12
            ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_compile_features(nexaui_d3d12_pipeline_tests PRIVATE c_std_17)
        target_link_libraries(nexaui_d3d12_pipeline_tests PRIVATE NexaUI::NexaUI d3d12 dxgi dxguid)
        add_test(NAME nexaui_d3d12_pipelines COMMAND nexaui_d3d12_pipeline_tests)
        add_test(NAME nexaui_d3d12_pipelines_warp COMMAND nexaui_d3d12_pipeline_tests)
        set_tests_properties(nexaui_d3d12_pipelines nexaui_d3d12_pipelines_warp
            PROPERTIES TIMEOUT 90 RESOURCE_LOCK native_display)
        set_tests_properties(nexaui_d3d12_pipelines_warp PROPERTIES ENVIRONMENT "NEXAUI_TEST_FORCE_WARP=1")
    endif()
    add_executable(nexaui_renderer_backend_tests
        tests/test_renderer_backend.c src/renderer_backend.c)
    target_compile_definitions(nexaui_renderer_backend_tests PRIVATE
        NEXAUI_RENDER_INTERNAL_IMPL)
    target_include_directories(nexaui_renderer_backend_tests PRIVATE src)
    target_link_libraries(nexaui_renderer_backend_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_renderer_backend COMMAND nexaui_renderer_backend_tests)
    set_tests_properties(nexaui_renderer_backend PROPERTIES TIMEOUT 15)
    add_test(NAME nexaui_dependency_audit
        COMMAND ${CMAKE_COMMAND}
            -DNEXAUI_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DNEXAUI_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -DNEXAUI_GENERATOR=${CMAKE_GENERATOR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/dependency_audit/verify.cmake)
    set_tests_properties(nexaui_dependency_audit PROPERTIES TIMEOUT 60)
    add_executable(nexaui_snapshot_tests tests/test_snapshot.c)
    target_link_libraries(nexaui_snapshot_tests PRIVATE NexaUI::NexaUI)
    if(NOT WIN32)
        target_link_libraries(nexaui_snapshot_tests PRIVATE Threads::Threads)
    endif()
    add_test(NAME nexaui_snapshot COMMAND nexaui_snapshot_tests)
    set_tests_properties(nexaui_snapshot PROPERTIES TIMEOUT 30)
    add_executable(nexaui_sim_channel_tests tests/test_sim_channel.c)
    target_link_libraries(nexaui_sim_channel_tests PRIVATE NexaUI::NexaUI)
    if(NOT WIN32)
        target_link_libraries(nexaui_sim_channel_tests PRIVATE Threads::Threads)
    endif()
    add_test(NAME nexaui_sim_channel COMMAND nexaui_sim_channel_tests)
    set_tests_properties(nexaui_sim_channel PROPERTIES TIMEOUT 30)
    add_executable(nexaui_series_tests tests/test_series.c)
    target_link_libraries(nexaui_series_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_series COMMAND nexaui_series_tests)
    set_tests_properties(nexaui_series PROPERTIES TIMEOUT 15)
    add_executable(nexaui_plot_tests tests/test_plot.c)
    add_executable(nexaui_plot_multi_tests tests/test_plot_multi.c)
    target_link_libraries(nexaui_plot_multi_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_plot_multi COMMAND nexaui_plot_multi_tests)
    set_tests_properties(nexaui_plot_multi PROPERTIES TIMEOUT 20)
    target_link_libraries(nexaui_plot_tests PRIVATE NexaUI::NexaUI)
    target_compile_definitions(nexaui_plot_tests PRIVATE
        NEXAUI_PLOT_BASELINE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/baselines")
    add_test(NAME nexaui_plot COMMAND nexaui_plot_tests)
    set_tests_properties(nexaui_plot PROPERTIES TIMEOUT 15)
    add_executable(nexaui_telemetry_tests tests/test_telemetry.c)
    target_link_libraries(nexaui_telemetry_tests PRIVATE NexaUI::NexaUI)
    target_compile_definitions(nexaui_telemetry_tests PRIVATE
        NEXAUI_TELEMETRY_BASELINE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/baselines")
    add_test(NAME nexaui_telemetry COMMAND nexaui_telemetry_tests)
    set_tests_properties(nexaui_telemetry PROPERTIES TIMEOUT 30)
    add_executable(nexaui_property_tests tests/test_property.c)
    target_link_libraries(nexaui_property_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_property COMMAND nexaui_property_tests)
    set_tests_properties(nexaui_property PROPERTIES TIMEOUT 15)
    add_executable(nexaui_viewport_tests tests/test_viewport.c)
    target_link_libraries(nexaui_viewport_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_viewport COMMAND nexaui_viewport_tests)
    set_tests_properties(nexaui_viewport PROPERTIES TIMEOUT 15)
    add_executable(nexaui_tests tests/test_nexaui.c)
    target_link_libraries(nexaui_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_core COMMAND nexaui_tests)
    set_tests_properties(nexaui_core PROPERTIES TIMEOUT 15)
    add_executable(nexaui_layout_tests tests/test_layout.c)
    target_link_libraries(nexaui_layout_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_layout COMMAND nexaui_layout_tests)
    set_tests_properties(nexaui_layout PROPERTIES TIMEOUT 15)
    add_executable(nexaui_docking_tests tests/test_docking.c)
    target_link_libraries(nexaui_docking_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_docking COMMAND nexaui_docking_tests)
    set_tests_properties(nexaui_docking PROPERTIES TIMEOUT 15)
    add_executable(nexaui_theme_tests tests/test_theme.c)
    target_link_libraries(nexaui_theme_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_theme COMMAND nexaui_theme_tests)
    set_tests_properties(nexaui_theme PROPERTIES TIMEOUT 15)
    add_executable(nexaui_color_tests tests/test_color.c)
    target_link_libraries(nexaui_color_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_color COMMAND nexaui_color_tests)
    set_tests_properties(nexaui_color PROPERTIES TIMEOUT 15)
    add_executable(nexaui_animation_tests tests/test_animation.c)
    target_link_libraries(nexaui_animation_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_animation COMMAND nexaui_animation_tests)
    set_tests_properties(nexaui_animation PROPERTIES TIMEOUT 15)
    add_executable(nexaui_widget_tests tests/test_widget.c)
    target_link_libraries(nexaui_widget_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_widget COMMAND nexaui_widget_tests)
    set_tests_properties(nexaui_widget PROPERTIES TIMEOUT 15)
    add_executable(nexaui_display_widget_tests tests/test_display_widgets.c)
    target_link_libraries(nexaui_display_widget_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_display_widget COMMAND nexaui_display_widget_tests)
    set_tests_properties(nexaui_display_widget PROPERTIES TIMEOUT 15)
    add_executable(nexaui_control_tests tests/test_controls.c)
    target_link_libraries(nexaui_control_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_controls COMMAND nexaui_control_tests)
    set_tests_properties(nexaui_controls PROPERTIES TIMEOUT 15)
    add_executable(nexaui_cloud_component_tests tests/test_cloud_components.c)
    add_executable(nexaui_transfer_icon_tests tests/test_transfer_icon.c)
    target_link_libraries(nexaui_transfer_icon_tests PRIVATE NexaUI::NexaUI)
    if(UNIX AND NOT APPLE)
        target_link_libraries(nexaui_transfer_icon_tests PRIVATE m)
    endif()
    add_test(NAME nexaui_transfer_icon COMMAND nexaui_transfer_icon_tests)
    set_tests_properties(nexaui_transfer_icon PROPERTIES TIMEOUT 15)
    add_executable(nexaui_canvas_cache_tests tests/test_canvas_cache.c)
    target_link_libraries(nexaui_canvas_cache_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_canvas_cache COMMAND nexaui_canvas_cache_tests)
    set_tests_properties(nexaui_canvas_cache PROPERTIES TIMEOUT 15)
    add_executable(nexaui_paint_tests tests/test_paint.c)
    target_link_libraries(nexaui_paint_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_paint COMMAND nexaui_paint_tests)
    set_tests_properties(nexaui_paint PROPERTIES TIMEOUT 20)
    add_executable(nexaui_design_a_fallback_tests tests/test_design_a_fallback.c)
    target_link_libraries(nexaui_design_a_fallback_tests PRIVATE NexaUI::NexaUI)
    if(UNIX AND NOT APPLE)
        target_link_libraries(nexaui_design_a_fallback_tests PRIVATE m)
    endif()
    add_test(NAME nexaui_design_a_fallback COMMAND nexaui_design_a_fallback_tests)
    set_tests_properties(nexaui_design_a_fallback PROPERTIES TIMEOUT 20)
    target_link_libraries(nexaui_cloud_component_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_cloud_components COMMAND nexaui_cloud_component_tests)
    set_tests_properties(nexaui_cloud_components PROPERTIES TIMEOUT 15)
    add_executable(nexaui_text_stamp_tests tests/test_text_stamp.c)
    target_link_libraries(nexaui_text_stamp_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_text_stamp COMMAND nexaui_text_stamp_tests)
    set_tests_properties(nexaui_text_stamp PROPERTIES TIMEOUT 15)
    add_executable(nexaui_text_density_tests tests/test_text_density.c)
    target_link_libraries(nexaui_text_density_tests PRIVATE NexaUI::NexaUI)
    if(UNIX AND NOT APPLE)
        target_link_libraries(nexaui_text_density_tests PRIVATE m)
    endif()
    add_test(NAME nexaui_text_density COMMAND nexaui_text_density_tests)
    set_tests_properties(nexaui_text_density PROPERTIES TIMEOUT 20)
    add_executable(nexaui_context_text_density_tests tests/test_context_text_density.c)
    target_link_libraries(nexaui_context_text_density_tests PRIVATE NexaUI::NexaUI)
    if(UNIX AND NOT APPLE)
        target_link_libraries(nexaui_context_text_density_tests PRIVATE m)
    endif()
    add_test(NAME nexaui_context_text_density COMMAND nexaui_context_text_density_tests)
    set_tests_properties(nexaui_context_text_density PROPERTIES TIMEOUT 20)
    add_executable(nexaui_visual_regression_tests tests/test_visual_regression.c)
    target_link_libraries(nexaui_visual_regression_tests PRIVATE NexaUI::NexaUI)
    target_compile_definitions(nexaui_visual_regression_tests PRIVATE
        NEXAUI_VISUAL_BASELINE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/baselines")
    add_test(NAME nexaui_visual_regression COMMAND nexaui_visual_regression_tests)
    set_tests_properties(nexaui_visual_regression PROPERTIES TIMEOUT 15)
    add_executable(nexaui_container_tests tests/test_containers.c)
    target_link_libraries(nexaui_container_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_containers COMMAND nexaui_container_tests)
    set_tests_properties(nexaui_containers PROPERTIES TIMEOUT 15)
    add_executable(nexaui_scroll_virtual_tests tests/test_scroll_virtual.c)
    target_link_libraries(nexaui_scroll_virtual_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_scroll_virtual COMMAND nexaui_scroll_virtual_tests)
    set_tests_properties(nexaui_scroll_virtual PROPERTIES TIMEOUT 15)
    add_executable(nexaui_table_tests tests/test_table.c)
    target_link_libraries(nexaui_table_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_table COMMAND nexaui_table_tests)
    set_tests_properties(nexaui_table PROPERTIES TIMEOUT 15)
    add_executable(nexaui_tree_tests tests/test_tree.c)
    target_link_libraries(nexaui_tree_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_tree COMMAND nexaui_tree_tests)
    set_tests_properties(nexaui_tree PROPERTIES TIMEOUT 15)
    add_executable(nexaui_overlay_tests tests/test_overlay.c)
    target_link_libraries(nexaui_overlay_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_overlay COMMAND nexaui_overlay_tests)
    set_tests_properties(nexaui_overlay PROPERTIES TIMEOUT 15)
    add_executable(nexaui_menu_tests tests/test_menu.c)
    target_link_libraries(nexaui_menu_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_menu COMMAND nexaui_menu_tests)
    set_tests_properties(nexaui_menu PROPERTIES TIMEOUT 15)
    add_executable(nexaui_search_tests tests/test_search.c)
    target_link_libraries(nexaui_search_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_search COMMAND nexaui_search_tests)
    set_tests_properties(nexaui_search PROPERTIES TIMEOUT 15)
    add_executable(nexaui_value_control_tests tests/test_value_controls.c)
    target_link_libraries(nexaui_value_control_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_value_controls COMMAND nexaui_value_control_tests)
    set_tests_properties(nexaui_value_controls PROPERTIES TIMEOUT 15)
    add_executable(nexaui_numeric_tests tests/test_numeric.c)
    target_link_libraries(nexaui_numeric_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_numeric COMMAND nexaui_numeric_tests)
    set_tests_properties(nexaui_numeric PROPERTIES TIMEOUT 15)
    add_executable(nexaui_numeric_field_tests tests/test_numeric_field.c)
    target_link_libraries(nexaui_numeric_field_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_numeric_field COMMAND nexaui_numeric_field_tests)
    set_tests_properties(nexaui_numeric_field PROPERTIES TIMEOUT 15)
    add_executable(nexaui_text_field_tests tests/test_text_field.c)
    target_link_libraries(nexaui_text_field_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_text_field COMMAND nexaui_text_field_tests)
    set_tests_properties(nexaui_text_field PROPERTIES TIMEOUT 15)
    add_executable(nexaui_text_value_tests tests/test_text_value.c)
    target_link_libraries(nexaui_text_value_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_text_value COMMAND nexaui_text_value_tests)
    set_tests_properties(nexaui_text_value PROPERTIES TIMEOUT 15)
    add_executable(nexaui_text_pointer_tests tests/test_text_pointer.c)
    target_link_libraries(nexaui_text_pointer_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_text_pointer COMMAND nexaui_text_pointer_tests)
    set_tests_properties(nexaui_text_pointer PROPERTIES TIMEOUT 15)
    add_executable(nexaui_input_tests tests/test_input.c)
    target_link_libraries(nexaui_input_tests PRIVATE NexaUI::NexaUI)
    if(WIN32)
        target_link_libraries(nexaui_input_tests PRIVATE user32)
        add_executable(nexaui_uia_client tests/uia_client.c)
        target_link_libraries(nexaui_uia_client PRIVATE user32 ole32 oleaut32 uiautomationcore uuid)
        set_target_properties(nexaui_uia_client PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_executable(nexaui_uia_tests tests/test_uia.c)
        target_include_directories(nexaui_uia_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows)
        target_link_libraries(nexaui_uia_tests PRIVATE NexaUI::NexaUI user32)
        if(MSVC)
            target_compile_options(nexaui_uia_tests PRIVATE /W4)
            target_compile_options(nexaui_uia_client PRIVATE /W4)
        endif()
        add_dependencies(nexaui_uia_tests nexaui_uia_client)
        add_executable(nexaui_uia_value_client tests/uia_value_client.c)
        target_link_libraries(nexaui_uia_value_client PRIVATE user32 ole32 oleaut32 uiautomationcore uuid)
        target_compile_features(nexaui_uia_value_client PRIVATE c_std_17)
        add_executable(nexaui_uia_value_tests tests/test_uia_value.c)
        target_include_directories(nexaui_uia_value_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows)
        target_link_libraries(nexaui_uia_value_tests PRIVATE NexaUI::NexaUI user32)
        set_target_properties(nexaui_uia_value_client nexaui_uia_value_tests PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        if(MSVC)
            target_compile_options(nexaui_uia_value_client PRIVATE /W4)
            target_compile_options(nexaui_uia_value_tests PRIVATE /W4)
        endif()
        add_dependencies(nexaui_uia_value_tests nexaui_uia_value_client)
        add_test(NAME nexaui_uia_value COMMAND nexaui_uia_value_tests $<TARGET_FILE:nexaui_uia_value_client>)
        add_test(NAME nexaui_uia_value_mta COMMAND nexaui_uia_value_tests $<TARGET_FILE:nexaui_uia_value_client> mta)
        add_test(NAME nexaui_uia_value_owned_com COMMAND nexaui_uia_value_tests $<TARGET_FILE:nexaui_uia_value_client> owned)
        set_tests_properties(nexaui_uia_value nexaui_uia_value_mta nexaui_uia_value_owned_com PROPERTIES TIMEOUT 120 RESOURCE_LOCK native_display)
        add_executable(nexaui_uia_multi_client tests/uia_multi_client.c)
        target_link_libraries(nexaui_uia_multi_client PRIVATE user32 ole32 oleaut32 uiautomationcore uuid)
        add_executable(nexaui_uia_multi_tests tests/test_uia_multi.c)
        target_include_directories(nexaui_uia_multi_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows)
        target_link_libraries(nexaui_uia_multi_tests PRIVATE NexaUI::NexaUI user32)
        set_target_properties(nexaui_uia_multi_client nexaui_uia_multi_tests PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        if(MSVC)
            target_compile_options(nexaui_uia_multi_client PRIVATE /W4)
            target_compile_options(nexaui_uia_multi_tests PRIVATE /W4)
        endif()
        add_dependencies(nexaui_uia_multi_tests nexaui_uia_multi_client)
        add_test(NAME nexaui_uia_multi COMMAND nexaui_uia_multi_tests $<TARGET_FILE:nexaui_uia_multi_client>)
        add_test(NAME nexaui_uia_multi_mta COMMAND nexaui_uia_multi_tests $<TARGET_FILE:nexaui_uia_multi_client> mta)
        add_test(NAME nexaui_uia_multi_owned_com COMMAND nexaui_uia_multi_tests $<TARGET_FILE:nexaui_uia_multi_client> owned)
        set_tests_properties(nexaui_uia_multi nexaui_uia_multi_mta nexaui_uia_multi_owned_com PROPERTIES TIMEOUT 120 RESOURCE_LOCK native_display)
        add_test(NAME nexaui_uia COMMAND nexaui_uia_tests $<TARGET_FILE:nexaui_uia_client>)
        add_test(NAME nexaui_uia_mta COMMAND nexaui_uia_tests $<TARGET_FILE:nexaui_uia_client> mta)
        add_test(NAME nexaui_uia_owned_com COMMAND nexaui_uia_tests $<TARGET_FILE:nexaui_uia_client> owned)
        add_test(NAME nexaui_design_a_uia COMMAND nexaui_uia_tests $<TARGET_FILE:nexaui_uia_client> design)
        set_tests_properties(nexaui_design_a_uia PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
        set_tests_properties(nexaui_uia nexaui_uia_mta nexaui_uia_owned_com PROPERTIES TIMEOUT 120 RESOURCE_LOCK native_display)
        add_executable(nexaui_multi_window_tests tests/test_multi_window.c)
        add_executable(nexaui_windows_management_tests tests/test_windows_management.c)
        target_link_libraries(nexaui_windows_management_tests PRIVATE NexaUI::NexaUI user32)
        add_test(NAME nexaui_windows_management COMMAND nexaui_windows_management_tests)
        set_tests_properties(nexaui_windows_management PROPERTIES TIMEOUT 20 RESOURCE_LOCK native_display)
        target_link_libraries(nexaui_multi_window_tests PRIVATE NexaUI::NexaUI user32)
        add_test(NAME nexaui_multi_window COMMAND nexaui_multi_window_tests)
        set_tests_properties(nexaui_multi_window PROPERTIES TIMEOUT 15)
        add_executable(nexaui_clipboard_tests tests/test_clipboard.c)
        target_link_libraries(nexaui_clipboard_tests PRIVATE NexaUI::NexaUI user32)
        add_test(NAME nexaui_clipboard COMMAND nexaui_clipboard_tests)
        add_test(NAME nexaui_clipboard_current_station_guard
            COMMAND nexaui_clipboard_tests reject-current-station)
        add_test(NAME nexaui_clipboard_busy_guard
            COMMAND nexaui_clipboard_tests reject-busy)
        set_tests_properties(nexaui_clipboard nexaui_clipboard_busy_guard
            PROPERTIES TIMEOUT 15 RESOURCE_LOCK windows_clipboard
            SKIP_RETURN_CODE 77)
        set_tests_properties(nexaui_clipboard_current_station_guard PROPERTIES TIMEOUT 15)
        add_executable(nexaui_file_drop_tests tests/test_file_drop.c)
        target_include_directories(nexaui_file_drop_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows)
        target_link_libraries(nexaui_file_drop_tests PRIVATE NexaUI::NexaUI
            user32 shell32)
        add_test(NAME nexaui_file_drop COMMAND nexaui_file_drop_tests)
        set_tests_properties(nexaui_file_drop PROPERTIES TIMEOUT 15)
        add_executable(nexaui_cursor_tests tests/test_cursor.c)
        target_link_libraries(nexaui_cursor_tests PRIVATE NexaUI::NexaUI user32)
        add_test(NAME nexaui_cursor COMMAND nexaui_cursor_tests)
        set_tests_properties(nexaui_cursor PROPERTIES TIMEOUT 15)
        add_executable(nexaui_ime_tests tests/test_ime.c)
        target_include_directories(nexaui_ime_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows)
        target_link_libraries(nexaui_ime_tests PRIVATE NexaUI::NexaUI
            user32 imm32)
        add_test(NAME nexaui_ime COMMAND nexaui_ime_tests)
        set_tests_properties(nexaui_ime PROPERTIES TIMEOUT 15
            SKIP_RETURN_CODE 77)
    endif()
    add_test(NAME nexaui_input COMMAND nexaui_input_tests)
    set_tests_properties(nexaui_input PROPERTIES TIMEOUT 15)
    if(APPLE)
        add_executable(nexaui_macos_multiselection_tests tests/test_macos_multiselection.m)
        target_include_directories(nexaui_macos_multiselection_tests PRIVATE ${PROJECT_SOURCE_DIR}/platform/macos)
        target_link_libraries(nexaui_macos_multiselection_tests PRIVATE NexaUI::NexaUI "-framework AppKit" Threads::Threads)
        target_compile_options(nexaui_macos_multiselection_tests PRIVATE -fno-objc-arc -Wall -Wextra -Wpedantic)
        set_target_properties(nexaui_macos_multiselection_tests PROPERTIES OBJC_STANDARD 17 OBJC_STANDARD_REQUIRED YES OBJC_EXTENSIONS NO)
        add_test(NAME nexaui_macos_multiselection COMMAND nexaui_macos_multiselection_tests)
        set_tests_properties(nexaui_macos_multiselection PROPERTIES TIMEOUT 60 RESOURCE_LOCK native_display)
        option(NEXAUI_TEST_MACOS_AX_VISIBLE "Allow a visible external AX fixture on an isolated test desktop" OFF)
        add_executable(nexaui_macos_multiselection_host tests/macos_multiselection_host.m)
        target_include_directories(nexaui_macos_multiselection_host PRIVATE ${PROJECT_SOURCE_DIR}/platform/macos)
        target_link_libraries(nexaui_macos_multiselection_host PRIVATE NexaUI::NexaUI "-framework AppKit" Threads::Threads)
        target_compile_options(nexaui_macos_multiselection_host PRIVATE -fno-objc-arc -Wall -Wextra -Wpedantic)
        set_target_properties(nexaui_macos_multiselection_host PROPERTIES OBJC_STANDARD 17 OBJC_STANDARD_REQUIRED YES OBJC_EXTENSIONS NO)
        add_executable(nexaui_macos_multiselection_client tests/macos_multiselection_client.c)
        target_link_libraries(nexaui_macos_multiselection_client PRIVATE "-framework ApplicationServices" "-framework CoreFoundation")
        target_compile_options(nexaui_macos_multiselection_client PRIVATE -Wall -Wextra -Wpedantic)
        set_target_properties(nexaui_macos_multiselection_client PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_dependencies(nexaui_macos_multiselection_host nexaui_macos_multiselection_client)
        set(nexaui_macos_ax_visibility hidden)
        if(NEXAUI_TEST_MACOS_AX_VISIBLE)
            set(nexaui_macos_ax_visibility visible)
        endif()
        add_test(NAME nexaui_macos_multiselection_external COMMAND nexaui_macos_multiselection_host $<TARGET_FILE:nexaui_macos_multiselection_client> ${nexaui_macos_ax_visibility})
        set_tests_properties(nexaui_macos_multiselection_external PROPERTIES TIMEOUT 180 RESOURCE_LOCK native_display SKIP_RETURN_CODE 77)
        add_executable(nexaui_macos_accessibility_tests tests/test_macos_accessibility.m)
        target_include_directories(nexaui_macos_accessibility_tests PRIVATE ${PROJECT_SOURCE_DIR}/platform/macos)
        target_link_libraries(nexaui_macos_accessibility_tests PRIVATE NexaUI::NexaUI "-framework AppKit" Threads::Threads)
        target_compile_options(nexaui_macos_accessibility_tests PRIVATE -fno-objc-arc -Wall -Wextra -Wpedantic)
        set_target_properties(nexaui_macos_accessibility_tests PROPERTIES OBJC_STANDARD 17 OBJC_STANDARD_REQUIRED YES OBJC_EXTENSIONS NO)
        add_executable(nexaui_macos_ax_client tests/macos_ax_client.c)
        target_link_libraries(nexaui_macos_ax_client PRIVATE "-framework ApplicationServices" "-framework CoreFoundation")
        target_compile_options(nexaui_macos_ax_client PRIVATE -Wall -Wextra -Wpedantic)
        set_target_properties(nexaui_macos_ax_client PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_dependencies(nexaui_macos_accessibility_tests nexaui_macos_ax_client)
        add_test(NAME nexaui_macos_ax_external COMMAND nexaui_macos_accessibility_tests $<TARGET_FILE:nexaui_macos_ax_client>)
        set_tests_properties(nexaui_macos_ax_external PROPERTIES TIMEOUT 90 RESOURCE_LOCK native_display SKIP_RETURN_CODE 77)
        add_test(NAME nexaui_macos_accessibility COMMAND nexaui_macos_accessibility_tests)
        set_tests_properties(nexaui_macos_accessibility PROPERTIES TIMEOUT 90 RESOURCE_LOCK native_display)
        add_executable(nexaui_macos_window_tests tests/test_macos_window.m)
        target_include_directories(nexaui_macos_window_tests PRIVATE ${PROJECT_SOURCE_DIR}/platform/macos)
        target_link_libraries(nexaui_macos_window_tests PRIVATE NexaUI::NexaUI "-framework AppKit" Threads::Threads)
        target_compile_options(nexaui_macos_window_tests PRIVATE -fno-objc-arc -Wall -Wextra -Wpedantic)
        set_target_properties(nexaui_macos_window_tests PROPERTIES OBJC_STANDARD 17 OBJC_STANDARD_REQUIRED YES OBJC_EXTENSIONS NO)
        add_test(NAME nexaui_macos_window COMMAND nexaui_macos_window_tests)
        set_tests_properties(nexaui_macos_window PROPERTIES TIMEOUT 60 RESOURCE_LOCK native_display)
        if(TARGET nexaui_renderer_metal)
            target_compile_definitions(nexaui_renderer_metal PRIVATE NEXAUI_METAL_TEST_HOOKS)
            add_executable(nexaui_metal_tests tests/test_metal.m)
            target_include_directories(nexaui_metal_tests PRIVATE
                ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/platform/macos ${PROJECT_SOURCE_DIR}/renderers/metal)
            target_link_libraries(nexaui_metal_tests PRIVATE NexaUI::NexaUI
                nexaui_renderer_metal "-framework AppKit" Threads::Threads)
            target_compile_definitions(nexaui_metal_tests PRIVATE NEXAUI_METAL_TEST_HOOKS)
            target_compile_options(nexaui_metal_tests PRIVATE -fno-objc-arc -Wall -Wextra -Wpedantic)
            set_target_properties(nexaui_metal_tests PROPERTIES OBJC_STANDARD 17 OBJC_STANDARD_REQUIRED YES OBJC_EXTENSIONS NO)
            add_test(NAME nexaui_metal COMMAND nexaui_metal_tests)
            set_tests_properties(nexaui_metal PROPERTIES TIMEOUT 90 RESOURCE_LOCK native_display
                SKIP_RETURN_CODE 77 ENVIRONMENT "MTL_DEBUG_LAYER=1")
        endif()
    endif()
    add_executable(nexaui_interaction_tests tests/test_interaction.c)
    target_link_libraries(nexaui_interaction_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_interaction COMMAND nexaui_interaction_tests)
    set_tests_properties(nexaui_interaction PROPERTIES TIMEOUT 15)
    add_executable(nexaui_semantics_tests tests/test_semantics.c)
    target_link_libraries(nexaui_semantics_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_semantics COMMAND nexaui_semantics_tests)
    set_tests_properties(nexaui_semantics PROPERTIES TIMEOUT 15)
    add_executable(nexaui_selection_model_tests tests/test_selection_model.c)
    target_link_libraries(nexaui_selection_model_tests PRIVATE NexaUI::NexaUI)
    set_target_properties(nexaui_selection_model_tests PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    if(MSVC)
        target_compile_options(nexaui_selection_model_tests PRIVATE /W4)
    else()
        target_compile_options(nexaui_selection_model_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME nexaui_selection_model COMMAND nexaui_selection_model_tests)
    set_tests_properties(nexaui_selection_model PROPERTIES TIMEOUT 15)
    # Isolated private model component; no SDK link or test-only exported API.
    add_executable(nexaui_selection_replace_tests tests/test_selection_replace.c src/selection_model.c)
    target_include_directories(nexaui_selection_replace_tests PRIVATE include src)
    set_target_properties(nexaui_selection_replace_tests PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    if(MSVC)
        target_compile_options(nexaui_selection_replace_tests PRIVATE /W4)
    else()
        target_compile_options(nexaui_selection_replace_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME nexaui_selection_replace COMMAND nexaui_selection_replace_tests)
    set_tests_properties(nexaui_selection_replace PROPERTIES TIMEOUT 15)
    # Internal routing entry points stay private. Static-only direct core tests
    # complement the real native shared/static adapter tests.
    if(NOT BUILD_SHARED_LIBS)
        add_executable(nexaui_selection_set_tests tests/test_selection_set.c)
        target_include_directories(nexaui_selection_set_tests PRIVATE src)
        target_link_libraries(nexaui_selection_set_tests PRIVATE NexaUI::NexaUI)
        set_target_properties(nexaui_selection_set_tests PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        if(MSVC)
            target_compile_options(nexaui_selection_set_tests PRIVATE /W4)
        else()
            target_compile_options(nexaui_selection_set_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME nexaui_selection_set COMMAND nexaui_selection_set_tests)
        set_tests_properties(nexaui_selection_set PROPERTIES TIMEOUT 15)
    endif()
    add_executable(nexaui_multiselection_tests tests/test_multiselection.c)
    target_link_libraries(nexaui_multiselection_tests PRIVATE NexaUI::NexaUI)
    set_target_properties(nexaui_multiselection_tests PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    if(MSVC)
        target_compile_options(nexaui_multiselection_tests PRIVATE /W4)
    else()
        target_compile_options(nexaui_multiselection_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME nexaui_multiselection COMMAND nexaui_multiselection_tests)
    set_tests_properties(nexaui_multiselection PROPERTIES TIMEOUT 15)
    add_executable(nexaui_shapes_tests tests/test_shapes.c)
    target_link_libraries(nexaui_shapes_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_shapes COMMAND nexaui_shapes_tests)
    set_tests_properties(nexaui_shapes PROPERTIES TIMEOUT 15)
    add_executable(nexaui_headless_parity_tests tests/test_headless_parity.c
        tests/reference/headless_scalar.c)
    set_target_properties(nexaui_headless_parity_tests PROPERTIES
        C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    target_include_directories(nexaui_headless_parity_tests PRIVATE src)
    target_link_libraries(nexaui_headless_parity_tests PRIVATE NexaUI::NexaUI)
    if(MSVC)
        target_compile_options(nexaui_headless_parity_tests PRIVATE /W4)
        set_source_files_properties(tests/test_headless_parity.c PROPERTIES COMPILE_OPTIONS /WX)
    else()
        target_compile_options(nexaui_headless_parity_tests PRIVATE -Wall -Wextra -Wpedantic)
        set_source_files_properties(tests/test_headless_parity.c PROPERTIES COMPILE_OPTIONS -Werror)
    endif()
    add_test(NAME nexaui_headless_parity COMMAND nexaui_headless_parity_tests)
    set_tests_properties(nexaui_headless_parity PROPERTIES TIMEOUT 15)
    add_executable(nexaui_bitmap_tests tests/test_bitmap.c)
    target_link_libraries(nexaui_bitmap_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_bitmap COMMAND nexaui_bitmap_tests)
    set_tests_properties(nexaui_bitmap PROPERTIES TIMEOUT 15)
    add_executable(nexaui_font_tests tests/test_font.c)
    target_link_libraries(nexaui_font_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_font COMMAND nexaui_font_tests)
    set_tests_properties(nexaui_font PROPERTIES TIMEOUT 15)
    add_executable(nexaui_kern_tests tests/test_kern.c)
    target_link_libraries(nexaui_kern_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_kern COMMAND nexaui_kern_tests)
    set_tests_properties(nexaui_kern PROPERTIES TIMEOUT 15)
    add_executable(nexaui_shape_tests tests/test_shape.c)
    target_link_libraries(nexaui_shape_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_shape COMMAND nexaui_shape_tests)
    set_tests_properties(nexaui_shape PROPERTIES TIMEOUT 15)
    add_executable(nexaui_text_layout_tests tests/test_text_layout.c)
    target_link_libraries(nexaui_text_layout_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_text_layout COMMAND nexaui_text_layout_tests)
    set_tests_properties(nexaui_text_layout PROPERTIES TIMEOUT 15)
    add_executable(nexaui_text_layout_work_tests tests/test_text_layout_work.c)
    if(BUILD_SHARED_LIBS)
        # The in-test layout TU uses private UCD classifiers, not DLL exports.
        target_sources(nexaui_text_layout_work_tests PRIVATE src/unicode_data.c)
    endif()
    target_include_directories(nexaui_text_layout_work_tests PRIVATE src)
    target_link_libraries(nexaui_text_layout_work_tests PRIVATE NexaUI::NexaUI)
    if(MSVC)
        target_compile_options(nexaui_text_layout_work_tests PRIVATE /W4)
    else()
        target_compile_options(nexaui_text_layout_work_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME nexaui_text_layout_work COMMAND nexaui_text_layout_work_tests)
    set_tests_properties(nexaui_text_layout_work PROPERTIES TIMEOUT 15)
    add_executable(nexaui_text_buffer_tests tests/test_text_buffer.c)
    target_link_libraries(nexaui_text_buffer_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_text_buffer COMMAND nexaui_text_buffer_tests)
    set_tests_properties(nexaui_text_buffer PROPERTIES TIMEOUT 15)
    add_executable(nexaui_text_buffer_alloc_tests tests/test_text_buffer_alloc.c
        src/utf8.c src/grapheme.c src/word.c src/unicode_data.c)
    target_include_directories(nexaui_text_buffer_alloc_tests PRIVATE include src)
    target_compile_features(nexaui_text_buffer_alloc_tests PRIVATE c_std_17)
    add_test(NAME nexaui_text_buffer_alloc COMMAND nexaui_text_buffer_alloc_tests)
    set_tests_properties(nexaui_text_buffer_alloc PROPERTIES TIMEOUT 15)
    add_executable(nexaui_gsub_tests tests/test_gsub.c)
    add_executable(nexaui_gsub_context_tests tests/test_gsub_context.c)
    target_include_directories(nexaui_gsub_context_tests PRIVATE src)
    target_link_libraries(nexaui_gsub_context_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_gsub_context COMMAND nexaui_gsub_context_tests)
    set_tests_properties(nexaui_gsub_context PROPERTIES TIMEOUT 20)
    target_link_libraries(nexaui_gsub_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_gsub COMMAND nexaui_gsub_tests)
    set_tests_properties(nexaui_gsub PROPERTIES TIMEOUT 15)
    add_executable(nexaui_gpos_tests tests/test_gpos.c)
    add_executable(nexaui_gpos_mark_tests tests/test_gpos_marks.c)
    add_executable(nexaui_font_mark_probe tests/probe_font_marks.c)
    target_link_libraries(nexaui_font_mark_probe PRIVATE NexaUI::NexaUI)
    set_target_properties(nexaui_font_mark_probe PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    target_link_libraries(nexaui_gpos_mark_tests PRIVATE NexaUI::NexaUI)
    set_target_properties(nexaui_gpos_mark_tests PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    add_test(NAME nexaui_gpos_marks COMMAND nexaui_gpos_mark_tests)
    set_tests_properties(nexaui_gpos_marks PROPERTIES TIMEOUT 15)
    target_link_libraries(nexaui_gpos_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_gpos COMMAND nexaui_gpos_tests)
    set_tests_properties(nexaui_gpos PROPERTIES TIMEOUT 15)
    add_executable(nexaui_cmap_tests tests/test_cmap.c)
    target_link_libraries(nexaui_cmap_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_cmap COMMAND nexaui_cmap_tests)
    set_tests_properties(nexaui_cmap PROPERTIES TIMEOUT 15)
    add_executable(nexaui_glyf_tests tests/test_glyf.c)
    target_link_libraries(nexaui_glyf_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_glyf COMMAND nexaui_glyf_tests)
    set_tests_properties(nexaui_glyf PROPERTIES TIMEOUT 15)
    add_executable(nexaui_raster_tests tests/test_raster.c)
    target_link_libraries(nexaui_raster_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_raster COMMAND nexaui_raster_tests)
    set_tests_properties(nexaui_raster PROPERTIES TIMEOUT 15)
    add_executable(nexaui_glyph_cache_tests tests/test_glyph_cache.c)
    target_link_libraries(nexaui_glyph_cache_tests PRIVATE NexaUI::NexaUI)
    if(UNIX AND NOT APPLE)
        target_link_libraries(nexaui_glyph_cache_tests PRIVATE m)
    endif()
    add_test(NAME nexaui_glyph_cache COMMAND nexaui_glyph_cache_tests)
    set_tests_properties(nexaui_glyph_cache PROPERTIES TIMEOUT 15)
    add_executable(nexaui_font_collection_tests tests/test_font_collection.c)
    target_link_libraries(nexaui_font_collection_tests PRIVATE NexaUI::NexaUI)
    add_test(NAME nexaui_font_collection COMMAND nexaui_font_collection_tests)
    set_tests_properties(nexaui_font_collection PROPERTIES TIMEOUT 15)
    add_executable(nexaui_unicode_data_tests tests/test_unicode_data.c src/unicode_data.c)
    target_include_directories(nexaui_unicode_data_tests PRIVATE src)
    target_compile_features(nexaui_unicode_data_tests PRIVATE c_std_17)
    add_test(NAME nexaui_unicode_data COMMAND nexaui_unicode_data_tests)
    set_tests_properties(nexaui_unicode_data PROPERTIES TIMEOUT 15)
    add_executable(nexaui_unicode_lookup_tests tests/test_unicode_lookup.c)
    target_compile_features(nexaui_unicode_lookup_tests PRIVATE c_std_17)
    add_test(NAME nexaui_unicode_lookup COMMAND nexaui_unicode_lookup_tests)
    set_tests_properties(nexaui_unicode_lookup PROPERTIES TIMEOUT 30)
    add_executable(nexaui_grapheme_tests tests/test_grapheme.c)
    target_link_libraries(nexaui_grapheme_tests PRIVATE NexaUI::NexaUI)
    target_compile_definitions(nexaui_grapheme_tests PRIVATE
        NEXAUI_GRAPHEME_TEST_FILE="${CMAKE_CURRENT_SOURCE_DIR}/data/unicode/18.0.0/auxiliary/GraphemeBreakTest.txt")
    add_test(NAME nexaui_grapheme COMMAND nexaui_grapheme_tests)
    set_tests_properties(nexaui_grapheme PROPERTIES TIMEOUT 15)
    add_executable(nexaui_word_tests tests/test_word.c)
    target_link_libraries(nexaui_word_tests PRIVATE NexaUI::NexaUI)
    target_compile_definitions(nexaui_word_tests PRIVATE
        NEXAUI_WORD_TEST_FILE="${CMAKE_CURRENT_SOURCE_DIR}/data/unicode/18.0.0/auxiliary/WordBreakTest.txt")
    add_test(NAME nexaui_word COMMAND nexaui_word_tests)
    set_tests_properties(nexaui_word PROPERTIES TIMEOUT 15)
    add_executable(nexaui_line_tests tests/test_line.c)
    target_link_libraries(nexaui_line_tests PRIVATE NexaUI::NexaUI)
    target_compile_definitions(nexaui_line_tests PRIVATE
        NEXAUI_LINE_TEST_FILE="${CMAKE_CURRENT_SOURCE_DIR}/data/unicode/18.0.0/auxiliary/LineBreakTest.txt")
    add_test(NAME nexaui_line COMMAND nexaui_line_tests)
    set_tests_properties(nexaui_line PROPERTIES TIMEOUT 30)
    add_executable(nexaui_bidi_tests tests/test_bidi.c)
    target_link_libraries(nexaui_bidi_tests PRIVATE NexaUI::NexaUI)
    target_compile_definitions(nexaui_bidi_tests PRIVATE
        NEXAUI_BIDI_CHARACTER_TEST_FILE="${CMAKE_CURRENT_SOURCE_DIR}/data/unicode/18.0.0/BidiCharacterTest.txt")
    add_test(NAME nexaui_bidi COMMAND nexaui_bidi_tests)
    set_tests_properties(nexaui_bidi PROPERTIES TIMEOUT 60)
    add_executable(nexaui_bidi_type_tests tests/test_bidi_types.c)
    target_link_libraries(nexaui_bidi_type_tests PRIVATE NexaUI::NexaUI)
    target_compile_definitions(nexaui_bidi_type_tests PRIVATE
        NEXAUI_BIDI_TYPE_TEST_FILE="${CMAKE_CURRENT_SOURCE_DIR}/data/unicode/18.0.0/BidiTest.txt")
    add_test(NAME nexaui_bidi_types COMMAND nexaui_bidi_type_tests)
    set_tests_properties(nexaui_bidi_types PROPERTIES TIMEOUT 90)
    # Test sources use C math directly. A shared core keeps its own libm
    # dependency private, so executables must declare theirs explicitly.
    if(UNIX AND NOT APPLE)
        get_property(_nexaui_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
        foreach(_target IN LISTS _nexaui_test_targets)
            get_target_property(_type ${_target} TYPE)
            if(_type STREQUAL "EXECUTABLE")
                target_link_libraries(${_target} PRIVATE m)
            endif()
        endforeach()
    endif()
endif()

if(NEXAUI_BUILD_EXAMPLES)
    add_subdirectory(examples/00_headless)
    add_subdirectory(examples/02_layout_headless)
    add_subdirectory(examples/03_layout)
    add_subdirectory(examples/04_forms)
    add_subdirectory(examples/03_interactive_region)
    add_subdirectory(examples/04_font_inspect)
    add_subdirectory(examples/05_scroll)
    add_subdirectory(examples/06_tables)
    add_subdirectory(examples/05_glyph_raster)
    add_subdirectory(examples/06_font_fallback)
    add_subdirectory(examples/07_text_headless)
    add_subdirectory(examples/07_text_editing)
    add_subdirectory(examples/08_themes)
    add_subdirectory(examples/09_custom_widget)
    add_subdirectory(examples/09_inspector_headless)
    add_subdirectory(examples/10_docking_editor)
    add_subdirectory(examples/12_accessibility)
    add_subdirectory(examples/13_realtime_plot)
    add_subdirectory(examples/14_simulation_dashboard)
    add_subdirectory(examples/15_3d_viewport_shell)
    add_subdirectory(examples/16_text_density)
    add_subdirectory(examples/17_widget_text_density)
    add_subdirectory(examples/nexacloud)
    if(WIN32 OR UNIX)
        add_subdirectory(examples/00_hello_window)
    endif()
    if(WIN32)
        add_subdirectory(examples/01_hello_window)
        add_subdirectory(examples/01_hello_ui)
        add_subdirectory(examples/02_buttons_and_input)
        add_subdirectory(examples/showcase)
        add_subdirectory(examples/10_native_clear)
        add_subdirectory(examples/11_multi_window)
    endif()
    if(TARGET nexaui_renderer_vulkan OR TARGET nexaui_renderer_metal)
        add_subdirectory(examples/10_native_clear)
    endif()
endif()

if(NEXAUI_BUILD_BENCHMARKS)
    set(NEXAUI_TEXT_PROFILE_CORE "" CACHE FILEPATH "Diagnostic core from tools/prepare_text_cost_profile.py (static developer benchmark only)")
    set(NEXAUI_TEXT_LAYOUT_BASELINE_SOURCE "" CACHE FILEPATH "Optional same-ABI text_layout.c for paired text cost measurement")
    if(NEXAUI_TEXT_PROFILE_CORE)
        if(BUILD_SHARED_LIBS)
            message(FATAL_ERROR "Text cost instrumentation requires BUILD_SHARED_LIBS=OFF")
        endif()
        add_executable(nexaui_text_cost_bench benchmarks/text_cost_bench.c
            "${NEXAUI_TEXT_PROFILE_CORE}")
        target_include_directories(nexaui_text_cost_bench PRIVATE src platform/common benchmarks)
        target_link_libraries(nexaui_text_cost_bench PRIVATE NexaUI::NexaUI)
        target_compile_features(nexaui_text_cost_bench PRIVATE c_std_17)
        set_target_properties(nexaui_text_cost_bench PROPERTIES C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        if(NEXAUI_TEXT_LAYOUT_BASELINE_SOURCE)
            file(TO_CMAKE_PATH "${NEXAUI_TEXT_LAYOUT_BASELINE_SOURCE}" NEXAUI_TEXT_LAYOUT_BASELINE_NORMALIZED)
            target_sources(nexaui_text_cost_bench PRIVATE benchmarks/text_layout_baseline.c)
            target_compile_definitions(nexaui_text_cost_bench PRIVATE
                NEXAUI_TEXT_LAYOUT_BASELINE_SOURCE="${NEXAUI_TEXT_LAYOUT_BASELINE_NORMALIZED}")
        endif()
        if(MSVC)
            target_compile_options(nexaui_text_cost_bench PRIVATE /W4 /utf-8)
        else()
            target_compile_options(nexaui_text_cost_bench PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endif()
    add_executable(nexaui_paint_bench benchmarks/paint_bench.c)
    target_link_libraries(nexaui_paint_bench PRIVATE NexaUI::NexaUI)
    if(UNIX AND NOT APPLE)
        target_link_libraries(nexaui_paint_bench PRIVATE m)
    endif()
    target_compile_features(nexaui_paint_bench PRIVATE c_std_17)
    set(NEXAUI_PAINT_BASELINE "" CACHE FILEPATH "Recorded NexaUI paint.c for optional paired benchmark")
    if(NEXAUI_PAINT_BASELINE)
        file(TO_CMAKE_PATH "${NEXAUI_PAINT_BASELINE}" NEXAUI_PAINT_BASELINE_NORMALIZED)
        target_sources(nexaui_paint_bench PRIVATE benchmarks/paint_baseline.c)
        target_compile_definitions(nexaui_paint_bench PRIVATE
            NEXAUI_PAINT_BASELINE_PATH="${NEXAUI_PAINT_BASELINE_NORMALIZED}")
    endif()
    set(NEXAUI_CORE_BASELINE "" CACHE PATH "Exported paint/core baseline directory")
    if(NEXAUI_CORE_BASELINE)
        file(TO_CMAKE_PATH "${NEXAUI_CORE_BASELINE}" NEXAUI_CORE_BASELINE_NORMALIZED)
        target_sources(nexaui_paint_bench PRIVATE benchmarks/plot_baseline.c)
        target_include_directories(nexaui_paint_bench PRIVATE src platform/common)
        target_compile_definitions(nexaui_paint_bench PRIVATE
            NEXAUI_CORE_BASELINE_PREFIX="${NEXAUI_CORE_BASELINE_NORMALIZED}/core_prefix.h"
            NEXAUI_CORE_BASELINE_PATH="${NEXAUI_CORE_BASELINE_NORMALIZED}/nexaui.c")
    endif()
    add_executable(nexaui_text_stress_bench benchmarks/text_stress_bench.c)
    target_link_libraries(nexaui_text_stress_bench PRIVATE NexaUI::NexaUI)
    add_executable(nexaui_layout_bench benchmarks/layout_bench.c)
    target_link_libraries(nexaui_layout_bench PRIVATE NexaUI::NexaUI)
    add_executable(nexaui_interaction_bench benchmarks/interaction_bench.c)
    target_link_libraries(nexaui_interaction_bench PRIVATE NexaUI::NexaUI)
    add_executable(nexaui_series_bench benchmarks/series_bench.c)
    target_link_libraries(nexaui_series_bench PRIVATE NexaUI::NexaUI)
    if(WIN32)
        add_executable(nexaui_d3d12_bench benchmarks/d3d12_bench.c)
        target_include_directories(nexaui_d3d12_bench PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/renderers/d3d12
            ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(nexaui_d3d12_bench PRIVATE
            NexaUI::NexaUI nexaui_renderer_d3d12)
        if(TARGET nexacloud_scene)
            add_executable(nexaui_d3d12_startup_bench benchmarks/d3d12_startup.c)
            target_include_directories(nexaui_d3d12_startup_bench PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/renderers/d3d12
                ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows
                ${CMAKE_CURRENT_SOURCE_DIR}/src
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/nexacloud)
            target_compile_features(nexaui_d3d12_startup_bench PRIVATE c_std_17)
            target_link_libraries(nexaui_d3d12_startup_bench PRIVATE
                NexaUI::NexaUI nexacloud_scene d3d12 d3dcompiler dxgi dxguid dwmapi)
            set(NEXAUI_D3D12_STARTUP_RENDERER_SOURCE "" CACHE FILEPATH
                "Historical D3D12 source for the startup diagnostic only; empty uses the current backend")
            if(NEXAUI_D3D12_STARTUP_RENDERER_SOURCE)
                file(TO_CMAKE_PATH "${NEXAUI_D3D12_STARTUP_RENDERER_SOURCE}" _startup_renderer)
                target_compile_definitions(nexaui_d3d12_startup_bench PRIVATE
                    "NEXAUI_D3D12_STARTUP_RENDERER_SOURCE=\"${_startup_renderer}\"")
            endif()
        endif()
    endif()
endif()

install(TARGETS nexaui EXPORT NexaUITargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/nexaui DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES data/unicode/18.0.0/license.txt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/NexaUI RENAME UNICODE-LICENSE.txt)
install(EXPORT NexaUITargets NAMESPACE NexaUI:: FILE NexaUITargets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NexaUI)
configure_package_config_file(cmake/NexaUIConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/NexaUIConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NexaUI)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/NexaUIConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/NexaUIConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/NexaUIConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NexaUI)
