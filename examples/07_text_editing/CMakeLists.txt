add_executable(nexaui_text_editing_example main.c)
target_link_libraries(nexaui_text_editing_example PRIVATE NexaUI::NexaUI)
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET nexaui_text_editing_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui>
            $<TARGET_FILE_DIR:nexaui_text_editing_example>
        VERBATIM)
endif()

include("${PROJECT_SOURCE_DIR}/cmake/NexaUIExampleFont.cmake")
nexaui_find_example_font(_nexaui_example_font)
if(NEXAUI_BUILD_TESTS AND EXISTS "${_nexaui_example_font}")
    add_test(NAME nexaui_text_editing_example
        COMMAND nexaui_text_editing_example
            "${_nexaui_example_font}"
            "${CMAKE_CURRENT_BINARY_DIR}/text-editing-example.ppm")
    set_tests_properties(nexaui_text_editing_example PROPERTIES TIMEOUT 20)
endif()
