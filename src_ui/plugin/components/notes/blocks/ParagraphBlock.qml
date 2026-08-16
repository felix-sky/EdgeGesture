import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0
import EdgeGesture.Notes 1.0

Item {
    id: root
    width: ListView.view ? ListView.view.width - 20 : 300
    height: loader.item ? Math.max(loader.item.contentHeight, loader.item.implicitHeight) + 16 : 24

    property string content: model.content ? model.content : ""
    property string formattedContent: model.formattedContent ? model.formattedContent : ""
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
    property string type: model.type ? model.type : "paragraph"
    property int level: model.level !== undefined ? model.level : 1

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

            readonly property int cachedFontSize: root.getFontSize()
            readonly property int cachedFontWeight: root.getFontWeight()

            text: {
                var raw = root.formattedContent ? root.formattedContent : root.content;
                var currentFontSize = textItem.cachedFontSize;
                var darkMode = root.editor ? root.editor.isDarkColor(root.editor.currentColor) : FluTheme.dark;
                var processed = MathHelper.processMathToPlaceholders(raw, currentFontSize, darkMode);
                return MathHelper.restoreMathPlaceholders(processed);
            }
            wrapMode: Text.Wrap
            color: root.editor ? root.editor.contrastColor : (FluTheme.dark ? "#cccccc" : "#222222")
            font.pixelSize: cachedFontSize
            font.weight: cachedFontWeight
            font.family: "Segoe UI"
            textFormat: Text.RichText
            linkColor: FluTheme.primaryColor

            onLinkActivated: link => {
                if (root.onLinkActivatedCallback) {
                    root.onLinkActivatedCallback(link);
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
                        if (root.editor) {
                            root.editor.beginEditing(root.blockIndex);
                        }
                    }
                }
            }
        }
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

            customTextColor: root.editor ? root.editor.contrastColor : (FluTheme.dark ? "#FFFFFF" : "#000000")
            customSelectionColor: FluTheme.primaryColor
            customBackgroundColor: root.editor ? root.editor.editBackgroundColor : "transparent"

            font.pixelSize: getFontSize()
            font.weight: getFontWeight()
            font.family: "Segoe UI"

            Keys.onReturnPressed: event => handleEnter(event)
            Keys.onEnterPressed: event => handleEnter(event)

            Keys.onUpPressed: event => {
                var lineHeight = font.pixelSize * 1.5;
                var isFirstLine = cursorRectangle.y < lineHeight;
                if (isFirstLine && root.blockIndex > 0) {
                    finishEdit();
                    if (root.editor) {
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
                    root.editor.goToNextBlock(root.blockIndex);
                    event.accepted = true;
                    return;
                }
                event.accepted = false;
            }

            Keys.onPressed: event => {
                if (event.matches(StandardKey.Paste)) {
                    if (root.notesFileHandler) {
                        var imagePath = root.notesFileHandler.saveClipboardImage(root.folderPath);
                        if (imagePath !== "") {
                            insert(cursorPosition, "![[" + imagePath + "]]");
                            event.accepted = true;
                            return;
                        }
                    }
                }

                if ((event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) && text === "") {
                    if (root.blockIndex >= 0 && root.noteListView && root.noteListView.model) {
                        var idxToRemove = root.blockIndex;
                        var count = root.noteListView.count;

                        if (idxToRemove > 0) {
                            if (root.editor) {
                                root.editor.navigateToBlock(idxToRemove - 1, true);
                            }
                            root.noteListView.model.removeBlock(idxToRemove);
                            event.accepted = true;
                        } else if (count > 1) {
                            root.noteListView.model.removeBlock(idxToRemove);
                            if (root.editor) {
                                root.editor.navigateToBlock(0, false);
                            }
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
                    root.noteListView.model.insertBlock(root.blockIndex + 1, "paragraph", postText);

                    root.nextBlockIndex = root.blockIndex + 1;
                    newBlockFocusTimer.start();
                }
                if (root.editor) {
                    root.editor.endEditing(root.blockIndex);
                }
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
                        root.noteListView.model.replaceBlock(root.blockIndex, text);
                    }
                    if (root.editor) {
                        root.editor.endEditing(root.blockIndex);
                    }
                }
            }
        }
    }
}
