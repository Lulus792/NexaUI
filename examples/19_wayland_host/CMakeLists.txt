cmake_minimum_required(VERSION 3.21)
project(NexaUIWaylandHostExample LANGUAGES C)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "This example requires Linux Wayland")
endif()
find_package(NexaUI CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
set(XDG_XML "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h"
    COMMAND "${WAYLAND_SCANNER}" client-header "${XDG_XML}"
            "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h"
    DEPENDS "${XDG_XML}" VERBATIM)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c"
    COMMAND "${WAYLAND_SCANNER}" private-code "${XDG_XML}"
            "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c"
    DEPENDS "${XDG_XML}" VERBATIM)
add_executable(nexaui_wayland_host_example main.c
    "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h"
    "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c")
target_include_directories(nexaui_wayland_host_example PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(nexaui_wayland_host_example PRIVATE c_std_17)
target_link_libraries(nexaui_wayland_host_example PRIVATE NexaUI::NexaUI PkgConfig::WAYLAND_CLIENT)
install(TARGETS nexaui_wayland_host_example RUNTIME DESTINATION bin)
