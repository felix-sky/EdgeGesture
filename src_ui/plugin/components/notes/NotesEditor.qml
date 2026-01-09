import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import FluentUI 1.0
import EdgeGesture.Notes 1.0

Item {
    id: editorPage

    property string notePath: ""
    property string noteTitle: ""
    property bool isEditing: false

    property var notesFileHandler: null
    property var notesIndex: null
    property var notesModel: null

    property string currentContent: ""
    property string currentColor: "#624a73"

    signal inputModeRequested(bool active)
    signal closeRequested
    signal addTagRequested(string path)
    signal linkOpened(string path, string title)

    Component.onCompleted: {
        editorPage.inputModeRequested(true);
        // Load note content using NotesFileHandler
        if (notePath !== "" && notesFileHandler && notesFileHandler.exists(notePath)) {
            var noteData = notesFileHandler.readNote(notePath);
            currentContent = noteData.content;
            currentColor = noteData.color;
        }
    }

    Component.onDestruction: editorPage.inputModeRequested(false)

    Rectangle {
        anchors.fill: parent
        color: currentColor
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top bar
        Item {
            id: topBar
            Layout.fillWidth: true
            Layout.preferredHeight: 60

            Rectangle {
                height: 32
                width: 150
                radius: 16
                color: Qt.rgba(1, 1, 1, 0.2)
                anchors.left: parent.left
                anchors.leftMargin: 15
                anchors.verticalCenter: parent.verticalCenter

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 5

                    TextInput {
                        id: titleInput
                        text: noteTitle
                        color: "#FFF"
                        font.pixelSize: 13
                        selectByMouse: true
                        Layout.fillWidth: true
                        onEditingFinished: {
                            if (text !== noteTitle && text.trim() !== "") {
                                var newPath = notesFileHandler.renameItem(notePath, text, false);
                                if (newPath !== "") {
                                    notePath = newPath;
                                    noteTitle = text;
                                }
                            }
                        }
                    }
                }
            }

            // Top right actions
            Row {
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 5

                // Edit mode toggle
                FluIconButton {
                    iconSource: isEditing ? FluentIcons.View : FluentIcons.Edit
                    iconSize: 16
                    iconColor: "white"
                    onClicked: {
                        if (isEditing) {
                            // Save when switching from edit to view
                            notesFileHandler.saveNote(notePath, contentArea.text, currentColor);
                            notesIndex.updateEntry(notePath);
                        }
                        isEditing = !isEditing;
                    }
                }

                FluIconButton {
                    iconSource: FluentIcons.Tag
                    iconSize: 16
                    iconColor: "white"
                    onClicked: {
                        editorPage.addTagRequested(notePath);
                    }

                    FluTooltip {
                        visible: parent.hovered
                        text: "Add Tag"
                    }
                }

                FluIconButton {
                    iconSource: FluentIcons.More
                    iconSize: 16
                    iconColor: "white"
                    onClicked: colorPickerMenu.open()
                }

                FluIconButton {
                    iconSource: FluentIcons.ChromeClose
                    iconSize: 16
                    iconColor: "white"
                    onClicked: {
                        notesFileHandler.saveNote(notePath, currentContent, currentColor);
                        notesIndex.updateEntry(notePath);
                        editorPage.closeRequested();
                    }
                }
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 15
            Layout.rightMargin: 15
            contentWidth: 320
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AlwaysOff
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            Text {
                id: viewText
                visible: !isEditing
                width: 320
                // Convert WikiLinks to markdown links for proper rendering
                text: convertWikiLinks(currentContent)
                color: "#FFFFFF"
                font.pixelSize: 16
                wrapMode: Text.Wrap
                textFormat: Text.MarkdownText
                linkColor: "#88CCFF"

                function convertWikiLinks(content) {
                    // Convert [[Title]] to markdown links with note:// protocol, URL encoding the title
                    return content.replace(/\[\[([^\]]+)\]\]/g, function (match, title) {
                        return "[" + title + "](note://" + encodeURIComponent(title) + ")";
                    });
                }

                onLinkActivated: link => {
                    if (link.startsWith("note://")) {
                        var encodedTitle = link.replace("note://", "");
                        var title = decodeURIComponent(encodedTitle);
                        var linkedPath = notesModel.findPathByTitle(title);
                        if (linkedPath !== "") {
                            editorPage.linkOpened(linkedPath, title);
                        }
                    } else {
                        Qt.openUrlExternally(link);
                    }
                }
            }

            // Edit view
            TextArea {
                id: contentArea
                visible: isEditing
                text: currentContent
                color: "#FFFFFF"
                font.pixelSize: 16
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.PlainText
                placeholderText: "Type anything..."
                placeholderTextColor: Qt.rgba(1, 1, 1, 0.5)
                background: null
                selectByMouse: true
                width: 320

                Keys.onPressed: event => {
                    if ((event.key === Qt.Key_V) && (event.modifiers & Qt.ControlModifier)) {
                        var dir = notesModel.currentPath + "/images";
                        if (!notesFileHandler.exists(dir)) {
                            notesFileHandler.createFolder(notesModel.currentPath, "images");
                        }
                        var fileUrl = SystemBridge.saveClipboardImage(dir);
                        if (fileUrl !== "") {
                            insert(cursorPosition, "<img src=\"" + fileUrl + "\" width=\"320\" />");
                            event.accepted = true;
                        }
                    }
                }

                // Force focus
                onVisibleChanged: if (visible)
                    forceActiveFocus()

                onTextChanged: {
                    currentContent = text;
                }
            }
        }

        Rectangle {
            visible: isEditing
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: Qt.rgba(0, 0, 0, 0.1)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 15
                anchors.rightMargin: 15

                Row {
                    spacing: 15
                    Layout.alignment: Qt.AlignVCenter
                    NotesFormatBtn {
                        label: "B"
                        code: "**"
                        textArea: contentArea
                    }
                    NotesFormatBtn {
                        label: "I"
                        code: "*"
                        textArea: contentArea
                    }
                    NotesFormatBtn {
                        label: "S"
                        code: "~~"
                        textArea: contentArea
                    }
                    NotesFormatBtn {
                        icon: FluentIcons.BulletedList
                        code: "- "
                        textArea: contentArea
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Rectangle {
                    width: 100
                    height: 36
                    radius: 6
                    color: Qt.rgba(1, 1, 1, 0.15)
                    Layout.alignment: Qt.AlignVCenter

                    Row {
                        anchors.centerIn: parent
                        spacing: 5
                        FluIcon {
                            iconSource: FluentIcons.Crop
                            iconSize: 14
                            color: "#FFF"
                        }
                        Text {
                            text: "Screenshot"
                            color: "#FFF"
                            font.pixelSize: 12
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: SystemBridge.launchSnippingTool()
                    }
                }

                FluIconButton {
                    iconSource: FluentIcons.Accept
                    iconSize: 18
                    Layout.alignment: Qt.AlignVCenter
                    onClicked: {
                        notesFileHandler.saveNote(notePath, currentContent, currentColor);
                        notesIndex.updateEntry(notePath);
                        isEditing = false;
                    }
                }
            }
        }
    }

    FluMenu {
        id: colorPickerMenu
        width: 150
        x: parent.width - 160
        y: 50
        background: Rectangle {
            implicitWidth: 150
            implicitHeight: 36
            color: Qt.rgba(252 / 255, 252 / 255, 252 / 255, 1)
            border.color: FluTheme.dark ? Qt.rgba(26 / 255, 26 / 255, 26 / 255, 1) : Qt.rgba(191 / 255, 191 / 255, 191 / 255, 1)
            border.width: 1
            radius: 5
            FluShadow {}
        }
        MenuItem {
            text: "Purple"
            onTriggered: changeColor("#624a73")
        }
        MenuItem {
            text: "Blue"
            onTriggered: changeColor('#0078d4')
        }
        MenuItem {
            text: "Yellow"
            onTriggered: changeColor("#c89100")
        }
        MenuItem {
            text: "Green"
            onTriggered: changeColor("#107C10")
        }
        MenuItem {
            text: "Red"
            onTriggered: changeColor("#b40d1b")
        }
    }

    function changeColor(c) {
        currentColor = c;
        notesFileHandler.saveNote(notePath, currentContent, c);
        notesIndex.updateEntry(notePath);
    }
}
