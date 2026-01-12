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
    property bool isEditing: false // Kept for compatibility, though blocks are interactive

    property var notesFileHandler: null
    property var notesIndex: null
    property var notesModel: null
    property string vaultRootPath: "" // Root of the notes vault for image search

    property int pendingFocusIndex: -1 // Track which block should get focus on load
    property int activeBlockIndex: -1 // Currently active/focused block (Obsidian-style)

    property string currentColor: "#624a73"

    signal inputModeRequested(bool active)
    signal closeRequested
    signal addTagRequested(string path)
    signal linkOpened(string path, string title)
    signal requestBlockEdit(int index)
    signal navigateUp(int fromIndex) // Signal to navigate to previous block
    signal navigateDown(int fromIndex) // Signal to navigate to next block

    // Navigation function: go to specific block
    function navigateToBlock(index, cursorAtEnd) {
        if (index >= 0 && index < blockModel.rowCount()) {
            activeBlockIndex = index;
            pendingFocusIndex = index;
            // Store cursor position preference
            editorPage._cursorAtEnd = cursorAtEnd !== undefined ? cursorAtEnd : true;
            requestBlockEdit(index);
        }
    }
    property bool _cursorAtEnd: true // Internal: cursor position for next focus

    // check brightness of background color
    function isDarkColor(c) {
        if (!c)
            return true; // Default to dark if undefined
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

    // Navigate to previous block (called from block when arrow up at first line)
    function goToPreviousBlock(fromIndex) {
        if (fromIndex > 0) {
            navigateToBlock(fromIndex - 1, true); // cursor at end
        }
    }

    // Navigate to next block (called from block when arrow down at last line)
    function goToNextBlock(fromIndex) {
        if (fromIndex < blockModel.rowCount() - 1) {
            navigateToBlock(fromIndex + 1, false); // cursor at start
        }
    }

    NoteBlockModel {
        id: blockModel
        onLoadingChanged: {
            if (!loading) {
                // Scroll to top or restore position?
            }
        }
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
            // noteData.content is the raw markdown string
            blockModel.loadMarkdown(noteData.content);
            currentColor = noteData.color;
            updateContainerColor();

            // Ensure we have at least one block for editing if empty
            if (noteData.content.trim() === "") {}
        }
    }

    Connections {
        target: blockModel
        function onLoadingChanged() {
            if (!blockModel.loading) {
                if (blockModel.rowCount() === 0) {
                    blockModel.insertBlock(0, "paragraph", "");
                }
                // Always auto-focus first block when note loads (Obsidian-style)
                Qt.callLater(function () {
                    editorPage.navigateToBlock(0, false);
                });
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

                // Save Button (Explicit save is good for MVVM/File ops)
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
                        saveNote(); // Auto-save on close
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

            // Handle clicks on empty area (below all delegates)
            MouseArea {
                anchors.fill: parent
                z: -1 // Behind delegates so delegate clicks still work
                onClicked: {
                    // Focus the last block when clicking empty area
                    var lastIndex = blockModel.rowCount() - 1;
                    if (lastIndex >= 0) {
                        editorPage.navigateToBlock(lastIndex, true);
                    }
                }
            }

            // DelegateChooser-based block rendering
            delegate: BlockDelegate {
                id: blockDelegate
                width: ListView.view.width

                // Pass editor context
                editor: editorPage
                noteListView: listView
                notesIndex: editorPage.notesIndex
                notesFileHandler: editorPage.notesFileHandler
                notePath: editorPage.notePath
                vaultRootPath: editorPage.vaultRootPath

                // Link callback
                onLinkActivatedCallback: function (link) {
                    var title = link.replace(/\\/g, "/");
                    var lastSlash = title.lastIndexOf("/");
                    if (lastSlash >= 0) {
                        title = title.substring(lastSlash + 1);
                    }
                    if (title.endsWith(".md")) {
                        title = title.substring(0, title.length - 3);
                    }
                    editorPage.linkOpened(link, title);
                }

                // Handle focus requests
                Connections {
                    target: editorPage
                    function onRequestBlockEdit(idx) {
                        if (idx === blockDelegate.index) {
                            listView.currentIndex = idx;
                            blockDelegate.isEditing = true;
                        }
                    }
                    ignoreUnknownSignals: true
                }

                // Handle pending focus on component ready
                Component.onCompleted: {
                    if (index === editorPage.pendingFocusIndex) {
                        listView.currentIndex = index;
                        isEditing = true;
                        editorPage.pendingFocusIndex = -1;
                    }
                }
            }

            // ScrollBar
            ScrollBar.vertical: FluScrollBar {}
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
