add_executable(nexaui_native_clear main.c)
target_include_directories(nexaui_native_clear PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/platform/windows
    ${CMAKE_SOURCE_DIR}/renderers/d3d12)
target_link_libraries(nexaui_native_clear PRIVATE
    NexaUI::NexaUI nexaui_renderer_d3d12)
