import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0
import EdgeGesture.Notes 1.0

Item {
    id: root
    width: parent.width
    height: Math.max(24, loader.item ? loader.item.height : 24) // Minimal height

    // Properties required by NotesEditor delegate
    property int blockIndex: -1
    property var noteListView: null
    property var notesIndex: null
    property var notesFileHandler: null
    property string notePath: ""
    property string folderPath: "" // Extracted from notePath

    property var editor: null
    property string content: ""
    property bool isEditing: false
    property var metadata: ({})
    property var listType: metadata["listType"] || "bullet"

    // Missing properties from Delegate
    property var type: ""
    property int level: 0
    property string vaultRootPath: ""

    // Image cache for async resolution
    property var imageCache: ({})
    property int imageCacheVersion: 0

    // Store next block index for timer
    property int nextBlockIndex: -1

    Timer {
        id: newBlockFocusTimer
        interval: 50
        repeat: false
        onTriggered: {
            if (root.nextBlockIndex >= 0 && root.editor) {
                root.editor.navigateToBlock(root.nextBlockIndex, false);
                root.nextBlockIndex = -1;
            }
        }
    }

    // Callback property for NotesEditor injection
    property var onLinkActivatedCallback: null

    // Async image resolver - caches results and triggers re-render when ready
    function resolveImageAsync(imageName) {
        if (root.imageCache.hasOwnProperty(imageName)) {
            return root.imageCache[imageName];
        }
        root.imageCache[imageName] = "";
        Qt.callLater(function () {
            var found = "";
            if (root.notesFileHandler && root.notePath && root.vaultRootPath) {
                found = root.notesFileHandler.findImage(imageName, root.notePath, root.vaultRootPath);
                if (found && found.length > 0) {
                    found = "file:///" + found.replace(/\\/g, "/");
                }
            }
            if (found === "" && root.notesFileHandler && root.folderPath) {
                var rel = root.folderPath + "/" + imageName;
                if (root.notesFileHandler.exists(rel)) {
                    found = "file:///" + rel.replace(/\\/g, "/");
                }
            }
            if (found !== root.imageCache[imageName]) {
                root.imageCache[imageName] = found;
                root.imageCacheVersion++;
            }
        });
        return "";
    }

    // Layout
    Row {
        anchors.fill: parent
        spacing: 8

        // Marker (Bullet or Number)
        Text {
            id: marker
            width: 24
            text: root.listType === "ordered" ? "1." : "•"
            font.pixelSize: 16
            color: root.editor ? root.editor.contrastColor : "#000000"
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignTop
            topPadding: 4 // Align with text
        }

        Loader {
            id: loader
            width: parent.width - 32
            sourceComponent: isEditing ? editorComp : viewerComp
        }
    }

    Component {
        id: viewerComp
        Text {
            id: textItem
            width: loader.width
            wrapMode: Text.Wrap
            color: root.editor ? root.editor.contrastColor : (FluTheme.dark ? "#cccccc" : "#222222")
            font.pixelSize: 16
            font.family: "Segoe UI"
            textFormat: Text.RichText
            linkColor: FluTheme.primaryColor

            readonly property int cachedFontSize: 16

            text: {
                var t = root.content;
                var currentFontSize = textItem.cachedFontSize;
                var darkMode = root.editor ? root.editor.isDarkColor(root.editor.currentColor) : FluTheme.dark;

                // Math processing
                t = MathHelper.processMathToPlaceholders(t, currentFontSize, darkMode);

                var _cacheVer = root.imageCacheVersion;

                // Obsidian Embed syntax: ![[...]]
                t = t.replace(/!\[\[(.*?)\]\]/g, function (match, p1) {
                    var embedContent = p1;
                    var imageExtensions = /\.(png|jpg|jpeg|gif|bmp|svg|webp|ico|tiff?)$/i;
                    if (imageExtensions.test(embedContent)) {
                        var src = root.resolveImageAsync(embedContent);
                        if (src !== "") {
                            return '<img src="' + src + '" width="200" style="vertical-align: middle;">';
                        }
                        return '<span style="opacity:0.5">[Loading: ' + embedContent + ']</span>';
                    }
                    return '<a href="' + encodeURIComponent(p1) + '">Embed: ' + p1 + '</a>';
                });

                // Link syntax: [[...]]
                t = t.replace(/\[\[([^\]]+)\]\]/g, function (match, p1) {
                    var linkTarget = p1;
                    var displayText = p1;
                    var pipeIndex = p1.indexOf('|');
                    if (pipeIndex !== -1) {
                        linkTarget = p1.substring(0, pipeIndex);
                        displayText = p1.substring(pipeIndex + 1);
                    } else {
                        var hashIndex = p1.indexOf('#');
                        if (hashIndex !== -1) {
                            var pagePart = p1.substring(0, hashIndex);
                            var anchorPart = p1.substring(hashIndex + 1);
                            if (pagePart === "") {
                                displayText = anchorPart.startsWith('^') ? anchorPart.substring(1) + " (block)" : anchorPart;
                            } else {
                                displayText = pagePart + " › " + (anchorPart.startsWith('^') ? anchorPart.substring(1) : anchorPart);
                            }
                        }
                    }
                    return '<a href="' + encodeURIComponent(linkTarget) + '">' + displayText + '</a>';
                });

                // Bold
                t = t.replace(/\*\*([^*]+)\*\*/g, '<b>$1</b>');
                t = t.replace(/__([^_]+)__/g, '<b>$1</b>');

                // Italic
                t = t.replace(/\*([^*]+)\*/g, '<i>$1</i>');
                t = t.replace(/_([^_]+)_/g, '<i>$1</i>');

                // Highlight
                var highlightColor = FluTheme.dark ? "rgba(255, 215, 0, 0.4)" : "rgba(255, 255, 0, 0.5)";
                t = t.replace(/==(.*?)==/g, '<span style="background-color: ' + highlightColor + ';">$1</span>');

                // Strikethrough
                t = t.replace(/~~([^~]+)~~/g, '<s>$1</s>');

                // Inline code
                var codeBg = FluTheme.dark ? "rgba(255, 255, 255, 0.12)" : "rgba(0, 0, 0, 0.05)";
                var codeColor = FluTheme.dark ? "rgba(0, 0, 0)" : "rgba(255, 255, 255)";
                t = t.replace(/`([^`]+)`/g, '<code style="background-color: ' + codeBg + '; color: ' + codeColor + '; padding: 2px 4px; border-radius: 5px;">$1</code>');

                t = MathHelper.restoreMathPlaceholders(t);
                return t;
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

                // 2. Parse anchor portion (block link or section reference)
                var pagePart = decodedLink;
                var anchorPart = "";
                var hashIndex = decodedLink.indexOf('#');
                if (hashIndex !== -1) {
                    pagePart = decodedLink.substring(0, hashIndex);
                    anchorPart = decodedLink.substring(hashIndex + 1);
                    console.log("WikiLink: Page:", pagePart, "Anchor:", anchorPart);
                }

                // 3. Handle self-reference (same page anchor like [[#heading]] or [[#^block-id]])
                if (pagePart === "" && anchorPart !== "") {
                    console.log("WikiLink: Self-reference to anchor:", anchorPart);
                    // TODO: Scroll to anchor in current note
                    // For now, just log - future: emit signal to scroll to block/heading
                    return;
                }

                // 4. Try to find note by title globally (searches entire vault including subfolders)
                var globalPath = "";
                console.log("WikiLink: notesIndex available:", root.notesIndex ? "yes" : "no");
                if (root.notesIndex) {
                    globalPath = root.notesIndex.findPathByTitle(pagePart);
                    console.log("WikiLink: findPathByTitle result:", globalPath);
                }

                if (globalPath !== "") {
                    console.log("WikiLink: Found in index, opening:", globalPath, "anchor:", anchorPart);
                    if (root.onLinkActivatedCallback) {
                        // Pass anchor as second parameter if callback supports it
                        root.onLinkActivatedCallback(globalPath, anchorPart);
                    }
                    return;
                }

                // 5. Try relative path (Classic .md link)
                if (pagePart.endsWith(".md")) {
                    var p = root.folderPath + "/" + pagePart;
                    console.log("WikiLink: Using .md relative path:", p);
                    if (root.onLinkActivatedCallback) {
                        root.onLinkActivatedCallback(p, anchorPart);
                    }
                    return;
                }

                // 6. Wiki link not in index - check if file exists at local path
                var wikiPath = root.folderPath + "/" + pagePart + ".md";
                console.log("WikiLink: Checking local path:", wikiPath);
                console.log("WikiLink: notesFileHandler available:", root.notesFileHandler ? "yes" : "no");
                if (root.notesFileHandler && root.notesFileHandler.exists(wikiPath)) {
                    console.log("WikiLink: File exists at local path, opening");
                    if (root.onLinkActivatedCallback) {
                        root.onLinkActivatedCallback(wikiPath, anchorPart);
                    }
                    return;
                }

                // 7. Note doesn't exist anywhere - create a new note with this title
                console.log("WikiLink: Note not found, creating new note in:", root.folderPath);
                if (root.notesFileHandler) {
                    var newPath = root.notesFileHandler.createNote(root.folderPath, pagePart, "", "#624a73");
                    console.log("WikiLink: Created new note at:", newPath);
                    if (newPath !== "" && root.onLinkActivatedCallback) {
                        // Update the index with the new entry
                        if (root.notesIndex) {
                            root.notesIndex.updateEntry(newPath);
                        }
                        root.onLinkActivatedCallback(newPath, anchorPart);
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
            id: textEdit
            width: loader.width
            text: root.content

            customTextColor: root.editor ? root.editor.contrastColor : "#000000"
            customSelectionColor: FluTheme.primaryColor
            customBackgroundColor: root.editor ? root.editor.editBackgroundColor : "transparent"

            Keys.onReturnPressed: event => handleEnter(event)
            Keys.onEnterPressed: event => handleEnter(event)

            font.pixelSize: 16
            font.family: "Segoe UI"

            onLinkActivated: link => {
                if (root.onLinkActivatedCallback) {
                    root.onLinkActivatedCallback(link);
                }
            }

            Keys.onPressed: event => {
                if (event.key === Qt.Key_Up) {
                    if (cursorPosition === 0 && root.editor) {
                        root.editor.navigateUp(root.blockIndex);
                        event.accepted = true;
                    }
                } else if (event.key === Qt.Key_Down) {
                    if (cursorPosition === length && root.editor) {
                        root.editor.navigateDown(root.blockIndex);
                        event.accepted = true;
                    }
                } else if (event.key === Qt.Key_Backspace) {
                    if (length === 0) {
                        if (root.noteListView) {
                            root.noteListView.model.removeBlock(root.blockIndex);
                        }
                        event.accepted = true;
                    }
                }
            }

            onEditingFinished: {
                if (root.blockIndex >= 0 && root.noteListView) {
                    root.noteListView.model.updateBlock(root.blockIndex, text);
                }
            }

            // Connection logic moved inside here as 'textEdit' must be valid
            Connections {
                target: root.noteListView
                function onCurrentIndexChanged() {
                    if (root.noteListView && root.noteListView.currentIndex !== root.blockIndex) {
                        root.isEditing = false;
                        textEdit.focus = false;
                    }
                }
                ignoreUnknownSignals: true
            }

            // Connection for root.isEditing changes
            Connections {
                target: root
                function onIsEditingChanged() {
                    if (root.isEditing) {
                        textEdit.forceActiveFocus();
                        if (root.editor && root.editor._cursorAtEnd) {
                            textEdit.cursorPosition = textEdit.length;
                        } else {
                            textEdit.cursorPosition = 0;
                        }
                    } else {
                        textEdit.focus = false;
                    }
                }
            }

            onVisibleChanged: {
                if (visible && root.isEditing) {
                    forceActiveFocus();
                }
            }

            onActiveFocusChanged: {
                if (activeFocus) {
                    if (root.noteListView) {
                        root.noteListView.currentIndex = root.blockIndex;
                    }
                    root.isEditing = true;
                }
            }

            function handleEnter(event) {
                event.accepted = true;
                var pos = cursorPosition;
                var fullText = text;
                var preText = fullText.substring(0, pos);
                var postText = fullText.substring(pos);

                var prefix = "";
                if (root.listType === "ordered") {
                    prefix = "1. ";
                } else {
                    prefix = "* ";
                }

                if (root.noteListView && root.noteListView.model) {
                    root.noteListView.model.updateBlock(root.blockIndex, preText);
                    root.noteListView.model.insertBlock(root.blockIndex + 1, "paragraph", prefix + postText);
                    root.nextBlockIndex = root.blockIndex + 1;
                    newBlockFocusTimer.start();
                }
                root.isEditing = false;
            }
        }
    }
}
