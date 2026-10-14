add_executable(nexaui_3d_viewport_shell main.c)
target_link_libraries(nexaui_3d_viewport_shell PRIVATE NexaUI::NexaUI)
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET nexaui_3d_viewport_shell POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui>
            $<TARGET_FILE_DIR:nexaui_3d_viewport_shell>
        VERBATIM)
endif()
if(NEXAUI_BUILD_TESTS)
    add_test(NAME nexaui_3d_viewport_shell_example
        COMMAND nexaui_3d_viewport_shell
            "${CMAKE_CURRENT_BINARY_DIR}/viewport-shell.ppm")
    set_tests_properties(nexaui_3d_viewport_shell_example PROPERTIES TIMEOUT 15)
endif()
