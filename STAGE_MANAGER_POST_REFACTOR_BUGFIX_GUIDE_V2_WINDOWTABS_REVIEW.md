# EdgeGesture Stage Manager — Post-Refactor Bugfix Guide

> **Target:** `felix-sky/EdgeGesture` — `feature/StageManager` branch  
> **Scope:** fix the new issues found after the Stage Manager architecture refactor.  
> **Important:** the core architecture is now basically correct and functional. **Do not redesign the whole Stage Manager again.**

---

# 0. Current status

The current architecture should remain:

```text
one StageContainer top-level Qt window
        |
        +-- native contentHost HWND
                |
                +-- reparented external HWND A
                +-- reparented external HWND B
                +-- reparented external HWND C
```

External applications are still genuinely embedded with Win32 `SetParent`.

Page switching should remain:

```text
attach once
    ↓
switch page
    ↓
hide old target
show new target
focus new target
```

Do **not** return to one top-level Qt window per application, and do **not** replace native hosting with capture/input forwarding just to fix the issues below.

---

## 0.1 Decision after reviewing WindowTabs

The open-source WindowTabs project was reviewed as a reference implementation.

Its most important architectural choice is **different from EdgeGesture Stage Manager**:

```text
WindowTabs:
    application windows remain normal top-level HWNDs
    +
    geometry synchronization
    +
    z-order stacking
    +
    separate floating tab/decorator HWND
```

It does **not** solve its grouping UI by converting application windows into `WS_CHILD` windows under a shared host.

This explains why WindowTabs naturally avoids several problems that EdgeGesture must handle:

```text
DWM/Mica behavior after reparent
native-child airspace
cross-process SetParent DPI behavior
child-window focus semantics
restore of WS_CHILD -> top-level state
```

However, adopting WindowTabs' architecture now would trade those problems for a much larger desktop-window orchestration problem:

```text
geometry synchronization
z-order synchronization
move/resize synchronization
maximize/minimize/restore synchronization
Alt+Tab behavior
taskbar behavior
Snap behavior
foreground activation
virtual desktops
owned/modal windows
multi-monitor movement
decorator-window positioning
```

The current EdgeGesture product model is also more naturally represented by a real container:

```text
StageContainer
    └── Pages
```

rather than:

```text
logical WindowGroup
    ├── top-level HWND A
    ├── top-level HWND B
    └── top-level HWND C
```

### Therefore: DO NOT migrate Stage Manager to the WindowTabs TopLevelStack architecture.

The current reparent-based architecture remains the chosen implementation unless a future hard blocker is proven.

A hard blocker would mean something like:

- a major required application class cannot function when reparented at all;
- cross-DPI behavior becomes fundamentally unusable;
- modal/owned-window behavior is broadly unfixable;
- a future Windows version makes cross-process reparenting broadly nonfunctional.

The currently known bugs do **not** meet that threshold.

They are local compatibility/lifecycle issues and should be fixed locally.

### What SHOULD be borrowed from WindowTabs

Use WindowTabs as a reference for:

```text
window identity / validity checks
WinEvent-driven lifecycle tracking
foreground/activation handling
move/size event awareness
minimize/restore state tracking
Explorer/window-class edge cases
defensive cleanup when windows disappear unexpectedly
```

Do not copy its entire grouping architecture.

In particular, the following WindowTabs ideas reinforce this guide:

1. Treat the **window group/page registry as logical state** that must survive transient show/hide events cleanly.
2. Use WinEvent as a source of lifecycle information rather than relying on a single close call.
3. Separate “window disappeared” from “window was actually destroyed”.
4. Treat Explorer and other shell/modern windows as special compatibility test cases.
5. Make foreground/focus transitions explicit instead of assuming a geometry/z-order change implies activation.
6. When an unexpected lifecycle event occurs, recover the remaining group/container deterministically instead of leaving stale entries.

### What MUST NOT be copied

Do not add:

```text
top-level application stacking
off-screen hiding of inactive pages
group-wide geometry mirroring
WindowTabs-style decorator ownership
taskbar/Alt+Tab emulation
```

unless the project explicitly decides in the future to abandon real HWND hosting.

For the present bugfix round, WindowTabs is a **reference for robustness**, not a migration target.

---

# 1. New issue summary

There are four newly observed problems.

## Issue A — Explorer / some modern apps lose their solid background

Observed:

```text
File Explorer before embedding:
white / dark solid background

after reparent:
background becomes transparent
```

Likely cause:

- target becomes `WS_CHILD` after `SetParent`;
- DWM/system backdrop semantics change;
- Stage Manager native content host is transparent / uses `NULL_BRUSH`;
- QML background behind a native child HWND is not a reliable native backing surface.

---

## Issue B — Qt tooltip / floating QML UI is hidden behind the embedded app

Observed:

```text
StageContainer QML tooltip
        ↓
embedded Explorer/Chrome HWND draws above it
```

This is native-child airspace behavior. A normal QML `Item`, `Rectangle`, `Overlay`, or `Popup.Item` cannot reliably draw above a real child HWND by increasing `z`.

---

## Issue C — Close an app inside StageContainer, reopen it, and it does not appear

Likely lifecycle failure:

- Stage Manager sends `WM_CLOSE` while the HWND is still reparented and registered as managed;
- target may hide/reuse the HWND instead of immediately destroying it;
- registry may retain stale HWND ownership;
- `LiveWindowManager` may remove it from candidates on `HIDE` without removing Stage Manager ownership;
- future `isManaged(hwnd)` rejects the reused/reopened window;
- discovery may also miss a window if `CREATE/SHOW` arrives before title/visibility stabilizes and `NAMECHANGE` only updates already-known entries.

This is a lifecycle/state-consistency bug, not an app-launch problem.

---

## Issue D — Mouse can drag the white handle, finger cannot

Current behavior is effectively:

```text
MouseArea
    ↓
Window.startSystemMove()
```

This works for mouse but is not a robust Windows-native touch dragging model.

Correct fix for this Windows-only Stage Manager:

```text
WM_NCHITTEST
    ↓
HTCAPTION / HTLEFT / HTRIGHT / HTBOTTOM / ...
```

Then Windows itself handles move/resize, touch, snap, and cross-monitor behavior.

---

# 2. Priority order

Fix in this order:

```text
P0  Close / reopen lifecycle
P1  Native host opacity / Explorer backdrop
P1  Native popup / tooltip airspace
P1  Native touch move / resize hit testing
P2  Application-specific composition fallbacks
```

Lifecycle comes first because stale managed HWNDs can leave applications hidden, parented to dead hosts, or permanently excluded from the window list.

---

# 3. Issue C — Correct close/reopen lifecycle

## 3.1 Do not close while Stage Manager still owns the child

Bad flow:

```text
target still has:
  parent = contentHost
  style = WS_CHILD
  registry = managed

send WM_CLOSE
    ↓
hope it dies
```

Applications may show Save dialogs, reject close, hide, delay destruction, or reuse the same HWND.

## 3.2 Correct semantic rule

Define:

```text
Release:
    restore to normal desktop state
    remove from Stage Manager

Close:
    restore to normal desktop state
    remove from Stage Manager
    then request WM_CLOSE
```

So:

```text
                  +--> stop
                  |
managed page -> release
                  |
                  +--> PostMessage(WM_CLOSE) = close
```

## 3.3 Recommended close transaction

Pseudo:

```cpp
CloseResult StageManagerService::closePage(ContainerId containerId,
                                           PageId pageId)
{
    auto page = lookupPage(containerId, pageId);
    if (!page)
        return NotFound;

    HWND hwnd = page->identity.hwnd;

    auto restoreResult = releasePageInternal(containerId, pageId);
    if (!restoreResult.success) {
        // Do not send WM_CLOSE while half-hosted.
        return CloseResult::RestoreFailed;
    }

    if (!IsWindow(hwnd))
        return CloseResult::AlreadyDestroyed;

    if (!PostMessageW(hwnd, WM_CLOSE, 0, 0)) {
        log Win32 error;
        return CloseResult::CloseMessageFailed;
    }

    return CloseResult::Requested;
}
```

Important:

- do not delete native state before restore completes;
- once restore succeeds, Stage Manager no longer owns the target;
- if the app rejects close, it remains a normal desktop window;
- this is safer for Save dialogs and modal UI.

## 3.4 Managed registry must validate identity

Do not permanently trust:

```text
HWND -> PageId
```

At minimum store:

```cpp
struct ManagedEntry {
    PageId pageId;
    ContainerId containerId;
    DWORD pid;
    DWORD tid;
};
```

Before `isManaged(hwnd)` returns true, verify:

```text
IsWindow(hwnd)
GetWindowThreadProcessId(hwnd, &pid)
pid matches entry
page/container still exists
```

If not, remove the stale entry and return false.

HWND values can be reused.

## 3.5 `EVENT_OBJECT_HIDE`

`HIDE` is not destruction.

For a managed target:

```text
if hide was caused by StageManager page switching:
    ignore as expected
else:
    update visibility/lifecycle state carefully
```

Do not merely remove it from the sidebar while leaving hidden managed ownership with no bookkeeping.

## 3.6 `EVENT_OBJECT_DESTROY`

On confirmed destruction:

```text
remove managed registry entry
mark page Destroyed
remove page from container model
activate another page if available
if container empty, close it according to UX
```

Do not call `SetParent`, `ShowWindow`, `SetWindowPos`, or restore on a confirmed-destroyed HWND.

## 3.7 `NAMECHANGE` must also discover unknown windows

New windows often initialize like:

```text
CREATE -> no title / not valid yet
SHOW   -> still incomplete
NAMECHANGE -> finally valid
```

So:

```cpp
if (event == EVENT_OBJECT_NAMECHANGE) {
    if (containsWindow(hwnd)) {
        updateWindow(hwnd);
    } else if (isValidWindow(hwnd)) {
        addWindow(hwnd);
    }
}
```

Do not limit `NAMECHANGE` to already-known candidates.

## 3.8 Optional one-shot reconciliation

For Explorer/Chromium/Electron startup timing:

```cpp
QTimer::singleShot(100, this, [this, hwnd] {
    if (!IsWindow(hwnd))
        return;

    if (!containsWindow(hwnd) && isValidWindow(hwnd))
        addWindow(hwnd);
});
```

This is not continuous polling; it is a one-shot verification after a real WinEvent.

---

# 4. Issue A — Explorer background becomes transparent

## 4.1 Why

Explorer and some newer Windows apps rely on top-level composition/backdrop behavior such as DWM/Mica/DirectComposition.

After:

```text
top-level HWND
    ↓
SetParent
    ↓
WS_CHILD
```

the backdrop conditions change. If the native content host itself is transparent, transparent regions inside the target reveal whatever is behind the native window hierarchy.

## 4.2 Diagnostic test

If the content-host window class uses:

```cpp
wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
```

temporarily test:

```cpp
wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
```

or `WHITE_BRUSH`.

If Explorer becomes solid instead of transparent, the backing-surface diagnosis is confirmed.

This is a diagnostic, not the final implementation.

---

# 5. Native content host must paint an opaque background

Do not depend only on a QML `Rectangle` behind the native host.

Give the content-host class a real WndProc and paint it natively.

Handle at least:

```text
WM_ERASEBKGND
WM_PAINT
WM_THEMECHANGED
WM_SETTINGCHANGE
```

Pseudo:

```cpp
LRESULT CALLBACK StageContentHostWndProc(HWND hwnd,
                                         UINT msg,
                                         WPARAM wParam,
                                         LPARAM lParam)
{
    switch (msg) {
    case WM_ERASEBKGND:
        PaintHostBackground(hwnd, reinterpret_cast<HDC>(wParam));
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintHostBackground(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
```

Use theme-aware solid colors:

```text
Windows app theme light -> light opaque background
Windows app theme dark  -> dark opaque background
```

First goal is opacity and stability, not pixel-perfect Explorer styling.

---

# 6. Optional hosted-state DWM backdrop suppression

If Explorer remains wrong after the host becomes opaque, test disabling system backdrop while hosted.

Capture before attach:

```cpp
DWM_SYSTEMBACKDROP_TYPE originalBackdrop{};
bool hasBackdropState = SUCCEEDED(
    DwmGetWindowAttribute(
        target,
        DWMWA_SYSTEMBACKDROP_TYPE,
        &originalBackdrop,
        sizeof(originalBackdrop)));
```

Hosted:

```cpp
DWM_SYSTEMBACKDROP_TYPE none = DWMSBT_NONE;
DwmSetWindowAttribute(
    target,
    DWMWA_SYSTEMBACKDROP_TYPE,
    &none,
    sizeof(none));
```

Restore the original value when releasing.

Treat failure as non-fatal because not every target/window version supports the attribute the same way.

---

# 7. `WS_EX_NOREDIRECTIONBITMAP` — diagnostic first

Log whether composition-heavy targets contain:

```text
WS_EX_NOREDIRECTIONBITMAP
```

If a target still renders transparent after the opaque-host fix, experimentally test clearing it only while hosted:

```cpp
hostedExStyle = originalExStyle & ~WS_EX_NOREDIRECTIONBITMAP;
```

Restore the exact original exStyle afterwards.

Do not globally clear it for every application without testing.

Do not build an Explorer-specific hack list until generic fixes are exhausted.

---

# 8. Do not use `WS_EX_LAYERED` as the opacity fix

Do not add `WS_EX_LAYERED` / `SetLayeredWindowAttributes` merely to force a background.

That changes composition semantics again and risks creating more rendering problems.

---

# 9. Issue B — Tooltip / popup hidden behind embedded HWND

This is a native airspace problem.

Do not solve it with QML `z`.

## 9.1 Rule

Any UI that may overlap the native content host must be a native/top-level popup window.

Unsafe over hosted HWND:

```text
Rectangle
Item
Overlay
Popup.Item
ordinary attached ToolTip
z: 999999
```

## 9.2 ToolTip fix

If the project Qt version supports `Popup.Window`, prefer a local ToolTip:

```qml
ToolTip {
    parent: pinButton
    visible: pinButton.hovered
    text: qsTr("Pin container")
    popupType: Popup.Window
}
```

Prefer this over attached syntax when the tooltip can cross the native content region.

## 9.3 General popup rule

```text
UI entirely outside native content area:
    ordinary QML is fine

UI that may overlap native content area:
    Popup.Window / separate Window

platform-native menu:
    Popup.Native only when desired
```

Review:

```text
ToolTip
Menu
context menu
page preview
floating HUD
confirmation popup
drag preview
toast inside StageContainer
```

## 9.4 Do not push the target HWND to `HWND_BOTTOM`

That does not turn the QML scene graph into a sibling native HWND and can create z-order/input bugs.

---

# 10. Issue D — Touch cannot drag StageContainer

Do not keep trying to make `MouseArea` emulate native window dragging.

Current anti-pattern:

```qml
MouseArea {
    onPressed: root.startSystemMove()
}
```

For this Windows-only plugin, use native non-client hit testing instead.

---

# 11. Implement `WM_NCHITTEST`

Concept:

```text
Windows pointer/touch interaction
        ↓
WM_NCHITTEST
        ↓
where is the point?
        ↓
HTCAPTION / resize edge / HTCLIENT
```

Then Windows owns:

```text
mouse drag
touch drag
snap
cross-monitor movement
native resize behavior
```

## 11.1 Header drag region

If the visual white pill is only 5 px tall, do **not** make only that 5 px strip draggable.

Use a larger invisible/native drag region, e.g. the full 32–40 logical px header.

Example:

```text
visual pill: 70 x 5
native drag area: full header width x 32–40
```

Return:

```cpp
HTCAPTION
```

for empty header space.

---

# 12. Move resize to the same hit-test system

If current resize uses QML `MouseArea + startSystemResize()`, migrate it too.

Return:

```text
HTTOPLEFT    HTTOP    HTTOPRIGHT
HTLEFT                HTRIGHT
HTBOTTOMLEFT HTBOTTOM HTBOTTOMRIGHT
```

Use touch-friendly hit margins while ensuring they do not steal input from the page strip.

---

# 13. Hit-test order

Important priority:

```text
interactive QML control region -> HTCLIENT
resize border                  -> resize HT code
empty drag header              -> HTCAPTION
native content area            -> HTCLIENT
```

Do not return `HTCAPTION` over:

```text
close button
pin button
page strip
menu button
other interactive QML controls
```

Otherwise Windows will drag instead of delivering clicks.

---

# 14. DPI-aware hit testing

Do not blindly compare native coordinates against hard-coded physical `32`.

Keep QML geometry and `WM_NCHITTEST` coordinates in one consistent coordinate system.

Test at:

```text
100%
125%
150%
```

and while moving between differently scaled monitors.

Avoid multiplying by DPR twice.

---

# 15. Where to implement native hit testing

Prefer a StageManager-specific native event filter/helper.

Possible design:

```cpp
class StageNativeEventFilter : public QAbstractNativeEventFilter
{
public:
    bool nativeEventFilter(const QByteArray& eventType,
                           void* message,
                           qintptr* result) override;
};
```

Maintain:

```text
StageContainer HWND -> controller/container metadata
```

When `msg->message == WM_NCHITTEST` and the HWND belongs to Stage Manager, return the appropriate custom hit result.

Do not pollute unrelated global Win32 helpers with Stage-specific logic unless that already matches project architecture.

---

# 16. Combined ownership rule after these fixes

```text
Qt Quick owns:
    visual chrome
    bottom page strip
    labels/icons
    ordinary non-overlapping controls

Win32 owns:
    StageContainer HWND
    move/resize hit testing
    native content host HWND
    reparented app HWND hierarchy
    target lifecycle

Native top-level popup owns:
    tooltip/menu/HUD that must overlap hosted app HWND
```

This division is intentional.

---

# 17. Files likely involved

Exact names may have changed after the refactor; grep before editing.

Expected C++ areas:

```text
src_plugin/StageManager/
    StageManagerService.*
    StageContainerController.*
    WindowEmbedder.*
    WindowRegistry.*
    LiveWindowManager.*
```

Expected QML:

```text
src_ui/plugin/components/
    StageManager.qml
    stagemanager/
        StageContainer.qml
        StagePageStrip.qml
        StageStrip.qml
```

Search:

```text
NULL_BRUSH
hbrBackground
CreateWindowEx
WM_PAINT
SetParent
WM_CLOSE
PostMessage
isManaged
EVENT_OBJECT_HIDE
EVENT_OBJECT_DESTROY
EVENT_OBJECT_NAMECHANGE
startSystemMove
startSystemResize
MouseArea
ToolTip
Popup
popupType
```

---

# 18. Logging additions

## Close lifecycle

Log:

```text
close requested
restore started
restore result
registry removed
WM_CLOSE posted
DESTROY received
HIDE received
same HWND reappeared
```

Always include:

```text
HWND
PID
TID
PageId
ContainerId
```

## Composition

On attach log:

```text
original style
original exStyle
WS_EX_NOREDIRECTIONBITMAP present?
DWM backdrop query result
host background mode
```

## Hit testing

Debug-only optional log:

```text
WM_NCHITTEST
screen point
client point
returned HT code
```

Do not leave high-frequency hit-test logging enabled by default.

---

# 19. Regression tests

## 19.1 Close/reopen

For each:

```text
Notepad
Explorer
Edge/Chrome
VS Code
```

Test:

```text
add to Stage
close via Stage UI
reopen application
verify window appears normally
verify StageStrip detects it
add again
repeat 20 times
```

Also test an application with unsaved state:

```text
close -> Save dialog -> Cancel
```

Expected:

```text
app remains a normal desktop top-level window
not stuck as a Stage child
```

## 19.2 Release vs Close

```text
Release:
app stays alive and returns to desktop

Close:
app first returns to normal top-level state,
then receives WM_CLOSE
```

No stale Stage page after either operation.

## 19.3 Explorer backdrop

Test Windows light and dark mode:

```text
attach Explorer
switch away/back
resize
maximize Stage
release
```

Expected:

```text
no transparent desktop-through background
no garbage backing surface
normal appearance after restore
```

## 19.4 Tooltip

Test tooltip/popups overlapping:

```text
Explorer
Chrome
VS Code
```

Expected: popup remains visible above hosted HWND whether Stage is pinned or not.

## 19.5 Real touchscreen drag

Using a real touchscreen:

```text
finger down on white-bar/header
move
release
```

Expected:

```text
moves immediately
tracks finger
Windows Snap still works
```

Mouse must remain functional.

## 19.6 Touch resize

If resize is supported, test right edge and bottom-right corner with a finger.

## 19.7 DPI

Repeat move/resize at:

```text
100%
125%
150%
```

and across two monitors with different scaling.

---

# 20. Do not regress the architecture

While fixing these issues:

- do not create one Qt top-level window per page again;
- do not reparent on every page switch;
- do not replace native hosting with screenshots;
- do not add input forwarding;
- do not use global hooks to move StageContainer;
- do not inject into external processes;
- do not use `z: 999999` as a native airspace fix;
- do not send `WM_CLOSE` while ownership cleanup is incomplete;
- do not trust a stale raw HWND forever;
- do not make every embedded app topmost;
- do not keep a transparent native host behind composition-heavy windows.

---

# 21. Suggested commit sequence

## Commit 1 — Close lifecycle

Implement:

```text
restore/release before WM_CLOSE
registry identity validation
NAMECHANGE rediscovery
DESTROY cleanup
```

Test close/reopen repeatedly.

## Commit 2 — Opaque native host

Implement:

```text
real host WndProc
WM_PAINT / WM_ERASEBKGND
theme-aware opaque backing
```

Test Explorer.

## Commit 3 — DWM compatibility

Only if needed:

```text
capture system backdrop
DWMSBT_NONE while hosted
restore original backdrop
log WS_EX_NOREDIRECTIONBITMAP
```

## Commit 4 — Native popup / tooltip

Convert overlapping ToolTip/Popup paths to native/top-level popup behavior, preferably `Popup.Window` when supported by the project Qt version.

## Commit 5 — Native move/resize

Implement:

```text
WM_NCHITTEST
HTCAPTION
resize HT codes
```

Remove redundant QML `startSystemMove/startSystemResize` code after the native path is verified.

---

# 22. Acceptance criteria

## Lifecycle

- [ ] Close from Stage UI does not leave the target parented to content host.
- [ ] Close removes Stage managed ownership before `WM_CLOSE`.
- [ ] Reopening the same application displays normally.
- [ ] Reopened app can appear in StageStrip again.
- [ ] Repeat close/reopen works many times.
- [ ] App canceling `WM_CLOSE` remains usable as a normal desktop window.
- [ ] No stale HWND blocks later windows.

## Explorer / composition

- [ ] Explorer no longer shows transparent background in light mode.
- [ ] Explorer no longer shows transparent background in dark mode.
- [ ] Native content host paints an opaque backing.
- [ ] Restore returns normal Explorer appearance.
- [ ] No global `WS_EX_LAYERED` workaround added.

## Popup

- [ ] Stage tooltip can display above embedded Explorer/Chrome/VS Code.
- [ ] QML `z` is not relied on to cross the native HWND boundary.
- [ ] Other overlapping popups follow the same native-popup rule.

## Touch

- [ ] Mouse drag works.
- [ ] Real touchscreen drag works.
- [ ] Windows Snap still works.
- [ ] Resize uses native hit testing if supported.
- [ ] Buttons/page strip are not misclassified as `HTCAPTION`.

---

# 23. Final rule for the coding agent

These four bugs share one root fact:

> Stage Manager is now a hybrid Qt Quick + Win32 native HWND application.

Fix each problem at its correct layer:

```text
Explorer background
    -> native HWND / DWM layer

Tooltip airspace
    -> native popup layer

Close/reopen
    -> HWND lifecycle / registry layer

Touch drag
    -> native non-client hit-test layer
```

Do not try to solve all four in QML.

Do not try to solve all four in `WindowEmbedder`.

The current architecture should stay intact; this round is about making the Qt/native boundary robust.
