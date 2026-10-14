add_executable(nexaui_02_buttons_and_input main.c)
target_include_directories(nexaui_02_buttons_and_input PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/platform/windows
    ${CMAKE_SOURCE_DIR}/renderers/d3d12)
target_link_libraries(nexaui_02_buttons_and_input PRIVATE
    NexaUI::NexaUI nexaui_renderer_d3d12)
if(BUILD_SHARED_LIBS)
    add_custom_command(TARGET nexaui_02_buttons_and_input POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui>
            $<TARGET_FILE_DIR:nexaui_02_buttons_and_input>
        VERBATIM)
endif()
if(NEXAUI_BUILD_TESTS)
    include("${PROJECT_SOURCE_DIR}/cmake/NexaUIExampleFont.cmake")
    nexaui_find_example_font(_nexaui_example_font)
    if(EXISTS "${_nexaui_example_font}")
        add_test(NAME nexaui_02_buttons_and_input_example
            COMMAND nexaui_02_buttons_and_input --smoke
                "${_nexaui_example_font}"
                "${CMAKE_CURRENT_BINARY_DIR}/buttons-and-input.ppm")
        set_tests_properties(nexaui_02_buttons_and_input_example
            PROPERTIES TIMEOUT 20)
    endif()
endif()
