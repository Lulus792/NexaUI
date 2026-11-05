cmake_minimum_required(VERSION 3.21)
project(NexaUIAetheriDockConsumer LANGUAGES C)
find_package(NexaUI 0.1.3 CONFIG REQUIRED)
add_executable(nexaui_aetheri_dock_consumer main.c)
set_target_properties(nexaui_aetheri_dock_consumer PROPERTIES
    C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_link_libraries(nexaui_aetheri_dock_consumer PRIVATE NexaUI::NexaUI)
