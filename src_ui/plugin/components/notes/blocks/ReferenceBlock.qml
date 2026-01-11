import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: root
    width: parent.width
    height: Math.min(Math.max(40, textEdit.contentHeight + 16), 500) // Min height + padding

    property int blockIndex: -1
    property var noteListView: null
    property var editor: null
    property bool isEditing: false
    property string content: model.content

    signal linkActivatedCallback(string link)

    Rectangle {
        anchors.fill: parent
        color: root.editor ? Qt.rgba(root.editor.contrastColor.r, root.editor.contrastColor.g, root.editor.contrastColor.b, 0.05) : "#10000000"
        radius: 4

        // Left Border
        Rectangle {
            width: 4
            height: parent.height
            color: FluentTheme.primaryColor
            anchors.left: parent.left
        }

        TextArea {
            id: textEdit
            anchors.fill: parent
            anchors.leftMargin: 12 // Space for border
            anchors.rightMargin: 4
            anchors.topMargin: 4
            anchors.bottomMargin: 4

            text: root.content
            wrapMode: Text.Wrap
            font.pixelSize: 15
            color: root.editor ? root.editor.contrastColor : "#000000"
            selectByMouse: true
            background: Item {}
            textFormat: Text.MarkdownText

            onLinkActivated: if (root.linkActivatedCallback)
                root.linkActivatedCallback(link)

            onEditingFinished: {
                if (root.blockIndex >= 0 && root.noteListView) {
                    root.noteListView.model.updateBlock(root.blockIndex, text);
                }
            }

            Keys.onPressed: {
                if (event.key === Qt.Key_Up && cursorPosition === 0 && root.editor) {
                    root.editor.navigateUp(root.blockIndex);
                    event.accepted = true;
                } else if (event.key === Qt.Key_Down && cursorPosition === length && root.editor) {
                    root.editor.navigateDown(root.blockIndex);
                    event.accepted = true;
                }
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
                    }
                }
            }
        }
    }
}
