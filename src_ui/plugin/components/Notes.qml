import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import Qt.labs.platform 1.1
import FluentUI 1.0

Item {
    id: root
    width: 360
    anchors.top: parent.top
    anchors.bottom: parent.bottom

    property string notesPath: StandardPaths.standardLocations(StandardPaths.DocumentsLocation)[0] + "/EdgeGesture/notes.json"

    Component.onCompleted: {
        var dir = StandardPaths.standardLocations(StandardPaths.DocumentsLocation)[0] + "/EdgeGesture";
        if (!FileBridge.exists(dir)) {
            FileBridge.createDirectory(dir);
        }
        loadData();
    }

    // Save to Json
    function saveData() {
        var data = [];
        for (var i = 0; i < notesModel.count; i++) {
            data.push(notesModel.get(i));
        }
        var jsonStr = JSON.stringify(data, null, 4);
        FileBridge.writeFile(notesPath, jsonStr);
    }

    function loadData() {
        if (FileBridge.exists(notesPath)) {
            var jsonStr = FileBridge.readFile(notesPath);
            if (jsonStr) {
                try {
                    var data = JSON.parse(jsonStr);
                    notesModel.clear();
                    for (var i = 0; i < data.length; i++) {
                        notesModel.append(data[i]);
                    }
                } catch (e) {
                    console.log("Error parsing notes:", e);
                }
            }
        }
    }

    signal closeRequested
    signal requestInputMode(bool active)

    ListModel {
        id: notesModel
    }

    // Main Navigation view
    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: homeComponent
        clip: true
    }

    // Home
    Component {
        id: homeComponent
        Item {
            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Title
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    spacing: 10

                    FluText {
                        text: "Notes"
                        font: FluTextStyle.Title
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    // Import button
                    FluIconButton {
                        iconSource: FluentIcons.Download
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: importDialog.open()
                    }

                    // New note
                    FluIconButton {
                        iconSource: FluentIcons.Add
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: {
                            var newNote = {
                                "type": "note",
                                "title": "Untitled",
                                "content": "",
                                "color": "#624a73",
                                "date": new Date().toLocaleString(Qt.locale(), "yyyy-MM-dd HH:mm")
                            };
                            notesModel.insert(0, newNote);
                            saveData();
                            stackView.push(editorComponent, {
                                noteIndex: 0,
                                noteData: newNote,
                                isEditing: true
                            });
                        }
                    }
                }

                // Note list
                ListView {
                    id: listView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: notesModel
                    spacing: 10
                    topMargin: 10
                    bottomMargin: 10

                    delegate: Rectangle {
                        width: 320
                        height: type === "folder" ? 60 : 100
                        radius: 8
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: FluTheme.dark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.03)

                        Rectangle {
                            width: 4
                            height: parent.height - 16
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 8
                            radius: 2
                            color: model.color
                        }

                        ColumnLayout {
                            anchors.left: parent.left
                            anchors.leftMargin: 20
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                FluIcon {
                                    iconSource: type === "folder" ? FluentIcons.FolderHorizontal : FluentIcons.QuickNote
                                    iconSize: 14
                                    color: FluTheme.dark ? "#ccc" : "#666"
                                }
                                FluText {
                                    text: title
                                    font: FluTextStyle.Subtitle
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                FluText {
                                    text: date
                                    font: FluTextStyle.Caption
                                    color: FluTheme.dark ? "#888" : "#999"
                                }
                            }

                            FluText {
                                visible: type === "note"
                                text: content.replace(/[#*`]/g, "")
                                color: FluTheme.dark ? "#aaa" : "#666"
                                elide: Text.ElideRight
                                maximumLineCount: 2
                                Layout.fillWidth: true
                                font.pixelSize: 12
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (type === "folder") {} else {
                                    stackView.push(editorComponent, {
                                        noteIndex: index,
                                        noteData: notesModel.get(index),
                                        isEditing: false
                                    });
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: editorComponent
        Item {
            id: editorPage
            property int noteIndex
            property var noteData
            property bool isEditing: false

            property string currentTitle: ""
            property string currentContent: ""
            property string currentColor: ""

            onNoteDataChanged: {
                if (noteData) {
                    currentTitle = noteData.title;
                    currentContent = noteData.content;
                    currentColor = noteData.color;
                }
            }

            Component.onCompleted: root.requestInputMode(true)
            Component.onDestruction: root.requestInputMode(false)

            Rectangle {
                anchors.fill: parent
                color: currentColor
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Top bar
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60

                    Rectangle {
                        height: 32
                        width: Math.min(parent.width - 80, 200)
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
                                text: currentTitle
                                color: "#FFF"
                                font.pixelSize: 13
                                selectByMouse: true
                                Layout.fillWidth: true
                                onTextEdited: currentTitle = text
                                onEditingFinished: {
                                    notesModel.setProperty(noteIndex, "title", text);
                                    saveData();
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
                            onClicked: {
                                isEditing = !isEditing;
                                saveData()
                            }
                        }

                        FluIconButton {
                            iconSource: FluentIcons.More
                            iconSize: 16
                            onClicked: colorPickerMenu.open()
                        }
                        FluIconButton {
                            iconSource: FluentIcons.ChromeClose
                            iconSize: 16
                            onClicked: {
                                stackView.pop()
                                saveData()
                            }
                        }
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 15
                    Layout.rightMargin: 15
                    clip: true

                    Text {
                        visible: !isEditing
                        width: parent.width
                        text: currentContent
                        color: "#FFFFFF"
                        font.pixelSize: 16
                        wrapMode: Text.Wrap
                        textFormat: Text.MarkdownText
                        onLinkActivated: Qt.openUrlExternally(link)
                    }

                    // Editview
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

                        Keys.onPressed: event => {
                            if ((event.key === Qt.Key_V) && (event.modifiers & Qt.ControlModifier)) {
                                var dir = StandardPaths.standardLocations(StandardPaths.DocumentsLocation)[0] + "/EdgeGesture/images";
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
                            notesModel.setProperty(noteIndex, "content", text);
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
                            FormatBtn {
                                label: "B"
                                code: "**"
                                textArea: contentArea
                            }
                            FormatBtn {
                                label: "I"
                                code: "*"
                                textArea: contentArea
                            }
                            FormatBtn {
                                label: "S"
                                code: "~~"
                                textArea: contentArea
                            }
                            FormatBtn {
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
                                isEditing = false;
                                saveData();
                            }
                        }
                    }
                }
            }

            Menu {
                id: colorPickerMenu
                MenuItem {
                    text: "Purple"
                    onTriggered: changeColor("#624a73")
                }
                MenuItem {
                    text: "Blue"
                    onTriggered: changeColor('#cf0078d4')
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
                notesModel.setProperty(noteIndex, "color", c);
                saveData();
            }
        }
    }

    // simple format button
    component FormatBtn: Item {
        property string label: ""
        property int icon: 0
        property string code: ""
        property var textArea
        property bool isUnderline: false

        width: 30
        height: 30

        FluText {
            visible: label !== ""
            text: label
            color: "#FFF"
            font.bold: true
            font.pixelSize: 16
            font.underline: isUnderline
            anchors.centerIn: parent
        }

        FluIcon {
            visible: icon !== 0
            iconSource: icon
            color: "#FFF"
            iconSize: 16
            anchors.centerIn: parent
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (textArea) {
                    var start = textArea.selectionStart;
                    var end = textArea.selectionEnd;
                    var txt = textArea.text;

                    if (start !== end) {
                        // Have selection, wrap content
                        textArea.remove(start, end);
                        textArea.insert(start, code + txt.substring(start, end) + code);
                    } else {
                        // No selection, insert directly
                        textArea.insert(textArea.cursorPosition, code);
                    }
                    textArea.forceActiveFocus();
                }
            }
        }
    }

    // File import dialog
    FileDialog {
        id: importDialog
        title: "Import Markdown file"
        nameFilters: ["Markdown Files (*.md)", "All Files (*)"]
        folder: StandardPaths.standardLocations(StandardPaths.DocumentsLocation)[0]
        onAccepted: {
            var path = FileBridge.urlToPath(importDialog.file.toString());
            var fileName = FileBridge.getFileName(path);
            var content = FileBridge.readFile(path);

            if (content !== "") {
                notesModel.insert(0, {
                    "type": "note",
                    "title": fileName,
                    "content": content,
                    "color": "#0078D4",
                    "date": "just now"
                });
                saveData();
            }
        }
    }
}
