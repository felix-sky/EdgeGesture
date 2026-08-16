import QtQuick
import QtQuick.Window
import QtQuick.Controls
import EdgeGesture.StageManager 1.0

Window {
    id: stageLayer
    visible: true
    title: "EdgeGesture Stage Layer"
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    width: Screen.width
    height: Screen.height
    x: 0
    y: 0

    // Enable click-through on transparent areas using WinAPI
    Component.onCompleted: {
        // Note: For QML Window, we need to access the native handle via C++ helper
        // or use a Timer to delay until window is fully created
        var hwnd = stageLayer.__nativeHandle ? stageLayer.__nativeHandle : 0;
        console.log("StageLayer HWND:", hwnd);
        // The click-through will be applied by C++ after window creation
    }

    // Background: REMOVED to allow click-through to desktop
    // The previous faint Rectangle was blocking all mouse events

    // Capsule Manager Integration
    // We use a Repeater to create AppCapsules for each managed window

    Repeater {
        model: CapsuleManager // The Singleton C++ Model

        delegate: AppCapsule {
            // "model" here refers to the data provided by CapsuleManager for this row
            // We defined roles: hwnd, title, controller

            // Set initial position (random or cascaded for now, ideally persistent)
            x: 100 + index * 40
            y: 100 + index * 40

            windowHandle: model.hwnd
            windowTitle: model.title

            // Icon Provider logic if needed: "image://iconProvider/" + model.hwnd
            windowIcon: "image://windowIcons/" + model.hwnd

            // Link Controller
            Component.onCompleted: {
                if (model.controller) {
                    model.controller.capsuleItem = this;
                }
            }

            onRequestClose: {
                if (model.controller)
                    model.controller.requestClose();
            }
            onRequestFocus: {
                if (model.controller)
                    model.controller.requestFocus();
            }
            onRequestMove: (x, y) => {
            // The Controller listens to xChanged/yChanged internally!
            // But we can explicitly call requestMove if we want logic separation
            // Currently internal binding in WindowController handles it.
            }
        }
    }

    // Debug helper
    Shortcut {
        sequence: "Ctrl+Alt+Q"
        onActivated: stageLayer.close()
    }
}
