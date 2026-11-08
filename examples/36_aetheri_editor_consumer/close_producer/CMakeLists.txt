cmake_minimum_required(VERSION 3.24)
project(EditorCloseProducer LANGUAGES C)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
set(AETHERI_EDITOR_SOURCE_DIR "" CACHE PATH "Checked immutable original Editor export")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../35_aetheri_files_consumer/check_source.py"
    "${AETHERI_EDITOR_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../source-inputs.json" COMMAND_ERROR_IS_FATAL ANY)
# The checked producer stays a disposable test process with an inherited socket.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../34_aetheri_software/output_producer" base)
set(editor_generated "${CMAKE_CURRENT_BINARY_DIR}/editor-generated")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py"
    "${CMAKE_CURRENT_BINARY_DIR}/base/output-generated/compositor_input_driver.c"
    "${editor_generated}" "${AETHERI_EDITOR_SOURCE_DIR}/tests/compositor_input_driver.c" COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py")
get_target_property(sources files_ime_producer SOURCES)
list(REMOVE_ITEM sources "${CMAKE_CURRENT_BINARY_DIR}/base/output-generated/compositor_input_driver.c")
set_property(TARGET files_ime_producer PROPERTY SOURCES "${sources}")
target_sources(files_ime_producer PRIVATE "${editor_generated}/compositor_input_driver.c")
set_target_properties(files_ime_producer PROPERTIES OUTPUT_NAME editor_close_producer)
