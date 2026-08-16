import QtQuick
import QtQuick.Controls

// Individual tab in the capsule switcher bar
// Shows window icon, truncated title, and close/release buttons
Rectangle {
    id: root

    property var controller: null
    property string windowTitle: "Window"
    property string windowIcon: ""
    property bool isActive: false

    signal tabClicked
    signal releaseClicked
    signal closeClicked

    width: tabRow.width + 16
    height: 28
    radius: 6
    color: isActive ? "#3D3D3D" : (tabMouse.containsMouse ? "#333333" : "#252525")
    border.color: isActive ? "#0078D4" : "transparent"
    border.width: 1

    Row {
        id: tabRow
        anchors.centerIn: parent
        spacing: 6

        // Window Icon
        Image {
            width: 14
            height: 14
            source: root.windowIcon
            visible: root.windowIcon !== ""
            anchors.verticalCenter: parent.verticalCenter
            smooth: true
            mipmap: true
        }

        // Window Title
        Text {
            text: root.windowTitle
            color: root.isActive ? "#FFFFFF" : "#B0B0B0"
            font.pixelSize: 11
            elide: Text.ElideRight
            width: Math.min(implicitWidth, 120)
            anchors.verticalCenter: parent.verticalCenter
        }

        // Release button (restore window)
        Rectangle {
            id: releaseBtn
            width: 18
            height: 18
            radius: 4
            color: releaseMouse.containsMouse ? "#4A4A4A" : "transparent"
            anchors.verticalCenter: parent.verticalCenter

            Text {
                anchors.centerIn: parent
                text: "↗"  // Arrow indicating "pop out"
                color: releaseMouse.containsMouse ? "#FFFFFF" : "#808080"
                font.pixelSize: 10
            }

            MouseArea {
                id: releaseMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.releaseClicked()

                ToolTip.visible: containsMouse
                ToolTip.text: "Release window"
                ToolTip.delay: 500
            }
        }

        // Close button (kill window)
        Rectangle {
            id: closeBtn
            width: 18
            height: 18
            radius: 4
            color: closeMouse.containsMouse ? "#C42B1C" : "transparent"
            anchors.verticalCenter: parent.verticalCenter

            Text {
                anchors.centerIn: parent
                text: "✕"
                color: closeMouse.containsMouse ? "#FFFFFF" : "#808080"
                font.pixelSize: 9
            }

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.closeClicked()

                ToolTip.visible: containsMouse
                ToolTip.text: "Close window"
                ToolTip.delay: 500
            }
        }
    }

    // Tab click (for switching)
    MouseArea {
        id: tabMouse
        anchors.fill: parent
        hoverEnabled: true
        // Don't eat clicks from child buttons
        z: -1
        onClicked: root.tabClicked()
    }
}
