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

                // Math: $$...$$ (Display) and $...$ (Inline) -> PNG via JKQTMathText
                // Process math early to avoid * or _ inside math being parsed as italic/bold
                // Uses C++ regex processing for better performance
                var currentFontSize = root.getFontSize();
                var currentFontSize = root.getFontSize();
                var darkMode = root.editor ? root.editor.isDarkColor(root.editor.currentColor) : FluTheme.dark;
                t = MathHelper.processMathToPlaceholders(t, currentFontSize, darkMode);

                // Obsidian Embed syntax: ![[...]]
                // - If ends with image extension (.png, .jpg, .jpeg, .gif, .bmp, .svg, .webp) -> Image embed
                // - Otherwise -> Section/Chapter embed (transclude note content)
                t = t.replace(/!\[\[(.*?)\]\]/g, function (match, p1) {
                    var embedContent = p1;

                    // Check if it's an image file by extension
                    var imageExtensions = /\.(png|jpg|jpeg|gif|bmp|svg|webp|ico|tiff?)$/i;
                    var isImage = imageExtensions.test(embedContent);

                    if (isImage) {
                        // Handle image embed
                        var imageName = embedContent;
                        var src = "";
                        var imageFound = false;

                        // Use NotesFileHandler.findImage for vault-wide search (like Obsidian)
                        if (root.notesFileHandler && root.notePath && root.vaultRootPath) {
                            var found = root.notesFileHandler.findImage(imageName, root.notePath, root.vaultRootPath);
                            if (found && found.length > 0) {
                                src = "file:///" + found.replace(/\\/g, "/");
                                imageFound = true;
                            }
                        }

                        // Fallback: check if image exists at relative path
                        if (!imageFound && root.folderPath && root.folderPath.length > 0) {
                            var relativePath = root.folderPath.replace(/\\/g, "/") + "/" + imageName;
                            if (root.notesFileHandler && root.notesFileHandler.exists(relativePath)) {
                                src = "file:///" + relativePath;
                                imageFound = true;
                            }
                        }

                        // Image not found - show error message
                        if (!imageFound) {
                            var errorBg = darkMode ? "rgba(0,0,0,0.3)" : "rgba(255,0,0,0.05)";
                            var errorText = darkMode ? "#ffa39e" : "#d9363e";
                            return '<div style="margin: 10px 0; padding: 12px; text-align: center; background:' + errorBg + '; border-radius: 4px; color:' + errorText + '; font-size:14px; font-family: Segoe UI;">' + '找不到 “' + imageName + '”' + '</div>';
                        }

                        // Only encode spaces - Qt handles raw Unicode paths better
                        // But we need to handle special chars if they break HTML
                        var imgPath = "file:///" + fullPath;
                        return '<img src="' + imgPath + '" width="' + displayWidth + '" style="vertical-align: middle;">';
                    } else {
                        // Handle section/chapter embed (transclude)
                        // Format: ![[NoteName]] or ![[NoteName#Section]] or ![[NoteName#^block-id]]
                        var pagePart = embedContent;
                        var sectionPart = "";
                        var hashIndex = embedContent.indexOf('#');
                        if (hashIndex !== -1) {
                            pagePart = embedContent.substring(0, hashIndex);
                            sectionPart = embedContent.substring(hashIndex + 1);
                        }

                        // Style variables
                        // Use hex with alpha for Qt table backgrounds if needed
                        var embedBg = darkMode ? "#4d000000" : "#0d000000"; // ~30% alpha black (dark mode)
                        var embedBorderColor = darkMode ? "#60ccff" : "#0099cc"; // Cyan/Teal accent
                        var embedText = darkMode ? "#e2e8f0" : "#2d3748";
                        var titleColor = darkMode ? "#60ccff" : "#0099cc"; // Match border/accent color for title (Callout style)
                        var iconColor = darkMode ? "#a0a0a0" : "#808080";

                        // Error styles
                        var errorBg = darkMode ? "rgba(0,0,0,0.3)" : "rgba(255,0,0,0.05)";
                        var errorText = darkMode ? "#ff7875" : "#d9363e";

                        // Find the note path
                        var targetNotePath = "";
                        if (root.notesIndex && pagePart !== "") {
                            targetNotePath = root.notesIndex.findPathByTitle(pagePart);
                        }

                        // Fallback: try local folder
                        if (targetNotePath === "" && pagePart !== "" && root.folderPath) {
                            var localPath = root.folderPath + "/" + pagePart + ".md";
                            if (root.notesFileHandler && root.notesFileHandler.exists(localPath)) {
                                targetNotePath = localPath;
                            }
                        }

                        // Helper for error creation
                        function createError(msg) {
                            return '<div style="margin: 8px 0; padding: 12px; text-align: center; background:' + errorBg + '; border-radius: 4px; color:' + errorText + '; font-size:13px; font-family: Segoe UI;">' + msg + '</div>';
                        }

                        // Helper: Create Table-based Callout-like Block for robust background unity
                        function createCalloutEmbed(title, content) {
                            // Use safe dark tint for background to ensure visibility
                            var bgCol = darkMode ? "#4d000000" : "#0d000000";
                            // Using a 2-column table to simulate the Left Border + Content Box
                            // Cell 1: 4px wide, colored background (The Border)
                            // Cell 2: Content with background
                            return '<table width="100%" cellspacing="0" cellpadding="0" style="margin: 12px 0;">' + '<tr>' + '<td width="4" bgcolor="' + embedBorderColor + '"></td>' + '<td bgcolor="' + bgCol + '" style="padding: 10px 10px 10px 15px;">' + '<div style="color: ' + titleColor + '; font-size: 16px; font-weight: 600; margin-bottom: 6px;">' + title + '</div>' + '<div style="color: ' + embedText + '; font-size: 14px; line-height: 1.6;">' + content + '</div>' + '</td>' + '</tr>' + '</table>';
                        }

                        if (sectionPart !== "" && targetNotePath !== "") {
                            // Section or block reference embed
                            var extractedContent = "";

                            if (sectionPart.startsWith('^')) {
                                // Block embed - extract block by ID
                                var blockId = sectionPart.substring(1);
                                if (root.notesFileHandler) {
                                    extractedContent = root.notesFileHandler.extractBlock(targetNotePath, blockId);
                                }

                                if (extractedContent && extractedContent.length > 0) {
                                    return createCalloutEmbed(pagePart + ' > ' + blockId, extractedContent);
                                } else {
                                    return createError('Block not found: <b>' + blockId + '</b>');
                                }
                            } else {
                                // Section embed - extract section by heading
                                if (root.notesFileHandler) {
                                    extractedContent = root.notesFileHandler.extractSection(targetNotePath, sectionPart);
                                }

                                if (extractedContent && extractedContent.length > 0) {
                                    return createCalloutEmbed(pagePart + ' > ' + sectionPart, extractedContent);
                                } else {
                                    return createError('Section not found: <b>' + sectionPart + '</b>');
                                }
                            }
                        } else if (sectionPart !== "" && targetNotePath === "") {
                            return createError('Note not found: <b>' + pagePart + '</b>');
                        } else {
                            // Full note embed
                            if (targetNotePath !== "" && root.notesFileHandler) {
                                var noteData = root.notesFileHandler.readNote(targetNotePath);
                                var noteContent = noteData.content || "";
                                if (noteContent.length > 0) {
                                    var preview = noteContent.length > 500 ? noteContent.substring(0, 500) + "..." : noteContent;
                                    return createCalloutEmbed(pagePart, preview);
                                }
                            }
                            return createError('Embed not found: <b>' + pagePart + '</b>');
                        }
                    }
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
                t = t.replace(/`([^`]+)`/g, '<code style="background:#333; padding:2px 4px; border-radius:3px;">$1</code>');

                // Restore math placeholders with actual rendered content (C++)
                t = MathHelper.restoreMathPlaceholders(t);

                return t;
            }
            wrapMode: Text.Wrap
            color: root.editor ? root.editor.contrastColor : (FluTheme.dark ? "#cccccc" : "#222222")
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
