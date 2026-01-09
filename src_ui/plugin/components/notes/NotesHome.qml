import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import FluentUI 1.0
import EdgeGesture.Notes 1.0

Item {
    id: homeRoot

    property var notesModel: null
    property var notesFileHandler: null

    signal requestInputMode(bool active)
    signal openFolderRequested
    signal importRequested
    signal newFolderRequested
    signal deleteRequested(string path, string title, bool isFolder)
    signal openNoteRequested(string path, string title, bool isEditing)

    // When notesModel is set (via StackView.push), reconnect signals
    onNotesModelChanged: {
        // Model is now set
    }

    // Listen for model updates
    Connections {
        target: notesModel
        enabled: notesModel !== null

        function onCurrentPathChanged() {
            // Path changed
        }

        // Force ListView to update when model finishes loading
        function onLoadingChanged() {
            if (notesModel && !notesModel.loading) {
                forceListViewUpdate();
            }
        }
    }

    // Timer to force ListView refresh on next event loop
    Timer {
        id: refreshTimer
        interval: 10
        repeat: false
        onTriggered: {
            if (listView && homeRoot.notesModel) {
                listView.model = null;
                listView.model = homeRoot.notesModel;
            }
        }
    }

    // Force ListView to re-read its model data
    function forceListViewUpdate() {
        refreshTimer.start();
    }

    // Refresh when this view becomes active again (after editor closes)
    StackView.onActivated: {
        if (notesModel) {
            notesModel.refresh();
        }
    }

    // Format path for display
    function formatDisplayPath(path) {
        var displayPath = path;
        if (displayPath.startsWith("file:///")) {
            displayPath = displayPath.substring(8);
        }

        // Replace slashes with separator
        displayPath = displayPath.replace(/\//g, " › ");
        displayPath = displayPath.replace(/\\/g, " › ");
        return displayPath;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Title bar with navigation
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            Layout.leftMargin: 15
            Layout.rightMargin: 15
            spacing: 8

            FluText {
                id: noteTitle
                text: (notesModel && notesModel.folderStack.length > 0) ? notesFileHandler.getFileName(notesModel.currentPath) : "Notes"
                font: FluTextStyle.Title
                Layout.alignment: Qt.AlignVCenter
                elide: Text.ElideMiddle
                Layout.maximumWidth: 130
                clip: true
            }

            // Back button (when in subfolder)
            FluIconButton {
                visible: notesModel ? notesModel.folderStack.length > 0 : false
                iconSource: FluentIcons.Back
                iconSize: 14
                Layout.leftMargin: 4
                Layout.alignment: Qt.AlignVCenter
                onClicked: notesModel.navigateBack()

                FluTooltip {
                    visible: parent.hovered
                    text: "Go back"
                }
            }

            Item {
                Layout.fillWidth: true
            }

            // Change folder button
            FluIconButton {
                iconSource: FluentIcons.OpenFolderHorizontal
                iconSize: 14
                Layout.alignment: Qt.AlignVCenter
                onClicked: homeRoot.openFolderRequested()

                FluTooltip {
                    visible: parent.hovered
                    text: "Open folder"
                }
            }

            // Import markdown button
            FluIconButton {
                iconSource: FluentIcons.Download
                iconSize: 14
                Layout.alignment: Qt.AlignVCenter
                onClicked: homeRoot.importRequested()

                FluTooltip {
                    visible: parent.hovered
                    text: "Import .md file"
                }
            }

            // New folder button
            FluIconButton {
                iconSource: FluentIcons.NewFolder
                iconSize: 14
                Layout.alignment: Qt.AlignVCenter
                onClicked: homeRoot.newFolderRequested()

                FluTooltip {
                    visible: parent.hovered
                    text: "New folder"
                }
            }

            // New note button
            FluIconButton {
                iconSource: FluentIcons.Add
                iconSize: 14
                Layout.alignment: Qt.AlignVCenter
                onClicked: {
                    var timestamp = new Date().getTime();
                    var filePath = notesFileHandler.createNote(notesModel.currentPath, "Untitled_" + timestamp, "", "#624a73");
                    notesModel.refresh();
                    homeRoot.openNoteRequested(filePath, "Untitled_" + timestamp, true);
                }
            }
        }

        // Current path indicator
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            Layout.leftMargin: 15
            Layout.rightMargin: 15
            color: FluTheme.dark ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(0, 0, 0, 0.02)
            radius: 4

            FluText {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                text: notesModel ? formatDisplayPath(notesModel.currentPath) : ""
                font.pixelSize: 11
                color: FluTheme.dark ? "#888" : "#666"
                elide: Text.ElideMiddle
                verticalAlignment: Text.AlignVCenter
            }
        }

        // Search bar
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            Layout.leftMargin: 15
            Layout.rightMargin: 15
            Layout.topMargin: 8
            spacing: 8

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                radius: 6
                color: FluTheme.dark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.03)
                border.width: searchInput.activeFocus ? 2 : 0
                border.color: FluTheme.primaryColor

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 6

                    FluIcon {
                        iconSource: FluentIcons.Search
                        iconSize: 14
                        color: FluTheme.dark ? "#888" : "#666"
                    }

                    TextInput {
                        id: searchInput
                        Layout.fillWidth: true
                        color: FluTheme.dark ? "#FFF" : "#000"
                        font.pixelSize: 13
                        selectByMouse: true
                        clip: true

                        onActiveFocusChanged: {
                            homeRoot.requestInputMode(activeFocus);
                        }

                        Text {
                            anchors.fill: parent
                            text: "Search notes..."
                            color: FluTheme.dark ? "#666" : "#999"
                            font.pixelSize: 13
                            visible: !searchInput.text && !searchInput.activeFocus
                            verticalAlignment: Text.AlignVCenter
                        }

                        onTextChanged: {
                            if (text.length > 0) {
                                notesModel.filterString = text;
                            } else {
                                notesModel.clearFilters();
                            }
                        }
                    }

                    FluIconButton {
                        visible: searchInput.text.length > 0 || (notesModel ? notesModel.isSearchMode : false)
                        iconSource: FluentIcons.Cancel
                        iconSize: 12
                        onClicked: {
                            searchInput.text = "";
                            notesModel.clearFilters();
                        }
                    }
                }
            }

            // Tag filter button
            FluIconButton {
                iconSource: FluentIcons.Tag
                iconSize: 14
                highlighted: notesModel ? notesModel.filterTag !== "" : false
                onClicked: tagFilterMenu.open()

                FluTooltip {
                    visible: parent.hovered
                    text: "Filter by tag"
                }
            }
        }

        // Tag filter indicator
        Rectangle {
            visible: notesModel ? (notesModel.filterTag !== "" || notesModel.isSearchMode) : false
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            Layout.leftMargin: 15
            Layout.rightMargin: 15
            color: FluTheme.primaryColor
            opacity: 0.2
            radius: 4

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10

                FluText {
                    text: (notesModel && notesModel.filterTag !== "") ? "Tag: " + notesModel.filterTag : "Search results"
                    font.pixelSize: 11
                    color: FluTheme.primaryColor
                }

                Item {
                    Layout.fillWidth: true
                }

                FluIconButton {
                    iconSource: FluentIcons.Cancel
                    iconSize: 10
                    onClicked: {
                        searchInput.text = "";
                        notesModel.clearFilters();
                    }
                }
            }
        }

        // Loading indicator
        FluProgressRing {
            visible: notesModel ? notesModel.loading : false
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: 30
            Layout.preferredWidth: 30
        }

        // File/Folder list
        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: notesModel
            spacing: 8
            topMargin: 10
            bottomMargin: 10

            delegate: Item {
                id: wrapper
                width: listView.width
                height: model.type === "folder" ? 56 : 80

                // Use model.* directly for live bindings (not required property)
                property int itemIndex: model.index
                property string itemType: model.type
                property string itemTitle: model.title
                property string itemPath: model.path
                property string itemDate: model.date
                property string itemColor: model.color
                property var itemTags: model.tags
                property bool itemIsPinned: model.isPinned

                Rectangle {
                    id: delegateRoot
                    width: 320
                    height: parent.height
                    radius: 8
                    anchors.centerIn: parent
                    color: itemMouseArea.containsMouse ? (FluTheme.dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.05)) : (FluTheme.dark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.02))

                    Behavior on color {
                        ColorAnimation {
                            duration: 150
                        }
                    }

                    // MouseArea first (behind content)
                    MouseArea {
                        id: itemMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        z: 0
                        onClicked: {
                            if (wrapper.itemType === "folder") {
                                notesModel.navigateToFolder(wrapper.itemPath);
                            } else {
                                homeRoot.openNoteRequested(wrapper.itemPath, wrapper.itemTitle, false);
                            }
                        }
                    }

                    // Color indicator for notes
                    Rectangle {
                        visible: wrapper.itemType === "note"
                        width: 4
                        height: parent.height - 16
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 8
                        radius: 2
                        color: wrapper.itemColor
                        z: 1
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: wrapper.itemType === "note" ? 20 : 12
                        anchors.rightMargin: 8
                        spacing: 10
                        z: 1

                        // Icon
                        FluIcon {
                            iconSource: wrapper.itemType === "folder" ? FluentIcons.FolderHorizontal : FluentIcons.QuickNote
                            iconSize: 20
                            color: wrapper.itemType === "folder" ? "#FFB900" : (FluTheme.dark ? "#ccc" : "#666")
                            Layout.alignment: Qt.AlignVCenter
                        }

                        // Title, date, and tags
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 2

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                // Pin indicator
                                FluIcon {
                                    visible: wrapper.itemIsPinned
                                    iconSource: FluentIcons.PinnedFill
                                    iconSize: 12
                                    color: FluTheme.primaryColor
                                }

                                FluText {
                                    text: wrapper.itemTitle
                                    font: FluTextStyle.Subtitle
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            // Tags row
                            Flow {
                                visible: wrapper.itemTags && wrapper.itemTags.length > 0
                                spacing: 4
                                Layout.fillWidth: true

                                Repeater {
                                    model: wrapper.itemTags || []
                                    Rectangle {
                                        width: tagText.width + 10
                                        height: 16
                                        radius: 8
                                        color: FluTheme.dark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.06)

                                        Text {
                                            id: tagText
                                            anchors.centerIn: parent
                                            text: modelData
                                            font.pixelSize: 9
                                            color: FluTheme.dark ? "#AAA" : "#666"
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: notesModel.filterTag = modelData
                                        }
                                    }
                                }
                            }

                            FluText {
                                text: wrapper.itemDate
                                font.pixelSize: 11
                                color: FluTheme.dark ? "#777" : "#999"
                            }
                        }

                        // Pin button
                        FluIconButton {
                            visible: wrapper.itemType === "note"
                            iconSource: wrapper.itemIsPinned ? FluentIcons.Unpin : FluentIcons.Pin
                            iconSize: 14
                            iconColor: wrapper.itemIsPinned ? FluTheme.primaryColor : (FluTheme.dark ? "#888" : "#666")
                            opacity: (itemMouseArea.containsMouse || hovered || wrapper.itemIsPinned) ? 1 : 0
                            Layout.alignment: Qt.AlignVCenter

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: 150
                                }
                            }

                            onClicked: notesModel.togglePin(wrapper.itemPath)

                            FluTooltip {
                                visible: parent.hovered
                                text: wrapper.itemIsPinned ? "Unpin" : "Pin to top"
                            }
                        }

                        // Delete button
                        FluIconButton {
                            iconSource: FluentIcons.Delete
                            iconSize: 14
                            opacity: (itemMouseArea.containsMouse || hovered) ? 1 : 0
                            Layout.alignment: Qt.AlignVCenter

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: 150
                                }
                            }

                            onClicked: {
                                homeRoot.deleteRequested(wrapper.itemPath, wrapper.itemTitle, wrapper.itemType === "folder");
                            }
                        }
                    }
                }
            }

            // Empty state
            Rectangle {
                visible: notesModel ? (notesModel.rowCount() === 0 && !notesModel.loading) : false
                anchors.centerIn: parent
                width: 200
                height: 120
                color: "transparent"

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 10

                    FluIcon {
                        iconSource: FluentIcons.Edit
                        iconSize: 48
                        color: FluTheme.dark ? "#555" : "#999"
                        Layout.alignment: Qt.AlignHCenter
                    }

                    FluText {
                        text: "No notes yet"
                        color: FluTheme.dark ? "#666" : "#888"
                        font.pixelSize: 14
                        Layout.alignment: Qt.AlignHCenter
                    }

                    FluText {
                        text: "Tap + to create one"
                        color: FluTheme.dark ? "#555" : "#999"
                        font.pixelSize: 12
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }
        }
    }

    // Tag filter menu
    Menu {
        id: tagFilterMenu

        MenuItem {
            text: "All Tags"
            onTriggered: notesModel.clearFilters()
        }

        MenuSeparator {}

        Repeater {
            model: notesModel ? notesModel.getAllTags() : []
            MenuItem {
                text: modelData
                onTriggered: notesModel.filterTag = modelData
            }
        }
    }
}
