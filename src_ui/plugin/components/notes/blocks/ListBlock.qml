import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0
import EdgeGesture.Notes 1.0

Item {
    id: root
    width: parent.width
    height: Math.max(24, loader.item ? loader.item.height : 24)

    property int blockIndex: -1
    property var noteListView: null
    property var notesIndex: null
    property var notesFileHandler: null
    property string notePath: ""
    property string folderPath: ""

    property var editor: null
    property string content: ""
    property bool isEditing: false
    property var metadata: ({})
    property var listType: metadata["listType"] || "bullet"

    property var type: ""
    property int level: 0
    property string vaultRootPath: ""

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

    property var onLinkActivatedCallback: null

    Row {
        anchors.fill: parent
        spacing: 8

        Text {
            id: marker
            width: 24
            text: root.listType === "ordered" ? "1." : "•"
            font.pixelSize: 16
            color: root.editor ? root.editor.contrastColor : "#000000"
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignTop
            topPadding: 4
        }

        Loader {
            id: loader
            width: parent.width - 32
            sourceComponent: isEditing ? editorComp : viewerComp
        }
    }

    Component {
        id: viewerComp
        Text {
            id: textItem
            width: loader.width
            wrapMode: Text.Wrap
            color: root.editor ? root.editor.contrastColor : (FluTheme.dark ? "#cccccc" : "#222222")
            font.pixelSize: 16
            font.family: "Segoe UI"
            textFormat: Text.RichText
            linkColor: FluTheme.primaryColor

            text: {
                var raw = root.content;
                var currentFontSize = 16;
                var darkMode = root.editor ? root.editor.isDarkColor(root.editor.currentColor) : FluTheme.dark;
                var processed = MathHelper.processMathToPlaceholders(raw, currentFontSize, darkMode);
                return MathHelper.restoreMathPlaceholders(processed);
            }

            onLinkActivated: link => {
                if (root.onLinkActivatedCallback) {
                    root.onLinkActivatedCallback(link);
                }
            }

            MouseArea {
                id: interactor
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.IBeamCursor
                hoverEnabled: true

                onClicked: mouse => {
                    var link = parent.linkAt(mouse.x, mouse.y);
                    if (link) {
                        parent.linkActivated(link);
                    } else {
                        if (root.editor) {
                            root.editor.beginEditing(root.blockIndex);
                        }
                    }
                }
            }
        }
    }

    Component {
        id: editorComp
        FluentEditorArea {
            id: textEdit
            width: loader.width
            text: root.content

            customTextColor: root.editor ? root.editor.contrastColor : "#000000"
            customSelectionColor: FluTheme.primaryColor
            customBackgroundColor: root.editor ? root.editor.editBackgroundColor : "transparent"

            Keys.onReturnPressed: event => handleEnter(event)
            Keys.onEnterPressed: event => handleEnter(event)

            font.pixelSize: 16
            font.family: "Segoe UI"

            Keys.onPressed: event => {
                if (event.key === Qt.Key_Up) {
                    if (cursorPosition === 0 && root.editor) {
                        finishEdit();
                        root.editor.goToPreviousBlock(root.blockIndex);
                        event.accepted = true;
                    }
                } else if (event.key === Qt.Key_Down) {
                    if (cursorPosition === length && root.editor) {
                        finishEdit();
                        root.editor.goToNextBlock(root.blockIndex);
                        event.accepted = true;
                    }
                } else if (event.key === Qt.Key_Backspace) {
                    if (length === 0) {
                        if (root.noteListView && root.noteListView.model) {
                            root.noteListView.model.removeBlock(root.blockIndex);
                        }
                        event.accepted = true;
                    }
                }
            }

            onEditingFinished: {
                finishEdit();
            }

            onActiveFocusChanged: {
                if (!activeFocus)
                    finishEdit();
            }

            function finishEdit() {
                if (root.isEditing) {
                    if (root.blockIndex >= 0 && root.noteListView && root.noteListView.model) {
                        root.noteListView.model.updateBlock(root.blockIndex, text);
                    }
                    if (root.editor) {
                        root.editor.endEditing(root.blockIndex);
                    }
                }
            }

            function handleEnter(event) {
                event.accepted = true;
                var pos = cursorPosition;
                var fullText = text;
                var preText = fullText.substring(0, pos);
                var postText = fullText.substring(pos);

                var prefix = (root.listType === "ordered") ? "1. " : "* ";

                if (root.noteListView && root.noteListView.model) {
                    root.noteListView.model.updateBlock(root.blockIndex, preText);
                    root.noteListView.model.insertBlock(root.blockIndex + 1, "paragraph", prefix + postText);
                    root.nextBlockIndex = root.blockIndex + 1;
                    newBlockFocusTimer.start();
                }
                if (root.editor) {
                    root.editor.endEditing(root.blockIndex);
                }
            }

            Component.onCompleted: {
                forceActiveFocus();
                if (root.editor && root.editor._cursorAtEnd !== undefined) {
                    cursorPosition = root.editor._cursorAtEnd ? length : 0;
                } else {
                    cursorPosition = length;
                }
            }
        }
    }
}
