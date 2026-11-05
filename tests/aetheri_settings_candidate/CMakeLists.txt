cmake_minimum_required(VERSION 3.21)
project(NexaUIAetheriOwnedSettingsCandidate LANGUAGES C)

# Isolated compatibility probe, not Aetheri's production CMake configuration.
find_package(NexaUI 0.1.19 EXACT CONFIG REQUIRED)
set(AETHERI_SOURCE_DIR "" CACHE PATH "Immutable Aetheri source snapshot")
set(AETHERI_SETTINGS_TEST_FONT "" CACHE FILEPATH "Font for the original Aetheri test")
foreach(path src/settings/model.c src/settings/model.h src/settings/nexaui_view.c
             src/settings/nexaui_view.h src/settings/nexaui_main.c tests/test_settings_view.c)
    if(NOT EXISTS "${AETHERI_SOURCE_DIR}/${path}")
        message(FATAL_ERROR "Missing ${path} in the selected Aetheri snapshot")
    endif()
endforeach()
if(NOT EXISTS "${AETHERI_SETTINGS_TEST_FONT}")
    message(FATAL_ERROR "Select a readable Aetheri Settings test font")
endif()

add_executable(aetheri_settings_candidate
    "${AETHERI_SOURCE_DIR}/src/settings/model.c"
    "${AETHERI_SOURCE_DIR}/src/settings/nexaui_view.c"
    "${AETHERI_SOURCE_DIR}/tests/test_settings_view.c")
target_include_directories(aetheri_settings_candidate PRIVATE "${AETHERI_SOURCE_DIR}/src")
target_link_libraries(aetheri_settings_candidate PRIVATE NexaUI::NexaUI m)
target_compile_options(aetheri_settings_candidate PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
set_target_properties(aetheri_settings_candidate PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_executable(aetheri_settings_candidate_native
    "${AETHERI_SOURCE_DIR}/src/settings/model.c"
    "${AETHERI_SOURCE_DIR}/src/settings/nexaui_view.c"
    "${AETHERI_SOURCE_DIR}/src/settings/nexaui_main.c")
target_include_directories(aetheri_settings_candidate_native PRIVATE "${AETHERI_SOURCE_DIR}/src")
target_link_libraries(aetheri_settings_candidate_native PRIVATE NexaUI::NexaUI m)
target_compile_options(aetheri_settings_candidate_native PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
set_target_properties(aetheri_settings_candidate_native PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ATSPI QUIET IMPORTED_TARGET atspi-2 gobject-2.0)
    if(ATSPI_FOUND)
        add_executable(aetheri_settings_candidate_client atspi_client.c)
        target_link_libraries(aetheri_settings_candidate_client PRIVATE PkgConfig::ATSPI)
        target_compile_options(aetheri_settings_candidate_client PRIVATE -Wall -Wextra -Wpedantic)
        set_target_properties(aetheri_settings_candidate_client PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_executable(aetheri_settings_candidate_focus atspi_focus.c)
        target_link_libraries(aetheri_settings_candidate_focus PRIVATE PkgConfig::ATSPI)
        target_compile_options(aetheri_settings_candidate_focus PRIVATE -Wall -Wextra -Wpedantic)
        set_target_properties(aetheri_settings_candidate_focus PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    endif()
endif()
enable_testing()
add_test(NAME aetheri_settings_candidate_density_1
    COMMAND aetheri_settings_candidate "${AETHERI_SETTINGS_TEST_FONT}" 1)
add_test(NAME aetheri_settings_candidate_density_2
    COMMAND aetheri_settings_candidate "${AETHERI_SETTINGS_TEST_FONT}" 2)
set_tests_properties(aetheri_settings_candidate_density_1 aetheri_settings_candidate_density_2 PROPERTIES TIMEOUT 180)
