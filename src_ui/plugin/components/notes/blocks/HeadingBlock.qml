import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: root
    width: ListView.view ? ListView.view.width - 20 : 300
    height: loader.item ? loader.item.implicitHeight : 32

    property string content: model.content ? model.content : ""
    property int level: model.level ? model.level : 1
    property bool isEditing: false
    property string folderPath: ""
    property var noteListView: null
    property var notesIndex: null
    property var onLinkActivatedCallback: null
    property var notesFileHandler: null // For compatibility
    property string notePath: "" // For compatibility
    property string vaultRootPath: "" // For compatibility
    property int blockIndex: -1

    Loader {
        id: loader
        width: parent.width
        sourceComponent: isEditing ? editorComp : viewerComp
    }

    Component {
        id: viewerComp
        Text {
            width: root.width
            text: root.content
            wrapMode: Text.Wrap
            color: FluTheme.dark ? "#FFFFFF" : "#000000"
            font.pixelSize: {
                switch (root.level) {
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
            font.weight: Font.Bold
            font.family: "Segoe UI"
            topPadding: 10
            bottomPadding: 5

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.IBeamCursor
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: mouse => {
                    // Headings don't usually have links, but to be consistent:
                    if (root.noteListView) {
                        root.noteListView.currentIndex = root.blockIndex;
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
        TextArea {
            width: root.width
            text: root.content
            wrapMode: Text.Wrap
            color: FluTheme.dark ? "#FFFFFF" : "#000000"
            font.pixelSize: viewerComp.item ? viewerComp.item.font.pixelSize : 16

            background: null
            selectByMouse: true

            Component.onCompleted: {
                forceActiveFocus();
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
                        root.noteListView.model.updateBlock(root.blockIndex, text);
                    }
                    root.isEditing = false;
                }
            }
        }
    }
}
