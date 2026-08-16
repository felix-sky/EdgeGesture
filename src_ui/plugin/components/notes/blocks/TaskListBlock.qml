import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import FluentUI 1.0
import EdgeGesture.Notes 1.0

Item {
    id: root
    width: ListView.view ? ListView.view.width - 20 : 300
    implicitHeight: Math.max(promptRect.height, textLoader.item ? textLoader.item.implicitHeight : 0)

    property string content: ""
    property var metadata: ({})
    property bool isChecked: metadata["checked"] === true
    property bool isEditing: false

    property string folderPath: ""
    property var noteListView: null
    property var editor: null
    property var onLinkActivatedCallback: null
    property var notesIndex: null
    property var notesFileHandler: null
    property string notePath: ""
    property string vaultRootPath: ""
    property int blockIndex: -1
    property string type: "tasklist"
    property int level: 0

    property string taskStatus: metadata["taskStatus"] ? metadata["taskStatus"] : (isChecked ? "x" : " ")

    function toggleCheck() {
        var newMark = " ";
        if (taskStatus === " " || taskStatus === "") {
            newMark = "x";
        } else {
            newMark = " ";
        }

        var newMarkdown = "- [" + newMark + "] " + root.content;

        if (root.noteListView && root.noteListView.model) {
            root.noteListView.model.replaceBlock(root.blockIndex, newMarkdown);
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 8

        Rectangle {
            id: promptRect
            width: 18
            height: 18
            radius: 4
            border.color: FluTheme.dark ? "#888888" : "#888888"
            border.width: 1
            color: (taskStatus === "x" || taskStatus === "X") ? FluTheme.primaryColor : "transparent"
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 3

            FluIcon {
                anchors.centerIn: parent
                iconSource: FluentIcons.Accept
                iconSize: 12
                iconColor: "white"
                visible: taskStatus === "x" || taskStatus === "X"
            }

            FluIcon {
                anchors.centerIn: parent
                iconSource: FluentIcons.Play
                iconSize: 10
                iconColor: FluTheme.primaryColor
                visible: taskStatus === "/"
            }

            FluIcon {
                anchors.centerIn: parent
                iconSource: FluentIcons.Remove
                iconSize: 12
                iconColor: "#d13438"
                visible: taskStatus === "-"
            }

            MouseArea {
                anchors.fill: parent
                onClicked: toggleCheck()
                cursorShape: Qt.PointingHandCursor
            }
        }

        Loader {
            id: textLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: isEditing ? editorComp : viewerComp
        }
    }

    Component {
        id: viewerComp
        Text {
            id: textItem
            width: parent.width
            text: {
                var raw = root.content;
                var currentFontSize = 16;
                var darkMode = root.editor ? root.editor.isDarkColor(root.editor.currentColor) : FluTheme.dark;
                var processed = MathHelper.processMathToPlaceholders(raw, currentFontSize, darkMode);
                return MathHelper.restoreMathPlaceholders(processed);
            }
            wrapMode: Text.Wrap
            color: (taskStatus === "x" || taskStatus === "-" || root.isChecked) ? "#888888" : (root.editor ? root.editor.contrastColor : (FluTheme.dark ? "#cccccc" : "#222222"))
            font.pixelSize: 16
            font.strikeout: (taskStatus === "x" || taskStatus === "-")
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

    Component {
        id: editorComp
        FluentEditorArea {
            width: parent.width
            text: root.content

            customTextColor: root.editor ? root.editor.contrastColor : "#000000"
            customSelectionColor: FluTheme.primaryColor
            customBackgroundColor: root.editor ? root.editor.editBackgroundColor : "transparent"

            Keys.onReturnPressed: event => handleEnter(event)
            Keys.onEnterPressed: event => handleEnter(event)

            Keys.onPressed: event => {
                if ((event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) && text === "") {
                    if (root.blockIndex >= 0 && root.noteListView && root.noteListView.model) {
                        var idxToRemove = root.blockIndex;
                        var count = root.noteListView.count;

                        if (idxToRemove > 0) {
                            if (root.editor) {
                                root.editor.navigateToBlock(idxToRemove - 1, true);
                            }
                            root.noteListView.model.removeBlock(idxToRemove);
                            event.accepted = true;
                        } else if (count > 1) {
                            root.noteListView.model.removeBlock(idxToRemove);
                            if (root.editor) {
                                root.editor.navigateToBlock(0, false);
                            }
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
                    root.noteListView.model.insertBlock(root.blockIndex + 1, "tasklist", postText);
                    if (root.editor) {
                        root.editor.navigateToBlock(root.blockIndex + 1, false);
                    }
                }
                if (root.editor) {
                    root.editor.endEditing(root.blockIndex);
                }
            }

            onEditingFinished: finishEdit()
            onActiveFocusChanged: {
                if (!activeFocus)
                    finishEdit();
            }

            function finishEdit() {
                if (root.isEditing) {
                    var newMarkdown = "- [" + root.taskStatus + "] " + text;
                    if (root.noteListView && root.noteListView.model) {
                        root.noteListView.model.replaceBlock(root.blockIndex, newMarkdown);
                    }
                    if (root.editor) {
                        root.editor.endEditing(root.blockIndex);
                    }
                }
            }

            Component.onCompleted: {
                forceActiveFocus();
                cursorPosition = length;
            }
        }
    }
}
