import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import Qt.labs.platform 1.1
import FluentUI 1.0

Item {
    id: root
    anchors.fill: parent

    property string configPath: StandardPaths.standardLocations(StandardPaths.DocumentsLocation)[0] + "/EdgeGesture/launchpad.json"

    Component.onCompleted: {
        var dir = StandardPaths.standardLocations(StandardPaths.DocumentsLocation)[0] + "/EdgeGesture";
        if (!FileBridge.exists(dir)) {
            FileBridge.createDirectory(dir);
        }
        loadConfig();
    }

    function saveConfig() {
        var data = [];
        for (var i = 0; i < appsModel.count; i++) {
            data.push(appsModel.get(i));
        }
        FileBridge.writeFile(configPath, JSON.stringify(data, null, 4));
    }

    function loadConfig() {
        if (FileBridge.exists(configPath)) {
            var content = FileBridge.readFile(configPath);
            if (content) {
                try {
                    var data = JSON.parse(content);
                    appsModel.clear();
                    for (var i = 0; i < data.length; i++) {
                        appsModel.append(data[i]);
                    }
                } catch (e) {
                    console.log("Error loading launchpad config:", e);
                }
            }
        }
    }

    function addApp(path) {
        if (!path)
            return;

        for (var i = 0; i < appsModel.count; ++i) {
            if (appsModel.get(i).path === path)
                return;
        }

        var fileName = FileBridge.getFileName(path);
        var name = fileName.replace(/\.[^/.]+$/, "");

        if (name.length > 0) {
            name = name.charAt(0).toUpperCase() + name.slice(1);
        }

        appsModel.append({
            "name": name,
            "path": path
        });
        saveConfig();
    }

    function removeApp(index) {
        if (index >= 0 && index < appsModel.count) {
            appsModel.remove(index);
            saveConfig();
        }
    }

    ListModel {
        id: appsModel
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header (Title)
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            Layout.leftMargin: 20
            Layout.rightMargin: 20

            FluText {
                text: "Launchpad"
                font: FluTextStyle.Title
                anchors.verticalCenter: parent.verticalCenter
            }

            // Add button
            FluIconButton {
                iconSource: FluentIcons.Add
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                onClicked: fileDialog.open()
            }
        }

        // Grid of app icons
        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 10
            cellWidth: 80
            cellHeight: 100
            clip: true

            model: appsModel

            delegate: Item {
                width: grid.cellWidth
                height: grid.cellHeight

                Column {
                    anchors.centerIn: parent
                    spacing: 5

                    Rectangle {
                        width: 56
                        height: 56
                        radius: 18 // Squircleish
                        color: "transparent"
                        anchors.horizontalCenter: parent.horizontalCenter

                        Image {
                            anchors.fill: parent
                            source: "image://exe_icon/" + model.path
                            smooth: true
                            mipmap: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onPressed: parent.scale = 0.9
                            onReleased: parent.scale = 1.0
                            onClicked: {
                                root.appLaunched();
                                Qt.openUrlExternally("file:///" + model.path);
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            onClicked: {
                                contextMenu.currentIndex = index;
                                contextMenu.popup();
                            }
                        }
                    }

                    FluText {
                        text: model.name
                        horizontalAlignment: Text.AlignHCenter
                        width: parent.width
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        color: FluTheme.dark ? "#eee" : "#333"
                    }
                }
            }
        }
    }

    signal appLaunched

    Menu {
        id: contextMenu
        property int currentIndex: -1
        MenuItem {
            text: "Remove"
            onTriggered: {
                if (contextMenu.currentIndex >= 0) {
                    root.removeApp(contextMenu.currentIndex);
                }
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: "Select Application"
        nameFilters: ["Executables (*.exe)", "All Files (*)"]
        folder: StandardPaths.standardLocations(StandardPaths.ApplicationsLocation)[0]
        onAccepted: {
            var path = FileBridge.urlToPath(fileDialog.file.toString());
            root.addApp(path);
        }
    }
}
