cmake_minimum_required(VERSION 3.24)
project(FilesIMEProducer LANGUAGES C)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The private producer targets Linux")
endif()
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WLR REQUIRED IMPORTED_TARGET wlroots-0.20=0.20.2)
pkg_check_modules(WS REQUIRED IMPORTED_TARGET wayland-server=1.26.0)
pkg_check_modules(WC REQUIRED IMPORTED_TARGET wayland-client=1.26.0)
pkg_check_modules(PIXMAN REQUIRED IMPORTED_TARGET pixman-1=0.46.2)
pkg_check_modules(DRM REQUIRED IMPORTED_TARGET libdrm=2.4.134)
pkg_check_modules(XKB REQUIRED IMPORTED_TARGET xkbcommon=1.13.1)
pkg_check_modules(PROTOCOLS REQUIRED wayland-protocols=1.49)
pkg_get_variable(protocols_dir wayland-protocols pkgdatadir)
find_program(scanner wayland-scanner REQUIRED)
set(AETHERI_PRODUCER_SOURCE_DIR "" CACHE PATH "Separate checked frozen d56 export")
set(original "${AETHERI_PRODUCER_SOURCE_DIR}")
set(generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py" "${CMAKE_CURRENT_SOURCE_DIR}/../check_source.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ime-producer-inputs.json")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../ime-producer-inputs.json" manifest)
string(JSON input_count LENGTH "${manifest}" files)
math(EXPR input_last "${input_count}-1")
foreach(index RANGE ${input_last})
    string(JSON path GET "${manifest}" files ${index} path)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${original}/${path}")
endforeach()
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py"
    "${original}" "${generated}" COMMAND_ERROR_IS_FATAL ANY)
file(MAKE_DIRECTORY "${generated}")
set(desktop "${original}/protocols/aetheri-development-desktop-v1.xml")
set(layer "${original}/protocols/wlr-layer-shell-unstable-v1.xml")
set(xdg "${protocols_dir}/stable/xdg-shell/xdg-shell.xml")
add_custom_command(OUTPUT "${generated}/aetheri-development-desktop-v1-server.h"
    "${generated}/aetheri-development-desktop-v1.c" "${generated}/wlr-layer-shell-unstable-v1-protocol.h"
    "${generated}/xdg-shell-protocol.h"
    COMMAND "${scanner}" server-header "${desktop}" "${generated}/aetheri-development-desktop-v1-server.h"
    COMMAND "${scanner}" private-code "${desktop}" "${generated}/aetheri-development-desktop-v1.c"
    COMMAND "${scanner}" server-header "${layer}" "${generated}/wlr-layer-shell-unstable-v1-protocol.h"
    COMMAND "${scanner}" server-header "${xdg}" "${generated}/xdg-shell-protocol.h"
    DEPENDS "${desktop}" "${layer}" "${xdg}" VERBATIM)
add_executable(files_ime_producer
    "${original}/src/compositor/main.c" "${original}/src/compositor/input.c" "${original}/src/compositor/desktop.c"
    "${generated}/compositor_input_driver.c" "${generated}/compositor_text_input.c"
    "${generated}/aetheri-development-desktop-v1.c"
    "${generated}/aetheri-development-desktop-v1-server.h" "${generated}/wlr-layer-shell-unstable-v1-protocol.h"
    "${generated}/xdg-shell-protocol.h")
set_target_properties(files_ime_producer PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_include_directories(files_ime_producer PRIVATE "${original}/src" "${original}/tests" "${generated}")
target_compile_definitions(files_ime_producer PRIVATE _GNU_SOURCE WLR_USE_UNSTABLE AETHERI_INPUT_TEST)
target_compile_options(files_ime_producer PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
target_link_libraries(files_ime_producer PRIVATE PkgConfig::WLR PkgConfig::WS PkgConfig::WC PkgConfig::PIXMAN PkgConfig::DRM PkgConfig::XKB m)
