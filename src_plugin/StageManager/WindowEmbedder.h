#pragma once

#include "StageManagerTypes.h"
#include <dwmapi.h>
#include <windows.h>

class WindowEmbedder {
public:
  static bool isCandidate(HWND hwnd, DWORD myPid);
  static bool captureOriginalState(HWND hwnd, OriginalWindowState &outState);
  static bool getWindowIdentity(HWND hwnd, WindowIdentity &outIdentity);

  static bool attachTarget(HWND target, HWND host,
                           const OriginalWindowState &original);
  static bool restoreTarget(HWND target, const OriginalWindowState &original);

  static void setTargetVisible(HWND target, bool visible, const RECT &hostRect);
  static void resizeTarget(HWND target, const RECT &hostRect);
  static void focusTarget(HWND target, HWND stageHwnd);
};
