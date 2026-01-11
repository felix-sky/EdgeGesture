import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0
import EdgeGesture.Notes 1.0

Item {
    id: root
    width: ListView.view ? ListView.view.width - 20 : 300 // Padding
    height: loader.item ? Math.max(loader.item.contentHeight, loader.item.implicitHeight) + 16 : 24

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

    // Image cache for async resolution (performance optimization)
    property var imageCache: ({})
    property int imageCacheVersion: 0  // Increment to trigger re-render

    // Store next block index for timer (at root level so it survives component destruction)
    property int nextBlockIndex: -1

    // Timer at root level - survives when editorComp is destroyed
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

    // Async image resolver - caches results and triggers re-render when ready
    function resolveImageAsync(imageName) {
        // Return cached result if available
        if (root.imageCache.hasOwnProperty(imageName)) {
            return root.imageCache[imageName];
        }
        // Mark as pending (empty string) to avoid duplicate lookups
        root.imageCache[imageName] = "";
        // Resolve asynchronously
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
            // Update cache and trigger re-render if we found something
            if (found !== root.imageCache[imageName]) {
                root.imageCache[imageName] = found;
                root.imageCacheVersion++;
            }
        });
        return "";
    }

    Component {
        id: viewerComp
        Text {
            id: textItem
            width: parent.width

            // Cache font properties to avoid repeated function calls in bindings
            readonly property int cachedFontSize: root.getFontSize()
            readonly property int cachedFontWeight: root.getFontWeight()
            // Pre-process content to handle Obsidian-style images and links via Markdown
            text: {
                var t = root.content;
                // Only build basePath if folderPath is non-empty
                var hasValidFolder = root.folderPath && root.folderPath.length > 0;
                var basePath = hasValidFolder ? ("file:///" + root.folderPath.replace(/\\/g, "/") + "/") : "";

                // Math: $$...$$ (Display) and $...$ (Inline) -> PNG via JKQTMathText
                // Process math early to avoid * or _ inside math being parsed as italic/bold
                // Uses C++ regex processing for better performance
                var currentFontSize = textItem.cachedFontSize;
                var darkMode = root.editor ? root.editor.isDarkColor(root.editor.currentColor) : FluTheme.dark;
                t = MathHelper.processMathToPlaceholders(t, currentFontSize, darkMode);

                // Obsidian Embed syntax: ![[...]]
                // - If ends with image extension (.png, .jpg, .jpeg, .gif, .bmp, .svg, .webp) -> Image embed
                // - Otherwise -> Section/Chapter embed (transclude note content)
                // Obsidian Embed syntax: ![[...]]
                // Since standalone embeds are now handled by EmbedBlock/ImageBlock,
                // we only handle INLINE images here if they appear in a text paragraph.
                // e.g. "Check this: ![[img.png]]"
                // But complex block returns (<div>...) are likely no longer needed or will only support inline images.

                // Reference imageCacheVersion to trigger re-render when cache updates
                var _cacheVer = root.imageCacheVersion;

                t = t.replace(/!\[\[(.*?)\]\]/g, function (match, p1) {
                    var embedContent = p1;
                    var imageExtensions = /\.(png|jpg|jpeg|gif|bmp|svg|webp|ico|tiff?)$/i;
                    if (imageExtensions.test(embedContent)) {
                        // Inline image - use async cache lookup
                        var src = root.resolveImageAsync(embedContent);

                        if (src !== "") {
                            // Limit width for inline
                            return '<img src="' + src + '" width="200" style="vertical-align: middle;">';
                        }
                        return '<span style="opacity:0.5">[Loading: ' + embedContent + ']</span>';
                    }

                    // For non-image embeds (sections) inside a paragraph, we render them as a link or text ref?
                    // "See ![[Note]]" -> "See [[Note]]" (link-like)
                    return '<a href="' + encodeURIComponent(p1) + '">Embed: ' + p1 + '</a>';
                });

                // Replace [[Link|Alias]] or [[Link#anchor]] or [[Link]] with proper links
                // Supports: [[Page]], [[Page|DisplayText]], [[Page#heading]], [[Page#^block-id]], [[Page#heading|Alias]]
                t = t.replace(/\[\[([^\]]+)\]\]/g, function (match, p1) {
                    var linkTarget = p1;
                    var displayText = p1;

                    // Check for alias syntax: [[target|alias]]
                    var pipeIndex = p1.indexOf('|');
                    if (pipeIndex !== -1) {
                        linkTarget = p1.substring(0, pipeIndex);
                        displayText = p1.substring(pipeIndex + 1);
                    } else {
                        // No alias - check if there's an anchor and format display accordingly
                        var hashIndex = p1.indexOf('#');
                        if (hashIndex !== -1) {
                            var pagePart = p1.substring(0, hashIndex);
                            var anchorPart = p1.substring(hashIndex + 1);

                            if (pagePart === "") {
                                // Self-reference like [[#heading]] or [[#^block-id]]
                                if (anchorPart.startsWith('^')) {
                                    // Block reference - show as (block)
                                    displayText = anchorPart.substring(1) + " (block)";
                                } else {
                                    // Section reference - show heading name
                                    displayText = anchorPart;
                                }
                            } else {
                                // Reference to another page with anchor
                                if (anchorPart.startsWith('^')) {
                                    // Block reference in another page
                                    displayText = pagePart + " › " + anchorPart.substring(1);
                                } else {
                                    // Section reference in another page
                                    displayText = pagePart + " › " + anchorPart;
                                }
                            }
                        }
                    }

                    return '<a href="' + encodeURIComponent(linkTarget) + '">' + displayText + '</a>';
                });

                // Convert markdown formatting to HTML
                // Bold: **text** or __text__ -> <b>text</b>
                t = t.replace(/\*\*([^*]+)\*\*/g, '<b>$1</b>');
                t = t.replace(/__([^_]+)__/g, '<b>$1</b>');

                // Italic: *text* or _text_
                // Using simpler regex to allow CJK characters and standard behavior
                t = t.replace(/\*([^*]+)\*/g, '<i>$1</i>');
                t = t.replace(/_([^_]+)_/g, '<i>$1</i>');

                // hightlight
                var highlightColor = FluTheme.dark ? "rgba(255, 215, 0, 0.4)" : "rgba(255, 255, 0, 0.5)";
                t = t.replace(/==(.*?)==/g, '<span style="background-color: ' + highlightColor + ';">$1</span>');

                // Strikethrough: ~~text~~ -> <s>text</s>
                t = t.replace(/~~([^~]+)~~/g, '<s>$1</s>');

                // Inline code: `code` -> <code>code</code>
                var codeBg = FluTheme.dark ? "rgba(255, 255, 255, 0.12)" : "rgba(0, 0, 0, 0.05)";
                var codeColor = FluTheme.dark ? "rgba(0, 0, 0)" : "rgba(255, 255, 255)";
                t = t.replace(/`([^`]+)`/g, '<code style="background-color: ' + codeBg + '; color: ' + codeColor + '; padding: 2px 4px; border-radius: 5px;">$1</code>');

                t = MathHelper.restoreMathPlaceholders(t);

                return t;
            }
            wrapMode: Text.Wrap
            color: root.editor ? root.editor.contrastColor : (FluTheme.dark ? "#cccccc" : "#222222")
            font.pixelSize: cachedFontSize
            font.weight: cachedFontWeight
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
        FluentEditorArea {
            id: editorArea
            width: root.width
            text: {
                if (root.type === "heading") {
                    return "#".repeat(root.level) + " " + root.content;
                }
                return root.content;
            }

            // Custom color properties passed to FluentEditorArea
            customTextColor: root.editor ? root.editor.contrastColor : (FluTheme.dark ? "#FFFFFF" : "#000000")
            customSelectionColor: FluTheme.primaryColor
            customBackgroundColor: root.editor ? root.editor.editBackgroundColor : "transparent"

            font.pixelSize: getFontSize()
            font.weight: getFontWeight()
            font.family: "Segoe UI"

            // Override the default Enter behavior
            Keys.onReturnPressed: event => handleEnter(event)
            Keys.onEnterPressed: event => handleEnter(event)

            // Arrow key navigation between blocks
            Keys.onUpPressed: event => {
                // Check if cursor is at the first line
                var lineHeight = font.pixelSize * 1.5; // Approximate line height
                var isFirstLine = cursorRectangle.y < lineHeight;

                if (isFirstLine && root.blockIndex > 0) {
                    // Save current content before navigating
                    finishEdit();
                    // Navigate to previous block
                    if (root.editor && typeof root.editor.goToPreviousBlock === "function") {
                        root.editor.goToPreviousBlock(root.blockIndex);
                        event.accepted = true;
                        return;
                    }
                }
                // Let default behavior handle in-block cursor movement
                event.accepted = false;
            }

            Keys.onDownPressed: event => {
                // Check if cursor is at the last line
                var lineHeight = font.pixelSize * 1.5;
                var textHeight = contentHeight > 0 ? contentHeight : lineHeight;
                var isLastLine = cursorRectangle.y >= (textHeight - lineHeight);

                if (isLastLine && root.editor) {
                    // Save current content before navigating
                    finishEdit();
                    // Navigate to next block
                    if (typeof root.editor.goToNextBlock === "function") {
                        root.editor.goToNextBlock(root.blockIndex);
                        event.accepted = true;
                        return;
                    }
                }
                // Let default behavior handle in-block cursor movement
                event.accepted = false;
            }

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
                    return;
                }

                if ((event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) && text === "") {
                    // Logic to delete block if empty
                    if (root.blockIndex >= 0 && root.noteListView && root.noteListView.model) {
                        var idxToRemove = root.blockIndex;
                        var count = root.noteListView.count;

                        if (idxToRemove > 0) {
                            // Focus previous block first (cursor at end)
                            if (root.editor) {
                                root.editor.navigateToBlock(idxToRemove - 1, true);
                            }
                            // Then remove the current block
                            if (typeof root.noteListView.model.removeBlock === "function") {
                                root.noteListView.model.removeBlock(idxToRemove);
                            }
                            event.accepted = true;
                        } else if (count > 1) {
                            // Deleting the first block (when others exist)
                            if (typeof root.noteListView.model.removeBlock === "function") {
                                root.noteListView.model.removeBlock(idxToRemove);
                            }
                            // Focus the new first block
                            if (root.editor) {
                                root.editor.navigateToBlock(0, false);
                            }
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

                    // Store the next block index and use timer to request focus
                    root.nextBlockIndex = root.blockIndex + 1;
                    newBlockFocusTimer.start();
                }
                root.isEditing = false;
            }

            Component.onCompleted: {
                focusTimer.start();
                // Position cursor based on navigation direction
                // If navigating from above (down), cursor at start
                // If navigating from below (up), cursor at end
                if (root.editor && root.editor._cursorAtEnd !== undefined) {
                    cursorPosition = root.editor._cursorAtEnd ? length : 0;
                } else {
                    cursorPosition = length;
                }
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
