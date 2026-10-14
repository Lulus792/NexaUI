add_executable(nexaui_01_hello_ui main.c)
target_include_directories(nexaui_01_hello_ui PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/platform/windows
    ${CMAKE_SOURCE_DIR}/renderers/d3d12)
target_link_libraries(nexaui_01_hello_ui PRIVATE
    NexaUI::NexaUI nexaui_renderer_d3d12)
if(BUILD_SHARED_LIBS)
    add_custom_command(TARGET nexaui_01_hello_ui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui>
            $<TARGET_FILE_DIR:nexaui_01_hello_ui>
        VERBATIM)
endif()
if(NEXAUI_BUILD_TESTS)
    add_test(NAME nexaui_01_hello_ui_example
        COMMAND nexaui_01_hello_ui --smoke)
    set_tests_properties(nexaui_01_hello_ui_example PROPERTIES TIMEOUT 20)
    include("${PROJECT_SOURCE_DIR}/cmake/NexaUIExampleFont.cmake")
    nexaui_find_example_font(_nexaui_example_font)
    if(EXISTS "${_nexaui_example_font}")
        add_test(NAME nexaui_01_hello_ui_font_example
            COMMAND nexaui_01_hello_ui --smoke
                "${_nexaui_example_font}"
                "${CMAKE_CURRENT_BINARY_DIR}/hello-ui.ppm")
        set_tests_properties(nexaui_01_hello_ui_font_example PROPERTIES TIMEOUT 20)
    endif()
endif()
