import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    width: 100
    height: 32
    radius: 16
    color: "#D0202020" // Semi-transparent dark
    border.color: "#30FFFFFF"
    border.width: 1

    signal closeClicked
    signal minimizeClicked
    signal maximizeClicked

    Row {
        anchors.centerIn: parent
        spacing: 8

        // Minimize
        MouseArea {
            width: 24
            height: 24
            hoverEnabled: true
            onClicked: root.minimizeClicked()

            Rectangle {
                anchors.centerIn: parent
                width: 12
                height: 2
                color: parent.containsMouse ? "white" : "#A0A0A0"
            }
        }

        // Maximize
        MouseArea {
            width: 24
            height: 24
            hoverEnabled: true
            onClicked: root.maximizeClicked()

            Rectangle {
                anchors.fill: parent
                anchors.margins: 6
                color: "transparent"
                border.width: 2
                border.color: parent.containsMouse ? "white" : "#A0A0A0"
            }
        }

        // Close
        MouseArea {
            width: 24
            height: 24
            hoverEnabled: true
            onClicked: root.closeClicked()

            Text {
                anchors.centerIn: parent
                text: "✕"
                color: parent.containsMouse ? "#FF4444" : "#A0A0A0"
                font.bold: true
            }
        }
    }
}
