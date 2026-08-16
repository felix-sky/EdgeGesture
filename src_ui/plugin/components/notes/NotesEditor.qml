import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import Qt.labs.platform 1.1
import Qt.labs.qmlmodels 1.0
import FluentUI 1.0
import EdgeGesture.Notes 1.0
import "./blocks"

Item {
    id: editorPage

    property string notePath: ""
    property string noteTitle: ""
    property bool isEditing: false

    property var notesFileHandler: null
    property var notesIndex: null
    property var notesModel: null
    property string vaultRootPath: "" // Root of the notes vault for image search

    // Single source of truth for editing state
    property int editingBlockIndex: isEditing ? 0 : -1

    property string currentColor: "#624a73"

    signal inputModeRequested(bool active)
    signal closeRequested
    signal addTagRequested(string path)
    signal linkOpened(string path, string title)

    function beginEditing(index) {
        editingBlockIndex = index;
    }

    function endEditing(index) {
        if (editingBlockIndex === index || index === undefined) {
            editingBlockIndex = -1;
        }
    }

    // Navigation function: go to specific block
    function navigateToBlock(index, cursorAtEnd) {
        if (index >= 0 && index < blockModel.rowCount()) {
            editorPage._cursorAtEnd = cursorAtEnd !== undefined ? cursorAtEnd : true;
            editingBlockIndex = index;
            listView.positionViewAtIndex(index, ListView.Contain);
        }
    }
    property bool _cursorAtEnd: true // Internal: cursor position for next focus

    // Navigate to anchor (heading or blockId)
    function navigateToAnchor(heading, blockId) {
        if (!heading && !blockId)
            return;
        for (var i = 0; i < blockModel.rowCount(); ++i) {
            var bType = blockModel.data(blockModel.index(i, 0), NoteBlockModel.TypeRole);
            var bContent = blockModel.data(blockModel.index(i, 0), NoteBlockModel.ContentRole);
            if (heading && bType === "heading" && bContent && bContent.toString().trim().toLowerCase() === heading.toLowerCase()) {
                listView.positionViewAtIndex(i, ListView.Beginning);
                return;
            }
            if (blockId && bContent && bContent.toString().indexOf("^" + blockId) !== -1) {
                listView.positionViewAtIndex(i, ListView.Beginning);
                return;
            }
        }
    }

    function handleLinkActivation(rawLink) {
        if (rawLink.startsWith("http://") || rawLink.startsWith("https://") || rawLink.startsWith("mailto:") || rawLink.startsWith("file://")) {
            Qt.openUrlExternally(rawLink);
            return;
        }

        if (!notesIndex)
            return;

        var info = notesIndex.resolveLinkInfo(rawLink, notePath);
        if (info.kind === "found") {
            if (info.bestMatch === notePath || info.bestMatch === "") {
                navigateToAnchor(info.heading, info.blockId);
            } else {
                var targetTitle = info.target;
                if (targetTitle.endsWith(".md")) {
                    targetTitle = targetTitle.substring(0, targetTitle.length - 3);
                }
                editorPage.linkOpened(info.bestMatch, targetTitle);
            }
        } else if (info.kind === "ambiguous") {
            // If ambiguous, open best match
            if (info.bestMatch) {
                editorPage.linkOpened(info.bestMatch, info.target);
            }
        } else {
            // Missing: prompt confirmation
            createMissingNoteDialog.missingTitle = info.target;
            createMissingNoteDialog.open();
        }
    }

    // Check brightness of background color
    function isDarkColor(c) {
        if (!c)
            return true;
        if (c.charAt(0) !== '#')
            return true;
        var r = parseInt(c.substr(1, 2), 16);
        var g = parseInt(c.substr(3, 2), 16);
        var b = parseInt(c.substr(5, 2), 16);
        var yiq = ((r * 299) + (g * 587) + (b * 114)) / 1000;
        return (yiq < 128);
    }

    property color contrastColor: isDarkColor(currentColor) ? "#FFFFFF" : "#202020"
    property color secondaryContrastColor: isDarkColor(currentColor) ? "#CCCCCC" : "#444444"
    property color editBackgroundColor: isDarkColor(currentColor) ? Qt.rgba(0, 0, 0, 0.2) : Qt.rgba(0, 0, 0, 0.05)

    function goToPreviousBlock(fromIndex) {
        if (fromIndex > 0) {
            navigateToBlock(fromIndex - 1, true);
        }
    }

    function goToNextBlock(fromIndex) {
        if (fromIndex < blockModel.rowCount() - 1) {
            navigateToBlock(fromIndex + 1, false);
        }
    }

    NoteBlockModel {
        id: blockModel
    }

    Component.onCompleted: {
        editorPage.inputModeRequested(true);
        loadNote();
    }

    Component.onDestruction: {
        editorPage.inputModeRequested(false);
        resetContainerColor();
    }

    function loadNote() {
        if (notePath !== "" && notesFileHandler && notesFileHandler.exists(notePath)) {
            var noteData = notesFileHandler.readNote(notePath);
            blockModel.loadMarkdown(noteData.content);
            currentColor = noteData.color;
            updateContainerColor();
        }
    }

    Connections {
        target: blockModel
        function onLoadingChanged() {
            if (!blockModel.loading) {
                if (blockModel.rowCount() === 0) {
                    blockModel.insertBlock(0, "paragraph", "");
                    if (isEditing) {
                        editorPage.beginEditing(0);
                    }
                } else if (isEditing) {
                    Qt.callLater(function () {
                        editorPage.navigateToBlock(0, false);
                    });
                }
            }
        }
    }

    function saveNote() {
        var markdown = blockModel.getMarkdown();
        notesFileHandler.saveNote(notePath, markdown, currentColor);
        notesIndex.updateEntry(notePath);
    }

    Rectangle {
        anchors.fill: parent
        color: currentColor
        Behavior on color {
            ColorAnimation {
                duration: 333
                easing.type: Easing.OutQuint
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top bar
        Item {
            id: topBar
            Layout.fillWidth: true
            Layout.preferredHeight: 60

            // Title
            Rectangle {
                height: 32
                width: 180
                radius: 16
                color: isDarkColor(currentColor) ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(0, 0, 0, 0.05)
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
                        color: contrastColor
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

                FluIconButton {
                    iconSource: FluentIcons.Save
                    iconSize: 16
                    iconColor: contrastColor
                    onClicked: {
                        saveNote();
                    }
                    FluTooltip {
                        visible: parent.hovered
                        text: "Save"
                    }
                }

                FluIconButton {
                    iconSource: FluentIcons.Tag
                    iconSize: 16
                    iconColor: contrastColor
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
                    iconColor: contrastColor
                    onClicked: colorPickerMenu.open()
                }

                FluIconButton {
                    iconSource: FluentIcons.Back
                    iconSize: 16
                    iconColor: contrastColor
                    onClicked: {
                        saveNote();
                        resetContainerColor();
                        editorPage.closeRequested();
                    }
                }
            }
        }

        // Virtualized Block List
        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 15
            Layout.rightMargin: 15
            clip: true
            model: blockModel
            spacing: 10
            reuseItems: true
            cacheBuffer: 1000

            MouseArea {
                anchors.fill: parent
                z: -1
                onClicked: {
                    var lastIndex = blockModel.rowCount() - 1;
                    if (lastIndex >= 0) {
                        editorPage.navigateToBlock(lastIndex, true);
                    }
                }
            }

            delegate: BlockDelegate {
                id: blockDelegate
                width: ListView.view.width

                editor: editorPage
                noteListView: listView
                notesIndex: editorPage.notesIndex
                notesFileHandler: editorPage.notesFileHandler
                notePath: editorPage.notePath
                vaultRootPath: editorPage.vaultRootPath

                onLinkActivatedCallback: function (link) {
                    editorPage.handleLinkActivation(link);
                }
            }

            ScrollBar.vertical: FluScrollBar {}
        }
    }

    FluContentDialog {
        id: createMissingNoteDialog
        title: "Create Note"
        implicitWidth: 340
        property string missingTitle: ""
        message: "The note \"" + missingTitle + "\" does not exist. Do you want to create it?"
        negativeText: "Cancel"
        positiveText: "Create"
        buttonFlags: FluContentDialogType.NegativeButton | FluContentDialogType.PositiveButton
        onPositiveClicked: {
            if (missingTitle !== "" && notesFileHandler) {
                var folder = notePath ? notePath.substring(0, notePath.lastIndexOf("/")) : vaultRootPath;
                var newPath = notesFileHandler.createNote(folder, missingTitle, "", "#624a73");
                if (newPath !== "") {
                    notesIndex.updateEntry(newPath);
                    editorPage.linkOpened(newPath, missingTitle);
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
        updateContainerColor();
        saveNote();
    }

    function updateContainerColor() {
        if (Window.window && typeof Window.window.setBackgroundColor === "function") {
            Window.window.setBackgroundColor(currentColor);
        }
    }

    function resetContainerColor() {
        if (Window.window && typeof Window.window.resetBackgroundColor === "function") {
            Window.window.resetBackgroundColor();
        }
    }
}
