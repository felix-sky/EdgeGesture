import QtQuick.Dialogs
import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import FluentUI 1.0

FluScrollablePage {
    title: "Advanced Settings"
    leftPadding: 20
    rightPadding: 20

    FileDialog {
        id: fileDialog
        title: "Select Application"
        nameFilters: ["Executables (*.exe)", "All Files (*)"]
        onAccepted: {
            // Parse URL to get filename (e.g. file:///C:/Windows/notepad.exe -> notepad.exe)
            var path = fileDialog.selectedFile.toString();
            if (path.startsWith("file:///")) {
                path = path.substring(8);
            } else if (path.startsWith("file://")) {
                path = path.substring(7);
            }

            var parts = path.split("/");
            var fileName = parts[parts.length - 1];

            if (fileName !== "") {
                ConfigBridge.actionRegistry.addBlacklistApp(fileName);
            }
        }
    }

    ColumnLayout {
        spacing: 20
        width: parent.width

        FluFrame {
            Layout.fillWidth: true
            padding: 15

            ColumnLayout {
                width: parent.width
                spacing: 15

                RowLayout {
                    Layout.fillWidth: true
                    FluText {
                        text: "Animation"
                        Layout.fillWidth: true
                    }
                    FluToggleSwitch {
                        checked: true
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    FluText {
                        text: "Start with Windows"
                        Layout.fillWidth: true
                    }
                    FluToggleSwitch {
                        checked: SystemBridge.startWithWindows
                        onClicked: {
                            SystemBridge.startWithWindows = checked;
                        }
                    }
                }
            }
        }

        FluText {
            text: "Blacklist (App Exceptions)"
            font: FluTextStyle.Subtitle
        }

        FluFrame {
            Layout.fillWidth: true
            height: 300
            padding: 10

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    FluTextBox {
                        id: appNameField
                        placeholderText: "App Executable (e.g. notepad.exe)"
                        Layout.fillWidth: true
                    }
                    FluButton {
                        text: "Add"
                        onClicked: {
                            if (appNameField.text !== "") {
                                ConfigBridge.actionRegistry.addBlacklistApp(appNameField.text);
                                appNameField.text = "";
                            }
                        }
                    }
                    FluButton {
                        text: "Find"
                        onClicked: {
                            fileDialog.open();
                        }
                    }
                }

                ListView {
                    id: blacklist
                    Layout.fillWidth: true
                    Layout.preferredHeight: 250 // Force height to ensure visibility
                    clip: true
                    model: ConfigBridge.actionRegistry.blacklist

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 36
                        color: "transparent"

                        // Hover effect
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: parent.color = FluTheme.dark ? "#333333" : "#f0f0f0"
                            onExited: parent.color = "transparent"
                        }

                        RowLayout {
                            anchors.fill: parent
                            spacing: 10

                            FluText {
                                text: modelData
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                leftPadding: 10
                            }

                            FluIconButton {
                                iconSource: FluentIcons.Delete
                                onClicked: {
                                    ConfigBridge.actionRegistry.removeBlacklistApp(index);
                                }
                            }
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: FluTheme.dark ? "#333333" : "#e0e0e0"
                        }
                    }
                }
            }
        }
    }
}
