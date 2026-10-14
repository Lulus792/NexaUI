add_executable(nexaui_docking_editor main.c)
target_link_libraries(nexaui_docking_editor PRIVATE NexaUI::NexaUI)
if(UNIX AND NOT APPLE)
    target_link_libraries(nexaui_docking_editor PRIVATE m)
endif()
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET nexaui_docking_editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui>
            $<TARGET_FILE_DIR:nexaui_docking_editor>
        VERBATIM)
endif()

include("${PROJECT_SOURCE_DIR}/cmake/NexaUIExampleFont.cmake")
nexaui_find_example_font(_nexaui_example_font)
if(NEXAUI_BUILD_TESTS AND EXISTS "${_nexaui_example_font}")
    add_test(NAME nexaui_docking_editor_example
        COMMAND nexaui_docking_editor
            "${_nexaui_example_font}"
            "${CMAKE_CURRENT_BINARY_DIR}/docking-editor.ppm"
            "${CMAKE_CURRENT_BINARY_DIR}/docking-editor.nxdock")
    set_tests_properties(nexaui_docking_editor_example PROPERTIES TIMEOUT 20)
endif()
