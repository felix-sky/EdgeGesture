import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: root
    width: parent.width
    height: 20 // Fixed height for divider area

    property int blockIndex: -1
    property var noteListView: null
    property var editor: null
    property bool isEditing: false

    Rectangle {
        height: 2
        width: parent.width
        color: root.editor ? root.editor.secondaryContrastColor : "#888888"
        anchors.centerIn: parent
    }

    // Hidden focus item to handle keyboard navigation/deletion
    Item {
        id: focusItem
        anchors.fill: parent
        focus: true

        Keys.onPressed: {
            if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
                if (root.noteListView) {
                    root.noteListView.model.removeBlock(root.blockIndex);
                }
                event.accepted = true;
            } else if (event.key === Qt.Key_Up && root.editor) {
                root.editor.navigateUp(root.blockIndex);
                event.accepted = true;
            } else if (event.key === Qt.Key_Down && root.editor) {
                root.editor.navigateDown(root.blockIndex);
                event.accepted = true;
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.isEditing = true;
        }
    }

    onIsEditingChanged: {
        if (isEditing) {
            focusItem.forceActiveFocus();
        }
    }
}
