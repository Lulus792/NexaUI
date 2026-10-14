add_executable(nexaui_scroll_example main.c)
target_link_libraries(nexaui_scroll_example PRIVATE NexaUI::NexaUI)
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET nexaui_scroll_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui>
            $<TARGET_FILE_DIR:nexaui_scroll_example>
        VERBATIM)
endif()
if(NEXAUI_BUILD_TESTS)
    add_test(NAME nexaui_scroll_example
        COMMAND nexaui_scroll_example
            "${CMAKE_CURRENT_BINARY_DIR}/scroll-example.ppm")
    set_tests_properties(nexaui_scroll_example PROPERTIES TIMEOUT 15)
endif()
