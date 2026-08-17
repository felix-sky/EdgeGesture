cmake_minimum_required(VERSION 3.16)

project(StageManagerPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Widgets)

set(STAGEMANAGER_PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/plugin/additional/EdgeGesture/StageManager")

qt_add_qml_module(stagemanagerplugin
    URI EdgeGesture.StageManager
    VERSION 1.0
    PLUGIN_TARGET stagemanagerplugin
    NO_GENERATE_PLUGIN_SOURCE
    OUTPUT_DIRECTORY ${STAGEMANAGER_PLUGIN_OUTPUT_DIR}
    SOURCES
        StageManagerTypes.h
        WindowEmbedder.h WindowEmbedder.cpp
        StageContainerController.h StageContainerController.cpp
        StageManagerService.h StageManagerService.cpp
        LiveWindowManager.h LiveWindowManager.cpp
        WinIconProvider.h WinIconProvider.cpp
        WindowThumbnailProvider.h WindowThumbnailProvider.cpp
        StageManagerPlugin.cpp
)

target_link_libraries(stagemanagerplugin PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Widgets
    Comctl32
)

if(WIN32)
    target_link_libraries(stagemanagerplugin PRIVATE dwmapi user32 gdi32)
endif()

set_target_properties(stagemanagerplugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${STAGEMANAGER_PLUGIN_OUTPUT_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${STAGEMANAGER_PLUGIN_OUTPUT_DIR}
)

# Output copy for debugging/running
add_custom_command(TARGET stagemanagerplugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${STAGEMANAGER_PLUGIN_OUTPUT_DIR}"
    "${CMAKE_BINARY_DIR}/$<CONFIG>/plugin/additional/EdgeGesture/StageManager/"
)
