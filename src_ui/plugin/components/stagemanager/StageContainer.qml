import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import EdgeGesture.StageManager 1.0

Window {
    id: root

    required property var containerController

    title: containerController ? containerController.title : "Stage Container"
    width: 900
    height: 650
    minimumWidth: 400
    minimumHeight: 300

    flags: Qt.Window | Qt.FramelessWindowHint
    visible: true
    color: "transparent"

    // Container Background
    Rectangle {
        id: bgFrame
        anchors.fill: parent
        radius: 12
        color: "#1E1E1E"
        border.color: "#30FFFFFF"
        border.width: 1

        // Top Header / Drag Area
        Rectangle {
            id: headerBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 32
            color: "transparent"

            // Drag Pill
            Rectangle {
                id: dragPill
                anchors.centerIn: parent
                width: 70
                height: 5
                radius: 2.5
                color: dragMouse.containsMouse ? "#B0FFFFFF" : "#60FFFFFF"

                Behavior on color {
                    ColorAnimation {
                        duration: 150
                    }
                }
            }

            // Window Title
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: root.title
                color: "#90FFFFFF"
                font.pixelSize: 12
                elide: Text.ElideRight
                width: Math.min(implicitWidth, 250)
            }

            MouseArea {
                id: dragMouse
                anchors.fill: parent
                hoverEnabled: true
                onPressed: {
                    if (root.containerController) {
                        StageManagerService.setActiveDestination(root.containerController.containerId);
                    }
                    root.startSystemMove();
                }
            }
        }

        // Native Content Anchor (Area for foreign embedded HWND)
        Item {
            id: nativeContentAnchor
            anchors.top: headerBar.bottom
            anchors.bottom: pageStrip.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 2
            anchors.rightMargin: 2

            // Black background behind native HWND
            Rectangle {
                anchors.fill: parent
                color: "#000000"
                radius: 6
                z: -1
            }
        }

        // Bottom Page Strip
        StagePageStrip {
            id: pageStrip
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottomMargin: 4
            height: 56
            containerController: root.containerController
        }

        // Resize Handles
        MouseArea {
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: 16
            height: 16
            cursorShape: Qt.SizeFDiagCursor
            onPressed: root.startSystemResize(Qt.BottomEdge | Qt.RightEdge)
        }

        MouseArea {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: 16
            height: 16
            cursorShape: Qt.SizeBDiagCursor
            onPressed: root.startSystemResize(Qt.BottomEdge | Qt.LeftEdge)
        }

        MouseArea {
            anchors.right: parent.right
            anchors.top: headerBar.bottom
            anchors.bottom: pageStrip.top
            width: 6
            cursorShape: Qt.SizeHorCursor
            onPressed: root.startSystemResize(Qt.RightEdge)
        }

        MouseArea {
            anchors.left: parent.left
            anchors.top: headerBar.bottom
            anchors.bottom: pageStrip.top
            width: 6
            cursorShape: Qt.SizeHorCursor
            onPressed: root.startSystemResize(Qt.LeftEdge)
        }

        MouseArea {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 6
            cursorShape: Qt.SizeVerCursor
            onPressed: root.startSystemResize(Qt.BottomEdge)
        }
    }

    Component.onCompleted: {
        if (containerController) {
            containerController.bindHostWindow(root, nativeContentAnchor);
        }
        StageManagerService.applyWin11RoundedCorners(root);
    }

    onClosing: function (close) {
        close.accepted = false;
        if (containerController) {
            containerController.requestContainerClose();
        }
    }
}
