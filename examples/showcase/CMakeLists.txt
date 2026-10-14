add_executable(nexaui_showcase main.c)
target_include_directories(nexaui_showcase PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/platform/windows
    ${CMAKE_SOURCE_DIR}/renderers/d3d12)
target_link_libraries(nexaui_showcase PRIVATE
    NexaUI::NexaUI nexaui_renderer_d3d12)
if(BUILD_SHARED_LIBS)
    add_custom_command(TARGET nexaui_showcase POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui>
            $<TARGET_FILE_DIR:nexaui_showcase>
        VERBATIM)
endif()
if(NEXAUI_BUILD_TESTS)
    include("${PROJECT_SOURCE_DIR}/cmake/NexaUIExampleFont.cmake")
    nexaui_find_example_font(_nexaui_example_font)
    if(EXISTS "${_nexaui_example_font}")
        add_test(NAME nexaui_showcase_example
            COMMAND nexaui_showcase --smoke "${_nexaui_example_font}"
                "${CMAKE_CURRENT_BINARY_DIR}/showcase-wide.ppm"
                "${CMAKE_CURRENT_BINARY_DIR}/showcase-detail.ppm"
                "${CMAKE_CURRENT_BINARY_DIR}/showcase-compact.ppm")
        set_tests_properties(nexaui_showcase_example PROPERTIES TIMEOUT 30)
    endif()
endif()
