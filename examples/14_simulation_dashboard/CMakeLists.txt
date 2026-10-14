add_executable(nexaui_simulation_dashboard main.c)
target_link_libraries(nexaui_simulation_dashboard PRIVATE NexaUI::NexaUI)
if(NOT WIN32)
    target_link_libraries(nexaui_simulation_dashboard PRIVATE Threads::Threads)
endif()
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET nexaui_simulation_dashboard POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:nexaui>
            $<TARGET_FILE_DIR:nexaui_simulation_dashboard>
        VERBATIM)
endif()
if(NEXAUI_BUILD_TESTS)
    add_test(NAME nexaui_simulation_dashboard_example
        COMMAND nexaui_simulation_dashboard
            "${CMAKE_CURRENT_BINARY_DIR}/simulation-dashboard")
    set_tests_properties(nexaui_simulation_dashboard_example PROPERTIES TIMEOUT 20)
    include("${PROJECT_SOURCE_DIR}/cmake/NexaUIExampleFont.cmake")
    nexaui_find_example_font(_nexaui_example_font)
    if(EXISTS "${_nexaui_example_font}")
        add_test(NAME nexaui_simulation_dashboard_font_example
            COMMAND nexaui_simulation_dashboard
                "${CMAKE_CURRENT_BINARY_DIR}/simulation-dashboard-font"
                "${_nexaui_example_font}")
        set_tests_properties(nexaui_simulation_dashboard_font_example
            PROPERTIES TIMEOUT 20)
    endif()
endif()
