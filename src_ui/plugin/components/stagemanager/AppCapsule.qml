import QtQuick
import QtQuick.Window
import QtQuick.Controls

Window {
    id: root

    // --- Injections ---
    required property var group
    property int stackIndex: 0
    property var controller: null

    // Helper: Am I the leader? (The front-most window)
    property bool isFront: stackIndex === group.activeIndex

    property var windowIcon: controller ? "image://windowIcons/" + controller.hwnd : ""
    property alias contentArea: contentItem

    // --- Window Flags ---
    visible: true
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    // --- Geometry (Bound to Group) ---
    // If we are dragging (mouse pressed), we break the binding temporarily?
    // Actually, if updateGroupPosition updates group.x, binding loop might occur.
    // But since we drive group.x from root.x, we should be careful.

    // Binding: Only strictly bind if NOT dragging this specific window?
    // "startSystemMove" takes control.

    property bool isDragging: false

    x: group.x
    y: group.y
    width: 800
    height: 600

    // Sync Group to Us (Leader Mode)
    onXChanged: if (isDragging && isFront)
        group.x = x
    onYChanged: if (isDragging && isFront)
        group.y = y

    // --- Lifecycle ---
    signal requestClose
    signal requestGlobalDetach
    signal requestGlobalExplode

    Component.onCompleted: {
        if (controller) {
            controller.capsuleItem = contentItem;
            controller.requestFocus();

            // Fix "Black Window": Verify geometry immediately
            controller.syncGeometry();
            // DOUBLE TAP: Sometimes the first window needs a beat to realize it's reparented
            syncTimer.start();
        }
    }

    Timer {
        id: syncTimer
        interval: 100
        repeat: false
        onTriggered: if (root.controller)
            root.controller.syncGeometry()
    }

    Connections {
        target: controller
        function onClosed() {
            root.close();
        }
    }

    // UI Layer
    Rectangle {
        id: bgRec
        anchors.fill: parent
        radius: 12
        color: "#1E1E1E" // Opaque Frame

        // 1. CONTENT AREA
        Item {
            id: contentItem
            anchors.fill: parent
            anchors.bottomMargin: switcherBar.visible ? 70 : 10 // Make room for dock
            anchors.topMargin: 20

            // Explorer Fix
            Rectangle {
                anchors.fill: parent
                color: "black"
                radius: 8
                z: -1
            }
        }

        // 2. DRAG PILL (System Move)
        Rectangle {
            id: dragPill
            visible: root.isFront
            width: 80
            height: 6
            radius: 3
            color: dragArea.containsMouse ? "white" : "#80FFFFFF"
            anchors.top: parent.top
            anchors.topMargin: 8
            anchors.horizontalCenter: parent.horizontalCenter

            Rectangle {
                width: 30
                height: 2
                radius: 1
                color: "black"
                opacity: 0.3
                anchors.centerIn: parent
            }

            MouseArea {
                id: dragArea
                anchors.fill: parent
                anchors.margins: -10
                hoverEnabled: true

                onPressed: {
                    group.bringToFront(root);
                    root.isDragging = true;
                    root.startSystemMove();
                }
                onReleased: {
                    root.isDragging = false;
                }
            }
        }

        // 3. SWITCHER DOCK
        Rectangle {
            id: switcherBar
            visible: root.isFront && group.capsuleModel.count > 0
            height: 50
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            anchors.horizontalCenter: parent.horizontalCenter
            width: switcherRow.width + 40
            radius: 25
            color: "#D0202020"
            border.color: "#30FFFFFF"
            border.width: 1

            Row {
                id: switcherRow
                anchors.centerIn: parent
                spacing: 12

                // A. App Icons
                Repeater {
                    model: group.capsuleModel
                    delegate: Item {
                        width: 36
                        height: 36
                        // Access the object from the model
                        property var capsuleRef: model.capsuleItem

                        Rectangle {
                            anchors.fill: parent
                            radius: 10
                            color: (capsuleRef === root) ? "#40FFFFFF" : "transparent"
                            border.color: (capsuleRef === root) ? "white" : "transparent"
                            border.width: 1
                        }

                        Image {
                            anchors.centerIn: parent
                            width: 24
                            height: 24
                            source: capsuleRef ? capsuleRef.windowIcon : ""
                            smooth: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: parent.scale = 1.1
                            onExited: parent.scale = 1.0
                            onClicked: group.bringToFront(capsuleRef)
                        }
                        Behavior on scale {
                            NumberAnimation {
                                duration: 100
                            }
                        }
                    }
                }

                // Divider
                Rectangle {
                    width: 1
                    height: 20
                    color: "#50FFFFFF"
                    anchors.verticalCenter: parent.verticalCenter
                    visible: group.capsuleModel.count > 0
                }

                // B. Window Controls
                // 1. Detach This Window
                Item {
                    width: 36
                    height: 36
                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: detachArea.containsMouse ? "#30FFFFFF" : "transparent"
                    }
                    Text {
                        text: "⮡"
                        color: "white"
                        anchors.centerIn: parent
                        font.pixelSize: 18
                    } // Standard symbol if no Fluent
                    MouseArea {
                        id: detachArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: root.requestGlobalDetach() // Signal Manager to separate us
                    }
                }

                // 2. Explode Fleet (Release All)
                Item {
                    width: 36
                    height: 36
                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: explodeArea.containsMouse ? "#30FFFFFF" : "transparent"
                    }
                    Text {
                        text: "💥"
                        color: "white"
                        anchors.centerIn: parent
                        font.pixelSize: 16
                    }
                    MouseArea {
                        id: explodeArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: root.requestGlobalExplode()
                    }
                }

                // 3. Minimize Fleet
                Item {
                    width: 36
                    height: 36
                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: minArea.containsMouse ? "#30FFFFFF" : "transparent"
                    }
                    Text {
                        text: "─"
                        color: "white"
                        anchors.centerIn: parent
                        font.pixelSize: 18
                    }
                    MouseArea {
                        id: minArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: group.minimizeAll()
                    }
                }
            }
        }

        // Resize
        MouseArea {
            width: 20
            height: 20
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            cursorShape: Qt.SizeFDiagCursor
            visible: root.isFront
            onPressed: root.startSystemResize(Qt.BottomEdge | Qt.RightEdge)
        }
    }
}
