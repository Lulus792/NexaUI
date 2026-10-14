add_executable(nexaui_accessibility_example main.c)
target_link_libraries(nexaui_accessibility_example PRIVATE NexaUI::NexaUI)
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET nexaui_accessibility_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui>
            $<TARGET_FILE_DIR:nexaui_accessibility_example>
        VERBATIM)
endif()
if(NEXAUI_BUILD_TESTS)
    add_test(NAME nexaui_accessibility_example
        COMMAND nexaui_accessibility_example)
    set_tests_properties(nexaui_accessibility_example PROPERTIES TIMEOUT 15)
endif()
