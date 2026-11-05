cmake_minimum_required(VERSION 3.21)
project(NexaUIInstalledConsumer LANGUAGES C)
find_package(NexaUI CONFIG REQUIRED)
add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE NexaUI::NexaUI)
target_compile_definitions(consumer PRIVATE
    NEXAUI_PACKAGE_VERSION_MAJOR=${NexaUI_VERSION_MAJOR}
    NEXAUI_PACKAGE_VERSION_MINOR=${NexaUI_VERSION_MINOR}
    NEXAUI_PACKAGE_VERSION_PATCH=${NexaUI_VERSION_PATCH}
    NEXAUI_PACKAGE_SOURCE_REVISION="${NexaUI_SOURCE_REVISION}")
