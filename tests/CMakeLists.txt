find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Test Concurrent)

set(NOTES_SRC_DIR "${CMAKE_SOURCE_DIR}/src_plugin/notes")

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
