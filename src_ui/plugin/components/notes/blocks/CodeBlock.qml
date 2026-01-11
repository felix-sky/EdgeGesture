import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: root
    width: ListView.view ? ListView.view.width - 20 : 300
    implicitHeight: loader.implicitHeight + 16

    property string content: model.content ? model.content : ""
    property string language: model.language ? model.language : ""
    property bool isEditing: false
    property string folderPath: ""
    property var noteListView: null
    property var notesIndex: null
    property var editor: null
    property var onLinkActivatedCallback: null
    property var notesFileHandler: null
    property string notePath: ""
    property string vaultRootPath: ""
    property int blockIndex: -1
    property string type: "code"
    property int level: 0

    Rectangle {
        anchors.fill: parent
        color: FluTheme.dark ? "#2d2d2d" : "#f0f0f0"
        radius: 4
        border.color: FluTheme.dark ? "#404040" : "#d0d0d0"
        border.width: 1
    }

    Loader {
        id: loader
        // Determine width but don't anchor.fill to avoid height loops
        width: parent.width - 16
        x: 8
        y: 8
        sourceComponent: isEditing ? editorComp : viewerComp
    }

    Component {
        id: viewerComp
        Text {
            width: root.width - 16
            text: root.content
            color: FluTheme.dark ? "#dcdcdc" : "#333333"
            font.family: "Consolas, monospace"
            font.pixelSize: 14
            textFormat: Text.PlainText

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.IBeamCursor
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: mouse => {
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
            width: root.width - 16
            text: root.content
            color: FluTheme.dark ? "#dcdcdc" : "#333333"
            font.family: "Consolas, monospace"
            font.pixelSize: 14
            background: null
            selectByMouse: true

            Keys.onPressed: event => {
                if ((event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) && text === "") {
                    if (root.blockIndex >= 0 && root.noteListView && root.noteListView.model) {
                        var idxToRemove = root.blockIndex;
                        var count = root.noteListView.count;

                        if (idxToRemove > 0) {
                            if (root.editor) {
                                root.editor.navigateToBlock(idxToRemove - 1, true);
                            }
                            if (typeof root.noteListView.model.removeBlock === "function")
                                root.noteListView.model.removeBlock(idxToRemove);
                            event.accepted = true;
                        } else if (count > 1) {
                            if (typeof root.noteListView.model.removeBlock === "function")
                                root.noteListView.model.removeBlock(idxToRemove);
                            if (root.editor) {
                                root.editor.navigateToBlock(0, false);
                            }
                            event.accepted = true;
                        }
                    }
                }
            }

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

    // Optional: Language Badge
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 4
        visible: root.language !== "" && !root.isEditing
        width: langText.width + 8
        height: langText.height + 4
        color: FluTheme.dark ? "#404040" : "#e0e0e0"
        radius: 3

        Text {
            id: langText
            anchors.centerIn: parent
            text: root.language
            font.pixelSize: 10
            color: FluTheme.dark ? "#aaa" : "#666"
        }
    }
}
