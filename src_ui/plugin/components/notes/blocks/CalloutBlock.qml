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
    property string foldState: metadata["foldState"] ? metadata["foldState"] : ""
    property bool isFoldable: foldState === "+" || foldState === "-"
    property bool isCollapsed: metadata["isCollapsed"] !== undefined ? metadata["isCollapsed"] : (foldState === "-")

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

                // Header row
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24

                    RowLayout {
                        anchors.fill: parent
                        spacing: 8

                        FluIcon {
                            iconSource: root.getCalloutIcon()
                            iconSize: 16
                            iconColor: root.getThemeColor()
                        }

                        Text {
                            text: root.title
                            font.bold: true
                            font.pixelSize: 15
                            color: root.getThemeColor()
                            Layout.fillWidth: true
                        }

                        // Fold toggle button
                        FluIcon {
                            visible: root.isFoldable
                            iconSource: root.isCollapsed ? FluentIcons.ChevronRight : FluentIcons.ChevronDown
                            iconSize: 12
                            iconColor: root.getThemeColor()
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: root.isFoldable ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (root.isFoldable && root.noteListView && root.noteListView.model) {
                                root.noteListView.model.toggleCalloutFold(root.blockIndex);
                            }
                        }
                    }
                }

                // Collapsible body
                Text {
                    id: bodyText
                    Layout.fillWidth: true
                    text: root.content
                    wrapMode: Text.Wrap
                    color: FluTheme.dark ? "#cccccc" : "#222222"
                    font.pixelSize: 15
                    font.family: "Segoe UI"
                    textFormat: Text.RichText
                    linkColor: FluTheme.primaryColor
                    visible: !root.isCollapsed && root.content.length > 0

                    onLinkActivated: link => {
                        if (root.onLinkActivatedCallback)
                            root.onLinkActivatedCallback(link);
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
                                if (root.editor) {
                                    root.editor.beginEditing(root.blockIndex);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: editorComp
        FluentEditorArea {
            width: parent.width
            text: {
                var foldMarker = root.foldState;
                var header = "> [!" + root.calloutType.toUpperCase() + "]" + foldMarker + " " + root.title;
                var bodyLines = root.content.split('\n');
                var body = "";
                for (var i = 0; i < bodyLines.length; ++i) {
                    body += "\n> " + bodyLines[i];
                }
                return header + (root.content.length > 0 ? body : "");
            }

            customTextColor: FluTheme.dark ? "#FFFFFF" : "#000000"
            customSelectionColor: FluTheme.primaryColor
            customBackgroundColor: FluTheme.dark ? Qt.rgba(0, 0, 0, 0.2) : Qt.rgba(0, 0, 0, 0.05)

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
                    if (root.editor) {
                        root.editor.endEditing(root.blockIndex);
                    }
                }
            }

            Component.onCompleted: {
                forceActiveFocus();
                cursorPosition = length;
            }
        }
    }
}
