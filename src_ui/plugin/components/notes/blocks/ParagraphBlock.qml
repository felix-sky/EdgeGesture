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
    property var onLinkActivatedCallback: null // Injected by NotesEditor
    property var notesIndex: null
    property var notesFileHandler: null // For image search
    property string notePath: "" // Current note path for image context
    property string vaultRootPath: "" // Root of the vault for image search
    property int blockIndex: -1

    // Sync content when model changes
    onContentChanged: {
        // Force refresh if needed
    }

    Loader {
        id: loader
        width: parent.width
        sourceComponent: isEditing ? editorComp : viewerComp
    }

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
            font.pixelSize: 16
            font.family: "Segoe UI"
            textFormat: Text.RichText  // Changed to RichText for HTML img width support
            linkColor: FluTheme.primaryColor

            Component.onCompleted: {
                // RichText handles img width directly
            }

            onLinkActivated: link => {
                // decode URI component if needed
                var decodedLink = decodeURIComponent(link);

                // 1. Try to find note by title globally
                var globalPath = "";
                if (root.notesIndex) {
                    globalPath = root.notesIndex.findPathByTitle(decodedLink);
                }

                if (globalPath !== "") {
                    if (root.onLinkActivatedCallback) {
                        root.onLinkActivatedCallback(globalPath);
                    }
                    return;
                }

                // 2. Try relative path (Classic .md link)
                if (decodedLink.endsWith(".md")) {
                    var p = root.folderPath + "/" + decodedLink;
                    if (root.onLinkActivatedCallback) {
                        root.onLinkActivatedCallback(p);
                    }
                    return;
                }

                // 3. Fallback: External browser
                Qt.openUrlExternally(link);
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
        TextArea {
            id: editorArea
            width: root.width
            text: root.content
            wrapMode: Text.Wrap
            color: FluTheme.dark ? "#FFFFFF" : "#000000"
            font.pixelSize: 16
            font.family: "Segoe UI"

            // Darker background for edit mode
            // Darker background for edit mode removed due to style customization warning
            // background: Rectangle { ... }

            selectByMouse: true

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
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    // Split block
                    event.accepted = true;
                    var pos = cursorPosition;
                    var fullText = text;
                    var preText = fullText.substring(0, pos);
                    var postText = fullText.substring(pos);

                    // Update current block
                    if (root.noteListView && root.noteListView.model) {
                        root.noteListView.model.updateBlock(root.blockIndex, preText);
                        // Insert new block
                        root.noteListView.model.insertBlock(root.blockIndex + 1, "paragraph", postText);

                        // Move focus to next block (tricky as it needs to be created first)
                        // We can set the current index, and the view should handle it.
                        root.noteListView.currentIndex = root.blockIndex + 1;
                    }
                    root.isEditing = false; // Close current editor
                }
            }

            Component.onCompleted: {
                forceActiveFocus();
                cursorPosition = length;
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
                        root.noteListView.model.updateBlock(root.blockIndex, text);
                    }
                    root.isEditing = false;
                }
            }
        }
    }
}
