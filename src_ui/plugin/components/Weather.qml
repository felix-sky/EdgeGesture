import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import Qt.labs.platform 1.1
import FluentUI 1.0

Item {
    id: root
    anchors.fill: parent

    signal closeRequested

    signal requestInputMode(bool active)

    property var weatherData: null
    property var dailyData: null
    property string cityName: "Beijing"
    property double latitude: 39.9042
    property double longitude: 116.4074
    property bool loading: false
    property string errorMsg: ""

    property bool showSearch: false

    property string configPath: StandardPaths.standardLocations(StandardPaths.DocumentsLocation)[0] + "/EdgeGesture/weather.json"

    Component.onCompleted: {
        var dir = StandardPaths.standardLocations(StandardPaths.DocumentsLocation)[0] + "/EdgeGesture";
        if (!FileBridge.exists(dir)) {
            FileBridge.createDirectory(dir);
        }
        loadConfig();
        initTimer.start();
    }

    function saveConfig() {
        var data = {
            "cityName": cityName,
            "latitude": latitude,
            "longitude": longitude
        };
        FileBridge.writeFile(configPath, JSON.stringify(data, null, 4));
    }

    function loadConfig() {
        if (FileBridge.exists(configPath)) {
            var content = FileBridge.readFile(configPath);
            if (content) {
                try {
                    var data = JSON.parse(content);
                    if (data.cityName)
                        cityName = data.cityName;
                    if (data.latitude)
                        latitude = data.latitude;
                    if (data.longitude)
                        longitude = data.longitude;
                } catch (e) {
                    console.log("Error loading weather config:", e);
                }
            }
        }
    }

    Timer {
        id: initTimer
        interval: 100
        repeat: false
        onTriggered: refreshWeather()
    }

    function refreshWeather() {
        loading = true;
        errorMsg = "";

        var url = "https://api.open-meteo.com/v1/forecast?latitude=" + latitude + "&longitude=" + longitude + "&current_weather=true&daily=weathercode,temperature_2m_max,temperature_2m_min&timezone=auto";

        var xhr = new XMLHttpRequest();
        xhr.onreadystatechange = function () {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                loading = false;
                if (xhr.status === 200) {
                    var json = JSON.parse(xhr.responseText);
                    root.weatherData = json.current_weather;
                    root.dailyData = json.daily;
                } else {
                    errorMsg = "Network Error";
                }
            }
        };
        xhr.open("GET", url);
        xhr.send();
    }

    function searchCity(name) {
        if (!name)
            return;
        loading = true;
        var url = "https://geocoding-api.open-meteo.com/v1/search?name=" + name + "&count=1&language=en&format=json";

        var xhr = new XMLHttpRequest();
        xhr.onreadystatechange = function () {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 200) {
                    var json = JSON.parse(xhr.responseText);
                    if (json.results && json.results.length > 0) {
                        root.latitude = json.results[0].latitude;
                        root.longitude = json.results[0].longitude;
                        root.cityName = json.results[0].name;
                        saveConfig();
                        root.showSearch = false;
                        root.requestInputMode(false);
                        refreshWeather();
                    } else {
                        loading = false;
                        errorMsg = "City not found";
                    }
                } else {
                    loading = false;
                    errorMsg = "Search failed";
                }
            }
        };
        xhr.open("GET", url);
        xhr.send();
    }

    function getWeatherDesc(code) {
        if (code === 0)
            return "Clear ☀️";
        if (code >= 1 && code <= 3)
            return "Cloudy ⛅";
        if (code >= 45 && code <= 48)
            return "Fog 🌫️";
        if (code >= 51 && code <= 67)
            return "Rain 🌧️";
        if (code >= 71 && code <= 77)
            return "Snow ❄️";
        if (code >= 95)
            return "Thunderstorm ⛈️";
        return "Unknown";
    }

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            width: 360
            spacing: 15

            Component.onCompleted: {
                console.log(parent.width);
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                spacing: 0

                FluText {
                    text: root.cityName
                    font: FluTextStyle.Title
                    Layout.alignment: Qt.AlignVCenter
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                }

                FluIconButton {
                    Layout.alignment: Qt.AlignVCenter
                    iconSource: FluentIcons.Search
                    onClicked: {
                        root.showSearch = !root.showSearch;
                        if (root.showSearch) {
                            root.requestInputMode(true);
                            inputTimer.start();
                        } else {
                            root.requestInputMode(false);
                        }
                    }
                }

                // Refresh Button
                FluIconButton {
                    Layout.alignment: Qt.AlignVCenter
                    iconSource: FluentIcons.Sync
                    onClicked: refreshWeather()
                }
            }

            FluTextBox {
                id: searchInput
                visible: root.showSearch
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                placeholderText: "Enter city name..."
                onAccepted: {
                    searchCity(text);
                    root.requestInputMode(false);
                }

                FluIconButton {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.rightMargin: 20
                    iconSource: FluentIcons.Search
                    iconSize: 15
                    onClicked: {
                        searchCity(searchInput.text);
                        root.requestInputMode(false);
                    }
                }
            }

            Timer {
                id: inputTimer
                interval: 100
                onTriggered: {
                    searchInput.forceActiveFocus();
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 160
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                radius: 12
                color: FluTheme.dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.05)

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 5
                    visible: !root.loading && !root.errorMsg

                    Text {
                        text: root.weatherData ? getWeatherDesc(root.weatherData.weathercode) : "--"
                        font.pixelSize: 32
                        color: FluTheme.dark ? "#FFF" : "#000"
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: root.weatherData ? Math.round(root.weatherData.temperature) + "°C" : "--"
                        font.pixelSize: 48
                        font.bold: true
                        color: FluTheme.dark ? "#FFF" : "#000"
                        Layout.alignment: Qt.AlignHCenter
                    }

                    FluText {
                        text: "Wind Speed: " + (root.weatherData ? root.weatherData.windspeed + " km/h" : "--")
                        color: FluTheme.dark ? "#AAA" : "#666"
                        Layout.alignment: Qt.AlignHCenter
                    }
                }

                FluProgressRing {
                    anchors.centerIn: parent
                    visible: root.loading
                }
                FluText {
                    anchors.centerIn: parent
                    text: root.errorMsg
                    color: "red"
                    visible: root.errorMsg !== ""
                }
            }

            FluText {
                text: "Daily Forecast"
                font: FluTextStyle.Subtitle
                Layout.leftMargin: 20
                Layout.topMargin: 10
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                spacing: 8

                Repeater {
                    model: root.dailyData ? Math.min(5, root.dailyData.time.length) : 0
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        color: "transparent"

                        RowLayout {
                            anchors.fill: parent

                            FluText {
                                text: root.dailyData.time[index].slice(5)
                                Layout.preferredWidth: 60
                            }

                            FluText {
                                text: getWeatherDesc(root.dailyData.weathercode[index])
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                            }

                            FluText {
                                text: Math.round(root.dailyData.temperature_2m_max[index]) + "° / " + Math.round(root.dailyData.temperature_2m_min[index]) + "°"
                                Layout.alignment: Qt.AlignRight
                            }
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: FluTheme.dark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.1)
                            visible: index < 4
                        }
                    }
                }
            }
        }
    }
}
