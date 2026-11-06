cmake_minimum_required(VERSION 3.21)
project(NexaUITextFieldVersionConsumer LANGUAGES C)
find_package(NexaUI 0.1.22 CONFIG REQUIRED)
add_executable(text_field_version_consumer main.c)
set_target_properties(text_field_version_consumer PROPERTIES
    C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_link_libraries(text_field_version_consumer PRIVATE NexaUI::NexaUI)
