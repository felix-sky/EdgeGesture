import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
id: root
anchors.fill: parent

signal closeRequested

ScrollView {
    anchors.fill: parent
    clip: true

    ColumnLayout {
        width: parent.width
        spacing: 15

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            spacing: 0

            FluText {
                id: topTitle
                text: "Quick Panel"
                font: FluTextStyle.Title
                Layout.alignment: Qt.AlignVCenter
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
            }

            FluIconButton {
                Layout.alignment: Qt.AlignVCenter
                iconSource: FluentIcons.Settings
                onClicked: {
                    SystemBridge.openSettings("");
                    root.closeRequested();
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            columns: 3
            rowSpacing: 15
            columnSpacing: 15

            QuickTile {
                title: "Calculator"
                iconSource: FluentIcons.Calculator
                onClicked: SystemBridge.launchCalculator()
            }

            QuickTile {
                title: "Snipping Tool"
                iconSource: FluentIcons.Cut
                onClicked: SystemBridge.launchSnippingTool()
            }

            QuickTile {
                title: "Lock Screen"
                iconSource: FluentIcons.Lock
                onClicked: {
                    SystemBridge.lockScreen();
                    root.closeRequested();
                }
            }

            QuickTile {
                title: "Task Manager"
                iconSource: FluentIcons.System
                onClicked: SystemBridge.launchTaskManager()
            }

            QuickTile {
                title: "File Explorer"
                iconSource: FluentIcons.Folder
                onClicked: SystemBridge.launchProcess("explorer.exe")
            }

            QuickTile {
                title: "Display"
                iconSource: FluentIcons.TVMonitor
                onClicked: SystemBridge.openSettings("display")
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            spacing: 10
        }
    }
}

component QuickTile: Rectangle {
    id: tile
    property string title
    property string iconSource
    property color activeColor: FluTheme.primaryColor
    property bool isActive: false
    signal clicked

    // Layout.fillWidth: true
    Layout.preferredWidth: 95
    Layout.preferredHeight: 95
    radius: 8
    color: isActive ? activeColor : (FluTheme.dark ? Qt.rgba(1,1,1,0.08) : Qt.rgba(0,0,0,0.05))

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onClicked: tile.clicked()
        onEntered: { if(!tile.isActive) tile.opacity = 0.8 }
        onExited: { tile.opacity = 1.0 }
    }

    Column {
        anchors.centerIn: parent
        spacing: 8

        FluIcon {
            iconSource: tile.iconSource
            iconSize: 24
            color: tile.isActive ? "#FFF" : (FluTheme.dark ? "#FFF" : "#000")
            anchors.horizontalCenter: parent.horizontalCenter
        }

        FluText {
            text: tile.title
            font: FluTextStyle.Body
            color: tile.isActive ? "#FFF" : (FluTheme.dark ? "#FFF" : "#000")
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            width: tile.width - 20
        }
    }
}

}
