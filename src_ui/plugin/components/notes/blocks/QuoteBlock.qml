import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: root
    width: ListView.view ? ListView.view.width - 20 : 300
    height: loader.item ? loader.item.height : 24

    property string content: ""
    property bool isEditing: false
    property string folderPath: ""
    property var noteListView: null
    property var notesIndex: null
    property int blockIndex: -1
    property string type: "quote"
    property int level: 0
    property var editor: null

    property var onLinkActivatedCallback: null
    property var notesFileHandler: null // For image search
    property string notePath: "" // Current note path
    property string vaultRootPath: "" // Root of the vault for image search

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

    // Strings for debugging
    Component.onCompleted: {}

    // Sync content when model changes
    onContentChanged: {
        if (!isEditing && loader.item && loader.item.text !== undefined) {
            // binding handles this
        }
    }

    Loader {
        id: loader
        width: parent.width
        sourceComponent: isEditing ? editorComp : viewerComp
    }

    Component {
        id: viewerComp
        Rectangle {
            width: root.width
            height: textItem.contentHeight + 20
            color: FluTheme.dark ? "#333333" : "#f0f0f0"
            radius: 4
            // Left accent border
            Rectangle {
                width: 4
                height: parent.height
                color: FluTheme.primaryColor
                anchors.left: parent.left
            }

            Text {
                id: textItem
                width: parent.width - 24
                anchors.centerIn: parent
                wrapMode: Text.Wrap
                // Pre-process content to handle Obsidian-style images and links via Markdown
                text: {
                    var t = root.content;
                    var basePath = "file:///" + root.folderPath.replace(/\\/g, "/") + "/";

                    // Highlight
                    var highlightColor = FluTheme.dark ? "rgba(255, 215, 0, 0.4)" : "rgba(255, 255, 0, 0.5)";
                    t = t.replace(/==(.*?)==/g, '<span style="background-color: ' + highlightColor + ';">$1</span>');

                    // Replace ![[image.png]] with standard Markdown ![image.png](url)
                    t = t.replace(/!\[\[(.*?)\]\]/g, function (match, p1) {
                        var src = p1;
                        if (src.indexOf(":") === -1 && src.indexOf("/") !== 0) {
                            src = basePath + src;
                        } else if (src.indexOf("file://") !== 0) {
                            src = "file:///" + src;
                        }
                        src = src.replace(/ /g, "%20");
                        return '<img src="' + src + '" width="320">';
                    });

                    // Replace [[Link]] with <a href="Link">Link</a>
                    t = t.replace(/\[\[(.*?)\]\]/g, function (match, p1) {
                        return '<a href="' + encodeURIComponent(p1) + '">' + p1 + '</a>';
                    });
                    return t;
                }
                color: FluTheme.dark ? "#cccccc" : "#555555"
                font.pixelSize: 16
                font.italic: true
                font.family: "Segoe UI"
                textFormat: Text.MarkdownText
                baseUrl: root.folderPath ? "file:///" + root.folderPath.replace(/\\/g, "/") + "/" : ""
                linkColor: FluTheme.primaryColor

                Component.onCompleted: {
                    console.log("QuoteBlock Ready.");
                }

                onLinkActivated: link => {
                    var decodedLink = decodeURIComponent(link);
                    var globalPath = "";
                    if (root.notesIndex) {
                        globalPath = root.notesIndex.findPathByTitle(decodedLink);
                    }
                    if (globalPath !== "") {
                        if (root.onLinkActivatedCallback)
                            root.onLinkActivatedCallback(globalPath);
                        return;
                    }
                    if (decodedLink.endsWith(".md")) {
                        var p = root.folderPath + "/" + decodedLink;
                        if (root.onLinkActivatedCallback)
                            root.onLinkActivatedCallback(p);
                        return;
                    }
                    Qt.openUrlExternally(link);
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.IBeamCursor
                    onClicked: mouse => {
                        var link = parent.linkAt(mouse.x, mouse.y);
                        if (link) {
                            parent.linkActivated(link);
                        } else {
                            if (root.noteListView) {
                                root.noteListView.currentIndex = root.blockIndex;
                                root.isEditing = true;
                            }
                        }
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
        Rectangle {
            id: editorContainer
            width: root.width
            height: editorArea.contentHeight + 20
            color: FluTheme.dark ? "#333333" : "#f0f0f0"
            radius: 4

            // Left accent border
            Rectangle {
                width: 4
                height: parent.height
                color: FluTheme.primaryColor
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
            }

            FluentEditorArea {
                id: editorArea
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.topMargin: 10

                text: root.content

                customBackgroundColor: "transparent"
                customTextColor: root.editor ? root.editor.contrastColor : (FluTheme.dark ? "#cccccc" : "#555555")
                customSelectionColor: FluTheme.primaryColor

                font.pixelSize: 16
                font.italic: true
                font.family: "Segoe UI"

                // Override the default Enter behavior
                Keys.onReturnPressed: event => handleEnter(event)
                Keys.onEnterPressed: event => handleEnter(event)

                // Arrow key navigation between blocks
                Keys.onUpPressed: event => {
                    var lineHeight = font.pixelSize * 1.5;
                    var isFirstLine = cursorRectangle.y < lineHeight;
                    if (isFirstLine && root.blockIndex > 0) {
                        finishEdit();
                        if (root.editor && typeof root.editor.goToPreviousBlock === "function") {
                            root.editor.goToPreviousBlock(root.blockIndex);
                            event.accepted = true;
                            return;
                        }
                    }
                    event.accepted = false;
                }

                Keys.onDownPressed: event => {
                    var lineHeight = font.pixelSize * 1.5;
                    var textHeight = contentHeight > 0 ? contentHeight : lineHeight;
                    var isLastLine = cursorRectangle.y >= (textHeight - lineHeight);
                    if (isLastLine && root.editor) {
                        finishEdit();
                        if (typeof root.editor.goToNextBlock === "function") {
                            root.editor.goToNextBlock(root.blockIndex);
                            event.accepted = true;
                            return;
                        }
                    }
                    event.accepted = false;
                }

                Keys.onPressed: event => {
                    if ((event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) && text === "") {
                        if (root.blockIndex >= 0 && root.noteListView && root.noteListView.model) {
                            var idxToRemove = root.blockIndex;
                            var count = root.noteListView.count;
                            if (idxToRemove > 0) {
                                if (root.editor)
                                    root.editor.navigateToBlock(idxToRemove - 1, true);
                                if (typeof root.noteListView.model.removeBlock === "function")
                                    root.noteListView.model.removeBlock(idxToRemove);
                                event.accepted = true;
                            } else if (count > 1) {
                                if (typeof root.noteListView.model.removeBlock === "function")
                                    root.noteListView.model.removeBlock(idxToRemove);
                                if (root.editor)
                                    root.editor.navigateToBlock(0, false);
                                event.accepted = true;
                            }
                        }
                    }
                }

                function handleEnter(event) {
                    event.accepted = true;
                    var pos = cursorPosition;
                    var fullText = text;
                    var preText = fullText.substring(0, pos);
                    var postText = fullText.substring(pos);

                    if (root.noteListView && root.noteListView.model) {
                        root.noteListView.model.updateBlock(root.blockIndex, preText);
                        // Continue quote by default
                        root.noteListView.model.insertBlock(root.blockIndex + 1, "quote", postText);

                        root.nextBlockIndex = root.blockIndex + 1;
                        newBlockFocusTimer.start();
                    }
                    root.isEditing = false;
                }

                Component.onCompleted: {
                    focusTimer.start();
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
                    onTriggered: editorArea.forceActiveFocus()
                }

                onEditingFinished: {
                    finishEdit();
                }

                onActiveFocusChanged: {
                    if (!activeFocus)
                        finishEdit();
                }

                function finishEdit() {
                    if (root.isEditing) {
                        if (root.noteListView && root.noteListView.model) {
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
}
