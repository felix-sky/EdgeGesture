import QtQuick
import QtQuick.Window
import QtQuick.Controls
import EdgeGesture.StageManager 1.0

Window {
    id: root
    width: 800
    height: 600
    title: "Stage Manager"

    // 1. Transparent background for rounded corners
    color: "transparent"

    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    visible: false

    property var currentHwnd: 0
    property var activeHwnd: 0

    onCurrentHwndChanged: {
        if (currentHwnd === activeHwnd)
            return;
        if (activeHwnd !== 0) {
            SafeWindowReparenter.restoreWindow(activeHwnd);
            activeHwnd = 0;
        }

        if (currentHwnd !== 0) {
            root.visible = true;
            root.requestActivate();

            if (SafeWindowReparenter.reparentWindow(currentHwnd, stageContainer)) {
                activeHwnd = currentHwnd;
            } else {
                console.warn("Failed to reparent window: " + currentHwnd);
                currentHwnd = 0;
                root.visible = false;
            }
        } else {
            root.visible = false;
        }
    }

    onClosing: {
        if (activeHwnd !== 0) {
            SafeWindowReparenter.restoreWindow(activeHwnd);
            activeHwnd = 0;
            currentHwnd = 0;
        }
    }

    Component.onDestruction: {
        if (activeHwnd !== 0) {
            SafeWindowReparenter.restoreWindow(activeHwnd);
        }
    }

    // 2. Visual Background Container (Rounded)
    Rectangle {
        id: windowBackground
        anchors.fill: parent
        radius: 12
        color: "#202020"
        clip: true

        // Header Bar
        Rectangle {
            id: header
            height: 32
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            color: "#2D2D2D"
            radius: 12

            // Cover bottom half of radius
            Rectangle {
                anchors.bottom: parent.bottom
                height: 10
                width: parent.width
                color: parent.color
            }

            Text {
                anchors.centerIn: parent
                text: root.title
                color: "#808080"
                font.pixelSize: 12
            }

            // Drag Handler
            MouseArea {
                anchors.fill: parent
                onPressed: root.startSystemMove()
            }

            // Close Button (Custom)
            Rectangle {
                id: closeBtn
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 48
                radius: 12
                color: closeBtnMouse.containsMouse ? "#C42B1C" : "transparent"

                // Cover left-bottom corner
                Rectangle {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    width: 24
                    height: 16
                    color: parent.color
                }

                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    color: closeBtnMouse.containsMouse ? "white" : "#A0A0A0"
                    font.pixelSize: 12
                }

                MouseArea {
                    id: closeBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.currentHwnd = 0
                }
            }
        }

        // Container for reparented window
        Item {
            id: stageContainer
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            onXChanged: updatePos()
            onYChanged: updatePos()
            onWidthChanged: updatePos()
            onHeightChanged: updatePos()

            function updatePos() {
                if (root.activeHwnd !== 0) {
                    SafeWindowReparenter.updateWindowPosition(root.activeHwnd, stageContainer);
                }
            }

            // Resize Handler (Bottom Right)
            MouseArea {
                width: 20
                height: 20
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                cursorShape: Qt.SizeFDiagCursor
                z: 999
                onPressed: root.startSystemResize(Qt.BottomEdge | Qt.RightEdge)
                Rectangle {
                    anchors.fill: parent
                    color: "#50FFFFFF"
                    radius: 6
                }
            }
        }

        // 3. Corner Masking Border (Covers HWND sharp corners)
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            radius: 12
            border.width: 3
            border.color: "#404040"
            z: 9999
        }
    }
}
