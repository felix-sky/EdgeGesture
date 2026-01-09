cmake_minimum_required(VERSION 3.16)

project(NotesPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick Concurrent)

# Plugin output directory
set(NOTES_PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/plugin/additional/EdgeGesture/Notes")

qt_add_qml_module(notesplugin
    URI EdgeGesture.Notes
    VERSION 1.0
    PLUGIN_TARGET notesplugin
    OUTPUT_DIRECTORY ${NOTES_PLUGIN_OUTPUT_DIR}
    SOURCES
        NotesModel.h NotesModel.cpp
        NotesFileHandler.h NotesFileHandler.cpp
)

target_link_libraries(notesplugin PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    Qt6::Concurrent
)

# Ensure the plugin is built as a shared library
set_target_properties(notesplugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${NOTES_PLUGIN_OUTPUT_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${NOTES_PLUGIN_OUTPUT_DIR}
)

# Copy plugin DLLs to SettingsUI output directory after compilation
# Access parent binary directory to find SettingsUI location
add_custom_command(TARGET notesplugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${NOTES_PLUGIN_OUTPUT_DIR}"
    "${CMAKE_BINARY_DIR}/$<CONFIG>/plugin/additional/EdgeGesture/Notes/"
    COMMENT "Copying Notes plugin to build directory..."
)
