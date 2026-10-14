add_executable(nexaui_realtime_plot main.c)
target_link_libraries(nexaui_realtime_plot PRIVATE NexaUI::NexaUI)
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET nexaui_realtime_plot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui>
            $<TARGET_FILE_DIR:nexaui_realtime_plot>
        VERBATIM)
endif()
if(NEXAUI_BUILD_TESTS)
    add_test(NAME nexaui_realtime_plot_example
        COMMAND nexaui_realtime_plot
            "${CMAKE_CURRENT_BINARY_DIR}/realtime-plot")
    set_tests_properties(nexaui_realtime_plot_example PROPERTIES TIMEOUT 20)
endif()
