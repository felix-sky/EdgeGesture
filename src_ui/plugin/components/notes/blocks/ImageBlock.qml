import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: root
    width: ListView.view ? ListView.view.width - 20 : 300
    // Height depends on image aspect ratio, but we limit max height
    implicitHeight: editorLoader.item ? editorLoader.item.height : 0

    property string content: model.content ? model.content : "" // "image.png"
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

    onContentChanged: loadImage()

    function loadImage() {
        // Content is filename "image.png"
        var imageName = root.content;

        // Handle markdown image syntax replacement if leaked: ![alt](src) -> src
        // But the parser should handle that.

        if (!notesFileHandler)
            return;

        var foundPath = "";

        // 1. Try vault search
        if (notePath && vaultRootPath) {
            foundPath = notesFileHandler.findImage(imageName, notePath, vaultRootPath);
        }

        // 2. Try relative
        if (foundPath === "" && folderPath) {
            var rel = folderPath + "/" + imageName;
            if (notesFileHandler.exists(rel)) {
                foundPath = rel;
            }
        }

        if (foundPath !== "") {
            sourceUrl = "file:///" + foundPath.replace(/\\/g, "/");
            imageLoaded = true;
            errorMsg = "";
        } else {
            sourceUrl = "";
            imageLoaded = false;
            errorMsg = "Image not found: " + imageName;
        }
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
            height: imgContainer.height + 10 // Padding

            Rectangle {
                id: imgContainer
                width: parent.width
                height: image.status === Image.Ready ? Math.min(image.sourceSize.height * (width / image.sourceSize.width), 500) : 50
                // logical height: maintain aspect ratio, max 500px height?

                color: "transparent"
                radius: 4
                clip: true

                anchors.centerIn: parent

                Image {
                    id: image
                    source: root.sourceUrl
                    width: parent.width
                    sourceSize.width: parent.width  // Limit decode to display width (memory optimization)
                    asynchronous: true  // Load in background thread to prevent UI blocking
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
        FluentEditorArea {
            width: parent.width
            // Reconstruct markdown syntax
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
