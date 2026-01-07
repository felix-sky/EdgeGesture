import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import FluentUI 1.0

FluScrollablePage {
    title: "EdgeGesture"
    leftPadding: 20
    rightPadding: 20


    ColumnLayout {
        spacing: 20
        width: parent.width

        FluText {
            text: "General"
            font: FluTextStyle.Subtitle
        }

        FluFrame {
            Layout.fillWidth: true
            padding: 15

            ColumnLayout {
                width: parent.width
                spacing: 15

                RowLayout {
                    Layout.fillWidth: true
                    FluText {
                        text: "Enable Gesture Engine"
                        font: FluTextStyle.BodyStrong
                        Layout.fillWidth: true
                    }
                    FluToggleSwitch {
                        checked: ConfigBridge.engineEnabled
                        onClicked: ConfigBridge.engineEnabled = checked
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    FluText {
                        text: "Vertical Trigger Range"
                        Layout.preferredWidth: 150
                    }
                    FluSlider {
                        Layout.fillWidth: true
                        from: 0
                        to: 100

                        value: ConfigBridge.physics.verticalRange
                        onMoved: {
                            ConfigBridge.physics.verticalRange = value;
                        }
                        onValueChanged: {
                            ConfigBridge.applySettings();
                        }
                    }
                    FluText {
                        text: ConfigBridge.physics.verticalRange + " px"
                        Layout.preferredWidth: 50
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    FluText {
                        text: "Split Mode"
                        Layout.fillWidth: true
                    }
                    FluComboBox {
                        model: ["None", "2 Zones (Half)", "3 Zones (Thirds)"]
                        currentIndex: ConfigBridge.splitMode
                        onActivated: {
                            ConfigBridge.splitMode = currentIndex;
                            ConfigBridge.applySettings();
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    FluText {
                        text: "Short Swipe Threshold"
                        Layout.preferredWidth: 150
                    }
                    FluSlider {
                        Layout.fillWidth: true
                        from: 10
                        to: 150
                        value: ConfigBridge.physics.shortSwipeThreshold
                        onMoved: {
                            ConfigBridge.physics.shortSwipeThreshold = value;
                        }
                        onValueChanged: {
                            ConfigBridge.applySettings();
                        }
                    }
                    FluText {
                        text: Math.round(ConfigBridge.physics.shortSwipeThreshold) + " px"
                        Layout.preferredWidth: 50
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    FluText {
                        text: "Long Swipe Threshold"
                        Layout.preferredWidth: 150
                    }
                    FluSlider {
                        Layout.fillWidth: true
                        from: 100
                        to: 600
                        value: ConfigBridge.physics.longSwipeThreshold
                        onValueChanged: {
                            if (pressed)
                                ConfigBridge.physics.longSwipeThreshold = value;
                        }
                        onPressedChanged: {
                            if (!pressed)
                                ConfigBridge.applySettings();
                        }
                    }
                    FluText {
                        text: Math.round(ConfigBridge.physics.longSwipeThreshold) + " px"
                        Layout.preferredWidth: 50
                    }
                }

                FluText {
                    text: "Swipe from the edges to navigate back, open task view, or trigger custom actions."
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    color: FluColors.Grey100
                    font: FluTextStyle.Caption
                }
            }
        }

        FluText {
            text: "Handles"
            font: FluTextStyle.Subtitle
        }

        FluFrame {
            Layout.fillWidth: true
            padding: 15

            ColumnLayout {
                width: parent.width
                spacing: 15

                // Left Handle
                RowLayout {
                    Layout.fillWidth: true
                    FluText {
                        text: "Left Handle"
                        Layout.fillWidth: true
                    }
                    FluToggleSwitch {
                        text: "On"
                        checked: ConfigBridge.leftHandle.enabled
                        onClicked: ConfigBridge.leftHandle.enabled = checked
                        // Binding needed in C++ bridge
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: FluTheme.dark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.1)
                }

                // RowLayout {
                //     Layout.fillWidth: true
                //     FluText {
                //         text: "Enable Right Handle"
                //         Layout.fillWidth: true
                //         font: FluTextStyle.BodyStrong

                //     }
                //     FluToggleSwitch {
                //         text: "On"
                //         checked: ConfigBridge.rightHandle.enabled
                //         onClicked: ConfigBridge.rightHandle.enabled = checked
                //         // Binding needed in C++ bridge
                //     }
                // }
            }
        }
        FluText {
            text: "Application"
            font: FluTextStyle.Subtitle
        }

        FluFrame {
            Layout.fillWidth: true
            padding: 15

            ColumnLayout {
                width: parent.width
                spacing: 15

                FluFilledButton {
                    text: "Exit Application"
                    Layout.alignment: Qt.AlignLeft
                    onClicked: Qt.quit()
                }
            }
        }
    }
}
