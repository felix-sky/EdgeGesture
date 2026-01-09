import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: control
    property string label: ""
    property int icon: 0
    property string code: ""
    property var textArea
    property bool isUnderline: false

    width: 30
    height: 30

    FluText {
        visible: control.label !== ""
        text: control.label
        color: "#FFF"
        font.bold: true
        font.pixelSize: 16
        font.underline: control.isUnderline
        anchors.centerIn: parent
    }

    FluIcon {
        visible: control.icon !== 0
        iconSource: control.icon
        color: "#FFF"
        iconSize: 16
        anchors.centerIn: parent
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            if (control.textArea) {
                var start = control.textArea.selectionStart;
                var end = control.textArea.selectionEnd;
                var txt = control.textArea.text;

                if (start !== end) {
                    // Have selection, wrap content
                    control.textArea.remove(start, end);
                    control.textArea.insert(start, control.code + txt.substring(start, end) + control.code);
                } else {
                    // No selection, insert directly
                    control.textArea.insert(control.textArea.cursorPosition, control.code);
                }
                control.textArea.forceActiveFocus();
            }
        }
    }
}
