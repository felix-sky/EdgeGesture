import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import FluentUI 1.0
import EdgeGesture.Notes 1.0

Item {
    id: root
    width: parent.width
    height: loader.item ? loader.item.height + 16 : 60

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
                                    if (index === 0) {
                                        return root.editor ? Qt.rgba(root.editor.contrastColor.r, root.editor.contrastColor.g, root.editor.contrastColor.b, 0.1) : (FluTheme.dark ? "rgba(255,255,255,0.08)" : "rgba(0,0,0,0.04)");
                                    }
                                    return index % 2 === 0 ? "transparent" : (root.editor ? Qt.rgba(root.editor.contrastColor.r, root.editor.contrastColor.g, root.editor.contrastColor.b, 0.03) : (FluTheme.dark ? "rgba(255,255,255,0.03)" : "rgba(0,0,0,0.02)"));
                                }

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
                                            width: {
                                                var colCount = root.metadata.rows[0] ? root.metadata.rows[0].length : 1;
                                                return (tableContent.width - (colCount - 1)) / colCount;
                                            }
                                            height: cellText.height + 16

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
                                                text: {
                                                    var raw = modelData ? modelData.toString() : "";
                                                    var darkMode = root.editor ? root.editor.isDarkColor(root.editor.currentColor) : FluTheme.dark;
                                                    var processed = MathHelper.processMathToPlaceholders(raw, 14, darkMode);
                                                    return MathHelper.restoreMathPlaceholders(processed);
                                                }
                                                wrapMode: Text.Wrap
                                                color: root.editor ? root.editor.contrastColor : (FluTheme.dark ? "#FFFFFF" : "#000000")
                                                font.pixelSize: 14
                                                font.family: "Segoe UI"
                                                font.weight: index === 0 ? Font.DemiBold : Font.Normal
                                                textFormat: Text.RichText
                                                linkColor: FluTheme.primaryColor
                                                horizontalAlignment: Text.AlignLeft

                                                onLinkActivated: link => {
                                                    if (root.onLinkActivatedCallback) {
                                                        root.onLinkActivatedCallback(link);
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
                    if (root.editor) {
                        root.editor.beginEditing(root.blockIndex);
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
            text: {
                var result = "";
                var rows = root.metadata && root.metadata.rows ? root.metadata.rows : [];
                for (var i = 0; i < rows.length; i++) {
                    var cells = rows[i];
                    result += "|";
                    for (var j = 0; j < cells.length; j++) {
                        result += " " + cells[j] + " |";
                    }
                    result += "\n";
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
                    if (root.blockIndex >= 0 && root.noteListView && root.noteListView.model) {
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
