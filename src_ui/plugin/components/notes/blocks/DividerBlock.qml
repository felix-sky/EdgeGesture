import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: root
    width: parent.width
    height: isEditing ? 40 : 20 // Active editor needs more height

    property int blockIndex: -1
    property var noteListView: null
    property var editor: null
    property bool isEditing: false

    // Missing properties from Delegate
    property var type: ""
    property int level: 0
    property string vaultRootPath: ""
    property string notePath: ""
    property string folderPath: ""
    property var notesFileHandler: null
    property var notesIndex: null

    // Callback property
    property var onLinkActivatedCallback: null

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

    // Visual Divider (Line)
    Rectangle {
        id: visualDivider
        height: 2
        width: parent.width
        color: root.editor ? root.editor.secondaryContrastColor : "#888888"
        anchors.centerIn: parent
        visible: !root.isEditing
    }

    // Editor Area (for editable "---")
    FluentEditorArea {
        id: textEdit
        anchors.fill: parent
        visible: root.isEditing

        // Default content for a divider is "---" if actual content is empty
        text: root.isEditing ? "---" : ""

        // Custom properties passed to FluentEditorArea
        customTextColor: root.editor ? root.editor.contrastColor : "#000000"
        customSelectionColor: FluTheme.primaryColor
        customBackgroundColor: root.editor ? root.editor.editBackgroundColor : "transparent"

        font.pixelSize: 16
        font.family: "Segoe UI"

        // Override Enter behavior
        Keys.onReturnPressed: event => handleEnter(event)
        Keys.onEnterPressed: event => handleEnter(event)

        function handleEnter(event) {
            event.accepted = true;
            if (root.noteListView && root.noteListView.model) {
                // Ensure current block is saved (usually "---")
                root.noteListView.model.updateBlock(root.blockIndex, text);

                // Insert new paragraph below
                root.noteListView.model.insertBlock(root.blockIndex + 1, "paragraph", "");

                // Focus new block
                root.nextBlockIndex = root.blockIndex + 1;
                newBlockFocusTimer.start();
            }
            root.isEditing = false;
        }

        onEditingFinished: {
            if (root.blockIndex >= 0 && root.noteListView) {
                // Update with new content. If it's still "---", it stays a divider.
                // If it changed, model will convert it.
                root.noteListView.model.updateBlock(root.blockIndex, text);
            }
        }

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
                if (length === 0) {
                    if (root.noteListView) {
                        root.noteListView.model.removeBlock(root.blockIndex);
                    }
                    event.accepted = true;
                }
            }
        }

        Connections {
            target: root.noteListView
            function onCurrentIndexChanged() {
                if (root.noteListView && root.noteListView.currentIndex !== root.blockIndex) {
                    root.isEditing = false;
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
    }

    // Interaction to enter edit mode
    MouseArea {
        anchors.fill: parent
        visible: !root.isEditing
        onClicked: {
            if (root.noteListView) {
                root.noteListView.currentIndex = root.blockIndex;
            }
            root.isEditing = true;
        }
    }

    onIsEditingChanged: {
        if (isEditing) {
            textEdit.forceActiveFocus();
            textEdit.cursorPosition = 0; // Start at beginning? Or select all?
            // "---" is short, cursor at end is fine too.
            textEdit.cursorPosition = textEdit.length;
        } else {
            textEdit.focus = false;
        }
    }
}
