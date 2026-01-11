import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: root
    width: parent.width
    height: Math.max(24, textEdit.height) // Minimal height

    // Properties required by NotesEditor delegate
    property int blockIndex: -1
    property var noteListView: null
    property var notesIndex: null
    property var notesFileHandler: null
    property string notePath: ""
    property string folderPath: "" // Extracted from notePath
    property var editor: null

    // Block-specific properties
    property var type: "" // "list"
    property var level: 0
    property bool isEditing: false

    // Metadata
    property var listType: model.metadata["listType"] || "bullet" // "bullet" or "ordered"
    property string content: model.content

    // Signals
    signal linkActivatedCallback(string link)

    function handleLinkClick(link) {
        if (linkActivatedCallback) {
            linkActivatedCallback(link);
        } else {
            console.warn("No link callback registered");
        }
    }

    // Layout
    Row {
        anchors.fill: parent
        spacing: 8

        // Marker (Bullet or Number)
        Text {
            id: marker
            width: 24
            text: root.listType === "ordered" ? "1." : "•" // Simple 1. for now, ideally binding to index
            font.pixelSize: 16
            color: root.editor ? root.editor.contrastColor : "#000000"
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignTop
            topPadding: 4 // Align with text
        }

        // Content Area
        TextArea {
            id: textEdit
            width: parent.width - 32 // Adjust for marker
            text: root.content
            wrapMode: Text.Wrap
            font.pixelSize: 16
            color: root.editor ? root.editor.contrastColor : "#000000"
            selectedTextColor: "#FFFFFF"
            selectionColor: FluentTheme.primaryColor
            selectByMouse: true
            font.family: "Segoe UI"

            // Transparent background
            background: Item {}

            // Edit vs View handling (TextArea handles both, but we might want Markdown rendering in view)
            // For now, simple text for MVP lists.
            // TODO: Add rich text rendering for links inside lists
            textFormat: Text.MarkdownText

            onLinkActivated: root.handleLinkClick(link)

            // Keys
            Keys.onPressed: {
                if (event.key === Qt.Key_Up) {
                    if (cursorPosition === 0 && root.editor) {
                        root.editor.navigateUp(root.blockIndex);
                        event.accepted = true;
                    }
                } else if (event.key === Qt.Key_Down) {
                    if (cursorPosition === length && root.editor) {
                        root.editor.navigateDown(root.blockIndex);
                        event.accepted = true;
                    }
                } else if (event.key === Qt.Key_Backspace) {
                    if (length === 0 && root.editor) {
                        // Empty list item -> Delete block
                        // We need a removeBlock signal or method in editor
                        // For now, let's try to assume empty -> delete behavior in model update?
                        // No, explicit delete is better.
                        // Plan had "Handle Backspace at start of line to merge..."
                        // For now: if empty, delete.
                        // But we don't have removeBlock exposed conveniently in QML?
                        // Models have removeRow...
                        // Let's rely on standard text editing for now or add a custom signal "requestRemove"
                        // Actually NotesEditor has signals. Let's add removeBlock signal to NotesEditor later?
                        // Or use the model directly via `notesModel`? No, `blockModel`.
                        // `blockModel` is internal to NotesEditor.
                        // We can iterate. For now stay simple.
                    }
                }
            }

            onEditingFinished: {
                // Update model
                if (root.blockIndex >= 0 && root.noteListView) {
                    root.noteListView.model.updateBlock(root.blockIndex, text);
                }
            }

            // Auto-focus logic
            onVisibleChanged: {
                if (visible && root.isEditing) {
                    forceActiveFocus();
                }
            }

            Connections {
                target: root
                function onIsEditingChanged() {
                    if (root.isEditing) {
                        textEdit.forceActiveFocus();
                        if (root.editor && root.editor._cursorAtEnd) {
                            textEdit.cursorPosition = textEdit.length;
                        } else {
                            textEdit.cursorPosition = 0;
                        }
                    }
                }
            }
        }
    }
}
