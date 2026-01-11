cmake_minimum_required(VERSION 3.16)

project(NotesPlugin LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Concurrent Svg)

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
        NotesIndex.h NotesIndex.cpp
        MarkdownParser.h MarkdownParser.cpp
        NoteBlockModel.h NoteBlockModel.cpp
        MathImageProvider.h MathImageProvider.cpp
        MathRender.h MathRender.cpp
        md4c/src/md4c.c
)

target_link_libraries(notesplugin PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Concurrent
    Qt6::Svg
    LaTeX
)

# MicroTeX configuration
set(QT ON CACHE BOOL "Enable Qt build for MicroTeX" FORCE)

include(FetchContent)
FetchContent_Declare(
    tinyxml2
    GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
    GIT_TAG 9.0.0
)
FetchContent_MakeAvailable(tinyxml2)

add_subdirectory(microtex)

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

# Copy MicroTeX res folder to the executable directory for runtime access
add_custom_command(TARGET notesplugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/microtex/res"
    "${CMAKE_BINARY_DIR}/res"
    COMMENT "Copying MicroTeX resources to build directory..."
)

# Also copy to the config-specific output directory
add_custom_command(TARGET notesplugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/microtex/res"
    "${CMAKE_BINARY_DIR}/$<CONFIG>/res"
    COMMENT "Copying MicroTeX resources to config build directory..."
)
