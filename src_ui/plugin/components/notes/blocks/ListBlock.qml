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
    property string content: model.content
    property bool isEditing: false
    property var listType: model.metadata["listType"] || "bullet"

    // Missing properties from Delegate
    property var type: ""
    property int level: 0
    property string vaultRootPath: ""

    // Store next block index for timer
    property int nextBlockIndex: -1

    Timer {
        id: newBlockFocusTimer
        interval: 50
        repeat: false
        onTriggered: {
            if (root.nextBlockIndex >= 0 && root.editor) {
                root.editor.navigateToBlock(root.nextBlockIndex, false);
                root.nextBlockIndex = -1;
            }
        }
    }

    // Callback property for NotesEditor injection
    property var onLinkActivatedCallback: null

    // Layout
    Row {
        anchors.fill: parent
        spacing: 8

        // Marker (Bullet or Number)
        Text {
            id: marker
            width: 24
            text: root.listType === "ordered" ? "1." : "•"
            font.pixelSize: 16
            color: root.editor ? root.editor.contrastColor : "#000000"
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignTop
            topPadding: 4 // Align with text
        }

        // Content Area using FluentEditorArea
        FluentEditorArea {
            id: textEdit
            width: parent.width - 32 // Adjust for marker
            text: root.content

            // Custom properties passed to FluentEditorArea
            customTextColor: root.editor ? root.editor.contrastColor : "#000000"
            customSelectionColor: FluTheme.primaryColor
            customBackgroundColor: root.editor ? root.editor.editBackgroundColor : "transparent"

            // Override Enter behavior
            Keys.onReturnPressed: event => handleEnter(event)
            Keys.onEnterPressed: event => handleEnter(event)

            font.pixelSize: 16
            font.family: "Segoe UI"

            // Handle link activation via property callback
            onLinkActivated: link => {
                if (root.onLinkActivatedCallback) {
                    root.onLinkActivatedCallback(link);
                }
            }

            // Keys
            Keys.onPressed: event => {
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
                    // Check logic: if cursor at 0 and length > 0, we might want to merge with previous?
                    // But here we care about EMPTY block deletion.
                    if (length === 0) {
                        // Empty list item -> Delete block
                        if (root.noteListView) {
                            root.noteListView.model.removeBlock(root.blockIndex);
                        }
                        event.accepted = true;
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
                target: root.noteListView
                function onCurrentIndexChanged() {
                    if (root.noteListView && root.noteListView.currentIndex !== root.blockIndex) {
                        root.isEditing = false;
                        textEdit.focus = false;
                    }
                }
                ignoreUnknownSignals: true
            }

            onActiveFocusChanged: {
                if (activeFocus) {
                    if (root.noteListView) {
                        root.noteListView.currentIndex = root.blockIndex;
                    }
                    root.isEditing = true;
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
                    } else {
                        textEdit.focus = false;
                    }
                }
            }
            function handleEnter(event) {
                event.accepted = true;

                var pos = cursorPosition;
                var fullText = text;
                var preText = fullText.substring(0, pos);
                var postText = fullText.substring(pos);

                // Prefix for new block to maintain list type
                var prefix = "";
                if (root.listType === "ordered") {
                    // ideally increment number, but auto-formatting handles "1." well enough for now?
                    // NoteBlockModel auto-format detects "1. ".
                    prefix = "1. ";
                } else {
                    prefix = "* ";
                }

                if (root.noteListView && root.noteListView.model) {
                    // Update current block
                    root.noteListView.model.updateBlock(root.blockIndex, preText);

                    // Insert new block as paragraph with prefix, model will likely convert it?
                    // Or insert as "list" directly if possible?
                    // Inserting as paragraph with prefix is safer if auto-format logic exists in updateBlock.
                    // But insertBlock might not trigger updateBlock logic immediately?
                    // Let's try inserting as Paragraph with prefix.
                    root.noteListView.model.insertBlock(root.blockIndex + 1, "paragraph", prefix + postText);

                    // Focus new block
                    root.nextBlockIndex = root.blockIndex + 1;
                    newBlockFocusTimer.start();
                }
                root.isEditing = false;
            }
        }
    }
}
