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

                // Math: $$...$$ (Display) and $...$ (Inline) -> SVG
                // We perform this early to avoid * or _ inside math being parsed as italic/bold

                // Get current font size for dynamic sizing
                var currentFontSize = root.getFontSize();
                var darkMode = FluTheme.dark;

                // Unified math processing function
                function processMath(latex, isBlock) {
                    var colorName = darkMode ? "white" : "black";

                    if (isBlock) {
                        // Block math: render at 1.5x font size, no height constraint
                        // This allows complex formulas (matrices, fractions) to display properly
                        var blockRenderSize = currentFontSize * 1.5;
                        var svgXml = MathRender.renderToSvg(latex.trim(), blockRenderSize, colorName);
                        if (svgXml && svgXml.length > 0) {
                            var dataUri = "data:image/svg+xml;base64," + Qt.btoa(svgXml);
                            return '<br><img src="' + dataUri + '" style="display:block;margin-left:auto;margin-right:auto;" /><br>';
                        }
                    } else {
                        // Inline math: render at 2x for HiDPI, constrain height to font size * 1.1
                        var scaleFactor = 2.0;
                        var renderSize = currentFontSize * scaleFactor;
                        var displayHeight = currentFontSize * 1.1;
                        var svgXml = MathRender.renderToSvg(latex.trim(), renderSize, colorName);
                        if (svgXml && svgXml.length > 0) {
                            var dataUri = "data:image/svg+xml;base64," + Qt.btoa(svgXml);
                            return '<img src="' + dataUri + '" height="' + displayHeight + '" style="vertical-align:text-bottom;" />';
                        }
                    }
                    // Fallback: return original if render failed
                    return isBlock ? ("$$" + latex + "$$") : ("$" + latex + "$");
                }

                // Block Math $$...$$
                t = t.replace(/\$\$([\s\S]+?)\$\$/g, function (match, p1) {
                    return processMath(p1, true);
                });

                // Inline Math $...$
                t = t.replace(/\$([^\$\n]+?)\$/g, function (match, p1) {
                    return processMath(p1, false);
                });

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
                            var errorBg = darkMode ? "#3d2020" : "#ffeeee";
                            var errorBorder = darkMode ? "#ff6b6b" : "#ff4444";
                            var errorText = darkMode ? "#ff9999" : "#cc0000";
                            return '<span style="display:inline-block;background:' + errorBg + ';border:1px solid ' + errorBorder + ';border-radius:4px;padding:4px 8px;color:' + errorText + ';font-size:12px;">找不到 "' + imageName + '"。</span>';
                        }

                        // Only encode spaces - Qt handles raw Unicode paths better
                        src = src.replace(/ /g, "%20");
                        return '<img src="' + src + '" width="320">';
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
                        var embedBg = darkMode ? "#2d3748" : "#f0f4f8";
                        var embedBorder = darkMode ? "#4a5568" : "#cbd5e0";
                        var embedText = darkMode ? "#e2e8f0" : "#2d3748";
                        var errorBg = darkMode ? "#3d2020" : "#ffeeee";
                        var errorBorder = darkMode ? "#ff6b6b" : "#ff4444";
                        var errorText = darkMode ? "#ff9999" : "#cc0000";

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

                        if (sectionPart !== "" && targetNotePath !== "") {
                            // Section or block reference embed
                            var extractedContent = "";

                            // Theme-aware accent color
                            var accentColor = darkMode ? "#7c3aed" : "#8b5cf6";
                            var headerBg = darkMode ? "#1e1b4b" : "#ede9fe";
                            var headerText = darkMode ? "#c4b5fd" : "#5b21b6";

                            if (sectionPart.startsWith('^')) {
                                // Block embed - extract block by ID
                                var blockId = sectionPart.substring(1);
                                if (root.notesFileHandler) {
                                    extractedContent = root.notesFileHandler.extractBlock(targetNotePath, blockId);
                                }

                                if (extractedContent && extractedContent.length > 0) {
                                    return '<div style="background:' + embedBg + ';border-left:4px solid ' + accentColor + ';border-radius:8px;margin:8px 0;overflow:hidden;">' + '<div style="background:' + headerBg + ';padding:6px 12px;font-size:11px;color:' + headerText + ';">📎 <b>' + pagePart + '</b> › ' + blockId + '</div>' + '<div style="padding:10px 14px;color:' + embedText + ';line-height:1.5;">' + extractedContent + '</div></div>';
                                } else {
                                    return '<span style="display:inline-block;background:' + errorBg + ';border-left:3px solid ' + errorBorder + ';border-radius:4px;padding:4px 8px;color:' + errorText + ';font-size:12px;">在 <b>' + pagePart + '</b> 中未找到 "' + blockId + '"</span>';
                                }
                            } else {
                                // Section embed - extract section by heading
                                if (root.notesFileHandler) {
                                    extractedContent = root.notesFileHandler.extractSection(targetNotePath, sectionPart);
                                }

                                if (extractedContent && extractedContent.length > 0) {
                                    return '<div style="background:' + embedBg + ';border-left:4px solid ' + accentColor + ';border-radius:8px;margin:8px 0;overflow:hidden;">' + '<div style="background:' + headerBg + ';padding:6px 12px;font-size:11px;color:' + headerText + ';">📑 <b>' + pagePart + '</b> › ' + sectionPart + '</div>' + '<div style="padding:10px 14px;color:' + embedText + ';line-height:1.5;">' + extractedContent + '</div></div>';
                                } else {
                                    return '<span style="display:inline-block;background:' + errorBg + ';border-left:3px solid ' + errorBorder + ';border-radius:4px;padding:4px 8px;color:' + errorText + ';font-size:12px;">在 <b>' + pagePart + '</b> 中未找到 "' + sectionPart + '"</span>';
                                }
                            }
                        } else if (sectionPart !== "" && targetNotePath === "") {
                            // Note not found
                            if (sectionPart.startsWith('^')) {
                                return '<span style="display:inline-block;background:' + errorBg + ';border-left:3px solid ' + errorBorder + ';border-radius:4px;padding:4px 8px;color:' + errorText + ';font-size:12px;">在 <b>' + pagePart + '</b> 中未找到 "' + sectionPart.substring(1) + '"</span>';
                            } else {
                                return '<span style="display:inline-block;background:' + errorBg + ';border-left:3px solid ' + errorBorder + ';border-radius:4px;padding:4px 8px;color:' + errorText + ';font-size:12px;">在 <b>' + pagePart + '</b> 中未找到 "' + sectionPart + '"</span>';
                            }
                        } else {
                            // Full note embed (no section specified)
                            var noteAccent = darkMode ? "#0ea5e9" : "#0284c7";
                            var noteHeaderBg = darkMode ? "#0c4a6e" : "#e0f2fe";
                            var noteHeaderText = darkMode ? "#7dd3fc" : "#0369a1";

                            if (targetNotePath !== "" && root.notesFileHandler) {
                                var noteData = root.notesFileHandler.readNote(targetNotePath);
                                var noteContent = noteData.content || "";
                                if (noteContent.length > 0) {
                                    // Limit preview length for full embeds
                                    var preview = noteContent.length > 500 ? noteContent.substring(0, 500) + "..." : noteContent;
                                    return '<div style="background:' + embedBg + ';border-left:4px solid ' + noteAccent + ';border-radius:8px;margin:8px 0;overflow:hidden;">' + '<div style="background:' + noteHeaderBg + ';padding:6px 12px;font-size:11px;color:' + noteHeaderText + ';">📄 <b>' + pagePart + '</b></div>' + '<div style="padding:10px 14px;color:' + embedText + ';line-height:1.5;">' + preview + '</div></div>';
                                }
                            }
                            return '<span style="display:inline-block;background:' + embedBg + ';border-left:3px solid ' + embedBorder + ';border-radius:4px;padding:4px 8px;color:' + embedText + ';font-size:12px;">📄 嵌入: ' + pagePart + '</span>';
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

            // Arrow key navigation between blocks
            Keys.onUpPressed: event => {
                // Check if cursor is at the first line
                var lineHeight = font.pixelSize * 1.2; // Approximate line height
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
                var lineHeight = font.pixelSize * 1.2;
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
