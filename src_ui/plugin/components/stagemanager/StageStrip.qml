import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import EdgeGesture.StageManager 1.0 // Our C++ plugin

Item {
    id: root
    width: 200

    signal windowSelected(var hwnd)

    // Background for the strip
    Rectangle {
        anchors.fill: parent
        color: "#2D2D2D" // Dark background
        opacity: 0.9
    }

    ListView {
        id: stageList
        anchors.fill: parent
        anchors.margins: 10
        spacing: 15

        // Connect to our C++ model
        model: LiveWindowManager

        delegate: Item {
            id: windowDelegate
            width: ListView.view.width
            height: width * 0.65 // Aspect ratio approximation, ideally dynamic

            // Background for the thumbnail
            Rectangle {
                anchors.fill: parent
                color: "#404040"
                radius: 8
                border.color: mouseArea.containsMouse ? "#0078D4" : "transparent"
                border.width: 2
            }

            // Static Thumbnail
            Image {
                id: thumbnail
                anchors.fill: parent
                anchors.margins: 4
                source: "image://windowThumbnails/" + model.hwnd
                visible: true
                asynchronous: true
                cache: false // Refresh on every load if needed, or rely on provider
                fillMode: Image.PreserveAspectFit
            }

            // Icon overlay (top-left)
            Image {
                width: 24
                height: 24
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: -8 // Overhang slightly
                source: model.iconSource // "image://windowIcons/12345"
                smooth: true
                mipmap: true
            }

            // Title overlay (bottom)
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 24
                color: "#AA000000"
                radius: 4

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 8
                    text: model.title
                    color: "white"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            // Interaction
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
