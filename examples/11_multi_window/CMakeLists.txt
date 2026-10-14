add_executable(nexaui_multi_window_example main.c)
target_include_directories(nexaui_multi_window_example PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/platform/windows
    ${CMAKE_SOURCE_DIR}/renderers/d3d12)
target_link_libraries(nexaui_multi_window_example PRIVATE
    NexaUI::NexaUI nexaui_renderer_d3d12)
if(BUILD_SHARED_LIBS)
    add_custom_command(TARGET nexaui_multi_window_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui>
            $<TARGET_FILE_DIR:nexaui_multi_window_example>
        VERBATIM)
endif()
if(NEXAUI_BUILD_TESTS)
    add_test(NAME nexaui_multi_window_example
        COMMAND nexaui_multi_window_example --smoke)
    set_tests_properties(nexaui_multi_window_example PROPERTIES TIMEOUT 20)
endif()
