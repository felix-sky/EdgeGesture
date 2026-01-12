import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import Qt.labs.platform 1.1
import FluentUI 1.0
import EdgeGesture.Notes 1.0
import "./notes"

Item {
    id: root
    width: 360
    anchors.top: parent.top
    anchors.bottom: parent.bottom

    // Base notes folder - convert URL to local path if needed
    property string configPath: StandardPaths.standardLocations(StandardPaths.DocumentsLocation)[0] + "/EdgeGesture/notes.json"
    property string notesRootPath: {
        var docPath = StandardPaths.standardLocations(StandardPaths.DocumentsLocation)[0];
        if (docPath.toString().startsWith("file:///")) {
            docPath = docPath.toString().substring(8);
        }
        return docPath + "/EdgeGesture/Notes";
    }

    Component.onCompleted: {
        var dir = StandardPaths.standardLocations(StandardPaths.DocumentsLocation)[0] + "/EdgeGesture";
        if (!FileBridge.exists(dir)) {
            FileBridge.createDirectory(dir);
        }

        if (FileBridge.exists(configPath)) {
            var content = FileBridge.readFile(configPath);
            if (content) {
                try {
                    var config = JSON.parse(content);
                    if (config.lastRootPath) {
                        notesRootPath = config.lastRootPath;
                        notesModel.rootPath = notesRootPath;
                        NotesIndex.setRootPath(notesRootPath);
                    }
                } catch (e) {
                    console.log("Error loading notes config:", e);
                }
            }
        }
    }

    signal closeRequested
    signal requestInputMode(bool active)

    // NotesModel from plugin - handles data and async scanning
    NotesModel {
        id: notesModel
        rootPath: notesRootPath
    }

    // NotesFileHandler from plugin - handles file operations
    NotesFileHandler {
        id: notesFileHandler
    }

    property var notesIndex: NotesIndex

    Component.onCompleted: {}

    // Main Navigation view
    StackView {
        id: stackView
        anchors.fill: parent
        clip: true

        // Push initial item after component is complete
        Component.onCompleted: {
            stackView.push(notesHomeComponent, {
                notesModel: notesModel,
                notesFileHandler: notesFileHandler
            });
        }
    }

    Component {
        id: notesHomeComponent
        NotesHome {
            // Properties are set via StackView.push()
            onRequestInputMode: active => root.requestInputMode(active)
            onOpenFolderRequested: folderDialog.open()
            onImportRequested: importDialog.open()
            onNewFolderRequested: newFolderDialog.open()
            onDeleteRequested: (path, title, isFolder) => {
                deleteConfirmDialog.itemPath = path;
                deleteConfirmDialog.itemName = title;
                deleteConfirmDialog.isFolder = isFolder;
                deleteConfirmDialog.open();
            }
            onOpenNoteRequested: (path, title, isEditing) => {
                stackView.push(notesEditorComponent, {
                    notePath: path,
                    noteTitle: title,
                    isEditing: isEditing,
                    notesFileHandler: notesFileHandler,
                    notesIndex: root.notesIndex,
                    notesModel: notesModel,
                    vaultRootPath: notesRootPath
                });
            }
        }
    }

    Component {
        id: notesEditorComponent
        NotesEditor {
            // Properties are set via StackView.push()
            onInputModeRequested: active => root.requestInputMode(active)
            onCloseRequested: {
                stackView.pop();
                // Force refresh the model after returning to home
                notesModel.refresh();
            }
            onAddTagRequested: path => {
                addTagDialog.notePath = path;
                addTagDialog.open();
            }
            onLinkOpened: (path, title) => {
                stackView.push(notesEditorComponent, {
                    notePath: path,
                    noteTitle: title,
                    isEditing: false,
                    notesFileHandler: notesFileHandler,
                    notesIndex: root.notesIndex,
                    notesModel: notesModel,
                    vaultRootPath: notesRootPath
                });
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
            var srcPath = notesFileHandler.urlToPath(importDialog.file.toString());
            var fileName = notesFileHandler.getBaseName(srcPath);
            var noteData = notesFileHandler.readNote(srcPath);

            if (noteData.content !== "") {
                // Copy file to current folder
                notesFileHandler.createNote(notesModel.currentPath, fileName, noteData.content, noteData.color);
                notesModel.refresh();
            }
        }
    }

    // Folder picker dialog
    FolderDialog {
        id: folderDialog
        title: "Select Notes Folder"
        folder: StandardPaths.standardLocations(StandardPaths.DocumentsLocation)[0]
        onAccepted: {
            var newPath = notesFileHandler.urlToPath(folderDialog.folder.toString());
            // Update the root path for model, index, AND notes editor
            notesRootPath = newPath;

            // Save config
            var data = {
                "lastRootPath": newPath
            };
            FileBridge.writeFile(configPath, JSON.stringify(data, null, 4));

            notesModel.rootPath = newPath;
            NotesIndex.setRootPath(newPath);
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
                notesFileHandler.createFolder(notesModel.currentPath, folderNameInput.text.trim());
                notesModel.refresh();
                folderNameInput.text = "";
                notesIndex.updateIndex();
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
            notesFileHandler.deleteItem(itemPath, isFolder);
            notesModel.refresh();
        }
    }

    // Add Tag dialog
    FluContentDialog {
        id: addTagDialog
        title: "Add Tag"
        implicitWidth: 340

        property string notePath: ""

        contentDelegate: Component {
            ColumnLayout {
                spacing: 10
                FluText {
                    text: "Enter tag name:"
                }
                FluTextBox {
                    id: newTagInput
                    Layout.fillWidth: true
                    placeholderText: "e.g. work, project, ideas"
                    Component.onCompleted: {
                        addTagDialog.tagInput = this;
                    }
                }
                FluText {
                    text: "Current tags: " + notesFileHandler.getTags(addTagDialog.notePath).join(", ")
                    color: FluTheme.dark ? "#888" : "#666"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
        }
        property var tagInput: null
        property string tagText: ""
        negativeText: "Cancel"
        positiveText: "Add"
        buttonFlags: FluContentDialogType.NegativeButton | FluContentDialogType.PositiveButton
        onOpened: {
            root.requestInputMode(true);
            tagText = "";
        }
        onClosed: {
            root.requestInputMode(false);
            tagText = "";
            tagInput = null;
        }
        onPositiveClicked: {
            var inputText = tagInput ? tagInput.text.trim() : "";
            if (inputText !== "") {
                var currentTags = notesFileHandler.getTags(addTagDialog.notePath);
                if (currentTags.indexOf(inputText) === -1) {
                    currentTags.push(inputText);
                    notesFileHandler.updateFrontmatter(addTagDialog.notePath, "tags", currentTags);
                    notesIndex.updateEntry(addTagDialog.notePath);
                }
            }
        }
    }
}
