add_executable(nexaui_00_hello_window main.c)
target_link_libraries(nexaui_00_hello_window PRIVATE NexaUI::NexaUI)
if(BUILD_SHARED_LIBS)
    add_custom_command(TARGET nexaui_00_hello_window POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui>
            $<TARGET_FILE_DIR:nexaui_00_hello_window>
        VERBATIM)
endif()
if(NEXAUI_BUILD_TESTS)
    add_test(NAME nexaui_00_hello_window_example
        COMMAND nexaui_00_hello_window --smoke)
    set_tests_properties(nexaui_00_hello_window_example PROPERTIES TIMEOUT 15)
endif()
