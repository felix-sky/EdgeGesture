cmake_minimum_required(VERSION 3.16)

project(NotesPlugin LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Setup Extra CMake Modules (ECM)
set(ECM_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extra-cmake-modules")
list(APPEND CMAKE_MODULE_PATH "${ECM_SOURCE_DIR}/modules" "${ECM_SOURCE_DIR}/find-modules")

# Mock ECMConfig.cmake so find_package(ECM) works for submodule
set(ECM_DUMMY_DIR "${CMAKE_CURRENT_BINARY_DIR}/ecm_mock")
file(MAKE_DIRECTORY "${ECM_DUMMY_DIR}")
file(WRITE "${ECM_DUMMY_DIR}/ECMConfig.cmake" "
set(ECM_MODULE_PATH \"${ECM_SOURCE_DIR}/modules\" \"${ECM_SOURCE_DIR}/kde-modules\")
set(ECM_FIND_MODULE_DIR \"${ECM_SOURCE_DIR}/find-modules\")
set(ECM_KDE_MODULE_DIR \"${ECM_SOURCE_DIR}/kde-modules\")
# Fix for ECMPoQmTools.cmake looking for ECMQmLoader.cpp.in
set(ECM_MODULE_DIR \"${ECM_SOURCE_DIR}/modules\")
")
file(WRITE "${ECM_DUMMY_DIR}/ECMConfigVersion.cmake" "
set(PACKAGE_VERSION \"6.23.0\")
set(PACKAGE_VERSION_COMPATIBLE TRUE)
set(PACKAGE_VERSION_EXACT TRUE)
")
set(ECM_DIR "${ECM_DUMMY_DIR}")

# Pre-load ECM modules to ensure available command
include("${ECM_SOURCE_DIR}/modules/ECMQueryQt.cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Concurrent Svg Widgets)

set(NOTES_PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/plugin/additional/EdgeGesture/Notes")

# Configure KSyntaxHighlighting
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_QCH OFF CACHE BOOL "" FORCE)
set(KSYNTAXHIGHLIGHTING_USE_GUI ON CACHE BOOL "" FORCE) # Required for QML plugin
set(KDE_INSTALL_QMLDIR "${NOTES_PLUGIN_OUTPUT_DIR}" CACHE PATH "Install QML modules to plugin dir" FORCE)

add_subdirectory(syntax-highlighting)

set(JKQTMATHTEXT_SOURCES
	jkqtplotter/lib/jkqtmathtext/jkqtmathtext.cpp
	jkqtplotter/lib/jkqtmathtext/jkqtmathtexttools.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtextnode.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtextnodetools.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtextnoopnode.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtexttextnode.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtextsymbolnode.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtextwhitespacenode.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtexthorizontallistnode.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtextverticallistnode.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtextbracenode.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtextdecoratednode.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtextfracnode.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtextinstructionnode.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtextboxinstructionnode.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtextmatrixnode.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtextmodifyenvironmentnode.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtextsqrtnode.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtextsubsupernode.cpp
	jkqtplotter/lib/jkqtmathtext/nodes/jkqtmathtextverbatimnode.cpp
	jkqtplotter/lib/jkqtmathtext/parsers/jkqtmathtextparser.cpp
	jkqtplotter/lib/jkqtmathtext/parsers/jkqtmathtextlatexparser.cpp
)

set(JKQTMATHTEXT_HEADERS
	jkqtplotter/lib/jkqtmathtext/jkqtmathtext.h
	jkqtplotter/lib/jkqtmathtext/jkqtmathtext_imexport.h
	jkqtplotter/lib/jkqtmathtext/jkqtmathtexttools.h
)

set(JKQTCOMMON_SOURCES
	jkqtplotter/lib/jkqtcommon/jkqtpcodestructuring.cpp
	jkqtplotter/lib/jkqtcommon/jkqtpstringtools.cpp
	jkqtplotter/lib/jkqtcommon/jkqtpmathtools.cpp
	jkqtplotter/lib/jkqtcommon/jkqtpdrawingtools.cpp
	jkqtplotter/lib/jkqtcommon/jkqtpenhancedpainter.cpp
	jkqtplotter/lib/jkqtcommon/jkqtpdebuggingtools.cpp
	jkqtplotter/lib/jkqtcommon/jkqtpcachingtools.cpp
	jkqtplotter/lib/jkqtcommon/jkqtpgeometrytools.cpp
	jkqtplotter/lib/jkqtcommon/jkqtpconcurrencytools.cpp
	jkqtplotter/lib/jkqtcommon/jkqtpexpected.cpp
	jkqtplotter/lib/jkqtcommon/jkqtpcsstools.cpp
	jkqtplotter/lib/jkqtcommon/jkqttools.cpp
	jkqtplotter/lib/jkqtcommon/jkqtpicons.cpp
	jkqtplotter/lib/jkqtcommon/jkqtphighrestimer.cpp
	jkqtplotter/lib/jkqtcommon/jkqtpbasicimagetools.cpp
)

set(JKQTCOMMON_HEADERS
	jkqtplotter/lib/jkqtcommon/jkqtcommon_imexport.h
	jkqtplotter/lib/jkqtcommon/jkqtpcodestructuring.h
	jkqtplotter/lib/jkqtcommon/jkqtpstringtools.h
	jkqtplotter/lib/jkqtcommon/jkqtpmathtools.h
	jkqtplotter/lib/jkqtcommon/jkqtpdrawingtools.h
	jkqtplotter/lib/jkqtcommon/jkqtpenhancedpainter.h
	jkqtplotter/lib/jkqtcommon/jkqtpdebuggingtools.h
	jkqtplotter/lib/jkqtcommon/jkqtpcachingtools.h
	jkqtplotter/lib/jkqtcommon/jkqtpgeometrytools.h
	jkqtplotter/lib/jkqtcommon/jkqtpconcurrencytools.h
	jkqtplotter/lib/jkqtcommon/jkqtpexpected.h
	jkqtplotter/lib/jkqtcommon/jkqtpcsstools.h
	jkqtplotter/lib/jkqtcommon/jkqttools.h
	jkqtplotter/lib/jkqtcommon/jkqtpicons.h
	jkqtplotter/lib/jkqtcommon/jkqtphighrestimer.h
	jkqtplotter/lib/jkqtcommon/jkqtpbasicimagetools.h
)

set(JKQTMATHTEXT_RESOURCES
	jkqtplotter/lib/jkqtmathtext/resources/xits.qrc
	jkqtplotter/lib/jkqtmathtext/resources/firamath.qrc
)

qt_add_qml_module(notesplugin
	URI EdgeGesture.Notes
	VERSION 1.0
	PLUGIN_TARGET notesplugin
	OUTPUT_DIRECTORY ${NOTES_PLUGIN_OUTPUT_DIR}
	SOURCES
	        ${CMAKE_CURRENT_SOURCE_DIR}/NotesModel.h ${CMAKE_CURRENT_SOURCE_DIR}/NotesModel.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/NotesFileHandler.h ${CMAKE_CURRENT_SOURCE_DIR}/NotesFileHandler.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/NotesIndex.h ${CMAKE_CURRENT_SOURCE_DIR}/NotesIndex.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/MarkdownParser.h ${CMAKE_CURRENT_SOURCE_DIR}/MarkdownParser.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/NoteBlockModel.h ${CMAKE_CURRENT_SOURCE_DIR}/NoteBlockModel.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/MathRenderItem.h ${CMAKE_CURRENT_SOURCE_DIR}/MathRenderItem.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/MathHelper.h ${CMAKE_CURRENT_SOURCE_DIR}/MathHelper.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/CodeHighlighter.h ${CMAKE_CURRENT_SOURCE_DIR}/CodeHighlighter.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/md4c/src/md4c.c
		${JKQTMATHTEXT_SOURCES}
		${JKQTMATHTEXT_HEADERS}
		${JKQTCOMMON_SOURCES}
		${JKQTCOMMON_HEADERS}
	RESOURCES
	        ${JKQTMATHTEXT_RESOURCES}
)

target_include_directories(notesplugin PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/jkqtplotter/lib
	${CMAKE_CURRENT_SOURCE_DIR}/syntax-highlighting/src/lib
)

target_link_libraries(notesplugin PRIVATE
	Qt6::Core
	Qt6::Gui
	Qt6::Qml
	Qt6::Quick
	Qt6::Concurrent
	Qt6::Svg
	Qt6::Widgets
	KF6SyntaxHighlighting
)

set_target_properties(notesplugin PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY ${NOTES_PLUGIN_OUTPUT_DIR}
	RUNTIME_OUTPUT_DIRECTORY ${NOTES_PLUGIN_OUTPUT_DIR}
)

add_custom_command(TARGET notesplugin POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"${NOTES_PLUGIN_OUTPUT_DIR}"
	"${CMAKE_BINARY_DIR}/$<CONFIG>/plugin/additional/EdgeGesture/Notes/"
	# Copy KF6SyntaxHighlighting.dll to the notes plugin directory
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"$<TARGET_FILE:KF6SyntaxHighlighting>"
	"${CMAKE_BINARY_DIR}/$<CONFIG>/plugin/additional/EdgeGesture/Notes/"
)
