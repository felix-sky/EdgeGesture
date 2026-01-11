import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: root
    width: ListView.view ? ListView.view.width - 20 : 300
    implicitHeight: editorLoader.item ? editorLoader.item.height : 0

    property string content: model.content ? model.content : "" // "NoteName#Section"
    property bool isEditing: false

    property string folderPath: ""
    property var noteListView: null
    property var editor: null
    property var onLinkActivatedCallback: null
    property var notesIndex: null
    onNotesIndexChanged: {
        console.log("EmbedBlock: notesIndex changed, reloading content");
        loadEmbedContent();
    }

    property var notesFileHandler: null
    onNotesFileHandlerChanged: {
        console.log("EmbedBlock: notesFileHandler changed, reloading content");
        loadEmbedContent();
    }

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

    function parseContent() {
        // Content format: "Note Name#Section" or "Note Name#^blockid" or "Note Name"
        // It comes from the parser as the inner content of ![[...]]

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
        console.log("EmbedBlock: loadEmbedContent called for content:", root.content);

        if (!notesFileHandler) {
            console.log("EmbedBlock: Error - notesFileHandler is null");
            return;
        }

        if (!notesIndex) {
            console.log("EmbedBlock: Warning - notesIndex is null");
        } else {
            console.log("EmbedBlock: notesIndex is available");
        }

        var notePathToLoad = "";
        if (notesIndex && targetNote !== "") {
            console.log("EmbedBlock: Searching index for title:", targetNote);
            notePathToLoad = notesIndex.findPathByTitle(targetNote);
            console.log("EmbedBlock: Index returned path:", notePathToLoad);
        }

        // Fallback to local
        if (notePathToLoad === "" && targetNote !== "" && folderPath) {
            console.log("EmbedBlock: Checking local folder for:", targetNote);
            var local = folderPath + "/" + targetNote + ".md";
            if (notesFileHandler.exists(local)) {
                notePathToLoad = local;
                console.log("EmbedBlock: Found locally:", notePathToLoad);
            }
        }

        if (notePathToLoad === "") {
            console.log("EmbedBlock: Failed to find note:", targetNote);
            errorMsg = "Note not found: " + targetNote;
            embedLoaded = false;
            return;
        }

        var extracted = "";
        if (targetBlockId !== "") {
            console.log("EmbedBlock: Extracting block:", targetBlockId);
            extracted = notesFileHandler.extractBlock(notePathToLoad, targetBlockId);
            embedTitle = targetNote + " > ^" + targetBlockId;
        } else if (targetSection !== "") {
            console.log("EmbedBlock: Extracting section:", targetSection);
            extracted = notesFileHandler.extractSection(notePathToLoad, targetSection);
            embedTitle = targetNote + " > " + targetSection;
        } else {
            // Full note
            console.log("EmbedBlock: Reading full note");
            var data = notesFileHandler.readNote(notePathToLoad);
            extracted = data.content;
            if (extracted.length > 500)
                extracted = extracted.substring(0, 500) + "...";
            embedTitle = targetNote;
        }

        if (extracted === "") {
            console.log("EmbedBlock: Extracted content is empty");
            errorMsg = "Content not found";
            embedLoaded = false;
        } else {
            console.log("EmbedBlock: Content loaded successfully, length:", extracted.length);
            embedBody = extracted;
            embedLoaded = true;
            errorMsg = "";
        }
    }

    // Callout-like Style Colors
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

            // Left border accent (only if loaded)
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

                // Title / Header
                Text {
                    text: root.embedLoaded ? root.embedTitle : "Embed Error"
                    font.bold: true
                    font.pixelSize: 14 // Slightly smaller than Callout title
                    color: root.embedLoaded ? root.borderColor : root.errorColor
                    Layout.fillWidth: true
                    visible: true
                }

                // Content
                Text {
                    Layout.fillWidth: true
                    text: root.embedLoaded ? root.embedBody : root.errorMsg

                    wrapMode: Text.Wrap
                    color: FluTheme.dark ? "#e2e8f0" : "#2d3748"
                    font.pixelSize: 14
                    font.family: "Segoe UI"
                    textFormat: Text.RichText // TODO: Add markdown processing if needed

                    // Simple markdown processing for preview
                    function process(t) {
                        t = t.replace(/\*\*([^*]+)\*\*/g, '<b>$1</b>');
                        t = t.replace(/\*([^*]+)\*/g, '<i>$1</i>');
                        t = t.replace(/`([^`]+)`/g, '<code style="background:#333; padding:2px 4px; border-radius:3px;">$1</code>');
                        return t;
                    }

                    Component.onCompleted: {
                        if (root.embedLoaded)
                            text = process(root.embedBody);
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onClicked: {
                    if (root.noteListView) {
                        root.noteListView.currentIndex = root.blockIndex;
                    }
                    root.isEditing = true;
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

            // Prevent newlines in embed block (single line)
            Keys.onReturnPressed: event => {
                event.accepted = true;
                // Move to next block or create new one?
                // For now, behave like finishing edit
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
                        var t = text.trim();
                        // Recursive embed fix: Check if user recursively added syntax
                        // We want to pass the raw markdown to replaceBlock, OR pass the inner content to updateBlock if it stays an embed.
                        // But replaceBlock expects Markdown.

                        // If the user typed "![[Note]]", replaceBlock parses it as Embed block with content "Note".
                        // If we just pass "Note" to replaceBlock, it becomes a paragraph "Note".
                        // So we MUST pass valid markdown "![[Note]]".

                        // The bug likely was:
                        // 1. We start with content="Note". Editor text="![[Note]]".
                        // 2. User edits to "![[Note2]]".
                        // 3. We call replaceBlock("![[Note2]]").
                        // 4. Parser parses "![" then "[Note2]]" ... wait.
                        // markdown parser for ![[...]] expects ![[...]].

                        // The user said: ![[![[![[...]]]]]]
                        // This implies we were wrapping an already wrapped string?
                        // text: "![[" + root.content + "]]" -> If root.content had "![[...]]" inside it!

                        // IMPORTANT: root.content MUST NOT have the syntax. It should be just the inner text.
                        // When we save/finishEdit, if we use replaceBlock(t), the parser runs.
                        // If t="![[Note]]", parser sees Embed block, content="Note".
                        // So the model updates block with content="Note".
                        // Then QML binds content="Note".
                        // Editor becomes "![[" + "Note" + "]]" = "![[Note]]". Correct.

                        // If the user somehow got ![[![[Note]]]], it means content became "![[Note]]".
                        // This happens if parser extracted "![[Note]]" as the content.
                        // This would happen if input was "![[ ![[Note]] ]]"

                        root.noteListView.model.replaceBlock(root.blockIndex, t);
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

    // Refresh content when file handler or index changes
    Connections {
        target: notesIndex
        function onIndexUpdated() {
            loadEmbedContent();
        }
        ignoreUnknownSignals: true
    }
}
