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

    // Base notes folder
    property string notesRootPath: StandardPaths.standardLocations(StandardPaths.DocumentsLocation)[0] + "/EdgeGesture/Notes"
    property string currentFolderPath: notesRootPath
    property var folderStack: []  // For navigation history

    Component.onCompleted: {
        // Ensure root directory exists
        if (!FileBridge.exists(notesRootPath)) {
            FileBridge.createDirectory(notesRootPath);
        }
        loadCurrentFolder();
    }

    // Load files and folders from current directory
    function loadCurrentFolder() {
        notesModel.clear();

        var jsonStr = FileBridge.listDirectory(currentFolderPath);
        try {
            var items = JSON.parse(jsonStr);
            for (var i = 0; i < items.length; i++) {
                var item = items[i];
                // Only show .md files and folders
                if (item.isDir || item.name.endsWith(".md")) {
                    notesModel.append({
                        "type": item.isDir ? "folder" : "note",
                        "title": item.isDir ? item.name : item.baseName,
                        "path": item.path,
                        "date": item.modifiedDate,
                        "color": "#624a73"  // Default color
                    });
                }
            }
        } catch (e) {
            console.log("Error loading folder:", e);
        }
    }

    // Navigate into a folder
    function navigateToFolder(folderPath) {
        folderStack.push(currentFolderPath);
        currentFolderPath = folderPath;
        loadCurrentFolder();
    }

    // Navigate back to parent folder
    function navigateBack() {
        if (folderStack.length > 0) {
            currentFolderPath = folderStack.pop();
            loadCurrentFolder();
        }
    }

    // Create a new note file
    function createNote(title, content, color) {
        var safeName = title.replace(/[\\/:*?"<>|]/g, "_");
        var filePath = currentFolderPath + "/" + safeName + ".md";

        // Add color as frontmatter
        var fileContent = "---\ncolor: " + color + "\n---\n" + content;
        FileBridge.writeFile(filePath, fileContent);
        loadCurrentFolder();
        return filePath;
    }

    // Save existing note
    function saveNote(filePath, content, color) {
        var fileContent = "---\ncolor: " + color + "\n---\n" + content;
        FileBridge.writeFile(filePath, fileContent);
    }

    // Read note content and metadata
    function readNote(filePath) {
        var content = FileBridge.readFile(filePath);
        var result = {
            "content": content,
            "color": "#624a73"
        };

        // Parse frontmatter if exists
        if (content.startsWith("---")) {
            var endIndex = content.indexOf("---", 3);
            if (endIndex > 0) {
                var frontmatter = content.substring(3, endIndex);
                var colorMatch = frontmatter.match(/color:\s*(#[a-fA-F0-9]+)/);
                if (colorMatch) {
                    result.color = colorMatch[1];
                }
                result.content = content.substring(endIndex + 4).trim();
            }
        }
        return result;
    }

    // Create a new folder
    function createFolder(name) {
        var safeName = name.replace(/[\\/:*?"<>|]/g, "_");
        var folderPath = currentFolderPath + "/" + safeName;
        FileBridge.createDirectory(folderPath);
        loadCurrentFolder();
    }

    // Delete file or folder
    function deleteItem(path, isFolder) {
        if (isFolder) {
            FileBridge.removeDirectory(path);
        } else {
            FileBridge.removeFile(path);
        }
        loadCurrentFolder();
    }

    // Rename a file or folder
    function renameItem(oldPath, newName, isFolder) {
        var parentPath = oldPath.substring(0, oldPath.lastIndexOf("/"));
        var safeName = newName.replace(/[\\/:*?"<>|]/g, "_");
        var newPath = parentPath + "/" + safeName + (isFolder ? "" : ".md");

        if (isFolder) {
            FileBridge.createDirectory(newPath);
        } else {
            var content = FileBridge.readFile(oldPath);
            FileBridge.writeFile(newPath, content);
            FileBridge.removeFile(oldPath);
        }
        loadCurrentFolder();
    }

    signal closeRequested
    signal requestInputMode(bool active)

    // Format path for display
    function formatDisplayPath(path) {
        var displayPath = path;
        if (displayPath.startsWith("file:///")) {
            displayPath = displayPath.substring(8);
        }

        // Replace slashes with separator
        displayPath = displayPath.replace(/\//g, " › ");
        displayPath = displayPath.replace(/\\/g, " › ");
        return displayPath;
    }

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

    // Home - File Browser View
    Component {
        id: homeComponent
        Item {
            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Title bar with navigation
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
                    Layout.leftMargin: 15
                    Layout.rightMargin: 15
                    spacing: 8

                    FluText {
                        id: noteTitle
                        text: folderStack.length > 0 ? FileBridge.getFileName(currentFolderPath) : "Notes"
                        font: FluTextStyle.Title
                        Layout.alignment: Qt.AlignVCenter
                        elide: Text.ElideMiddle
                    }

                    // Back button (when in subfolder)
                    FluIconButton {
                        visible: folderStack.length > 0
                        iconSource: FluentIcons.Back
                        iconSize: 14
                        Layout.leftMargin: 4
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: navigateBack()

                        FluTooltip {
                            visible: parent.hovered
                            text: "Go back"
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    // Change folder button
                    FluIconButton {
                        iconSource: FluentIcons.OpenFolderHorizontal
                        iconSize: 14
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: folderDialog.open()

                        FluTooltip {
                            visible: parent.hovered
                            text: "Open folder"
                        }
                    }

                    // Import markdown button
                    FluIconButton {
                        iconSource: FluentIcons.Download
                        iconSize: 14
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: importDialog.open()

                        FluTooltip {
                            visible: parent.hovered
                            text: "Import .md file"
                        }
                    }

                    // New folder button
                    FluIconButton {
                        iconSource: FluentIcons.NewFolder
                        iconSize: 14
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: newFolderDialog.open()

                        FluTooltip {
                            visible: parent.hovered
                            text: "New folder"
                        }
                    }

                    // New note button
                    FluIconButton {
                        iconSource: FluentIcons.Add
                        iconSize: 14
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: {
                            var timestamp = new Date().getTime();
                            var filePath = createNote("Untitled_" + timestamp, "", "#624a73");
                            stackView.push(editorComponent, {
                                notePath: filePath,
                                noteTitle: "Untitled_" + timestamp,
                                isEditing: true
                            });
                        }
                    }
                }

                // Current path indicator
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    Layout.leftMargin: 15
                    Layout.rightMargin: 15
                    color: FluTheme.dark ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(0, 0, 0, 0.02)
                    radius: 4

                    FluText {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        text: formatDisplayPath(currentFolderPath)
                        font.pixelSize: 11
                        color: FluTheme.dark ? "#888" : "#666"
                        elide: Text.ElideMiddle
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                // File/Folder list
                ListView {
                    id: listView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: notesModel
                    spacing: 8
                    topMargin: 10
                    bottomMargin: 10

                    delegate: Rectangle {
                        id: delegateRoot
                        width: 320
                        height: type === "folder" ? 56 : 80
                        radius: 8
                        x: (listView.width - width) / 2
                        color: itemMouseArea.containsMouse ? (FluTheme.dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.05)) : (FluTheme.dark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.02))

                        Behavior on color {
                            ColorAnimation {
                                duration: 150
                            }
                        }

                        // MouseArea first (behind content)
                        MouseArea {
                            id: itemMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            z: 0
                            onClicked: {
                                if (type === "folder") {
                                    navigateToFolder(model.path);
                                } else {
                                    stackView.push(editorComponent, {
                                        notePath: model.path,
                                        noteTitle: model.title,
                                        isEditing: false
                                    });
                                }
                            }
                        }

                        // Color indicator for notes
                        Rectangle {
                            visible: type === "note"
                            width: 4
                            height: parent.height - 16
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 8
                            radius: 2
                            color: model.color
                            z: 1
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: type === "note" ? 20 : 12
                            anchors.rightMargin: 8
                            spacing: 10
                            z: 1

                            // Icon
                            FluIcon {
                                iconSource: type === "folder" ? FluentIcons.FolderHorizontal : FluentIcons.QuickNote
                                iconSize: 20
                                color: type === "folder" ? "#FFB900" : (FluTheme.dark ? "#ccc" : "#666")
                                Layout.alignment: Qt.AlignVCenter
                            }

                            // Title and date
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 2

                                FluText {
                                    text: title
                                    font: FluTextStyle.Subtitle
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                FluText {
                                    text: date
                                    font.pixelSize: 11
                                    color: FluTheme.dark ? "#777" : "#999"
                                }
                            }

                            // Delete button
                            FluIconButton {
                                iconSource: FluentIcons.Delete
                                iconSize: 14
                                opacity: itemMouseArea.containsMouse ? 1 : 0
                                Layout.alignment: Qt.AlignVCenter

                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: 150
                                    }
                                }

                                onClicked: {
                                    deleteConfirmDialog.itemPath = model.path;
                                    deleteConfirmDialog.itemName = model.title;
                                    deleteConfirmDialog.isFolder = (type === "folder");
                                    deleteConfirmDialog.open();
                                }
                            }
                        }
                    }

                    // Empty state
                    Rectangle {
                        visible: notesModel.count === 0
                        anchors.centerIn: parent
                        width: 200
                        height: 120
                        color: "transparent"

                        Column {
                            anchors.centerIn: parent
                            spacing: 10

                            FluIcon {
                                iconSource: FluentIcons.Edit
                                iconSize: 48
                                color: FluTheme.dark ? "#555" : "#999"
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            FluText {
                                text: "No notes yet"
                                color: FluTheme.dark ? "#666" : "#888"
                                font.pixelSize: 14
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            FluText {
                                text: "Tap + to create one"
                                color: FluTheme.dark ? "#555" : "#999"
                                font.pixelSize: 12
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }
                    }
                }
            }
        }
    }

    // Editor Component
    Component {
        id: editorComponent
        Item {
            id: editorPage
            property string notePath: ""
            property string noteTitle: ""
            property bool isEditing: false

            property string currentContent: ""
            property string currentColor: "#624a73"

            Component.onCompleted: {
                root.requestInputMode(true);
                // Load note content
                if (notePath !== "" && FileBridge.exists(notePath)) {
                    var noteData = readNote(notePath);
                    currentContent = noteData.content;
                    currentColor = noteData.color;
                }
            }

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
                                id: titleInput
                                text: noteTitle
                                color: "#FFF"
                                font.pixelSize: 13
                                selectByMouse: true
                                Layout.fillWidth: true
                                onEditingFinished: {
                                    if (text !== noteTitle && text.trim() !== "") {
                                        renameItem(notePath, text, false);
                                        var parentPath = notePath.substring(0, notePath.lastIndexOf("/"));
                                        notePath = parentPath + "/" + text.replace(/[\\/:*?"<>|]/g, "_") + ".md";
                                        noteTitle = text;
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
                            onClicked: {
                                if (isEditing) {
                                    // Save when switching from edit to view
                                    saveNote(notePath, contentArea.text, currentColor);
                                }
                                isEditing = !isEditing;
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
                                saveNote(notePath, currentContent, currentColor);
                                loadCurrentFolder();
                                stackView.pop();
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

                        Keys.onPressed: event => {
                            if ((event.key === Qt.Key_V) && (event.modifiers & Qt.ControlModifier)) {
                                var dir = currentFolderPath + "/images";
                                if (!FileBridge.exists(dir)) {
                                    FileBridge.createDirectory(dir);
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
                                saveNote(notePath, currentContent, currentColor);
                                isEditing = false;
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
                saveNote(notePath, currentContent, c);
            }
        }
    }

    // Simple format button
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
            var srcPath = FileBridge.urlToPath(importDialog.file.toString());
            var fileName = FileBridge.getBaseName(srcPath);
            var content = FileBridge.readFile(srcPath);

            if (content !== "") {
                // Copy file to current folder
                var destPath = currentFolderPath + "/" + fileName + ".md";
                FileBridge.writeFile(destPath, content);
                loadCurrentFolder();
            }
        }
    }

    // Folder picker dialog
    FolderDialog {
        id: folderDialog
        title: "Select Notes Folder"
        folder: StandardPaths.standardLocations(StandardPaths.DocumentsLocation)[0]
        onAccepted: {
            var newPath = FileBridge.urlToPath(folderDialog.folder.toString());
            notesRootPath = newPath;
            currentFolderPath = newPath;
            folderStack = [];
            loadCurrentFolder();
        }
    }

    // New folder dialog
    FluContentDialog {
        id: newFolderDialog
        title: "New Folder"
        implicitWidth: 340

        contentDelegate: Component {
            ColumnLayout {
                spacing: 10
                FluText {
                    text: "Enter a name for the new folder:"
                }
                FluTextBox {
                    id: newFolderNameInput
                    Layout.fillWidth: true
                    placeholderText: "Folder name"
                    Component.onCompleted: {
                        newFolderDialog.folderNameInput = this;
                    }
                }
            }
        }
        property var folderNameInput: null
        negativeText: "Cancel"
        positiveText: "Create"
        buttonFlags: FluContentDialogType.NegativeButton | FluContentDialogType.PositiveButton
        onOpened: root.requestInputMode(true)
        onClosed: root.requestInputMode(false)
        onPositiveClicked: {
            if (folderNameInput && folderNameInput.text.trim() !== "") {
                createFolder(folderNameInput.text.trim());
                folderNameInput.text = "";
            }
        }
    }

    // Delete confirmation dialog
    FluContentDialog {
        id: deleteConfirmDialog
        title: "Delete Item"

        implicitWidth: 340

        property string itemPath: ""
        property string itemName: ""
        property bool isFolder: false

        message: "Are you sure you want to delete \"" + itemName + "\"?" + (isFolder ? "\n\nThis will delete all contents inside the folder." : "")
        negativeText: "Cancel"
        positiveText: "Delete"
        buttonFlags: FluContentDialogType.NegativeButton | FluContentDialogType.PositiveButton
        onOpened: root.requestInputMode(true)
        onClosed: root.requestInputMode(false)
        onPositiveClicked: {
            deleteItem(itemPath, isFolder);
        }
    }
}
