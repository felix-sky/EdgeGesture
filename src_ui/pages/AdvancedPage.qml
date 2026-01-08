import Qt.labs.platform 1.1
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
            var path = fileDialog.file.toString();
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

    FileDialog {
        id: profileFileDialog
        title: "Select Application for Profile"
        nameFilters: ["Executables (*.exe)", "All Files (*)"]
        onAccepted: {
            var path = profileFileDialog.file.toString();
            if (path.startsWith("file:///")) {
                path = path.substring(8);
            } else if (path.startsWith("file://")) {
                path = path.substring(7);
            }

            var parts = path.split("/");
            var fileName = parts[parts.length - 1];

            if (fileName !== "") {
                ConfigBridge.profileManager.addProfile(fileName);
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
            text: "Per-App Profiles"
            font: FluTextStyle.Subtitle
        }

        FluText {
            text: "Create custom gesture configurations for specific applications. When an app with a profile is in focus, its settings will automatically be applied."
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            opacity: 0.7
        }

        FluFrame {
            Layout.fillWidth: true
            height: 300
            padding: 10

            ColumnLayout {
                id: profileScope
                anchors.fill: parent
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    FluTextBox {
                        id: profileNameField
                        placeholderText: "App Executable (e.g. notepad.exe)"
                        Layout.fillWidth: true
                    }
                    FluButton {
                        text: "Add"
                        onClicked: {
                            if (profileNameField.text !== "") {
                                ConfigBridge.profileManager.addProfile(profileNameField.text);
                                profileNameField.text = "";
                            }
                        }
                    }
                    FluButton {
                        text: "Find"
                        onClicked: {
                            profileFileDialog.open();
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    FluText {
                        text: "Current Profile:"
                        font: FluTextStyle.BodyStrong
                        Layout.alignment: Qt.AlignVCenter
                    }
                    FluText {
                        text: ConfigBridge.profileManager.currentProfile
                        color: FluTheme.primaryColor
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Item {
                        Layout.fillWidth: true
                    } // Spacer

                    FluButton {
                        text: "Refresh"
                        onClicked: {
                            ConfigBridge.profileManager.scanProfiles();
                            profileScope.profileModel = ConfigBridge.profileManager.profiles;
                        }
                    }
                }

                property var profileModel: ConfigBridge.profileManager.profiles

                Connections {
                    target: ConfigBridge.profileManager
                    function onProfilesChanged() {
                        profileScope.profileModel = ConfigBridge.profileManager.profiles;
                    }
                }

                ListView {
                    id: profileList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: profileScope.profileModel

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 40
                        color: ConfigBridge.profileManager.currentProfile === modelData ? (FluTheme.dark ? "#2a4d3e" : "#d4edda") : "transparent"
                        radius: 4

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: if (ConfigBridge.profileManager.currentProfile !== modelData)
                                parent.color = FluTheme.dark ? "#333333" : "#f0f0f0"
                            onExited: parent.color = ConfigBridge.profileManager.currentProfile === modelData ? (FluTheme.dark ? "#2a4d3e" : "#d4edda") : "transparent"
                            onClicked: {
                                ConfigBridge.profileManager.switchToProfile(modelData);
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            FluIcon {
                                iconSource: FluentIcons.Processing
                                visible: ConfigBridge.profileManager.currentProfile === modelData
                            }

                            FluText {
                                text: modelData
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                            }

                            FluIconButton {
                                iconSource: FluentIcons.Delete
                                onClicked: {
                                    ConfigBridge.profileManager.removeProfile(modelData);
                                }
                            }
                        }
                    }

                    // Empty state
                    FluText {
                        anchors.centerIn: parent
                        visible: profileList.count === 0
                        text: "No app-specific profiles configured.\nClick 'Add' or 'Find' to create one."
                        horizontalAlignment: Text.AlignHCenter
                        opacity: 0.5
                    }
                }

                // Switch to Default button
                FluButton {
                    text: "Use Default Profile"
                    Layout.alignment: Qt.AlignRight
                    enabled: ConfigBridge.profileManager.currentProfile !== "default"
                    onClicked: {
                        ConfigBridge.profileManager.switchToDefault();
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
