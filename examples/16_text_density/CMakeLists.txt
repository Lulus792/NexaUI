add_executable(nexaui_text_density main.c)
target_link_libraries(nexaui_text_density PRIVATE NexaUI::NexaUI)
target_compile_features(nexaui_text_density PRIVATE c_std_17)
if(MSVC)
    target_compile_options(nexaui_text_density PRIVATE /W4 /utf-8)
    target_compile_definitions(nexaui_text_density PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET nexaui_text_density POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:nexaui> $<TARGET_FILE_DIR:nexaui_text_density> VERBATIM)
endif()
