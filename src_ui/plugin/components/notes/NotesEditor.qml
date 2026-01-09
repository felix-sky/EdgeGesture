import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import Qt.labs.platform 1.1
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

            // title
            Rectangle {
                height: 32
                width: 180
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
                        clip: true
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

        Flickable {
            id: contentFlickable
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 15
            Layout.rightMargin: 15
            contentWidth: 320
            contentHeight: isEditing ? contentArea.implicitHeight : viewText.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick

            Text {
                id: viewText
                visible: !isEditing
                width: 320
                // Convert WikiLinks and Obsidian images for proper rendering
                text: convertObsidianContent(currentContent)
                color: "#FFFFFF"
                font.pixelSize: 16
                wrapMode: Text.Wrap
                textFormat: Text.MarkdownText
                linkColor: "#88CCFF"

                // Get the notes root folder from the note path
                function getNotesRoot() {
                    if (!editorPage.notePath)
                        return "";
                    // Navigate up to find the root (look for common patterns or use model's root)
                    if (editorPage.notesModel && editorPage.notesModel.rootPath) {
                        return editorPage.notesModel.rootPath;
                    }
                    return "";
                }

                // Convert Obsidian image embeds ![[image.png]] to HTML img tags
                function convertObsidianImages(content) {
                    var root = getNotesRoot();
                    var currentNotePath = editorPage.notePath;

                    // Match ![[filename]] pattern (image embeds in Obsidian)
                    return content.replace(/!\[\[([^\]]+)\]\]/g, function (match, imageName) {
                        // Use C++ findImage method for efficient search with recursive fallback
                        if (editorPage.notesFileHandler) {
                            var foundPath = editorPage.notesFileHandler.findImage(imageName, currentNotePath, root);
                            if (foundPath !== "") {
                                // Found the image! Return an img tag with file:// protocol
                                var normalizedPath = foundPath.replace(/\\/g, "/");
                                return '<img src="file:///' + normalizedPath + '" width="320" />';
                            }
                        }

                        // Image not found - return a placeholder
                        console.warn("NotesEditor: Image not found: " + imageName);
                        return '<span style="color: #ff6b6b;">[Image not found: ' + imageName + ']</span>';
                    });
                }

                // Convert WikiLinks [[Title]] to clickable markdown links
                function convertWikiLinks(content) {
                    // Convert [[Title]] to markdown links with note:// protocol, URL encoding the title
                    return content.replace(/\[\[([^\]]+)\]\]/g, function (match, title) {
                        return "[" + title + "](note://" + encodeURIComponent(title) + ")";
                    });
                }

                // Main conversion function - applies all Obsidian syntax conversions
                function convertObsidianContent(content) {
                    if (!content)
                        return "";
                    var result = content;
                    // First convert images (before WikiLinks to avoid conflicts)
                    result = convertObsidianImages(result);
                    // Then convert WikiLinks
                    result = convertWikiLinks(result);
                    return result;
                }

                onLinkActivated: link => {
                    if (link.startsWith("note://")) {
                        var encodedTitle = link.replace("note://", "");
                        var title = decodeURIComponent(encodedTitle);
                        var linkedPath = editorPage.notesModel.findPathByTitle(title);
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
                        var dir = editorPage.notesModel.currentPath + "/images";
                        if (!editorPage.notesFileHandler.exists(dir)) {
                            editorPage.notesFileHandler.createFolder(editorPage.notesModel.currentPath, "images");
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

        FluMenuItem {
            text: "Purple"
            onTriggered: changeColor("#624a73")
        }
        FluMenuItem {
            text: "Blue"
            onTriggered: changeColor('#0078d4')
        }
        FluMenuItem {
            text: "Yellow"
            onTriggered: changeColor("#c89100")
        }
        FluMenuItem {
            text: "Green"
            onTriggered: changeColor("#107C10")
        }
        FluMenuItem {
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
