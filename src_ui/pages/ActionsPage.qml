import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import FluentUI

FluScrollablePage {
    title: "Actions"
    property bool isRecording: false
    property string recordedShortcut: ""

    // Model for the list
    ListModel {
        id: actionModel
    }

    property string tempActionName: ""
    property string tempActionShortcut: ""

    function refreshActions() {
        actionModel.clear();
        var actions = ConfigBridge.actionRegistry.getActions();
        for (var key in actions) {
            actionModel.append({
                "name": key,
                "shortcut": actions[key]
            });
        }
    }

    Component.onCompleted: {
        refreshActions();
    }

    Connections {
        target: ConfigBridge.actionRegistry
        function onActionsChanged() {
            refreshActions();
        }
    }

    Connections {
        target: ConfigBridge
        function onSettingsChanged() {
            refreshActions();
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: 20

        FluText {
            text: "Register custom keyboard shortcuts here."
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        FluButton {
            text: "Add New Action"
            onClicked: addActionDialog.open()
        }

        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: contentHeight
            interactive: false // Allow the page to scroll instead
            model: actionModel
            clip: true
            spacing: 10

            delegate: Rectangle {
                width: parent.width
                height: 50
                color: "transparent"
                radius: 4

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: parent.color = FluTheme.dark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.05)
                    onExited: parent.color = "transparent"
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 20

                    FluText {
                        text: model.name
                        font: FluTextStyle.BodyStrong
                        Layout.preferredWidth: 150
                    }

                    FluText {
                        text: model.shortcut
                        font: FluTextStyle.Body
                        Layout.fillWidth: true
                    }

                    FluIconButton {
                        iconSource: FluentIcons.Delete
                        onClicked: {
                            ConfigBridge.actionRegistry.removeAction(model.name);
                        }
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: FluTheme.dark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.1)
                }
            }
        }
    }

    FluContentDialog {
        id: addActionDialog
        title: "Add Action"
        message: "Press Record to start recording shortcut"
        negativeText: "Cancel"
        positiveText: "Add"

        contentDelegate: Component {
            ColumnLayout {
                spacing: 10
                width: parent.width

                FluTextBox {
                    id: nameInput
                    placeholderText: "Action Name (e.g. Copy)"
                    Layout.fillWidth: true
                    text: tempActionName
                    onTextChanged: tempActionName = text
                }

                FluTextBox {
                    id: shortcutInput
                    placeholderText: "Shortcut (e.g. ctrl+c)"
                    Layout.fillWidth: true
                    text: tempActionShortcut
                    readOnly: true
                }

                FluButton {
                    text: isRecording ? "Stop Recording" : "Record Shortcut"
                    Layout.fillWidth: true
                    onClicked: {
                        isRecording = !isRecording;
                        if (isRecording) {
                            tempActionShortcut = "Press keys...";
                            keyListener.forceActiveFocus();
                        } else {
                            // Stop recording
                        }
                    }
                }

                Item {
                    id: keyListener
                    focus: isRecording
                    Keys.onPressed: event => {
                        if (!isRecording)
                            return;

                        var keys = [];
                        if (event.modifiers & Qt.ControlModifier)
                            keys.push("ctrl");
                        if (event.modifiers & Qt.AltModifier)
                            keys.push("alt");
                        if (event.modifiers & Qt.ShiftModifier)
                            keys.push("shift");
                        if (event.modifiers & Qt.MetaModifier)
                            keys.push("win");

                        var keyName = "";
                        // Handle special keys
                        if (event.key === Qt.Key_Left)
                            keyName = "left";
                        else if (event.key === Qt.Key_Right)
                            keyName = "right";
                        else if (event.key === Qt.Key_Up)
                            keyName = "up";
                        else if (event.key === Qt.Key_Down)
                            keyName = "down";
                        else if (event.key === Qt.Key_Return)
                            keyName = "enter";
                        else if (event.key === Qt.Key_Space)
                            keyName = "space";
                        else if (event.key === Qt.Key_Tab)
                            keyName = "tab";
                        else if (event.key === Qt.Key_Backspace)
                            keyName = "backspace";
                        else if (event.key === Qt.Key_Delete)
                            keyName = "delete";
                        else if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
                            keyName = String.fromCharCode(event.key);
                        } else if (event.key >= Qt.Key_A && event.key <= Qt.Key_Z) {
                            keyName = String.fromCharCode(event.key).toLowerCase();
                        } else if (event.key >= Qt.Key_F1 && event.key <= Qt.Key_F12) {
                            keyName = "f" + (event.key - Qt.Key_F1 + 1);
                        } else if (event.key === Qt.Key_Control || event.key === Qt.Key_Alt || event.key === Qt.Key_Shift || event.key === Qt.Key_Meta) {
                            // Just modifiers, wait
                        } else if (event.text !== "" && event.text.charCodeAt(0) > 31) {
                            // Fallback for punctuation, but ignore control chars
                            keyName = event.text.toLowerCase();
                        }

                        if (keyName !== "") {
                            keys.push(keyName);
                            tempActionShortcut = keys.join("+");
                            isRecording = false; // Stop after capturing valid sequence
                        } else if (keys.length > 0) {
                            // Show modifiers while holding
                            tempActionShortcut = keys.join("+") + "+...";
                        }
                    }
                }
            }
        }

        onPositiveClicked: {
            if (tempActionName !== "" && tempActionShortcut !== "") {
                ConfigBridge.actionRegistry.addAction(tempActionName, tempActionShortcut);
                tempActionName = "";
                tempActionShortcut = "";
            }
        }
    }
}
