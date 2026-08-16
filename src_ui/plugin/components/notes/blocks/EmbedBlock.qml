import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: root
    width: ListView.view ? ListView.view.width - 20 : 300
    implicitHeight: editorLoader.item ? editorLoader.item.height : 0

    property string content: model.content ? model.content : ""
    property bool isEditing: false

    property string folderPath: ""
    property var noteListView: null
    property var editor: null
    property var onLinkActivatedCallback: null
    property var notesIndex: null
    property var notesFileHandler: null
    property string notePath: ""
    property string vaultRootPath: ""
    property int blockIndex: -1
    property string type: "embed"
    property int level: 0

    // Parsed properties
    property string targetNote: ""
    property string targetSection: ""
    property string targetBlockId: ""
    property string embedTitle: ""
    property string embedBody: ""
    property bool embedLoaded: false
    property string errorMsg: ""

    onContentChanged: parseContent()
    onNotesIndexChanged: loadEmbedContent()
    onNotesFileHandlerChanged: loadEmbedContent()

    function parseContent() {
        var text = root.content;
        var hashIndex = text.indexOf('#');
        if (hashIndex !== -1) {
            targetNote = text.substring(0, hashIndex);
            var rest = text.substring(hashIndex + 1);
            if (rest.startsWith('^')) {
                targetBlockId = rest.substring(1);
                targetSection = "";
            } else {
                targetSection = rest;
                targetBlockId = "";
            }
        } else {
            targetNote = text;
            targetSection = "";
            targetBlockId = "";
        }

        loadEmbedContent();
    }

    function loadEmbedContent() {
        if (!notesFileHandler)
            return;

        var notePathToLoad = "";
        if (notesIndex && targetNote !== "") {
            notePathToLoad = notesIndex.findPathByTitle(targetNote);
        }

        if (notePathToLoad === "" && targetNote !== "" && folderPath) {
            var local = folderPath + "/" + targetNote + ".md";
            if (notesFileHandler.exists(local)) {
                notePathToLoad = local;
            }
        }

        if (notePathToLoad === "") {
            errorMsg = "Note not found: " + targetNote;
            embedLoaded = false;
            return;
        }

        var extracted = "";
        if (targetBlockId !== "") {
            extracted = notesFileHandler.extractBlock(notePathToLoad, targetBlockId);
            embedTitle = targetNote + " > ^" + targetBlockId;
        } else if (targetSection !== "") {
            extracted = notesFileHandler.extractSection(notePathToLoad, targetSection);
            embedTitle = targetNote + " > " + targetSection;
        } else {
            var data = notesFileHandler.readNote(notePathToLoad);
            extracted = data.content;
            if (extracted.length > 500)
                extracted = extracted.substring(0, 500) + "...";
            embedTitle = targetNote;
        }

        if (extracted === "") {
            errorMsg = "Content not found";
            embedLoaded = false;
        } else {
            embedBody = extracted;
            embedLoaded = true;
            errorMsg = "";
        }
    }

    readonly property color borderColor: FluTheme.dark ? "#60ccff" : "#0099cc"
    readonly property color errorColor: FluTheme.dark ? "#ff4d4f" : "#c50f1f"
    readonly property color bgColor: FluTheme.dark ? "#4d000000" : "#0d000000"

    Loader {
        id: editorLoader
        width: parent.width
        sourceComponent: isEditing ? editorComp : viewerComp
    }

    Component {
        id: viewerComp
        Rectangle {
            id: container
            width: root.width
            height: layout.implicitHeight + 20
            color: root.embedLoaded ? root.bgColor : (FluTheme.dark ? "rgba(255,0,0,0.1)" : "rgba(255,0,0,0.05)")
            border.color: root.embedLoaded ? "transparent" : root.errorColor
            border.width: root.embedLoaded ? 0 : 1
            radius: 4

            Rectangle {
                width: 4
                height: parent.height
                color: root.borderColor
                anchors.left: parent.left
                visible: root.embedLoaded
                radius: 4
            }

            ColumnLayout {
                id: layout
                anchors.fill: parent
                anchors.leftMargin: 15
                anchors.rightMargin: 10
                anchors.topMargin: 10
                anchors.bottomMargin: 10
                spacing: 5

                Text {
                    text: root.embedLoaded ? root.embedTitle : "Embed Error"
                    font.bold: true
                    font.pixelSize: 14
                    color: root.embedLoaded ? root.borderColor : root.errorColor
                    Layout.fillWidth: true
                }

                Text {
                    Layout.fillWidth: true
                    text: root.embedLoaded ? root.embedBody : root.errorMsg
                    wrapMode: Text.Wrap
                    color: FluTheme.dark ? "#e2e8f0" : "#2d3748"
                    font.pixelSize: 14
                    font.family: "Segoe UI"
                    textFormat: Text.RichText
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onClicked: {
                    if (root.editor) {
                        root.editor.beginEditing(root.blockIndex);
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

            onEditingFinished: finishEdit()
            onActiveFocusChanged: {
                if (!activeFocus)
                    finishEdit();
            }

            function finishEdit() {
                if (root.isEditing) {
                    if (root.noteListView && root.noteListView.model) {
                        root.noteListView.model.replaceBlock(root.blockIndex, text.trim());
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

    Connections {
        target: notesIndex
        function onIndexUpdated() {
            loadEmbedContent();
        }
        ignoreUnknownSignals: true
    }
}
