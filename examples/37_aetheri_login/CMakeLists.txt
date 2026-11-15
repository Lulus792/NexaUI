cmake_minimum_required(VERSION 3.21)
project(NexaUIAetheriLogin LANGUAGES C)

find_package(NexaUI 0.1.33 CONFIG REQUIRED)
set(AETHERI_SOURCE_DIR "" CACHE PATH "Frozen Aetheri source export")
foreach(path src/login/protocol.c src/login/protocol.h src/text/grapheme.c
             src/text/grapheme.h src/text/grapheme_data.h)
    if(NOT EXISTS "${AETHERI_SOURCE_DIR}/${path}")
        message(FATAL_ERROR "Missing original Aetheri source: ${path}")
    endif()
endforeach()

add_library(aetheri_login_view STATIC view.c
    "${AETHERI_SOURCE_DIR}/src/login/protocol.c"
    "${AETHERI_SOURCE_DIR}/src/text/grapheme.c")
target_include_directories(aetheri_login_view PUBLIC "${AETHERI_SOURCE_DIR}/src")
target_link_libraries(aetheri_login_view PUBLIC NexaUI::NexaUI)
set_target_properties(aetheri_login_view PROPERTIES
    C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)

include(CTest)
if(BUILD_TESTING)
    add_executable(aetheri_login_view_test test_view.c)
    target_include_directories(aetheri_login_view_test PRIVATE ../../tests)
    target_link_libraries(aetheri_login_view_test PRIVATE aetheri_login_view)
    set_target_properties(aetheri_login_view_test PROPERTIES
        C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    add_test(NAME aetheri_login_view COMMAND aetheri_login_view_test)
    set_tests_properties(aetheri_login_view PROPERTIES TIMEOUT 15)
endif()

option(NEXAUI_LOGIN_BUILD_NATIVE_FIXTURE "Build the private Linux Wayland login fixture" OFF)
if(NEXAUI_LOGIN_BUILD_NATIVE_FIXTURE)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "The login fixture requires Linux Wayland and AT-SPI")
    endif()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ATSPI REQUIRED IMPORTED_TARGET atspi-2 gobject-2.0)
    add_executable(aetheri_login_native_host native_host.c)
    target_link_libraries(aetheri_login_native_host PRIVATE aetheri_login_view)
    add_executable(aetheri_login_atspi_peer atspi_peer.c)
    target_link_libraries(aetheri_login_atspi_peer PRIVATE PkgConfig::ATSPI)
    set_target_properties(aetheri_login_native_host aetheri_login_atspi_peer PROPERTIES
        C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
endif()
