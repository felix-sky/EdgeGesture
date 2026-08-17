import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import EdgeGesture.StageManager 1.0
import "./stagemanager"

Item {
    id: root

    // Signals required by PluginContainer
    signal getCloseRequested
    signal requestInputMode(bool active)

    property var containerWindows: ({})
    property int cascadeOffset: 0

    // Component for StageContainer
    Component {
        id: containerComponent
        StageContainer {}
    }

    Connections {
        target: StageManagerService

        function onContainerCreated(containerId, controller) {
            console.log("[StageManager] Container created:", containerId);

            var winWidth = 900;
            var winHeight = 650;
            var startX = Math.max(50, (Screen.width - winWidth) / 2 + (root.cascadeOffset % 200));
            var startY = Math.max(50, (Screen.height - winHeight) / 2 + (root.cascadeOffset % 150));
            root.cascadeOffset += 30;

            var containerWin = containerComponent.createObject(null, {
                "containerController": controller,
                "x": startX,
                "y": startY,
                "width": winWidth,
                "height": winHeight
            });

            if (containerWin) {
                root.containerWindows[containerId] = containerWin;
            } else {
                console.error("[StageManager] Failed to create StageContainer window");
            }
        }

        function onContainerClosed(containerId) {
            console.log("[StageManager] Container closed:", containerId);
            if (root.containerWindows[containerId]) {
                var win = root.containerWindows[containerId];
                delete root.containerWindows[containerId];
                win.destroy();
            }
        }
    }

    // Left Sidebar (Window Strip)
    StageStrip {
        id: stageStrip
        width: 320
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left

        onWindowSelected: function (hwnd) {
            console.log("[StageManager] Selected window:", hwnd);
            StageManagerService.addWindowToActiveContainer(hwnd);
        }

        onNewContainerRequested: function () {
            console.log("[StageManager] New container requested");
            StageManagerService.createContainer();
        }
    }
}
