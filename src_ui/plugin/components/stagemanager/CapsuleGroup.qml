import QtQuick

QtObject {
    id: group

    // 1. Fleet Coordinates (The "Tower" Position)
    property int x: 100
    property int y: 100

    // 2. Member Registry (NOW A ListModel)
    // We can't expose ListModel directly as a property of QtObject easily in all versions,
    // but we can expose it as a 'property alias' if we were an Item.
    // Since we are QtObject, we hold the reference property.
    property var capsuleModel: ListModel {}

    // 3. Activity State
    property int activeIndex: -1

    signal requestDestroy

    // Logic: Add a capsule to the fleet
    function addCapsule(capsule) {
        // Enforce the 'group' property on the capsule
        capsule.group = group;

        // Add to Model
        // We store the OBJECT reference in a custom role 'capsuleObject'
        capsuleModel.append({
            "capsuleItem": capsule
        });

        // Update index on the object
        capsule.stackIndex = capsuleModel.count - 1;

        activeIndex = capsuleModel.count - 1;

        console.log("CapsuleGroup: Added capsule. Fleet size:", capsuleModel.count);

        // Listen for capsule death
        capsule.closing.connect(function () {
            removeCapsule(capsule);
        });
    }

    // Logic: Remove a capsule
    function removeCapsule(capsule) {
        var removedIdx = -1;
        for (var i = 0; i < capsuleModel.count; i++) {
            if (capsuleModel.get(i).capsuleItem === capsule) {
                removedIdx = i;
                break;
            }
        }

        if (removedIdx !== -1) {
            capsuleModel.remove(removedIdx);
        }

        // Re-index remaining
        for (var j = 0; j < capsuleModel.count; j++) {
            var item = capsuleModel.get(j).capsuleItem;
            if (item)
                item.stackIndex = j;
        }

        if (capsuleModel.count === 0) {
            group.requestDestroy();
        } else {
            if (activeIndex >= capsuleModel.count) {
                activeIndex = capsuleModel.count - 1;
            }
            // Activate the last one
            var next = capsuleModel.get(activeIndex).capsuleItem;
            if (next)
                bringToFront(next);
        }
    }

    // Logic: Move
    function moveBy(dx, dy) {
        group.x += dx;
        group.y += dy;
    }

    // Logic: Minimize All
    function minimizeAll() {
        for (var i = 0; i < capsuleModel.count; i++) {
            var item = capsuleModel.get(i).capsuleItem;
            if (item)
                item.showMinimized();
        }
    }

    // Logic: Bring to front
    function bringToFront(capsule) {
        capsule.raise();
        capsule.requestActivate();

        for (var i = 0; i < capsuleModel.count; i++) {
            if (capsuleModel.get(i).capsuleItem === capsule) {
                activeIndex = i;
                break;
            }
        }
    }

    // Logic: Detach (Release) a capsule
    // Returns the capsule so StageManager can put it in a new group
    // For now, we just remove it from THIS group, but the AppCapsule needs a group to survive.
    // So this logic likely belongs in StageManager?
    // We'll leave it to StageManager to coordinate the transfer.
}
