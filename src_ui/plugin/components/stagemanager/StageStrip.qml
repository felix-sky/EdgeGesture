import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import EdgeGesture.StageManager 1.0

Item {
    id: root
    width: 260

    signal windowSelected(var hwnd)
    signal newContainerRequested()

    Rectangle {
        anchors.fill: parent
        color: "#252525"
        opacity: 0.95
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        // Header / New Container Action
        Rectangle {
            Layout.fillWidth: true
            height: 38
            radius: 8
            color: newContMouse.containsMouse ? "#40FFFFFF" : "#20FFFFFF"
            border.color: "#30FFFFFF"
            border.width: 1

            RowLayout {
                anchors.centerIn: parent
                spacing: 8

                Text {
                    text: "＋"
                    color: "white"
                    font.pixelSize: 16
                    font.bold: true
                }

                Text {
                    text: "New Stage Container"
                    color: "white"
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }
            }

            MouseArea {
                id: newContMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    root.newContainerRequested();
                }
            }
        }

        // Window List
        ListView {
            id: stageList
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12
            clip: true

            model: LiveWindowManager

            delegate: Item {
                id: windowDelegate
                width: ListView.view.width
                height: width * 0.62

                Rectangle {
                    anchors.fill: parent
                    color: "#383838"
                    radius: 8
                    border.color: mouseArea.containsMouse ? "#0078D4" : "#20FFFFFF"
                    border.width: mouseArea.containsMouse ? 2 : 1

                    Behavior on border.color {
                        ColorAnimation {
                            duration: 120
                        }
                    }
                }

                // Thumbnail
                Image {
                    id: thumbnail
                    anchors.fill: parent
                    anchors.margins: 4
                    source: "image://windowThumbnails/" + model.hwnd
                    sourceSize.width: 320
                    sourceSize.height: 200
                    asynchronous: true
                    cache: false
                    fillMode: Image.PreserveAspectFit
                }

                // App Icon (top-left)
                Image {
                    width: 22
                    height: 22
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 6
                    source: model.iconSource
                    smooth: true
                    mipmap: true
                }

                // Title overlay (bottom)
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 22
                    color: "#CC181818"
                    bottomLeftRadius: 8
                    bottomRightRadius: 8

                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 12
                        text: model.title
                        color: "white"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        root.windowSelected(model.hwnd);
                    }
                }
            }
        }
    }
}
