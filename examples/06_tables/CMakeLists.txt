add_executable(nexaui_tables_example main.c)
target_link_libraries(nexaui_tables_example PRIVATE NexaUI::NexaUI)
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET nexaui_tables_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui>
            $<TARGET_FILE_DIR:nexaui_tables_example>
        VERBATIM)
endif()
if(NEXAUI_BUILD_TESTS)
    add_test(NAME nexaui_tables_example
        COMMAND nexaui_tables_example
            "${CMAKE_CURRENT_BINARY_DIR}/tables-example.ppm")
    set_tests_properties(nexaui_tables_example PROPERTIES TIMEOUT 15)
endif()
