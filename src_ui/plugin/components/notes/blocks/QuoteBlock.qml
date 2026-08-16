import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: root
    width: ListView.view ? ListView.view.width - 20 : 300
    height: loader.item ? loader.item.height : 24

    property string content: ""
    property bool isEditing: false
    property string folderPath: ""
    property var noteListView: null
    property var notesIndex: null
    property int blockIndex: -1
    property string type: "quote"
    property int level: 0
    property var editor: null

    property var onLinkActivatedCallback: null
    property var notesFileHandler: null
    property string notePath: ""
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

    Loader {
        id: loader
        width: parent.width
        sourceComponent: isEditing ? editorComp : viewerComp
    }

    Component {
        id: viewerComp
        Rectangle {
            width: root.width
            height: textItem.contentHeight + 20
            color: FluTheme.dark ? "#333333" : "#f0f0f0"
            radius: 4

            Rectangle {
                width: 4
                height: parent.height
                color: FluTheme.primaryColor
                anchors.left: parent.left
                radius: 2
            }

            Text {
                id: textItem
                width: parent.width - 24
                anchors.centerIn: parent
                wrapMode: Text.Wrap
                text: {
                    var t = root.content;
                    var highlightColor = FluTheme.dark ? "rgba(255, 215, 0, 0.4)" : "rgba(255, 255, 0, 0.5)";
                    t = t.replace(/==(.*?)==/g, '<span style="background-color: ' + highlightColor + ';">$1</span>');
                    return t;
                }
                color: root.editor ? root.editor.contrastColor : (FluTheme.dark ? "#cccccc" : "#555555")
                font.pixelSize: 16
                font.italic: true
                font.family: "Segoe UI"
                textFormat: Text.RichText
                linkColor: FluTheme.primaryColor

                onLinkActivated: link => {
                    if (root.onLinkActivatedCallback)
                        root.onLinkActivatedCallback(link);
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.IBeamCursor
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
    }

    Component {
        id: editorComp
        Rectangle {
            id: editorContainer
            width: root.width
            height: editorArea.contentHeight + 20
            color: FluTheme.dark ? "#333333" : "#f0f0f0"
            radius: 4

            Rectangle {
                width: 4
                height: parent.height
                color: FluTheme.primaryColor
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                radius: 2
            }

            FluentEditorArea {
                id: editorArea
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.topMargin: 10

                text: root.content

                customBackgroundColor: "transparent"
                customTextColor: root.editor ? root.editor.contrastColor : (FluTheme.dark ? "#cccccc" : "#555555")
                customSelectionColor: FluTheme.primaryColor

                font.pixelSize: 16
                font.italic: true
                font.family: "Segoe UI"

                Keys.onReturnPressed: event => handleEnter(event)
                Keys.onEnterPressed: event => handleEnter(event)

                Keys.onUpPressed: event => {
                    var lineHeight = font.pixelSize * 1.5;
                    var isFirstLine = cursorRectangle.y < lineHeight;
                    if (isFirstLine && root.blockIndex > 0) {
                        finishEdit();
                        if (root.editor && typeof root.editor.goToPreviousBlock === "function") {
                            root.editor.goToPreviousBlock(root.blockIndex);
                            event.accepted = true;
                            return;
                        }
                    }
                    event.accepted = false;
                }

                Keys.onDownPressed: event => {
                    var lineHeight = font.pixelSize * 1.5;
                    var textHeight = contentHeight > 0 ? contentHeight : lineHeight;
                    var isLastLine = cursorRectangle.y >= (textHeight - lineHeight);
                    if (isLastLine && root.editor) {
                        finishEdit();
                        if (typeof root.editor.goToNextBlock === "function") {
                            root.editor.goToNextBlock(root.blockIndex);
                            event.accepted = true;
                            return;
                        }
                    }
                    event.accepted = false;
                }

                Keys.onPressed: event => {
                    if ((event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) && text === "") {
                        if (root.blockIndex >= 0 && root.noteListView && root.noteListView.model) {
                            var idxToRemove = root.blockIndex;
                            var count = root.noteListView.count;
                            if (idxToRemove > 0) {
                                if (root.editor)
                                    root.editor.navigateToBlock(idxToRemove - 1, true);
                                if (typeof root.noteListView.model.removeBlock === "function")
                                    root.noteListView.model.removeBlock(idxToRemove);
                                event.accepted = true;
                            } else if (count > 1) {
                                if (typeof root.noteListView.model.removeBlock === "function")
                                    root.noteListView.model.removeBlock(idxToRemove);
                                if (root.editor)
                                    root.editor.navigateToBlock(0, false);
                                event.accepted = true;
                            }
                        }
                    }
                }

                function handleEnter(event) {
                    event.accepted = true;
                    var pos = cursorPosition;
                    var fullText = text;
                    var preText = fullText.substring(0, pos);
                    var postText = fullText.substring(pos);

                    if (root.noteListView && root.noteListView.model) {
                        root.noteListView.model.updateBlock(root.blockIndex, preText);
                        root.noteListView.model.insertBlock(root.blockIndex + 1, "quote", postText);

                        root.nextBlockIndex = root.blockIndex + 1;
                        newBlockFocusTimer.start();
                    }
                    if (root.editor) {
                        root.editor.endEditing(root.blockIndex);
                    }
                }

                Component.onCompleted: {
                    focusTimer.start();
                    if (root.editor && root.editor._cursorAtEnd !== undefined) {
                        cursorPosition = root.editor._cursorAtEnd ? length : 0;
                    } else {
                        cursorPosition = length;
                    }
                }

                Timer {
                    id: focusTimer
                    interval: 50
                    repeat: false
                    onTriggered: editorArea.forceActiveFocus()
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
                        if (root.noteListView && root.noteListView.model) {
                            root.noteListView.model.replaceBlock(root.blockIndex, text);
                        }
                        if (root.editor) {
                            root.editor.endEditing(root.blockIndex);
                        }
                    }
                }
            }
        }
    }
}
