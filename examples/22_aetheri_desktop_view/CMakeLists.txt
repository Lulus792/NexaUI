cmake_minimum_required(VERSION 3.21)
project(NexaUIAetheriDesktopView LANGUAGES C)
find_package(NexaUI 0.1.14 CONFIG REQUIRED)
set(AETHERI_SOURCE_DIR "" CACHE PATH "Aetheri source snapshot")
if(NOT EXISTS "${AETHERI_SOURCE_DIR}/src/desktop/model.c" OR NOT EXISTS "${AETHERI_SOURCE_DIR}/src/desktop/model.h")
    message(FATAL_ERROR "Set AETHERI_SOURCE_DIR to an Aetheri source snapshot")
endif()
add_library(aetheri_desktop_model STATIC "${AETHERI_SOURCE_DIR}/src/desktop/model.c")
target_include_directories(aetheri_desktop_model PUBLIC "${AETHERI_SOURCE_DIR}/src/desktop")
add_library(aetheri_desktop_view STATIC view.c)
target_link_libraries(aetheri_desktop_view PUBLIC NexaUI::NexaUI aetheri_desktop_model)
add_executable(aetheri_desktop_view_test test_view.c)
target_link_libraries(aetheri_desktop_view_test PRIVATE aetheri_desktop_view)
add_executable(aetheri_desktop_ime_transaction_test test_ime_transaction.c ime_transaction.c)
target_link_libraries(aetheri_desktop_ime_transaction_test PRIVATE NexaUI::NexaUI)
set_target_properties(aetheri_desktop_ime_transaction_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(aetheri_desktop_ax_host_test main_ax_host_test.c)
    target_link_libraries(aetheri_desktop_ax_host_test PRIVATE aetheri_desktop_view)
    set_target_properties(aetheri_desktop_ax_host_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    add_executable(aetheri_desktop_preview main_linux.c)
    target_link_libraries(aetheri_desktop_preview PRIVATE aetheri_desktop_view)
    set_target_properties(aetheri_desktop_preview PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(ATSPI QUIET IMPORTED_TARGET atspi-2 gobject-2.0)
        if(ATSPI_FOUND)
            add_executable(aetheri_desktop_atspi_client atspi_client.c)
            target_link_libraries(aetheri_desktop_atspi_client PRIVATE PkgConfig::ATSPI)
            set_target_properties(aetheri_desktop_atspi_client PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        endif()
        pkg_check_modules(WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
        pkg_check_modules(XKBCOMMON QUIET IMPORTED_TARGET xkbcommon)
        pkg_check_modules(WAYLAND_PROTOCOLS QUIET wayland-protocols)
        find_program(WAYLAND_SCANNER wayland-scanner)
        find_package(Threads QUIET)
        if(WAYLAND_CLIENT_FOUND AND WAYLAND_PROTOCOLS_FOUND AND WAYLAND_SCANNER AND Threads_FOUND)
            pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
            set(XDG_XML "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
            add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h"
                COMMAND "${WAYLAND_SCANNER}" client-header "${XDG_XML}" "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h"
                DEPENDS "${XDG_XML}" VERBATIM)
            add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c"
                COMMAND "${WAYLAND_SCANNER}" private-code "${XDG_XML}" "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c"
                DEPENDS "${XDG_XML}" VERBATIM)
            add_library(aetheri_desktop_xdg_protocol STATIC
                "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h"
                "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c")
            target_include_directories(aetheri_desktop_xdg_protocol PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
            target_link_libraries(aetheri_desktop_xdg_protocol PUBLIC PkgConfig::WAYLAND_CLIENT)
            add_executable(aetheri_desktop_borrowed main_borrowed.c)
            target_include_directories(aetheri_desktop_borrowed PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
            target_link_libraries(aetheri_desktop_borrowed PRIVATE aetheri_desktop_view aetheri_desktop_xdg_protocol Threads::Threads)
            set_target_properties(aetheri_desktop_borrowed PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
            if(XKBCOMMON_FOUND AND EXISTS "${AETHERI_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml" AND
               EXISTS "${AETHERI_SOURCE_DIR}/protocols/aetheri-development-desktop-v1.xml")
                set(TEXT_INPUT_XML "${WAYLAND_PROTOCOLS_DIR}/unstable/text-input/text-input-unstable-v3.xml")
                add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/text-input-v3-client.h"
                    COMMAND "${WAYLAND_SCANNER}" client-header "${TEXT_INPUT_XML}" "${CMAKE_CURRENT_BINARY_DIR}/text-input-v3-client.h"
                    DEPENDS "${TEXT_INPUT_XML}" VERBATIM)
                add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/text-input-v3.c"
                    COMMAND "${WAYLAND_SCANNER}" private-code "${TEXT_INPUT_XML}" "${CMAKE_CURRENT_BINARY_DIR}/text-input-v3.c"
                    DEPENDS "${TEXT_INPUT_XML}" VERBATIM)
                foreach(protocol wlr-layer-shell-unstable-v1 aetheri-development-desktop-v1)
                    set(xml "${AETHERI_SOURCE_DIR}/protocols/${protocol}.xml")
                    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${protocol}-client.h"
                        COMMAND "${WAYLAND_SCANNER}" client-header "${xml}" "${CMAKE_CURRENT_BINARY_DIR}/${protocol}-client.h"
                        DEPENDS "${xml}" VERBATIM)
                    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${protocol}.c"
                        COMMAND "${WAYLAND_SCANNER}" private-code "${xml}" "${CMAKE_CURRENT_BINARY_DIR}/${protocol}.c"
                        DEPENDS "${xml}" VERBATIM)
                endforeach()
                add_executable(aetheri_desktop_layer_test main_layer_test.c ime_transaction.c
                    "${CMAKE_CURRENT_BINARY_DIR}/text-input-v3-client.h"
                    "${CMAKE_CURRENT_BINARY_DIR}/text-input-v3.c"
                    "${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-client.h"
                    "${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1.c"
                    "${CMAKE_CURRENT_BINARY_DIR}/aetheri-development-desktop-v1-client.h"
                    "${CMAKE_CURRENT_BINARY_DIR}/aetheri-development-desktop-v1.c")
                target_include_directories(aetheri_desktop_layer_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
                target_link_libraries(aetheri_desktop_layer_test PRIVATE aetheri_desktop_view aetheri_desktop_xdg_protocol PkgConfig::XKBCOMMON)
                set_target_properties(aetheri_desktop_layer_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
                add_executable(aetheri_desktop_layer_focus_test test_layer_focus.c ime_transaction.c
                    "${CMAKE_CURRENT_BINARY_DIR}/text-input-v3-client.h"
                    "${CMAKE_CURRENT_BINARY_DIR}/text-input-v3.c"
                    "${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-client.h"
                    "${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1.c"
                    "${CMAKE_CURRENT_BINARY_DIR}/aetheri-development-desktop-v1-client.h"
                    "${CMAKE_CURRENT_BINARY_DIR}/aetheri-development-desktop-v1.c")
                target_include_directories(aetheri_desktop_layer_focus_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
                target_link_libraries(aetheri_desktop_layer_focus_test PRIVATE aetheri_desktop_view aetheri_desktop_xdg_protocol PkgConfig::XKBCOMMON)
                set_target_properties(aetheri_desktop_layer_focus_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
                pkg_check_modules(WAYLAND_SERVER QUIET IMPORTED_TARGET wayland-server)
                if(WAYLAND_SERVER_FOUND)
                    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/text-input-v3-server.h"
                        COMMAND "${WAYLAND_SCANNER}" server-header "${TEXT_INPUT_XML}" "${CMAKE_CURRENT_BINARY_DIR}/text-input-v3-server.h"
                        DEPENDS "${TEXT_INPUT_XML}" VERBATIM)
                    add_executable(aetheri_desktop_layer_ime_wire_test test_layer_ime_wire.c ime_transaction.c
                        "${CMAKE_CURRENT_BINARY_DIR}/text-input-v3-client.h" "${CMAKE_CURRENT_BINARY_DIR}/text-input-v3-server.h"
                        "${CMAKE_CURRENT_BINARY_DIR}/text-input-v3.c"
                        "${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-client.h"
                        "${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1.c"
                        "${CMAKE_CURRENT_BINARY_DIR}/aetheri-development-desktop-v1-client.h"
                        "${CMAKE_CURRENT_BINARY_DIR}/aetheri-development-desktop-v1.c")
                    target_include_directories(aetheri_desktop_layer_ime_wire_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
                    target_link_libraries(aetheri_desktop_layer_ime_wire_test PRIVATE aetheri_desktop_view aetheri_desktop_xdg_protocol PkgConfig::XKBCOMMON PkgConfig::WAYLAND_SERVER Threads::Threads)
                    set_target_properties(aetheri_desktop_layer_ime_wire_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
                endif()
            endif()
        endif()
    endif()
endif()
foreach(target aetheri_desktop_model aetheri_desktop_view aetheri_desktop_view_test)
    set_target_properties(${target} PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
endforeach()
