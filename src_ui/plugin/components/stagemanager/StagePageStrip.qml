import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import EdgeGesture.StageManager 1.0

Item {
    id: root
    height: 56

    required property var containerController

    Rectangle {
        id: bgPill
        anchors.fill: parent
        anchors.margins: 6
        radius: height / 2
        color: "#E0202020"
        border.color: "#35FFFFFF"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 10

            // 1. Page Tabs
            Row {
                id: pageRow
                Layout.alignment: Qt.AlignVCenter
                spacing: 8

                Repeater {
                    model: root.containerController ? root.containerController.pagesModel : []

                    delegate: Rectangle {
                        id: pageTab
                        width: 36
                        height: 36
                        radius: 10
                        color: modelData.isActive ? "#45FFFFFF" : (tabMouse.containsMouse ? "#20FFFFFF" : "transparent")
                        border.color: modelData.isActive ? "#60FFFFFF" : "transparent"
                        border.width: 1

                        Image {
                            anchors.centerIn: parent
                            width: 24
                            height: 24
                            source: modelData.iconSource
                            smooth: true
                            mipmap: true
                        }

                        MouseArea {
                            id: tabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (root.containerController) {
                                    root.containerController.activatePage(modelData.index);
                                }
                            }
                        }

                        ToolTip.visible: tabMouse.containsMouse
                        ToolTip.text: modelData.title
                        ToolTip.delay: 400
                    }
                }
            }

            // Spacer
            Item {
                Layout.fillWidth: true
            }

            // Divider
            Rectangle {
                width: 1
                height: 22
                color: "#30FFFFFF"
                Layout.alignment: Qt.AlignVCenter
            }

            // 2. Actions
            Row {
                Layout.alignment: Qt.AlignVCenter
                spacing: 6

                // Release Current Page (Detach back to desktop)
                Rectangle {
                    width: 32
                    height: 32
                    radius: 8
                    color: relMouse.containsMouse ? "#30FFFFFF" : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "⮡"
                        color: "white"
                        font.pixelSize: 16
                    }

                    MouseArea {
                        id: relMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (root.containerController) {
                                root.containerController.releasePage(root.containerController.activeIndex);
                            }
                        }
                    }
                    ToolTip.visible: relMouse.containsMouse
                    ToolTip.text: "Release Page to Desktop"
                    ToolTip.delay: 400
                }

                // Pin / Unpin Container
                Rectangle {
                    width: 32
                    height: 32
                    radius: 8
                    color: (root.containerController && root.containerController.isPinned) ? "#45FFFFFF" : (pinMouse.containsMouse ? "#30FFFFFF" : "transparent")

                    Text {
                        anchors.centerIn: parent
                        text: (root.containerController && root.containerController.isPinned) ? "📌" : "📍"
                        color: "white"
                        font.pixelSize: 14
                    }

                    MouseArea {
                        id: pinMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (root.containerController) {
                                root.containerController.setPinned(!root.containerController.isPinned);
                            }
                        }
                    }
                    ToolTip.visible: pinMouse.containsMouse
                    ToolTip.text: (root.containerController && root.containerController.isPinned) ? "Unpin Container" : "Pin on Top"
                    ToolTip.delay: 400
                }

                // Minimize Container
                Rectangle {
                    width: 32
                    height: 32
                    radius: 8
                    color: minMouse.containsMouse ? "#30FFFFFF" : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "─"
                        color: "white"
                        font.pixelSize: 14
                    }

                    MouseArea {
                        id: minMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (root.containerController) {
                                root.containerController.minimizeContainer();
                            }
                        }
                    }
                    ToolTip.visible: minMouse.containsMouse
                    ToolTip.text: "Minimize Container"
                    ToolTip.delay: 400
                }

                // Close Current Application
                Rectangle {
                    width: 32
                    height: 32
                    radius: 8
                    color: closeAppMouse.containsMouse ? "#80E81123" : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        color: "white"
                        font.pixelSize: 13
                    }

                    MouseArea {
                        id: closeAppMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (root.containerController) {
                                root.containerController.closePage(root.containerController.activeIndex);
                            }
                        }
                    }
                    ToolTip.visible: closeAppMouse.containsMouse
                    ToolTip.text: "Close Active App"
                    ToolTip.delay: 400
                }

                // Close Entire Container
                Rectangle {
                    width: 32
                    height: 32
                    radius: 8
                    color: closeContMouse.containsMouse ? "#80E81123" : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "⏹"
                        color: "white"
                        font.pixelSize: 12
                    }

                    MouseArea {
                        id: closeContMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (root.containerController) {
                                root.containerController.requestContainerClose();
                            }
                        }
                    }
                    ToolTip.visible: closeContMouse.containsMouse
                    ToolTip.text: "Close Container"
                    ToolTip.delay: 400
                }
            }
        }
    }
}
