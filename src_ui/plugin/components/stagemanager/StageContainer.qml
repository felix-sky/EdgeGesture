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

            // Touch Drag Handler
            DragHandler {
                id: touchDragHandler
                target: null
                acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus
                property point initialPos: Qt.point(0, 0)
                onActiveChanged: {
                    if (active) {
                        initialPos = Qt.point(root.x, root.y);
                        if (root.containerController) {
                            StageManagerService.setActiveDestination(root.containerController.containerId);
                        }
                    }
                }
                onTranslationChanged: {
                    if (active) {
                        root.x = initialPos.x + translation.x;
                        root.y = initialPos.y + translation.y;
                    }
                }
            }

            // Mouse Drag Area
            MouseArea {
                id: dragMouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton
                onPressed: function (mouse) {
                    if (root.containerController) {
                        StageManagerService.setActiveDestination(root.containerController.containerId);
                    }
                    if (mouse.source === Qt.MouseEventNotSynthesized) {
                        root.startSystemMove();
                    }
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

        // Resize Handles (Touch via DragHandler, Mouse via startSystemResize)
        Item {
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: 20
            height: 20
            z: 10

            DragHandler {
                target: null
                acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus
                property size initialSize: Qt.size(0, 0)
                onActiveChanged: {
                    if (active)
                        initialSize = Qt.size(root.width, root.height);
                }
                onTranslationChanged: {
                    if (active) {
                        root.width = Math.max(root.minimumWidth, initialSize.width + translation.x);
                        root.height = Math.max(root.minimumHeight, initialSize.height + translation.y);
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeFDiagCursor
                onPressed: function (mouse) {
                    if (mouse.source === Qt.MouseEventNotSynthesized) {
                        root.startSystemResize(Qt.BottomEdge | Qt.RightEdge);
                    }
                }
            }
        }

        Item {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: 20
            height: 20
            z: 10

            DragHandler {
                target: null
                acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus
                property point initialPos: Qt.point(0, 0)
                property size initialSize: Qt.size(0, 0)
                onActiveChanged: {
                    if (active) {
                        initialPos = Qt.point(root.x, root.y);
                        initialSize = Qt.size(root.width, root.height);
                    }
                }
                onTranslationChanged: {
                    if (active) {
                        var newW = Math.max(root.minimumWidth, initialSize.width - translation.x);
                        var deltaW = newW - initialSize.width;
                        root.x = initialPos.x - deltaW;
                        root.width = newW;
                        root.height = Math.max(root.minimumHeight, initialSize.height + translation.y);
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeBDiagCursor
                onPressed: function (mouse) {
                    if (mouse.source === Qt.MouseEventNotSynthesized) {
                        root.startSystemResize(Qt.BottomEdge | Qt.LeftEdge);
                    }
                }
            }
        }

        Item {
            anchors.right: parent.right
            anchors.top: headerBar.bottom
            anchors.bottom: pageStrip.top
            width: 12
            z: 10

            DragHandler {
                target: null
                acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus
                property real initialW: 0
                onActiveChanged: {
                    if (active)
                        initialW = root.width;
                }
                onTranslationChanged: {
                    if (active) {
                        root.width = Math.max(root.minimumWidth, initialW + translation.x);
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeHorCursor
                onPressed: function (mouse) {
                    if (mouse.source === Qt.MouseEventNotSynthesized) {
                        root.startSystemResize(Qt.RightEdge);
                    }
                }
            }
        }

        Item {
            anchors.left: parent.left
            anchors.top: headerBar.bottom
            anchors.bottom: pageStrip.top
            width: 12
            z: 10

            DragHandler {
                target: null
                acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus
                property point initialPos: Qt.point(0, 0)
                property real initialW: 0
                onActiveChanged: {
                    if (active) {
                        initialPos = Qt.point(root.x, root.y);
                        initialW = root.width;
                    }
                }
                onTranslationChanged: {
                    if (active) {
                        var newW = Math.max(root.minimumWidth, initialW - translation.x);
                        var deltaW = newW - initialW;
                        root.x = initialPos.x - deltaW;
                        root.width = newW;
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeHorCursor
                onPressed: function (mouse) {
                    if (mouse.source === Qt.MouseEventNotSynthesized) {
                        root.startSystemResize(Qt.LeftEdge);
                    }
                }
            }
        }

        Item {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            height: 10
            z: 10

            DragHandler {
                target: null
                acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus
                property real initialH: 0
                onActiveChanged: {
                    if (active)
                        initialH = root.height;
                }
                onTranslationChanged: {
                    if (active) {
                        root.height = Math.max(root.minimumHeight, initialH + translation.y);
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeVerCursor
                onPressed: function (mouse) {
                    if (mouse.source === Qt.MouseEventNotSynthesized) {
                        root.startSystemResize(Qt.BottomEdge);
                    }
                }
            }
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
