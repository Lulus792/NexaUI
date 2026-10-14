add_executable(nexaui_custom_widget main.c)
target_link_libraries(nexaui_custom_widget PRIVATE NexaUI::NexaUI)
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET nexaui_custom_widget POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui>
            $<TARGET_FILE_DIR:nexaui_custom_widget>
        VERBATIM)
endif()
if(NEXAUI_BUILD_TESTS)
    add_test(NAME nexaui_custom_widget_example
        COMMAND nexaui_custom_widget
            "${CMAKE_CURRENT_BINARY_DIR}/custom-widget-example")
    set_tests_properties(nexaui_custom_widget_example PROPERTIES TIMEOUT 15)
endif()
