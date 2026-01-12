import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import FluentUI 1.0

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

    // Update Model when CheckBox changes
    function toggleCheck() {
        // Cycle: [ ] -> [x] -> [ ]
        // If [/] or [-], clicking should probably mark as done [x]?
        var newMark = " ";
        if (taskStatus === " " || taskStatus === "") {
            newMark = "x";
        } else {
            newMark = " ";
        }

        var newMarkdown = "- [" + newMark + "] " + root.content;

        // Update via replaceBlock
        if (root.noteListView && root.noteListView.model) {
            root.noteListView.model.replaceBlock(root.blockIndex, newMarkdown);
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 5

        // Custom CheckBox Rendering
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

            // Icon for Checked [x]
            FluIcon {
                anchors.centerIn: parent
                iconSource: FluentIcons.Accept
                iconSize: 12
                iconColor: "white"
                visible: taskStatus === "x" || taskStatus === "X"
            }

            // Icon for In-Progress [/]
            FluIcon {
                anchors.centerIn: parent
                iconSource: FluentIcons.Play
                iconSize: 10
                iconColor: FluTheme.primaryColor
                visible: taskStatus === "/"
            }

            // Icon for Canceled [-]
            FluIcon {
                anchors.centerIn: parent
                iconSource: FluentIcons.Remove
                iconSize: 12
                iconColor: "#d13438" // Red
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
                var t = root.content;
                // Basic Markdown Rendering (Simplified copy from ParagraphBlock)

                // Math
                var mathColor = FluTheme.dark ? "\\color{white} " : "\\color{black} ";
                t = t.replace(/\$\$([^\$]+)\$\$/g, function (match, p1) {
                    var encoded = encodeURIComponent(p1);
                    var url = "image://microtex/" + encoded + "?size=24&color=" + (FluTheme.dark ? "white" : "black");
                    return '<br><img src="' + url + '" /><br>';
                });
                t = t.replace(/\$([^\$]+)\$/g, function (match, p1) {
                    var encoded = encodeURIComponent(p1);
                    var url = "image://microtex/" + encoded + "?size=16&color=" + (FluTheme.dark ? "white" : "black");
                    return '<img src="' + url + '" align="middle" />';
                });

                // Bold
                t = t.replace(/\*\*([^*]+)\*\*/g, '<b>$1</b>');
                t = t.replace(/__([^_]+)__/g, '<b>$1</b>');
                // Italic
                t = t.replace(/\*([^*]+)\*/g, '<i>$1</i>');
                t = t.replace(/_([^_]+)_/g, '<i>$1</i>');

                // Highlight
                var highlightColor = FluTheme.dark ? "rgba(255, 215, 0, 0.4)" : "rgba(255, 255, 0, 0.5)";
                t = t.replace(/==(.*?)==/g, '<span style="background-color: ' + highlightColor + ';">$1</span>');

                // Strikethrough
                t = t.replace(/~~([^~]+)~~/g, '<s>$1</s>');
                // Code
                var codeBg = FluTheme.dark ? "rgba(255, 255, 255, 0.12)" : "rgba(0, 0, 0, 0.05)";
                var codeColor = FluTheme.primaryColor;
                t = t.replace(/`([^`]+)`/g, '<code style="background-color: ' + codeBg + '; color: ' + codeColor + '; padding: 2px 4px; border-radius: 5px;">$1</code>');

                // Links (Simplified)
                t = t.replace(/\[\[(.*?)\]\]/g, function (match, p1) {
                    return '<a href="' + encodeURIComponent(p1) + '">' + p1 + '</a>';
                });

                return t;
            }
            wrapMode: Text.Wrap
            color: (taskStatus === "x" || taskStatus === "-" || root.isChecked) ? "#888888" : (FluTheme.dark ? "#cccccc" : "#222222")
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
                        // Switch to edit mode
                        if (root.noteListView) {
                            root.noteListView.currentIndex = root.blockIndex;
                        }
                        root.isEditing = true;
                    }
                }
            }
        }
    }

    Component {
        id: editorComp
        FluMultilineTextBox {
            width: parent.width
            // Reconstruct full task line for editing or just content?
            // If we edit just content, we keep the checkbox state.
            text: root.content

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
                            if (typeof root.noteListView.model.removeBlock === "function")
                                root.noteListView.model.removeBlock(idxToRemove);
                            event.accepted = true;
                        } else if (count > 1) {
                            if (typeof root.noteListView.model.removeBlock === "function")
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
                    // Update current block content (preserving status via updateBlock only updating content)
                    root.noteListView.model.updateBlock(root.blockIndex, preText);

                    // Insert new task block
                    root.noteListView.model.insertBlock(root.blockIndex + 1, "tasklist", postText);

                    // Focus new block
                    if (root.editor) {
                        root.editor.requestBlockEdit(root.blockIndex + 1);
                    }
                }
                root.isEditing = false;
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
                    root.isEditing = false;
                }
            }
            Component.onCompleted: {
                forceActiveFocus();
                cursorPosition = length;
            }
        }
    }
}
