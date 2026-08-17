#pragma once

#include <QString>
#include <QtGlobal>
#include <dwmapi.h>
#include <windows.h>

using ContainerId = quint64;
using PageId = quint64;

enum class PageState {
  Candidate,
  Attaching,
  AttachedInactive,
  AttachedActive,
  Restoring,
  Detached,
  Destroyed,
  Failed
};

struct WindowIdentity {
  HWND hwnd{nullptr};
  DWORD pid{0};
  DWORD tid{0};
  QString title;
  QString className;
};

struct OriginalWindowState {
  LONG_PTR style{0};
  LONG_PTR exStyle{0};
  RECT rect{0, 0, 0, 0};
  WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
  HWND owner{nullptr};
  bool visible{false};
  bool iconic{false};
  bool zoomed{false};
  bool topMost{false};
  DPI_AWARENESS_CONTEXT dpiContext{nullptr};
  DWM_SYSTEMBACKDROP_TYPE backdropType{DWMSBT_NONE};
  bool hasBackdropState{false};
};

struct ManagedEntry {
  PageId pageId{0};
  ContainerId containerId{0};
  DWORD pid{0};
  DWORD tid{0};
};

struct PageRecord {
  PageId id{0};
  ContainerId containerId{0};
  WindowIdentity identity;
  OriginalWindowState original;
  PageState state{PageState::Candidate};
  QString title;
};
