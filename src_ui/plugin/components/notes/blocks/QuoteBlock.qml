import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0

Item {
    id: root
    width: ListView.view ? ListView.view.width - 20 : 300
    height: loader.item ? loader.item.implicitHeight : 24

    property string content: model.content ? model.content : ""
    property bool isEditing: false
    property string folderPath: ""
    property var noteListView: null
    property var notesIndex: null
    property int blockIndex: -1

    property var onLinkActivatedCallback: null
    property var notesFileHandler: null // For image search
    property string notePath: "" // Current note path
    property string vaultRootPath: "" // Root of the vault for image search

    // Strings for debugging
    Component.onCompleted: { }

    // Sync content when model changes
    onContentChanged: {
        if (!isEditing && loader.item && loader.item.text !== undefined) {
            // binding handles this
        }
    }

    Loader {
        id: loader
        width: parent.width
        sourceComponent: isEditing ? editorComp : viewerComp
    }

    Component {
        id: viewerComp
        Rectangle {
            width: root.width
            height: textItem.contentHeight + 20
            color: FluTheme.dark ? "#333333" : "#f0f0f0"
            radius: 4
            border.left: 4
            border.color: FluTheme.primaryColor

            Text {
                id: textItem
                width: parent.width - 24
                anchors.centerIn: parent
                wrapMode: Text.Wrap
                // Pre-process content to handle Obsidian-style images and links via Markdown
                text: {
                    var t = root.content;
                    var basePath = "file:///" + root.folderPath.replace(/\\/g, "/") + "/";
                    
                    // Replace ![[image.png]] with standard Markdown ![image.png](url)
                    t = t.replace(/!\[\[(.*?)\]\]/g, function(match, p1) {
                        var src = p1;
                        if (src.indexOf(":") === -1 && src.indexOf("/") !== 0) {
                            src = basePath + src;
                        } else if (src.indexOf("file://") !== 0) {
                            src = "file:///" + src;
                        }
                        src = src.replace(/ /g, "%20");
                        return '<img src="' + src + '" width="320">';
                    });

                    // Replace [[Link]] with <a href="Link">Link</a>
                    t = t.replace(/\[\[(.*?)\]\]/g, function(match, p1) {
                        return '<a href="' + encodeURIComponent(p1) + '">' + p1 + '</a>';
                    });
                    return t;
                }
                color: FluTheme.dark ? "#cccccc" : "#555555"
                font.pixelSize: 16
                font.italic: true
                font.family: "Segoe UI"
                textFormat: Text.MarkdownText
                baseUrl: root.folderPath ? "file:///" + root.folderPath.replace(/\\/g, "/") + "/" : ""
                linkColor: FluTheme.primaryColor

                Component.onCompleted: {
                    // Set CSS stylesheet to constrain image width
                    if (textDocument && textDocument.textDocument) {
                        textDocument.textDocument.defaultStyleSheet = "img { max-width: 320px; height: auto; }";
                    }
                    console.log("QuoteBlock Ready.");
                    console.log("  > QC Processed Text:", text);
                    console.log("  > QC BaseUrl:", baseUrl);
                }

                onLinkActivated: (link) => {
                    var decodedLink = decodeURIComponent(link);
                    var globalPath = "";
                    if (root.notesIndex) {
                        globalPath = root.notesIndex.findPathByTitle(decodedLink);
                    }
                    if (globalPath !== "") {
                        if (root.onLinkActivatedCallback) root.onLinkActivatedCallback(globalPath);
                        return;
                    }
                    if (decodedLink.endsWith(".md")) {
                         var p = root.folderPath + "/" + decodedLink;
                         if (root.onLinkActivatedCallback) root.onLinkActivatedCallback(p);
                         return;
                    }
                    Qt.openUrlExternally(link);
                }

                MouseArea {
                        } else {
                            if (root.noteListView) {
                                root.noteListView.currentIndex = root.blockIndex;
                                root.isEditing = true;
                            }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: root.noteListView
        function onCurrentIndexChanged() {
            if (root.noteListView && root.noteListView.currentIndex !== root.blockIndex) {
                root.isEditing = false;
            }
        }
        ignoreUnknownSignals: true
    }

    Component {
        id: editorComp
        TextArea {
            width: root.width
            text: root.content
            wrapMode: Text.Wrap
            color: FluTheme.dark ? "#cccccc" : "#555555"
            font.pixelSize: 16
            font.italic: true
            font.family: "Segoe UI"

            // Darker background for edit mode
            background: Rectangle {
                color: FluTheme.dark ? "#33000000" : "#1A000000"
                radius: 4
                border.width: 1
                border.color: FluTheme.primaryColor
            }

            selectByMouse: true

            Component.onCompleted: {
                forceActiveFocus();
            }

            onEditingFinished: {
                finishEdit();
            }

            onActiveFocusChanged: {
                if (!activeFocus)
                    finishEdit();
            }

            function finishEdit() {
                if (root.isEditing) {
                    if (root.noteListView && root.noteListView.model) {
                        root.noteListView.model.updateBlock(root.blockIndex, text);
                    }
                    root.isEditing = false;
                }
            }
        }
    }
}
