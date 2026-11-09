cmake_minimum_required(VERSION 3.24)
project(OriginalEditorIndependentReader LANGUAGES C)
find_package(PkgConfig REQUIRED)
set(NEXAUI_TEST_ATSPI_PREFIX "" CACHE PATH "Reviewed OS development prefix; independent reader test only")
add_executable(editor_document_client ../native_document_client.c)
if(NEXAUI_TEST_ATSPI_PREFIX)
    target_include_directories(editor_document_client PRIVATE
        "${NEXAUI_TEST_ATSPI_PREFIX}/include/at-spi-2.0"
        "${NEXAUI_TEST_ATSPI_PREFIX}/include/glib-2.0"
        "${NEXAUI_TEST_ATSPI_PREFIX}/lib64/glib-2.0/include"
        "${NEXAUI_TEST_ATSPI_PREFIX}/include/dbus-1.0"
        "${NEXAUI_TEST_ATSPI_PREFIX}/lib64/dbus-1.0/include")
    foreach(name atspi gobject-2.0 glib-2.0 dbus-1)
        find_library(reader_${name} NAMES ${name} PATHS "${NEXAUI_TEST_ATSPI_PREFIX}/lib64" NO_DEFAULT_PATH)
        if(NOT reader_${name})
            find_file(reader_${name} NAMES lib${name}.so.0 lib${name}.so.3
                PATHS "${NEXAUI_TEST_ATSPI_PREFIX}/lib64" /usr/lib64 /usr/lib NO_DEFAULT_PATH REQUIRED)
        endif()
        target_link_libraries(editor_document_client PRIVATE "${reader_${name}}")
    endforeach()
else()
    pkg_check_modules(ATSPI REQUIRED IMPORTED_TARGET atspi-2 dbus-1)
    target_link_libraries(editor_document_client PRIVATE PkgConfig::ATSPI)
endif()
set_target_properties(editor_document_client PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_compile_options(editor_document_client PRIVATE -Wall -Wextra -Wpedantic -Werror)
