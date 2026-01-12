import QtQuick 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0
import EdgeGesture.Notes 1.0

/**
 * BlockDelegate - Wrapper component that handles common properties
 * for all block types and uses Loader with sourceComponent for type selection.

 */
Item {
    id: blockDelegate
    width: ListView.view ? ListView.view.width : 300
    height: contentLoader.item ? contentLoader.item.height : 24

    // Required properties from model
    required property int index
    required property string type
    required property string content
    required property string formattedContent  // Pre-rendered HTML from C++
    required property var metadata
    required property int level
    required property string language

    // Passed from NotesEditor
    property var editor: null
    property var noteListView: null
    property var notesIndex: null
    property var notesFileHandler: null
    property string notePath: ""
    property string vaultRootPath: ""
    property var onLinkActivatedCallback: null

    // Computed property for folder path
    readonly property string folderPath: {
        var p = notePath.replace(/\\/g, "/");
        return p.substring(0, p.lastIndexOf("/"));
    }

    // Editing state
    property bool isEditing: false

    // Reset state when pooled for reuse
    ListView.onPooled: {
        isEditing = false;
    }

    // Content loader - switches component based on block type
    Loader {
        id: contentLoader
        width: parent.width
        sourceComponent: {
            switch (blockDelegate.type) {
            case "heading":
            case "paragraph":
                return paragraphComp;
            case "code":
                return codeComp;
            case "quote":
                return quoteComp;
            case "callout":
                return calloutComp;
            case "tasklist":
                return tasklistComp;
            case "embed":
                return embedComp;
            case "image":
                return imageComp;
            case "list":
                return listComp;
            case "divider":
                return dividerComp;
            case "table":
                return tableComp;
            default:
                return paragraphComp;
            }
        }

        // Sync editing state bidirectionally
        onItemChanged: {
            if (item) {
                item.isEditing = Qt.binding(function () {
                    return blockDelegate.isEditing;
                });
            }
        }
    }

    // Handle editing state changes from child
    Connections {
        target: contentLoader.item
        function onIsEditingChanged() {
            if (contentLoader.item) {
                blockDelegate.isEditing = contentLoader.item.isEditing;
            }
        }
        ignoreUnknownSignals: true
    }

    // Component definitions
    Component {
        id: paragraphComp
        ParagraphBlock {
            width: blockDelegate.width
            content: blockDelegate.content
            formattedContent: blockDelegate.formattedContent
            folderPath: blockDelegate.folderPath
            noteListView: blockDelegate.noteListView
            editor: blockDelegate.editor
            onLinkActivatedCallback: blockDelegate.onLinkActivatedCallback
            notesIndex: blockDelegate.notesIndex
            notesFileHandler: blockDelegate.notesFileHandler
            notePath: blockDelegate.notePath
            vaultRootPath: blockDelegate.vaultRootPath
            blockIndex: blockDelegate.index
            type: blockDelegate.type
            level: blockDelegate.level
        }
    }

    Component {
        id: codeComp
        CodeBlock {
            width: blockDelegate.width
            content: blockDelegate.content
            folderPath: blockDelegate.folderPath
            noteListView: blockDelegate.noteListView
            editor: blockDelegate.editor
            notesFileHandler: blockDelegate.notesFileHandler
            blockIndex: blockDelegate.index
            language: blockDelegate.language
        }
    }

    Component {
        id: quoteComp
        QuoteBlock {
            width: blockDelegate.width
            content: blockDelegate.content
            folderPath: blockDelegate.folderPath
            noteListView: blockDelegate.noteListView
            editor: blockDelegate.editor
            blockIndex: blockDelegate.index
        }
    }

    Component {
        id: calloutComp
        CalloutBlock {
            width: blockDelegate.width
            content: blockDelegate.content
            folderPath: blockDelegate.folderPath
            noteListView: blockDelegate.noteListView
            editor: blockDelegate.editor
            blockIndex: blockDelegate.index
            metadata: blockDelegate.metadata
        }
    }

    Component {
        id: tasklistComp
        TaskListBlock {
            width: blockDelegate.width
            content: blockDelegate.content
            folderPath: blockDelegate.folderPath
            noteListView: blockDelegate.noteListView
            editor: blockDelegate.editor
            blockIndex: blockDelegate.index
            metadata: blockDelegate.metadata
        }
    }

    Component {
        id: embedComp
        EmbedBlock {
            width: blockDelegate.width
            content: blockDelegate.content
            folderPath: blockDelegate.folderPath
            noteListView: blockDelegate.noteListView
            editor: blockDelegate.editor
            notesIndex: blockDelegate.notesIndex
            notesFileHandler: blockDelegate.notesFileHandler
            notePath: blockDelegate.notePath
            vaultRootPath: blockDelegate.vaultRootPath
            blockIndex: blockDelegate.index
        }
    }

    Component {
        id: imageComp
        ImageBlock {
            width: blockDelegate.width
            content: blockDelegate.content
            folderPath: blockDelegate.folderPath
            noteListView: blockDelegate.noteListView
            editor: blockDelegate.editor
            notesFileHandler: blockDelegate.notesFileHandler
            notePath: blockDelegate.notePath
            vaultRootPath: blockDelegate.vaultRootPath
            blockIndex: blockDelegate.index
        }
    }

    Component {
        id: listComp
        ListBlock {
            width: blockDelegate.width
            content: blockDelegate.content
            folderPath: blockDelegate.folderPath
            noteListView: blockDelegate.noteListView
            editor: blockDelegate.editor
            notesIndex: blockDelegate.notesIndex
            notesFileHandler: blockDelegate.notesFileHandler
            notePath: blockDelegate.notePath
            vaultRootPath: blockDelegate.vaultRootPath
            onLinkActivatedCallback: blockDelegate.onLinkActivatedCallback
            blockIndex: blockDelegate.index
            metadata: blockDelegate.metadata
        }
    }

    Component {
        id: dividerComp
        DividerBlock {
            width: blockDelegate.width
            noteListView: blockDelegate.noteListView
            editor: blockDelegate.editor
            blockIndex: blockDelegate.index
        }
    }

    Component {
        id: tableComp
        TableBlock {
            width: blockDelegate.width
            content: blockDelegate.content
            metadata: blockDelegate.metadata
            folderPath: blockDelegate.folderPath
            noteListView: blockDelegate.noteListView
            editor: blockDelegate.editor
            notesIndex: blockDelegate.notesIndex
            notesFileHandler: blockDelegate.notesFileHandler
            notePath: blockDelegate.notePath
            vaultRootPath: blockDelegate.vaultRootPath
            onLinkActivatedCallback: blockDelegate.onLinkActivatedCallback
            blockIndex: blockDelegate.index
        }
    }
}
