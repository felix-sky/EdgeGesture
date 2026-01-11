import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: root
    width: parent.width
    height: Math.min(Math.max(40, textEdit.contentHeight + 16), 500) // Min height + padding

    property int blockIndex: -1
    property var noteListView: null
    property var editor: null
    property bool isEditing: false
    property string content: model.content

    // Missing properties from Delegate variables to avoid binding errors
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

    Rectangle {
        anchors.fill: parent
        color: root.editor ? Qt.rgba(root.editor.contrastColor.r, root.editor.contrastColor.g, root.editor.contrastColor.b, 0.05) : "#10000000"
        radius: 4

        // Left Border
        Rectangle {
            width: 4
            height: parent.height
            color: FluTheme.primaryColor
            anchors.left: parent.left
        }

        FluentEditorArea {
            id: textEdit
            anchors.fill: parent
            anchors.leftMargin: 12 // Space for border
            anchors.rightMargin: 4
            anchors.topMargin: 4
            anchors.bottomMargin: 4

            text: root.content

            // Custom properties passed to FluentEditorArea
            customTextColor: root.editor ? root.editor.contrastColor : "#000000"
            customSelectionColor: FluTheme.primaryColor
            customBackgroundColor: "transparent"

            font.pixelSize: 15
            font.family: "Segoe UI"

            onLinkActivated: link => {
                if (root.onLinkActivatedCallback) {
                    root.onLinkActivatedCallback(link);
                }
            }

            // Override Enter behavior
            Keys.onReturnPressed: event => handleEnter(event)
            Keys.onEnterPressed: event => handleEnter(event)

            function handleEnter(event) {
                event.accepted = true;

                var pos = cursorPosition;
                var fullText = text;
                var preText = fullText.substring(0, pos);
                var postText = fullText.substring(pos);

                if (root.noteListView && root.noteListView.model) {
                    // Update current block
                    root.noteListView.model.updateBlock(root.blockIndex, preText);

                    // Insert new reference block
                    // "reference" type should automatically implied "| " prefix in backend
                    root.noteListView.model.insertBlock(root.blockIndex + 1, "reference", postText);

                    // Focus new block
                    root.nextBlockIndex = root.blockIndex + 1;
                    newBlockFocusTimer.start();
                }
                root.isEditing = false;
            }

            onEditingFinished: {
                if (root.blockIndex >= 0 && root.noteListView) {
                    root.noteListView.model.updateBlock(root.blockIndex, text);
                }
            }

            Keys.onPressed: event => {
                if (event.key === Qt.Key_Up && cursorPosition === 0 && root.editor) {
                    root.editor.navigateUp(root.blockIndex);
                    event.accepted = true;
                } else if (event.key === Qt.Key_Down && cursorPosition === length && root.editor) {
                    root.editor.navigateDown(root.blockIndex);
                    event.accepted = true;
                } else if (event.key === Qt.Key_Backspace && length === 0) {
                    if (root.noteListView) {
                        root.noteListView.model.removeBlock(root.blockIndex);
                    }
                    event.accepted = true;
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
        }
    }
}
