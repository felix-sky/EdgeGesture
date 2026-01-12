import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: root
    width: ListView.view ? ListView.view.width - 20 : 300
    implicitHeight: editorLoader.item ? editorLoader.item.height : 0

    property string content: ""
    property var metadata: ({})
    property string calloutType: metadata["calloutType"] ? metadata["calloutType"] : "note"
    property string title: metadata["title"] ? metadata["title"] : "Note"

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
    property string type: "callout"
    property int level: 0

    function getThemeColor() {
        var isDark = FluTheme.dark;
        switch (calloutType.toLowerCase()) {
        case "info":
        case "note":
        case "todo":
            return isDark ? "#60ccff" : "#0078d4"; // Cyan/Blue
        case "tip":
        case "success":
        case "check":
        case "done":
            return isDark ? "#1cc24d" : "#107C10"; // Light Green / Green
        case "question":
        case "help":
        case "warning":
        case "caution":
        case "attention":
            return isDark ? "#ffaa44" : "#d83b01"; // Orange/Gold
        case "failure":
        case "fail":
        case "missing":
        case "danger":
        case "error":
        case "bug":
            return isDark ? "#ff4d4f" : "#c50f1f"; // Red/Salmon
        case "example":
            return isDark ? "#d2a8ff" : "#624a73"; // Light Purple / Purple
        case "quote":
        case "cite":
            return isDark ? "#a1a1a1" : "#505050"; // Gray
        default:
            return isDark ? "#60ccff" : "#0078d4";
        }
    }

    function getCalloutIcon() {
        switch (calloutType.toLowerCase()) {
        case "info":
            return FluentIcons.Info;
        case "tip":
            return FluentIcons.Lightbulb;
        case "success":
            return FluentIcons.Completed;
        case "warning":
            return FluentIcons.Warning;
        case "failure":
            return FluentIcons.Error;
        case "question":
            return FluentIcons.Help;
        case "quote":
            return FluentIcons.Quote;
        default:
            return FluentIcons.Info;
        }
    }

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
            color: Qt.rgba(root.getThemeColor().r, root.getThemeColor().g, root.getThemeColor().b, 0.1)
            border.color: root.getThemeColor()
            border.width: 0
            radius: 4

            // Left border accent
            Rectangle {
                width: 4
                height: parent.height
                color: root.getThemeColor()
                anchors.left: parent.left
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

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    FluIcon {
                        iconSource: root.getCalloutIcon()
                        iconSize: 16
                        iconColor: root.getThemeColor()
                    }

                    Text {
                        text: root.title
                        font.bold: true
                        font.pixelSize: 16
                        color: root.getThemeColor()
                        Layout.fillWidth: true
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: {
                        var t = root.content;
                        // Basic Markdown Rendering (Same as ParagraphBlock/TaskListBlock)
                        // Bold
                        t = t.replace(/\*\*([^*]+)\*\*/g, '<b>$1</b>');
                        t = t.replace(/__([^_]+)__/g, '<b>$1</b>');
                        // Italic
                        // Italic
                        t = t.replace(/\*([^*]+)\*/g, '<i>$1</i>');
                        t = t.replace(/_([^_]+)_/g, '<i>$1</i>');

                        // Highlight
                        var highlightColor = FluTheme.dark ? "rgba(255, 215, 0, 0.4)" : "rgba(255, 255, 0, 0.5)";
                        t = t.replace(/==(.*?)==/g, '<span style="background-color: ' + highlightColor + ';">$1</span>');
                        // Strikethrough
                        t = t.replace(/~~([^~]+)~~/g, '<s>$1</s>');
                        // Code
                        var codeBg = FluTheme.dark ? "rgba(255, 255, 255, 0.12)" : "rgba(0, 0, 0, 0.05)";
                        var codeColor = FluTheme.primaryColor;
                        t = t.replace(/`([^`]+)`/g, '<code style="background-color: ' + codeBg + '; color: ' + codeColor + '; padding: 2px 4px; border-radius: 5px;">$1</code>');

                        // Links
                        t = t.replace(/\[\[(.*?)\]\]/g, function (match, p1) {
                            return '<a href="' + encodeURIComponent(p1) + '">' + p1 + '</a>';
                        });

                        return t;
                    }
                    wrapMode: Text.Wrap
                    color: FluTheme.dark ? "#cccccc" : "#222222"
                    font.pixelSize: 16
                    font.family: "Segoe UI"
                    textFormat: Text.RichText
                    linkColor: FluTheme.primaryColor
                    visible: root.content.length > 0

                    onLinkActivated: link => {
                        if (root.onLinkActivatedCallback)
                            root.onLinkActivatedCallback(link);
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton

                onClicked: {
                    // Edit mode
                    if (root.noteListView) {
                        root.noteListView.currentIndex = root.blockIndex;
                    }
                    root.isEditing = true;
                }
                z: -1 // Behind text
            }
        }
    }

    Component {
        id: editorComp
        FluentEditorArea {
            width: parent.width
            text: {
                // Reconstruct Callout Markup
                var header = "> [!" + root.calloutType.toUpperCase() + "] " + root.title;
                var body = root.content.split('\n').map(line => "> " + line).join('\n');
                return header + (body.length > 0 ? "\n" + body : "");
            }

            customTextColor: FluTheme.dark ? "#FFFFFF" : "#000000"
            customSelectionColor: FluTheme.primaryColor
            customBackgroundColor: FluTheme.dark ? Qt.rgba(0, 0, 0, 0.2) : Qt.rgba(0, 0, 0, 0.05)

            // Allow newlines in callout
            Keys.onReturnPressed: event => {
                event.accepted = false;
            }

            onEditingFinished: finishEdit()
            onActiveFocusChanged: {
                if (!activeFocus)
                    finishEdit();
            }

            function finishEdit() {
                if (root.isEditing) {
                    if (root.noteListView && root.noteListView.model) {
                        root.noteListView.model.replaceBlock(root.blockIndex, text);
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
}
