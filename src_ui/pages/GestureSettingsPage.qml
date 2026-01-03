import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import FluentUI 1.0

FluScrollablePage {
    title: "Gesture Settings"
    leftPadding: 20
    rightPadding: 20

    property string currentSide: "Left"
    property string currentZone: "Top" // Default for multi-zone
    property int splitMode: ConfigBridge.splitMode

    property var actionList: []
    property int reloadCount: 0

    function refreshActionList() {
        var names = ConfigBridge.actionRegistry.getActionNames();
        names.push("None");
        actionList = names;
    }

    Connections {
        target: ConfigBridge
        function onSettingsChanged() {
            refreshActionList();
            splitMode = ConfigBridge.splitMode;
            reloadCount++;
        }
    }

    Connections {
        target: ConfigBridge.actionRegistry
        function onActionsChanged() {
            refreshActionList();
        }
    }

    Component.onCompleted: {
        refreshActionList();
    }

    function getFullKey(gestureSuffix) {
        var side = currentSide.toLowerCase();
        var zone = "";

        if (splitMode > 0) {
            // If Global/None is selected? Logic:
            // If splitMode > 0, we must use zone prefix.
            zone = "_" + currentZone.toLowerCase();
        }

        return side + zone + gestureSuffix;
    }

    // Model for Zone ComboBox
    property var zoneModel: {
        if (splitMode === 0)
            return ["Global"];
        if (splitMode === 1)
            return ["Top", "Bottom"];
        if (splitMode === 2)
            return ["Top", "Middle", "Bottom"];
        return ["Global"];
    }

    // Reset currentZone if invalid for mode?
    onZoneModelChanged: {
        if (zoneModel.indexOf(currentZone) === -1) {
            currentZone = zoneModel[0];
        }
    }

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
                // model: ["Left", "Right"]
                model: ["left"]
                onCurrentTextChanged: currentSide = currentText
            }
        }

        // Zone Selector
        RowLayout {
            spacing: 10
            visible: splitMode > 0
            FluText {
                text: "Select Zone:"
                font: FluTextStyle.BodyStrong
            }
            FluComboBox {
                model: zoneModel
                currentIndex: zoneModel.indexOf(currentZone)
                onCurrentTextChanged: if (currentText !== "")
                    currentZone = currentText
            }
        }

        FluText {
            text: "Short Swipe"
            font: FluTextStyle.Subtitle
        }

        FluFrame {
            Layout.fillWidth: true
            padding: 15

            ColumnLayout {
                width: parent.width
                spacing: 10

                // Straight
                RowLayout {
                    FluIcon {
                        iconSource: FluentIcons.Forward
                    }
                    FluText {
                        text: "Straight"
                        Layout.fillWidth: true
                    }
                    FluComboBox {
                        model: actionList
                        property string gestureKey: getFullKey("_right")
                        // Need to update when key changes
                        property string currentAction: ConfigBridge.actionRegistry.getGesture(gestureKey)
                        // Force update binding? ConfigBridge emits settingsChanged which might not trigger re-eval unless property depends on it.
                        // Actually getGesture is a function. We need to bind it.

                        currentIndex: {
                            // Dummy dependency to force update on settings changed or zone changed
                            var rc = reloadCount;
                            var mode = splitMode;
                            var side = currentSide;
                            var zone = currentZone;
                            return model.indexOf(ConfigBridge.actionRegistry.getGesture(getFullKey("_right")));
                        }

                        onActivated: {
                            ConfigBridge.actionRegistry.setGesture(gestureKey, currentText);
                            ConfigBridge.applySettings();
                        }
                    }
                }

                // Diagonal Up
                RowLayout {
                    FluIcon {
                        iconSource: FluentIcons.Up
                    }
                    FluText {
                        text: "Diagonal Up"
                        Layout.fillWidth: true
                    }
                    FluComboBox {
                        model: actionList
                        property string gestureKey: getFullKey("_diag_up")
                        currentIndex: {
                            var rc = reloadCount;
                            var mode = splitMode;
                            var side = currentSide;
                            var zone = currentZone;
                            return model.indexOf(ConfigBridge.actionRegistry.getGesture(getFullKey("_diag_up")));
                        }
                        onActivated: {
                            ConfigBridge.actionRegistry.setGesture(gestureKey, currentText);
                            ConfigBridge.applySettings();
                        }
                    }
                }

                // Diagonal Down
                RowLayout {
                    FluIcon {
                        iconSource: FluentIcons.Down
                    }
                    FluText {
                        text: "Diagonal Down"
                        Layout.fillWidth: true
                    }
                    FluComboBox {
                        model: actionList
                        property string gestureKey: getFullKey("_diag_down")
                        currentIndex: {
                            var rc = reloadCount;
                            var mode = splitMode;
                            var side = currentSide;
                            var zone = currentZone;
                            return model.indexOf(ConfigBridge.actionRegistry.getGesture(getFullKey("_diag_down")));
                        }
                        onActivated: {
                            ConfigBridge.actionRegistry.setGesture(gestureKey, currentText);
                            ConfigBridge.applySettings();
                        }
                    }
                }
            }
        }

        FluText {
            text: "Long Swipe"
            font: FluTextStyle.Subtitle
        }

        FluFrame {
            Layout.fillWidth: true
            padding: 15
            ColumnLayout {
                width: parent.width
                spacing: 10

                RowLayout {
                    FluIcon {
                        iconSource: FluentIcons.Forward
                    }
                    FluText {
                        text: "Long Straight"
                        Layout.fillWidth: true
                    }
                    FluComboBox {
                        model: actionList
                        property string gestureKey: getFullKey("_long_right")
                        currentIndex: {
                            var rc = reloadCount;
                            var mode = splitMode;
                            var side = currentSide;
                            var zone = currentZone;
                            return model.indexOf(ConfigBridge.actionRegistry.getGesture(getFullKey("_long_right")));
                        }
                        onActivated: {
                            ConfigBridge.actionRegistry.setGesture(gestureKey, currentText);
                            ConfigBridge.applySettings();
                        }
                    }
                }
            }
        }
    }
}
