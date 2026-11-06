cmake_minimum_required(VERSION 3.21)
project(NexaUIAetheriClipboardPeer LANGUAGES C)
find_package(NexaUI 0.1.1 CONFIG REQUIRED)
add_executable(nexaui_aetheri_clipboard_peer main.c)
set_target_properties(nexaui_aetheri_clipboard_peer PROPERTIES
    C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_link_libraries(nexaui_aetheri_clipboard_peer PRIVATE NexaUI::NexaUI)
