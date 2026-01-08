import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import FluentUI 1.0
import Qt5Compat.GraphicalEffects

FluPage {
    id: root
    title: "Plugins"

    Connections {
        target: ConfigBridge.profileManager
        function onCurrentProfileChanged() {
            listView.model = null;
            listView.model = Plugin.plugins;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Item {
                Layout.fillWidth: true
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: listView
                anchors.fill: parent
                orientation: ListView.Horizontal
                spacing: 30
                clip: true

                model: Plugin.plugins

                delegate: Item {
                    width: 200
                    height: listView.height

                    property string pName: modelData.name
                    property bool isEnabled: ConfigBridge.enabledPlugins.indexOf(pName) !== -1

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: parent.width
                        spacing: 15

                        // Header: Status and Name
                        ColumnLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 8

                            Rectangle {
                                width: 28
                                height: 28
                                radius: 14
                                color: isEnabled ? "#FF9F0A" : "#333333" // Orange active, dark inactive
                                Layout.alignment: Qt.AlignHCenter

                                FluIcon {
                                    anchors.centerIn: parent
                                    iconSource: FluentIcons.Accept
                                    iconSize: 14
                                    iconColor: isEnabled ? "black" : "white"
                                    visible: isEnabled
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: togglePlugin()
                                    cursorShape: Qt.PointingHandCursor
                                }
                            }

                            FluText {
                                text: pName
                                font: FluTextStyle.BodyStrong
                                Layout.alignment: Qt.AlignHCenter
                                color: isEnabled ? (FluTheme.dark ? "#FFFFFF" : "#000000") : "#808080"
                            }
                        }

                        // Card Preview
                        Item {
                            Layout.preferredWidth: 180
                            Layout.preferredHeight: 400 // Phone aspect ratio
                            Layout.alignment: Qt.AlignHCenter

                            // Source Image (Hidden)
                            Image {
                                id: sourceImg
                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                source: "file:///" + FileBridge.getAppPath() + "/plugin/preview/" + pName + ".png"
                                visible: false

                                onStatusChanged: {
                                    if (status === Image.Error) {
                                        source = "file:///" + FileBridge.getAppPath() + "/plugin/preview/default.png";
                                    }
                                }
                            }

                            // Mask Rectangle (Hidden)
                            Rectangle {
                                id: maskRect
                                anchors.fill: parent
                                radius: 30
                                visible: false
                            }

                            // Mask Effect
                            OpacityMask {
                                anchors.fill: parent
                                source: sourceImg
                                maskSource: maskRect
                            }

                            // Dim effect if disabled
                            Rectangle {
                                anchors.fill: parent
                                color: "black"
                                opacity: isEnabled ? 0.0 : 0.5
                                radius: 30
                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: 200
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: togglePlugin()
                                cursorShape: Qt.PointingHandCursor
                            }
                        }

                        // Footer: Edit Button
                        FluText {
                            text: "Edit"
                            font: FluTextStyle.Caption
                            color: isEnabled ? "#FF9F0A" : "#555555"
                            Layout.alignment: Qt.AlignHCenter

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    // Placeholder for Edit functionality
                                    console.log("Edit clicked for " + pName);
                                }
                            }
                        }
                    }

                    function togglePlugin() {
                        var list = [];
                        for (var i = 0; i < ConfigBridge.enabledPlugins.length; i++) {
                            list.push(ConfigBridge.enabledPlugins[i]);
                        }

                        if (!isEnabled) {
                            if (list.indexOf(pName) === -1) {
                                list.push(pName);
                            }
                        } else {
                            var idx = list.indexOf(pName);
                            if (idx !== -1) {
                                list.splice(idx, 1);
                            }
                        }
                        ConfigBridge.enabledPlugins = list;
                    }
                }
            }
        }
    }
}
