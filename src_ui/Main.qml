import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import FluentUI 1.0
import "pages"
import Qt.labs.platform 1.1

FluWindow {
    id: window
    title: "EdgeGesture Config"
    appBar: FluAppBar {
        title: window.title
        showDark: true
        closeClickListener: () => {
            window.hide();
        }
    }
    width: 600
    height: 750
    color: "transparent"
    windowIcon: "qrc:/icon.ico"

    background: Item {}

    Component.onCompleted: {
        FluTheme.darkMode = FluThemeType.System;
        ConfigBridge.setWindowDark(FluTheme.dark);
    }

    Connections {
        target: FluTheme
        function onDarkChanged() {
            ConfigBridge.setWindowDark(FluTheme.dark);
        }
    }

    // NavigationView
    FluNavigationView {
        id: nav_view
        anchors.fill: parent
        items: FluObject {
            FluPaneItem {
                title: "Home"
                icon: FluentIcons.Home
                url: "qrc:/pages/HomePage.qml"
                onTap: {
                    nav_view.push(url);
                }
            }
            FluPaneItem {
                title: "Handles"
                icon: FluentIcons.Edit
                url: "qrc:/pages/HandleSettingsPage.qml"
                onTap: {
                    nav_view.push(url);
                }
            }
            FluPaneItem {
                title: "Gestures"
                icon: FluentIcons.Touch
                url: "qrc:/pages/GestureSettingsPage.qml"
                onTap: {
                    nav_view.push(url);
                }
            }
            FluPaneItem {
                title: "Plugins"
                icon: FluentIcons.Pencil
                url: "qrc:/pages/PluginSettingsPage.qml"
                onTap: {
                    nav_view.push(url);
                }
            }
            FluPaneItem {
                title: "Actions"
                icon: FluentIcons.KeyboardClassic
                url: "qrc:/pages/ActionsPage.qml"
                onTap: {
                    nav_view.push(url);
                }
            }
            FluPaneItem {
                title: "Advanced"
                icon: FluentIcons.Settings
                url: "qrc:/pages/AdvancedPage.qml"
                onTap: {
                    nav_view.push(url);
                }
            }
        }

        // Initial Page
        Component.onCompleted: {
            nav_view.setCurrentIndex(0);
            nav_view.push("qrc:/pages/HomePage.qml");
        }
    }

    // Hidden QuickPanel loader (or triggered via C++ signal)
    property bool shouldShowQuickPanel: false

    // Plugin Container Loader
    Loader {
        id: pluginContainerLoader
        active: false
        source: ""

        Connections {
            target: Plugin
            function onPluginsChanged() {
                // Determine what to reload if needed
            }
        }

        onLoaded: {
            if (item) {
                // Connect to visibleChanged signal to detect when window closes/hides
                item.visibleChanged.connect(function () {
                    if (!item.visible) {
                        // Reset or destroy if needed, or keep alive for performance
                        // destroyTimer.start();
                    }
                });
            }
        }
    }

    Connections {
        target: ConfigBridge
        function onShowPlugin(name) {
            console.log("Request showing plugin: " + name);

            if (!pluginContainerLoader.item) {
                pluginContainerLoader.source = "qrc:/plugin/PluginContainer.qml";
                pluginContainerLoader.active = true;
            }

            if (pluginContainerLoader.item) {
                var win = pluginContainerLoader.item;

                win.initialPlugin = name;

                win.show();
                win.raise();
                win.requestActivate();
            } else {
                window.targetPluginName = name;
            }
        }
    }

    property string targetPluginName: ""
    onTargetPluginNameChanged: {
        if (pluginContainerLoader.item) {
            pluginContainerLoader.item.initialPlugin = targetPluginName;
            pluginContainerLoader.item.show();
            pluginContainerLoader.item.raise();
            pluginContainerLoader.item.requestActivate();
        }
    }

    // Connect loader loaded to checking targetPluginName
    Connections {
        target: pluginContainerLoader
        function onLoaded() {
            if (window.targetPluginName !== "") {
                pluginContainerLoader.item.initialPlugin = window.targetPluginName;
                pluginContainerLoader.item.show();
                pluginContainerLoader.item.raise();
                pluginContainerLoader.item.requestActivate();
            }
        }
    }
    // System Tray
    SystemTrayIcon {
        visible: true
        icon.source: "qrc:/icon.ico" // Use a default icon or handle one
        tooltip: "EdgeGesture"

        menu: Menu {
            MenuItem {
                text: "Open Settings"
                onTriggered: {
                    window.show();
                    window.raise();
                    window.requestActivate();
                }
            }
            MenuItem {
                text: "Quit"
                onTriggered: Qt.quit()
            }
        }

        onActivated: {
            window.show();
            window.raise();
            window.requestActivate();
        }
    }
}
