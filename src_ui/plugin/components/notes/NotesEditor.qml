import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import Qt.labs.platform 1.1
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

    property string currentColor: "#624a73"

    signal inputModeRequested(bool active)
    signal closeRequested
    signal addTagRequested(string path)
    signal linkOpened(string path, string title)

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

    Component.onDestruction: editorPage.inputModeRequested(false)

    function loadNote() {
        if (notePath !== "" && notesFileHandler && notesFileHandler.exists(notePath)) {
            var noteData = notesFileHandler.readNote(notePath);
            // noteData.content is the raw markdown string
            blockModel.loadMarkdown(noteData.content);
            currentColor = noteData.color;
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

                // Save Button (Explicit save is good for MVVM/File ops)
                FluIconButton {
                    iconSource: FluentIcons.Save
                    iconSize: 16
                    iconColor: "white"
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
                        saveNote(); // Auto-save on close
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

            // Optimization: Cache buffer for smooth scrolling
            cacheBuffer: 1000

            // Delegate Chooser Logic
            delegate: Loader {
                id: delegateLoader

                // Set the width for the delegate to fill the view
                width: ListView.view.width

                // Helper to get folder path from notePath, handling both slashes
                property string folderPath: {
                    var p = notePath.replace(/\\/g, "/");
                    return p.substring(0, p.lastIndexOf("/"));
                }

                onLoaded: {
                    if (item) {
                        // Manually assign the callback function to avoid QML Binding type issues with functions
                        item.onLinkActivatedCallback = function (link) {
                            editorPage.linkOpened(link, "");
                        };
                    }
                }

                Binding {
                    target: delegateLoader.item
                    property: "folderPath"
                    value: delegateLoader.folderPath
                    when: delegateLoader.status === Loader.Ready
                }

                Binding {
                    target: delegateLoader.item
                    property: "blockIndex" // Pass index explicitly
                    value: index
                    when: delegateLoader.status === Loader.Ready
                }

                Binding {
                    target: delegateLoader.item
                    property: "noteListView" // Pass ListView explicitly
                    value: listView
                    when: delegateLoader.status === Loader.Ready
                }

                Binding {
                    target: delegateLoader.item
                    property: "notesIndex" // Pass NotesIndex for lookup
                    value: notesIndex
                    when: delegateLoader.status === Loader.Ready
                }

                Binding {
                    target: delegateLoader.item
                    property: "notesFileHandler" // Pass NotesFileHandler for image search
                    value: notesFileHandler
                    when: delegateLoader.status === Loader.Ready
                }

                Binding {
                    target: delegateLoader.item
                    property: "notePath" // Pass notePath for image search context
                    value: notePath
                    when: delegateLoader.status === Loader.Ready
                }

                Binding {
                    target: delegateLoader.item
                    property: "vaultRootPath" // Pass vault root for image search
                    value: vaultRootPath
                    when: delegateLoader.status === Loader.Ready
                }

                source: {
                    switch (model.type) {
                    case "heading":
                        return "blocks/HeadingBlock.qml";
                    case "code":
                        return "blocks/CodeBlock.qml";
                    case "quote":
                        return "blocks/QuoteBlock.qml";
                    // Future: ImageBlock, ListBlock
                    default:
                        return "blocks/ParagraphBlock.qml";
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
        saveNote();
    }
}
