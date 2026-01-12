import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import FluentUI 1.0

Window {
    id: containerWin
    title: "Plugins"
    width: 360
    height: 800

    color: "transparent"

    readonly property int defaultFlags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool | Qt.WindowDoesNotAcceptFocus
    readonly property int inputFlags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    flags: defaultFlags
    visible: false

    property string initialPlugin: ""

    readonly property int targetX: 20
    property bool isSwitchingFlags: false

    Component.onCompleted: {
        // x = 20;
        y = (Screen.height - height) / 2;
        // not working since it's not active at all
        // SystemBridge.applyWindowEffects(containerWin);
    }

    onVisibleChanged: {
        // Exclude when input flag change
        if (visible && !isSwitchingFlags) {
            x = -width;
            opacity = 0;
            drawerEntryAnim.start();
        }
    }

    ParallelAnimation {
        id: drawerEntryAnim
        NumberAnimation {
            target: containerWin
            property: "x"
            to: targetX
            duration: 350
            easing.type: Easing.OutQuart
        }
        NumberAnimation {
            target: containerWin
            property: "opacity"
            from: 0
            to: 1
            duration: 250
        }
    }

    onInitialPluginChanged: {
        if (initialPlugin !== "") {
            for (var i = 0; i < ConfigBridge.enabledPlugins.length; i++) {
                if (ConfigBridge.enabledPlugins[i] === initialPlugin) {
                    swipeView.currentIndex = i;
                    break;
                }
            }
        }
    }

    Connections {
        target: ConfigBridge
        function onEnabledPluginsChanged() {
            if (swipeView.currentIndex >= ConfigBridge.enabledPlugins.length) {
                swipeView.currentIndex = Math.max(0, ConfigBridge.enabledPlugins.length - 1);
            }
        }
    }

    Connections {
        target: ConfigBridge.profileManager
        function onCurrentProfileChanged() {
            swipeView.currentIndex = 0;
        }
    }

    property var customBackgroundColor: null

    function setBackgroundColor(c) {
        customBackgroundColor = c;
    }

    function resetBackgroundColor() {
        customBackgroundColor = null;
    }

    Rectangle {
        id: rootBackground
        anchors.fill: parent
        radius: 12
        color: customBackgroundColor ? customBackgroundColor : (FluTheme.dark ? "#1a1a1a" : '#f3f3f3')
        Behavior on color {
            ColorAnimation {
                duration: 333
                easing.type: Easing.OutQuint
            }
        }
        border.color: FluTheme.dark ? "#1a1a1a" : '#f3f3f3'
        border.width: 1

        Item {
            id: titleBar
            height: 40
            width: parent.width
            anchors.top: parent.top
            z: 999

            Text {
                text: "Plugin"
                anchors.centerIn: parent
                color: FluTheme.dark ? "#FFFFFF" : "#000000"
                font.bold: true
                font.pixelSize: 14
            }

            FluIconButton {
                id: closeBtn
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                iconSource: FluentIcons.ChromeClose
                iconSize: 16
                z: 1000

                onClicked: {
                    containerWin.hide();
                }
            }

            MouseArea {
                anchors.fill: parent
                onPressed: {
                    containerWin.startSystemMove();
                }
            }
        }

        SwipeView {
            id: swipeView
            anchors.top: titleBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 30
            clip: true

            Repeater {
                model: ConfigBridge.enabledPlugins
                Loader {
                    property string pluginName: modelData
                    width: swipeView.width
                    height: swipeView.height
                    asynchronous: true
                    active: Math.abs(index - swipeView.currentIndex) <= 1 || item !== null

                    source: {
                        var path = Plugin.getPluginPath(pluginName);
                        if (path === "") {
                            return "qrc:/plugin/" + pluginName + ".qml";
                        }
                        return path;
                    }

                    FluProgressRing {
                        anchors.centerIn: parent
                        visible: parent.status === Loader.Loading
                    }

                    onLoaded: {
                        if (item) {
                            if (item.closeRequested)
                                item.closeRequested.connect(containerWin.close);
                            if (item.appLaunched)
                                item.appLaunched.connect(containerWin.close);

                            // Switch between input and default flags
                            if (item.requestInputMode) {
                                item.requestInputMode.connect(function (active) {
                                    var wasVisible = containerWin.visible;

                                    containerWin.isSwitchingFlags = true;

                                    if (active) {
                                        containerWin.flags = containerWin.inputFlags;
                                        containerWin.requestActivate();
                                        containerWin.raise();
                                    } else {
                                        containerWin.flags = containerWin.defaultFlags;
                                    }

                                    containerWin.color = "transparent";

                                    if (wasVisible)
                                        containerWin.show();
                                    containerWin.isSwitchingFlags = false;
                                });
                            }
                        }
                    }
                }
            }
        }

        PageIndicator {
            id: indicator
            count: swipeView.count
            currentIndex: swipeView.currentIndex
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 10

            delegate: Rectangle {
                width: 8
                height: 8
                radius: 4
                color: index === indicator.currentIndex ? (FluTheme.dark ? "#FFFFFF" : "#000000") : (FluTheme.dark ? "#666666" : "#CCCCCC")
                opacity: index === indicator.currentIndex ? 0.9 : 0.5
            }
        }
    }
}
