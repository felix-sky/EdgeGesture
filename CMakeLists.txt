cmake_minimum_required(VERSION 3.16)

project(EdgeGesture VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


if(MSVC)
    add_compile_options(/Zc:__cplusplus)
    add_compile_options(/permissive-)
endif()

add_compile_definitions(_WIN32_WINNT=0x0A00) # Windows 10
add_compile_definitions(WINVER=0x0A00)
add_compile_definitions(NOMINMAX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(QT_QML_GENERATE_QMLLS_INI ON)

set(FLUENTUI_BUILD_STATIC_LIB OFF CACHE BOOL "Build FluentUI as static lib" FORCE)
remove_definitions(-DFLUENTUI_BUILD_STATIC_LIB)


find_package(Qt6 6.10.1 REQUIRED COMPONENTS Core Gui Qml Quick Network Svg Widgets PrintSupport Core5Compat QuickDialogs2)

include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)


set(FLUENTUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FLUENTUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(src_ui/FluentUI)

add_executable(GestureEngine
    "src_engine/main.cpp"
    "src_engine/core/EngineCore.cpp" "src_engine/core/EngineCore.h"
    "src_engine/input/InputWindow.cpp" "src_engine/input/InputWindow.h"
    "src_engine/input/InputHook.cpp" "src_engine/input/InputHook.h"
    "src_engine/ui/Visualizer.cpp" "src_engine/ui/Visualizer.h"
    "src_engine/core/ConfigManager.cpp" "src_engine/core/ConfigManager.h"
    "src_engine/actions/ActionDispatcher.cpp" "src_engine/actions/ActionDispatcher.h"
)

target_link_libraries(GestureEngine PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(GestureEngine PRIVATE
    src_engine
    src_engine/core
    src_engine/input
    src_engine/actions
    src_engine/ui
)

if(WIN32)
    target_link_libraries(GestureEngine PRIVATE user32 gdi32 d2d1 dwrite ole32 shell32)
    set_target_properties(GestureEngine PROPERTIES WIN32_EXECUTABLE ON)
    if(MINGW)
        target_link_options(GestureEngine PRIVATE -municode -static-libgcc -static-libstdc++)
    endif()
endif()

set_target_properties(GestureEngine PROPERTIES AUTOMOC OFF AUTORCC OFF AUTOUIC OFF)


file(GLOB UI_SRCS
    "src_ui/*.cpp" "src_ui/*.h"
    "src_ui/utils/*.cpp" "src_ui/utils/*.h"
    "src_ui/utils/config/*.cpp" "src_ui/utils/config/*.h"
    "src_ui/utils/config/services/*.cpp" "src_ui/utils/config/services/*.h"
    "src_ui/utils/config/system/*.cpp" "src_ui/utils/config/system/*.h"
    "src_ui/utils/config/models/*.cpp" "src_ui/utils/config/models/*.h"
    "src_ui/utils/config/bridge/*.cpp" "src_ui/utils/config/bridge/*.h"
)

set(UI_RESOURCES "src_ui/resources.qrc")

add_executable(SettingsUI
    ${UI_SRCS}
    ${UI_RESOURCES}
    "src_ui/app_icon.rc"
)

target_include_directories(SettingsUI PRIVATE "src_ui/FluentUI/src")

target_link_libraries(SettingsUI PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network Qt6::Svg Qt6::Widgets Qt6::Core5Compat Qt6::QuickDialogs2
    fluentuiplugin
)

if(WIN32)
    set_target_properties(SettingsUI PROPERTIES WIN32_EXECUTABLE ON)
    target_link_libraries(SettingsUI PRIVATE dwmapi user32 shell32 wlanapi wbemuuid)
endif()

# Copy UI plugins to build directory after compilation
add_custom_command(TARGET SettingsUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/src_ui/plugin"
    "$<TARGET_FILE_DIR:SettingsUI>/plugin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:KF6SyntaxHighlighting>"
    "$<TARGET_FILE_DIR:SettingsUI>/bin/org/kde/syntaxhighlighting/KF6SyntaxHighlighting.dll"
    COMMENT "Copying UI plugins and dependencies to build directory..."
)

# Build plugin DLLs
add_subdirectory(src_plugin)
