import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import FluentUI 1.0

FluScrollablePage {
    title: "Handle Settings"
    leftPadding: 20
    rightPadding: 20

    property string currentSide: "Left"

    ColumnLayout {
        spacing: 20
        width: parent.width

        RowLayout {
            spacing: 10
            FluText {
                text: "Select Side:"
                font: FluTextStyle.BodyStrong
            }
            FluComboBox {
                model: ["Left", "Right"]
                currentIndex: 0
                onCurrentTextChanged: currentSide = currentText
            }
        }

        FluText {
            text: "Appearance & Position"
            font: FluTextStyle.Subtitle
        }

        FluFrame {
            id: settingsFrame
            Layout.fillWidth: true
            padding: 15

            property bool isLeft: currentSide === "Left"

            ColumnLayout {
                width: parent.width
                spacing: 15

                // Width
                RowLayout {
                    Layout.fillWidth: true
                    FluText {
                        text: "Width"
                        Layout.preferredWidth: 100
                    }
                    FluSlider {
                        Layout.fillWidth: true
                        from: 10
                        to: 200
                        value: validValue(settingsFrame.isLeft ? ConfigBridge.leftHandle.width : ConfigBridge.rightHandle.width)
                        onMoved: {
                            if (settingsFrame.isLeft)
                                ConfigBridge.leftHandle.width = value;
                            else
                                ConfigBridge.rightHandle.width = value;
                        }
                        onPressedChanged: {
                            ConfigBridge.setPreviewHandle(pressed, settingsFrame.isLeft);
                            if (!pressed)
                                ConfigBridge.applySettings();
                        }
                        // Helper to break binding loop issues if any, though standard binding usually ignores own updates
                        function validValue(v) {
                            return v;
                        }
                    }
                    FluText {
                        text: (settingsFrame.isLeft ? ConfigBridge.leftHandle.width : ConfigBridge.rightHandle.width) + " px"
                        Layout.preferredWidth: 50
                    }
                }

                // Size (Height)
                RowLayout {
                    Layout.fillWidth: true
                    FluText {
                        text: "Size"
                        Layout.preferredWidth: 100
                    }
                    FluSlider {
                        Layout.fillWidth: true
                        from: 10
                        to: 100
                        value: settingsFrame.isLeft ? ConfigBridge.leftHandle.size : ConfigBridge.rightHandle.size
                        onMoved: {
                            if (settingsFrame.isLeft)
                                ConfigBridge.leftHandle.size = value;
                            else
                                ConfigBridge.rightHandle.size = value;
                        }
                        onPressedChanged: {
                            ConfigBridge.setPreviewHandle(pressed, settingsFrame.isLeft);
                            if (!pressed)
                                ConfigBridge.applySettings();
                        }
                    }
                    FluText {
                        text: (settingsFrame.isLeft ? ConfigBridge.leftHandle.size : ConfigBridge.rightHandle.size) + " %"
                        Layout.preferredWidth: 50
                    }
                }

                // Position (Vertical Offset)
                RowLayout {
                    Layout.fillWidth: true
                    FluText {
                        text: "Position"
                        Layout.preferredWidth: 100
                    }
                    FluSlider {
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: settingsFrame.isLeft ? ConfigBridge.leftHandle.position : ConfigBridge.rightHandle.position
                        onMoved: {
                            if (settingsFrame.isLeft)
                                ConfigBridge.leftHandle.position = value;
                            else
                                ConfigBridge.rightHandle.position = value;
                        }
                        onPressedChanged: {
                            ConfigBridge.setPreviewHandle(pressed, settingsFrame.isLeft);
                            if (!pressed)
                                ConfigBridge.applySettings();
                        }
                    }
                    FluText {
                        text: (settingsFrame.isLeft ? ConfigBridge.leftHandle.position : ConfigBridge.rightHandle.position) + " %"
                        Layout.preferredWidth: 50
                    }
                }

                // Color
                RowLayout {
                    Layout.fillWidth: true
                    FluText {
                        text: "Color"
                        Layout.preferredWidth: 100
                    }
                    FluColorPicker {
                        Layout.alignment: Qt.AlignLeft
                        current: settingsFrame.isLeft ? ConfigBridge.leftHandle.color : ConfigBridge.rightHandle.color
                        onAccepted: {
                            var hex = current.toString();
                            if (settingsFrame.isLeft)
                                ConfigBridge.leftHandle.color = hex;
                            else
                                ConfigBridge.rightHandle.color = hex;
                            ConfigBridge.applySettings();
                        }
                    }
                }
            }
        }
    }
}
