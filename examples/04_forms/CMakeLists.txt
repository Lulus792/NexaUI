add_executable(nexaui_04_forms main.c)
target_link_libraries(nexaui_04_forms PRIVATE NexaUI::NexaUI)
if(BUILD_SHARED_LIBS AND WIN32)
    add_custom_command(TARGET nexaui_04_forms POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui>
            $<TARGET_FILE_DIR:nexaui_04_forms>
        VERBATIM)
endif()
if(NEXAUI_BUILD_TESTS)
    include("${PROJECT_SOURCE_DIR}/cmake/NexaUIExampleFont.cmake")
    nexaui_find_example_font(_nexaui_example_font)
    if(EXISTS "${_nexaui_example_font}")
        add_test(NAME nexaui_04_forms_example
            COMMAND nexaui_04_forms "${_nexaui_example_font}"
                "${CMAKE_CURRENT_BINARY_DIR}/forms")
        set_tests_properties(nexaui_04_forms_example PROPERTIES TIMEOUT 20)
    endif()
endif()
