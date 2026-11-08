cmake_minimum_required(VERSION 3.24)
project(SoftwareOutputProducer LANGUAGES C)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
# Reuse the source-checked Files producer and its exact system dependencies.
# All additions are confined to this standalone test executable.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../35_aetheri_files_consumer/ime_producer" base)
set(output_generated "${CMAKE_CURRENT_BINARY_DIR}/output-generated")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py"
    "${AETHERI_PRODUCER_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/base/generated"
    "${output_generated}" COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py" "${CMAKE_CURRENT_SOURCE_DIR}/commands.h")
get_target_property(producer_sources files_ime_producer SOURCES)
list(REMOVE_ITEM producer_sources "${AETHERI_PRODUCER_SOURCE_DIR}/src/compositor/main.c"
    "${CMAKE_CURRENT_BINARY_DIR}/base/generated/compositor_input_driver.c")
set_property(TARGET files_ime_producer PROPERTY SOURCES "${producer_sources}")
target_sources(files_ime_producer PRIVATE "${output_generated}/main.c" "${output_generated}/compositor_input_driver.c")
target_include_directories(files_ime_producer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${AETHERI_PRODUCER_SOURCE_DIR}/src/compositor")
set_target_properties(files_ime_producer PROPERTIES OUTPUT_NAME software_output_producer)
