import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: root
    width: ListView.view ? ListView.view.width - 20 : 300 // Padding
    height: loader.item ? loader.item.implicitHeight : 24

    property string content: model.content ? model.content : ""
    property bool isEditing: false
    property string folderPath: ""
    property var noteListView: null
    property var editor: null // Reference to NotesEditor
    property var onLinkActivatedCallback: null // Injected by NotesEditor
    property var notesIndex: null
    property var notesFileHandler: null // For image search
    property string notePath: "" // Current note path for image context
    property string vaultRootPath: "" // Root of the vault for image search
    property int blockIndex: -1
    property string type: model.type ? model.type : "paragraph"
    property int level: model.level !== undefined ? model.level : 1

    // Helper to determine font size
    function getFontSize() {
        if (type === "heading") {
            switch (level) {
            case 1:
                return 32;
            case 2:
                return 24;
            case 3:
                return 20;
            case 4:
                return 18;
            default:
                return 16;
            }
        }
        return 16;
    }

    function getFontWeight() {
        if (type === "heading")
            return Font.Bold;
        return Font.Normal;
    }

    // Sync content when model changes
    onContentChanged: {
        // Force refresh if needed
    }

    Loader {
        id: loader
        width: parent.width
        sourceComponent: isEditing ? editorComp : viewerComp
    }

    // Add mouse area to capture clicks when not editing but empty?
    // Actually the viewerComp handles clicks.
    // If content is empty, viewerComp might have 0 height.
    // Ensure minimum height in Item.

    Component {
        id: viewerComp
        Text {
            id: textItem
            width: parent.width
            // Pre-process content to handle Obsidian-style images and links via Markdown
            text: {
                var t = root.content;
                // Only build basePath if folderPath is non-empty
                var hasValidFolder = root.folderPath && root.folderPath.length > 0;
                var basePath = hasValidFolder ? ("file:///" + root.folderPath.replace(/\\/g, "/") + "/") : "";

                t = t.replace(/!\[\[(.*?)\]\]/g, function (match, p1) {
                    var imageName = p1;
                    var src = "";

                    // Use NotesFileHandler.findImage for vault-wide search (like Obsidian)
                    if (root.notesFileHandler && root.notePath && root.vaultRootPath) {
                        var found = root.notesFileHandler.findImage(imageName, root.notePath, root.vaultRootPath);
                        if (found && found.length > 0) {
                            src = "file:///" + found.replace(/\\/g, "/");
                        }
                    }

                    // Fallback: simple relative path
                    if (src === "" && root.folderPath && root.folderPath.length > 0) {
                        src = "file:///" + root.folderPath.replace(/\\/g, "/") + "/" + imageName;
                    }

                    if (src === "") {
                        return match; // Keep original if not found
                    }

                    // Only encode spaces - Qt handles raw Unicode paths better
                    src = src.replace(/ /g, "%20");
                    return '<img src="' + src + '" width="320">';
                });

                // Replace [[Link]] with <a href="Link">Link</a> to control handling
                t = t.replace(/\[\[(.*?)\]\]/g, function (match, p1) {
                    return '<a href="' + encodeURIComponent(p1) + '">' + p1 + '</a>';
                });

                // Convert markdown formatting to HTML
                // Bold: **text** or __text__ -> <b>text</b>
                t = t.replace(/\*\*([^*]+)\*\*/g, '<b>$1</b>');
                t = t.replace(/__([^_]+)__/g, '<b>$1</b>');

                // Italic: *text* or _text_ -> <i>text</i> (but not inside words)
                t = t.replace(/(?<![a-zA-Z])\*([^*]+)\*(?![a-zA-Z])/g, '<i>$1</i>');
                t = t.replace(/(?<![a-zA-Z])_([^_]+)_(?![a-zA-Z])/g, '<i>$1</i>');

                // Strikethrough: ~~text~~ -> <s>text</s>
                t = t.replace(/~~([^~]+)~~/g, '<s>$1</s>');

                // Inline code: `code` -> <code>code</code>
                t = t.replace(/`([^`]+)`/g, '<code style="background:#333; padding:2px 4px; border-radius:3px;">$1</code>');

                return t;
            }
            wrapMode: Text.Wrap
            color: FluTheme.dark ? "#cccccc" : "#222222"
            font.pixelSize: getFontSize()
            font.weight: getFontWeight()
            font.family: "Segoe UI"
            textFormat: Text.RichText  // Changed to RichText for HTML img width support
            linkColor: FluTheme.primaryColor

            Component.onCompleted: {
                // RichText handles img width directly
            }

            onLinkActivated: link => {
                // decode URI component if needed
                var decodedLink = decodeURIComponent(link);
                console.log("WikiLink: Raw link:", link);
                console.log("WikiLink: Decoded link:", decodedLink);

                // 1. Check if it's an external URL (http/https/mailto etc.) - handle first
                if (decodedLink.startsWith("http://") || decodedLink.startsWith("https://") || decodedLink.startsWith("mailto:") || decodedLink.startsWith("file://")) {
                    console.log("WikiLink: Opening as external URL");
                    Qt.openUrlExternally(link);
                    return;
                }

                // 2. Try to find note by title globally (searches entire vault including subfolders)
                var globalPath = "";
                console.log("WikiLink: notesIndex available:", root.notesIndex ? "yes" : "no");
                if (root.notesIndex) {
                    globalPath = root.notesIndex.findPathByTitle(decodedLink);
                    console.log("WikiLink: findPathByTitle result:", globalPath);
                }

                if (globalPath !== "") {
                    console.log("WikiLink: Found in index, opening:", globalPath);
                    if (root.onLinkActivatedCallback) {
                        root.onLinkActivatedCallback(globalPath);
                    }
                    return;
                }

                // 3. Try relative path (Classic .md link)
                if (decodedLink.endsWith(".md")) {
                    var p = root.folderPath + "/" + decodedLink;
                    console.log("WikiLink: Using .md relative path:", p);
                    if (root.onLinkActivatedCallback) {
                        root.onLinkActivatedCallback(p);
                    }
                    return;
                }

                // 4. Wiki link not in index - check if file exists at local path
                var wikiPath = root.folderPath + "/" + decodedLink + ".md";
                console.log("WikiLink: Checking local path:", wikiPath);
                console.log("WikiLink: notesFileHandler available:", root.notesFileHandler ? "yes" : "no");
                if (root.notesFileHandler && root.notesFileHandler.exists(wikiPath)) {
                    console.log("WikiLink: File exists at local path, opening");
                    if (root.onLinkActivatedCallback) {
                        root.onLinkActivatedCallback(wikiPath);
                    }
                    return;
                }

                // 5. Note doesn't exist anywhere - create a new note with this title
                console.log("WikiLink: Note not found, creating new note in:", root.folderPath);
                if (root.notesFileHandler) {
                    var newPath = root.notesFileHandler.createNote(root.folderPath, decodedLink, "", "#624a73");
                    console.log("WikiLink: Created new note at:", newPath);
                    if (newPath !== "" && root.onLinkActivatedCallback) {
                        // Update the index with the new entry
                        if (root.notesIndex) {
                            root.notesIndex.updateEntry(newPath);
                        }
                        root.onLinkActivatedCallback(newPath);
                    }
                }
            }

            MouseArea {
                id: interactor
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.IBeamCursor
                hoverEnabled: true

                onClicked: mouse => {
                    var link = parent.linkAt(mouse.x, mouse.y);
                    if (link) {
                        parent.linkActivated(link);
                    } else {
                        // Edit mode - set currentIndex first to ensure proper focus
                        if (root.noteListView) {
                            root.noteListView.currentIndex = root.blockIndex;
                        }
                        root.isEditing = true;
                    }
                }
            }
        }
    }

    Connections {
        target: root.noteListView
        function onCurrentIndexChanged() {
            if (root.noteListView && root.noteListView.currentIndex !== root.blockIndex) {
                root.isEditing = false;
            }
        }
        ignoreUnknownSignals: true
    }

    Component {
        id: editorComp
        FluMultilineTextBox {
            id: editorArea
            width: root.width
            text: {
                if (root.type === "heading") {
                    return "#".repeat(root.level) + " " + root.content;
                }
                return root.content;
            }

            // Override the default Enter behavior of FluMultilineTextBox
            Keys.onReturnPressed: event => handleEnter(event)
            Keys.onEnterPressed: event => handleEnter(event)

            Keys.onPressed: event => {
                if (event.matches(StandardKey.Paste)) {
                    var handler = null;
                    var p = root;
                    while (p) {
                        if (p.notesFileHandler) {
                            handler = p.notesFileHandler;
                            break;
                        }
                        p = p.parent;
                    }
                    if (handler) {
                        var imagePath = handler.saveClipboardImage(root.folderPath);
                        if (imagePath !== "") {
                            insert(cursorPosition, "![[" + imagePath + "]]");
                            event.accepted = true;
                        }
                    }
                }
            }

            function handleEnter(event) {
                // Split block
                event.accepted = true;
                var pos = cursorPosition;
                var fullText = text;
                var preText = fullText.substring(0, pos);
                var postText = fullText.substring(pos);

                // Update current block
                if (root.noteListView && root.noteListView.model) {
                    root.noteListView.model.updateBlock(root.blockIndex, preText);
                    root.noteListView.model.insertBlock(root.blockIndex + 1, "paragraph", postText);

                    // Request focus on the new block via editor signal
                    if (root.editor) {
                        root.editor.requestBlockEdit(root.blockIndex + 1);
                    }
                }
                root.isEditing = false; // Close current editor
            }

            Component.onCompleted: {
                focusTimer.start();
                cursorPosition = length;
            }

            Timer {
                id: focusTimer
                interval: 50
                repeat: false
                onTriggered: {
                    editorArea.forceActiveFocus();
                }
            }

            onEditingFinished: {
                finishEdit();
            }

            onActiveFocusChanged: {
                if (!activeFocus) {
                    finishEdit();
                }
            }

            function finishEdit() {
                if (root.isEditing) {
                    if (root.noteListView && root.noteListView.model) {
                        // Use replaceBlock if it exists to allow re-parsing type/level
                        // Fallback to updateBlock if C++ not yet updated/compiled
                        if (typeof root.noteListView.model.replaceBlock === "function") {
                            root.noteListView.model.replaceBlock(root.blockIndex, text);
                        } else {
                            root.noteListView.model.updateBlock(root.blockIndex, text);
                        }
                    }
                    root.isEditing = false;
                }
            }
        }
    }
}
