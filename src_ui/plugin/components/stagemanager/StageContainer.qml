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
                color: headerHover.hovered ? "#B0FFFFFF" : "#60FFFFFF"

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

            // Hover state for header
            HoverHandler {
                id: headerHover
                acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus
            }

            // Click / Tap to activate destination container
            TapHandler {
                onTapped: {
                    if (root.containerController) {
                        StageManagerService.setActiveDestination(root.containerController.containerId);
                    }
                }
            }

            // Touch / Stylus Drag Handler (directly moves root.x / root.y)
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

            // Mouse Drag Handler (triggers startSystemMove)
            DragHandler {
                id: mouseDragHandler
                target: null
                acceptedDevices: PointerDevice.Mouse
                onActiveChanged: {
                    if (active) {
                        if (root.containerController) {
                            StageManagerService.setActiveDestination(root.containerController.containerId);
                        }
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
        // 1. Bottom-Right Corner
        Item {
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: 20
            height: 20
            z: 10

            HoverHandler {
                cursorShape: Qt.SizeFDiagCursor
                acceptedDevices: PointerDevice.Mouse
            }

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

            DragHandler {
                target: null
                acceptedDevices: PointerDevice.Mouse
                onActiveChanged: {
                    if (active)
                        root.startSystemResize(Qt.BottomEdge | Qt.RightEdge);
                }
            }
        }

        // 2. Bottom-Left Corner
        Item {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: 20
            height: 20
            z: 10

            HoverHandler {
                cursorShape: Qt.SizeBDiagCursor
                acceptedDevices: PointerDevice.Mouse
            }

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

            DragHandler {
                target: null
                acceptedDevices: PointerDevice.Mouse
                onActiveChanged: {
                    if (active)
                        root.startSystemResize(Qt.BottomEdge | Qt.LeftEdge);
                }
            }
        }

        // 3. Right Edge
        Item {
            anchors.right: parent.right
            anchors.top: headerBar.bottom
            anchors.bottom: pageStrip.top
            width: 12
            z: 10

            HoverHandler {
                cursorShape: Qt.SizeHorCursor
                acceptedDevices: PointerDevice.Mouse
            }

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

            DragHandler {
                target: null
                acceptedDevices: PointerDevice.Mouse
                onActiveChanged: {
                    if (active)
                        root.startSystemResize(Qt.RightEdge);
                }
            }
        }

        // 4. Left Edge
        Item {
            anchors.left: parent.left
            anchors.top: headerBar.bottom
            anchors.bottom: pageStrip.top
            width: 12
            z: 10

            HoverHandler {
                cursorShape: Qt.SizeHorCursor
                acceptedDevices: PointerDevice.Mouse
            }

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

            DragHandler {
                target: null
                acceptedDevices: PointerDevice.Mouse
                onActiveChanged: {
                    if (active)
                        root.startSystemResize(Qt.LeftEdge);
                }
            }
        }

        // 5. Bottom Edge
        Item {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            height: 10
            z: 10

            HoverHandler {
                cursorShape: Qt.SizeVerCursor
                acceptedDevices: PointerDevice.Mouse
            }

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

            DragHandler {
                target: null
                acceptedDevices: PointerDevice.Mouse
                onActiveChanged: {
                    if (active)
                        root.startSystemResize(Qt.BottomEdge);
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
