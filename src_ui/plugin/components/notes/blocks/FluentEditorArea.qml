import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Basic 2.15 as Basic
import FluentUI 1.0

Basic.TextArea {
    id: control

    property color customTextColor: FluTheme.dark ? "#FFFFFF" : "#000000"
    property color customSelectionColor: FluTheme.primaryColor
    property color customBackgroundColor: "transparent"

    color: customTextColor
    selectionColor: Qt.alpha(customSelectionColor, 0.4)
    selectedTextColor: customTextColor

    wrapMode: Text.Wrap
    selectByMouse: true

    // Add small padding for background visibility
    padding: 2
    leftPadding: 4
    rightPadding: 4
    topPadding: 2
    bottomPadding: 2

    background: Rectangle {
        color: control.activeFocus ? control.customBackgroundColor : "transparent"
        radius: 4
    }

    cursorDelegate: Rectangle {
        id: cursor
        width: 1
        color: control.customTextColor
        visible: control.activeFocus

        // Blink animation
        SequentialAnimation on opacity {
            running: control.activeFocus
            loops: Animation.Infinite
            NumberAnimation {
                to: 0
                duration: 500
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                to: 1
                duration: 500
                easing.type: Easing.InOutQuad
            }
        }
    }
}
