find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Test Concurrent Widgets)

set(NOTES_SRC_DIR "${CMAKE_SOURCE_DIR}/src_plugin/notes")
set(STAGEMANAGER_SRC_DIR "${CMAKE_SOURCE_DIR}/src_plugin/StageManager")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(notes_tests
    notes/main.cpp
    notes/TestObsidianParser.h
    notes/TestObsidianParser.cpp
    notes/TestNotesIndex.h
    notes/TestNotesIndex.cpp
    notes/TestRoundTrip.h
    notes/TestRoundTrip.cpp
    ${NOTES_SRC_DIR}/ObsidianParser.h
    ${NOTES_SRC_DIR}/ObsidianParser.cpp
    ${NOTES_SRC_DIR}/MarkdownParser.h
    ${NOTES_SRC_DIR}/MarkdownParser.cpp
    ${NOTES_SRC_DIR}/MarkdownFormatter.h
    ${NOTES_SRC_DIR}/MarkdownFormatter.cpp
    ${NOTES_SRC_DIR}/NoteBlockModel.h
    ${NOTES_SRC_DIR}/NoteBlockModel.cpp
    ${NOTES_SRC_DIR}/NotesIndex.h
    ${NOTES_SRC_DIR}/NotesIndex.cpp
    ${NOTES_SRC_DIR}/NotesFileHandler.h
    ${NOTES_SRC_DIR}/NotesFileHandler.cpp
    ${NOTES_SRC_DIR}/md4c/src/md4c.c
)

target_include_directories(notes_tests PRIVATE
    ${NOTES_SRC_DIR}
    ${NOTES_SRC_DIR}/md4c/src
)

target_link_libraries(notes_tests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    Qt6::Concurrent
)

add_test(NAME notes_tests COMMAND notes_tests)

# Stage Manager Tests
add_executable(stage_manager_tests
    stagemanager/TestStageManager.cpp
    ${STAGEMANAGER_SRC_DIR}/WindowEmbedder.cpp
    ${STAGEMANAGER_SRC_DIR}/StageContainerController.cpp
    ${STAGEMANAGER_SRC_DIR}/StageManagerService.cpp
    ${STAGEMANAGER_SRC_DIR}/LiveWindowManager.cpp
    ${STAGEMANAGER_SRC_DIR}/WinIconProvider.cpp
    ${STAGEMANAGER_SRC_DIR}/WindowThumbnailProvider.cpp
)

target_include_directories(stage_manager_tests PRIVATE
    ${STAGEMANAGER_SRC_DIR}
)

target_link_libraries(stage_manager_tests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Widgets
    Qt6::Test
    dwmapi
    user32
    gdi32
    Comctl32
)

add_test(NAME stage_manager_tests COMMAND stage_manager_tests)
