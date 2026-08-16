import QtQuick 2.15
import QtQuick.Controls 2.15
import EdgeGesture.StageManager 1.0
import "./stagemanager"

Item {
    id: root

    // Signal required by PluginContainer
    signal getCloseRequested
    signal requestInputMode(bool active)

    // Fleet Command: Track the active GROUP (The Tower)
    property var activeGroup: null
    property int fleetCounter: 0

    // Entry Point: Add a window controller
    function addWindowToCapsule(controller) {
        if (root.activeGroup) {
            // Fleet exists: Add a wingman
            createCapsuleForGroup(root.activeGroup, controller);
            console.log("StageManager: Added wingman to existing fleet");
        } else {
            // New Fleet needed
            createNewFleet(controller);
        }
    }

    // 1. Create a new Fleet (Group + First Capsule)
    function createNewFleet(controller) {
        var groupComponent = Qt.createComponent("stagemanager/CapsuleGroup.qml");
        if (groupComponent.status !== Component.Ready) {
            console.error("StageManager: Failed to load CapsuleGroup:", groupComponent.errorString());
            return;
        }

        // --- POSITION CALCULATION ---
        var capsuleWidth = 800; // Expected default width
        var capsuleHeight = 600; // Expected default height

        // Center X
        var startX = (Screen.width - capsuleWidth) / 2;
        // Bottom Y (With 100px margin from bottom presumably for taskbar/dock)
        var startY = (Screen.height - capsuleHeight) - 100;

        // Fallback if Screen is 0 (headless/init issue)
        if (startX < 0 || isNaN(startX))
            startX = 100;
        if (startY < 0 || isNaN(startY))
            startY = 100;

        var group = groupComponent.createObject(root, {
            "x": startX,
            "y": startY
        });

        // Handle Group Lifecycle
        group.requestDestroy.connect(function () {
            if (root.activeGroup === group) {
                root.activeGroup = null;
            }
            group.destroy();
            console.log("StageManager: Fleet destroyed");
        });

        root.activeGroup = group;
        root.fleetCounter++;

        // Add the first wingman
        createCapsuleForGroup(group, controller);
        console.log("StageManager: Created new fleet");
    }

    // 2. Create a Wingman (AppCapsule) assigned to a Group
    function createCapsuleForGroup(group, controller) {
        var capsuleComponent = Qt.createComponent("stagemanager/AppCapsule.qml");
        if (capsuleComponent.status !== Component.Ready) {
            console.error("StageManager: Failed to load AppCapsule:", capsuleComponent.errorString());
            return;
        }

        var capsule = capsuleComponent.createObject(null, {
            "group": group,
            "controller": controller
        });

        // Register with Tower
        group.addCapsule(capsule);

        // Apply Win11 native rounded corners (Defensive)
        Qt.callLater(function () {
            SafeWindowReparenter.applyWin11RoundedCorners(capsule);
        });

        // Handle Detach (Release to Desktop)
        capsule.requestGlobalDetach.connect(function () {
            console.log("StageManager: Releasing capsule to desktop");
            // 1. Remove from current group (Handled by closing signal logic)
            // group.removeCapsule(capsule); // Optional since controller.requestRelease emits closed()

            // 2. Release the window back to Windows OS
            if (capsule.controller) {
                capsule.controller.requestRelease();
            }

        // 3. Close the capsule (Handled by onClosed connection in AppCapsule, but can force check)
        });

        // Handle Explode (Break everyone up)
        capsule.requestGlobalExplode.connect(function () {
            console.log("StageManager: Exploding fleet");
            // Iterate all members of the group
            // We need a snapshot because the list will change
            var members = [];
            for (var i = 0; i < group.capsuleModel.count; i++) {
                members.push(group.capsuleModel.get(i).capsuleItem);
            }

            // Destroy the group (which might auto-close capsules?)
            // Logic in group.removeCapsule handles cleanup.
            // Let's just create NEW fleets for everyone first, then kill old group?
            // HWNDs are shared via controller. If we create new capsule for same controller,
            // the controller logic might need to handle 'reassign'.
            // WindowController::setCapsuleItem handles swap.

            for (var j = 0; j < members.length; j++) {
                var cap = members[j];
                var ctrl = cap.controller;
                createNewFleet(ctrl);
                cap.close();
            }

        // Old group should die automatically when empty or we force it
        // group.requestDestroy() called by empty check?
        });
    }

    // Listen to CapsuleManager for new windows (C++ Model)
    Connections {
        target: CapsuleManager

        function onRowsInserted(parent, first, last) {
            for (var i = first; i <= last; i++) {
                var idx = CapsuleManager.index(i, 0);
                var controller = CapsuleManager.data(idx, 259);
                if (controller) {
                    root.addWindowToCapsule(controller);
                }
            }
        }
    }

    // Initialize existing windows on component load
    Component.onCompleted: {
        for (var i = 0; i < CapsuleManager.rowCount(); i++) {
            var idx = CapsuleManager.index(i, 0);
            var controller = CapsuleManager.data(idx, 259);
            if (controller) {
                root.addWindowToCapsule(controller);
            }
        }
    }

    // Sidebar
    StageStrip {
        id: stageStrip
        width: 360
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left

        onWindowSelected: function (hwnd) {
            console.log("Requesting capsule for HWND:", hwnd);
            CapsuleManager.addWindow(hwnd);
        }
    }
}
