import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: root
    width: 300
    implicitHeight: editorLoader.item ? editorLoader.item.height : 0

    property string content: model.content ? model.content : ""
    property var metadata: ({})
    property bool isEditing: false

    property string folderPath: ""
    property var noteListView: null
    property var editor: null
    property var notesIndex: null
    property var notesFileHandler: null
    property string notePath: ""
    property string vaultRootPath: ""
    property int blockIndex: -1
    property string type: "image"

    property string sourceUrl: ""
    property bool imageLoaded: false
    property string errorMsg: ""
    property string pendingImageName: ""

    onContentChanged: loadImage()
    onNotesFileHandlerChanged: loadImage()
    onNotePathChanged: loadImage()
    onVaultRootPathChanged: loadImage()
    onFolderPathChanged: loadImage()

    Connections {
        target: notesFileHandler
        function onImagePathFound(originalName, foundPath) {
            if (originalName !== root.pendingImageName)
                return;

            if (foundPath !== "") {
                root.sourceUrl = "file:///" + foundPath.replace(/\\/g, "/");
                root.imageLoaded = true;
                root.errorMsg = "";
            } else {
                root.sourceUrl = "";
                root.imageLoaded = false;
                root.errorMsg = "Image not found: " + originalName;
            }
            root.pendingImageName = "";
        }
    }

    function loadImage() {
        var imageName = root.content;
        if (!imageName || imageName === "")
            return;

        if (!notesFileHandler)
            return;

        imageLoaded = false;
        pendingImageName = imageName;

        // Try fast attachment index first
        if (notesIndex) {
            var found = notesIndex.findAttachment(imageName, notePath);
            if (found && found !== "") {
                sourceUrl = "file:///" + found.replace(/\\/g, "/");
                imageLoaded = true;
                errorMsg = "";
                pendingImageName = "";
                return;
            }
        }

        // Try async vault search
        if (notePath && vaultRootPath) {
            notesFileHandler.findImageAsync(imageName, notePath, vaultRootPath);
            return;
        }

        // Try relative path
        if (folderPath) {
            var rel = folderPath + "/" + imageName;
            if (notesFileHandler.exists(rel)) {
                sourceUrl = "file:///" + rel.replace(/\\/g, "/");
                imageLoaded = true;
                errorMsg = "";
                pendingImageName = "";
                return;
            }
        }

        sourceUrl = "";
        imageLoaded = false;
        errorMsg = "Image not found: " + imageName;
        pendingImageName = "";
    }

    Component.onCompleted: loadImage()

    Loader {
        id: editorLoader
        width: parent.width
        sourceComponent: isEditing ? editorComp : viewerComp
    }

    Component {
        id: viewerComp
        Item {
            width: root.width
            height: imgContainer.height + 10

            Rectangle {
                id: imgContainer
                width: {
                    if (root.metadata && root.metadata.width && root.metadata.width > 0) {
                        return Math.min(root.metadata.width, parent.width);
                    }
                    return parent.width;
                }
                height: image.status === Image.Ready ? Math.min(image.implicitHeight, 600) : 50
                color: "transparent"
                radius: 4
                clip: true
                anchors.left: parent.left

                Image {
                    id: image
                    source: root.sourceUrl
                    width: parent.width
                    sourceSize.width: parent.width
                    asynchronous: true
                    fillMode: Image.PreserveAspectFit
                    visible: root.imageLoaded && status === Image.Ready
                    horizontalAlignment: Image.AlignLeft

                    onStatusChanged: {
                        if (status === Image.Error) {
                            root.imageLoaded = false;
                            root.errorMsg = "Failed to load image";
                        }
                    }
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    running: image.status === Image.Loading
                    visible: running
                }

                Text {
                    anchors.centerIn: parent
                    text: root.errorMsg
                    color: FluTheme.dark ? "#ff4d4f" : "#c50f1f"
                    visible: !root.imageLoaded
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
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
            text: "![[" + root.content + "]]"

            customTextColor: FluTheme.dark ? "#FFFFFF" : "#000000"
            customSelectionColor: FluTheme.primaryColor
            customBackgroundColor: FluTheme.dark ? Qt.rgba(0, 0, 0, 0.2) : Qt.rgba(0, 0, 0, 0.05)

            Keys.onReturnPressed: event => {
                event.accepted = true;
                finishEdit();
            }

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

            onEditingFinished: finishEdit()
            onActiveFocusChanged: {
                if (!activeFocus)
                    finishEdit();
            }

            function finishEdit() {
                if (root.isEditing) {
                    if (root.noteListView && root.noteListView.model) {
                        root.noteListView.model.replaceBlock(root.blockIndex, text);
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
