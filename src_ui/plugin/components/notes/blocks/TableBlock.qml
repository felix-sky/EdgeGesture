import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import FluentUI 1.0
import EdgeGesture.Notes 1.0

Item {
    id: root
    width: parent.width
    height: loader.item ? loader.item.height + 16 : 60

    // Properties required by NotesEditor delegate
    property int blockIndex: -1
    property var noteListView: null
    property var notesIndex: null
    property var notesFileHandler: null
    property string notePath: ""
    property string folderPath: ""
    property var editor: null
    property string content: ""
    property bool isEditing: false
    property var metadata: ({})
    property string vaultRootPath: ""
    property var onLinkActivatedCallback: null

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

    // Format cell content with markdown rendering (same as ParagraphBlock)
    function formatCellContent(cellText) {
        var t = cellText;

        // Math processing
        var currentFontSize = 14;
        var darkMode = root.editor ? root.editor.isDarkColor(root.editor.currentColor) : FluTheme.dark;
        t = MathHelper.processMathToPlaceholders(t, currentFontSize, darkMode);

        var _cacheVer = root.imageCacheVersion;

        // Obsidian Embed syntax: ![[...]]
        t = t.replace(/!\[\[(.*?)\]\]/g, function (match, p1) {
            var embedContent = p1;
            var imageExtensions = /\.(png|jpg|jpeg|gif|bmp|svg|webp|ico|tiff?)$/i;
            if (imageExtensions.test(embedContent)) {
                var src = root.resolveImageAsync(embedContent);
                if (src !== "") {
                    return '<img src="' + src + '" width="100" style="vertical-align: middle;">';
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

    Loader {
        id: loader
        width: parent.width
        sourceComponent: isEditing ? editorComp : viewerComp
    }

    Component {
        id: viewerComp
        Item {
            width: loader.width
            height: tableColumn.height

            ColumnLayout {
                id: tableColumn
                width: parent.width
                spacing: 0

                // Table container with border
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: tableContent.height + 2
                    color: "transparent"
                    border.color: root.editor ? Qt.rgba(root.editor.contrastColor.r, root.editor.contrastColor.g, root.editor.contrastColor.b, 0.3) : (FluTheme.dark ? "#444444" : "#CCCCCC")
                    border.width: 1
                    radius: 6

                    Column {
                        id: tableContent
                        width: parent.width - 2
                        x: 1
                        y: 1
                        spacing: 0

                        Repeater {
                            model: root.metadata && root.metadata.rows ? root.metadata.rows : []

                            delegate: Rectangle {
                                width: tableContent.width
                                height: rowContent.height
                                color: {
                                    // Header row (first row) has slightly different bg
                                    if (index === 0) {
                                        return root.editor ? Qt.rgba(root.editor.contrastColor.r, root.editor.contrastColor.g, root.editor.contrastColor.b, 0.1) : (FluTheme.dark ? "rgba(255,255,255,0.08)" : "rgba(0,0,0,0.04)");
                                    }
                                    // Alternating rows
                                    return index % 2 === 0 ? "transparent" : (root.editor ? Qt.rgba(root.editor.contrastColor.r, root.editor.contrastColor.g, root.editor.contrastColor.b, 0.03) : (FluTheme.dark ? "rgba(255,255,255,0.03)" : "rgba(0,0,0,0.02)"));
                                }

                                // Bottom border
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: 1
                                    color: root.editor ? Qt.rgba(root.editor.contrastColor.r, root.editor.contrastColor.g, root.editor.contrastColor.b, 0.15) : (FluTheme.dark ? "#333333" : "#E0E0E0")
                                    visible: index < (root.metadata.rows.length - 1)
                                }

                                Row {
                                    id: rowContent
                                    width: parent.width
                                    spacing: 0

                                    Repeater {
                                        model: modelData

                                        delegate: Item {
                                            // Calculate cell width based on number of columns
                                            width: {
                                                var colCount = root.metadata.rows[0] ? root.metadata.rows[0].length : 1;
                                                return (tableContent.width - (colCount - 1)) / colCount;
                                            }
                                            height: cellText.height + 16

                                            // Right border for all but last cell
                                            Rectangle {
                                                anchors.right: parent.right
                                                width: 1
                                                height: parent.height
                                                color: root.editor ? Qt.rgba(root.editor.contrastColor.r, root.editor.contrastColor.g, root.editor.contrastColor.b, 0.15) : (FluTheme.dark ? "#333333" : "#E0E0E0")
                                                visible: index < (root.metadata.rows[0].length - 1)
                                            }

                                            Text {
                                                id: cellText
                                                anchors.centerIn: parent
                                                width: parent.width - 16
                                                text: formatCellContent(modelData)
                                                wrapMode: Text.Wrap
                                                color: root.editor ? root.editor.contrastColor : (FluTheme.dark ? "#FFFFFF" : "#000000")
                                                font.pixelSize: 14
                                                font.family: "Segoe UI"
                                                font.weight: index === 0 ? Font.DemiBold : Font.Normal
                                                textFormat: Text.RichText
                                                linkColor: FluTheme.primaryColor
                                                horizontalAlignment: Text.AlignLeft

                                                onLinkActivated: link => {
                                                    var decodedLink = decodeURIComponent(link);

                                                    if (decodedLink.startsWith("http://") || decodedLink.startsWith("https://") || decodedLink.startsWith("mailto:") || decodedLink.startsWith("file://")) {
                                                        Qt.openUrlExternally(link);
                                                        return;
                                                    }

                                                    var pagePart = decodedLink;
                                                    var anchorPart = "";
                                                    var hashIndex = decodedLink.indexOf('#');
                                                    if (hashIndex !== -1) {
                                                        pagePart = decodedLink.substring(0, hashIndex);
                                                        anchorPart = decodedLink.substring(hashIndex + 1);
                                                    }

                                                    if (pagePart === "" && anchorPart !== "") {
                                                        return;
                                                    }

                                                    var globalPath = "";
                                                    if (root.notesIndex) {
                                                        globalPath = root.notesIndex.findPathByTitle(pagePart);
                                                    }

                                                    if (globalPath !== "") {
                                                        if (root.onLinkActivatedCallback) {
                                                            root.onLinkActivatedCallback(globalPath, anchorPart);
                                                        }
                                                        return;
                                                    }

                                                    if (pagePart.endsWith(".md")) {
                                                        var p = root.folderPath + "/" + pagePart;
                                                        if (root.onLinkActivatedCallback) {
                                                            root.onLinkActivatedCallback(p, anchorPart);
                                                        }
                                                        return;
                                                    }

                                                    var wikiPath = root.folderPath + "/" + pagePart + ".md";
                                                    if (root.notesFileHandler && root.notesFileHandler.exists(wikiPath)) {
                                                        if (root.onLinkActivatedCallback) {
                                                            root.onLinkActivatedCallback(wikiPath, anchorPart);
                                                        }
                                                        return;
                                                    }

                                                    if (root.notesFileHandler) {
                                                        var newPath = root.notesFileHandler.createNote(root.folderPath, pagePart, "", "#624a73");
                                                        if (newPath !== "" && root.onLinkActivatedCallback) {
                                                            if (root.notesIndex) {
                                                                root.notesIndex.updateEntry(newPath);
                                                            }
                                                            root.onLinkActivatedCallback(newPath, anchorPart);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                hoverEnabled: true

                onClicked: mouse => {
                    // Check if we clicked on a link first
                    // If not, enter edit mode
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
            id: textEdit
            width: loader.width
            text: {
                // Reconstruct markdown table from metadata
                var result = "";
                var rows = root.metadata && root.metadata.rows ? root.metadata.rows : [];
                for (var i = 0; i < rows.length; i++) {
                    var cells = rows[i];
                    result += "|";
                    for (var j = 0; j < cells.length; j++) {
                        result += " " + cells[j] + " |";
                    }
                    result += "\n";
                    // Add separator after header
                    if (i === 0 && cells.length > 0) {
                        result += "|";
                        for (var k = 0; k < cells.length; k++) {
                            result += " --- |";
                        }
                        result += "\n";
                    }
                }
                return result.trim();
            }

            customTextColor: root.editor ? root.editor.contrastColor : "#000000"
            customSelectionColor: FluTheme.primaryColor
            customBackgroundColor: root.editor ? root.editor.editBackgroundColor : "transparent"

            font.pixelSize: 14
            font.family: "Consolas"

            Keys.onReturnPressed: event => handleEnter(event)
            Keys.onEnterPressed: event => handleEnter(event)

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
                    root.noteListView.model.replaceBlock(root.blockIndex, text);
                }
            }

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
                // For tables, allow normal multi-line editing
                // Don't split on Enter - just insert newline
                event.accepted = false;
            }
        }
    }
}
