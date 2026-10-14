add_executable(nexaui_03_layout main.c)
target_link_libraries(nexaui_03_layout PRIVATE NexaUI::NexaUI)
if(BUILD_SHARED_LIBS AND WIN32)
    add_custom_command(TARGET nexaui_03_layout POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui>
            $<TARGET_FILE_DIR:nexaui_03_layout>
        VERBATIM)
endif()
if(NEXAUI_BUILD_TESTS)
    include("${PROJECT_SOURCE_DIR}/cmake/NexaUIExampleFont.cmake")
    nexaui_find_example_font(_nexaui_example_font)
    if(EXISTS "${_nexaui_example_font}")
        add_test(NAME nexaui_03_layout_example
            COMMAND nexaui_03_layout "${_nexaui_example_font}"
                "${CMAKE_CURRENT_BINARY_DIR}/layout")
        set_tests_properties(nexaui_03_layout_example PROPERTIES TIMEOUT 20)
    endif()
endif()
