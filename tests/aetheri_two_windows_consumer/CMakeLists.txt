cmake_minimum_required(VERSION 3.21)
project(NexaUIAetheriTwoWindows LANGUAGES C)
find_package(NexaUI 0.1.3 CONFIG REQUIRED)
add_executable(nexaui_aetheri_two_windows ../wayland_two_windows.c)
set_target_properties(nexaui_aetheri_two_windows PROPERTIES
    C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_link_libraries(nexaui_aetheri_two_windows PRIVATE NexaUI::NexaUI)
