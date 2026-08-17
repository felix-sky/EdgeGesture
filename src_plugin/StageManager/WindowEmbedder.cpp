#include "WindowEmbedder.h"
#include <QDebug>
#include <vector>

bool WindowEmbedder::getWindowIdentity(HWND hwnd, WindowIdentity &outIdentity) {
  if (!IsWindow(hwnd))
    return false;

  outIdentity.hwnd = hwnd;
  outIdentity.tid = GetWindowThreadProcessId(hwnd, &outIdentity.pid);

  int len = GetWindowTextLengthW(hwnd);
  if (len > 0) {
    std::vector<wchar_t> buf(len + 1);
    GetWindowTextW(hwnd, buf.data(), len + 1);
    outIdentity.title = QString::fromWCharArray(buf.data());
  }

  wchar_t clsBuf[256];
  if (GetClassNameW(hwnd, clsBuf, 256) > 0) {
    outIdentity.className = QString::fromWCharArray(clsBuf);
  }

  return true;
}

bool WindowEmbedder::isCandidate(HWND hwnd, DWORD myPid) {
  if (!IsWindow(hwnd) || !IsWindowVisible(hwnd))
    return false;

  // Must be a top-level root window
  if (GetAncestor(hwnd, GA_ROOT) != hwnd)
    return false;

  // Must have a title
  if (GetWindowTextLengthW(hwnd) == 0)
    return false;

  // Exclude tool windows
  LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
  if (exStyle & WS_EX_TOOLWINDOW)
    return false;

  // Exclude our own process
  DWORD processId = 0;
  GetWindowThreadProcessId(hwnd, &processId);
  if (processId == 0 || processId == myPid)
    return false;

  // Exclude cloaked windows (virtual desktop ghosts / modern app suspensions)
  int cloaked = 0;
  HRESULT hr =
      DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
  if (SUCCEEDED(hr) && cloaked != 0)
    return false;

  // Exclude desktop and shell windows
  wchar_t className[256];
  if (GetClassNameW(hwnd, className, 256) > 0) {
    if (wcscmp(className, L"Progman") == 0 ||
        wcscmp(className, L"WorkerW") == 0 ||
        wcscmp(className, L"Shell_TrayWnd") == 0 ||
        wcscmp(className, L"Shell_SecondaryTrayWnd") == 0) {
      return false;
    }
  }

  return true;
}

bool WindowEmbedder::captureOriginalState(HWND hwnd,
                                          OriginalWindowState &outState) {
  if (!IsWindow(hwnd))
    return false;

  outState.style = GetWindowLongPtrW(hwnd, GWL_STYLE);
  outState.exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
  GetWindowRect(hwnd, &outState.rect);

  outState.placement.length = sizeof(WINDOWPLACEMENT);
  GetWindowPlacement(hwnd, &outState.placement);

  outState.owner = GetWindow(hwnd, GW_OWNER);
  outState.visible = (IsWindowVisible(hwnd) != FALSE);
  outState.iconic = (IsIconic(hwnd) != FALSE);
  outState.zoomed = (IsZoomed(hwnd) != FALSE);
  outState.topMost = ((outState.exStyle & WS_EX_TOPMOST) != 0);

  outState.dpiContext = GetWindowDpiAwarenessContext(hwnd);

  return true;
}

bool WindowEmbedder::attachTarget(HWND target, HWND host,
                                  const OriginalWindowState &original) {
  if (!IsWindow(target) || !IsWindow(host)) {
    qWarning() << "[StageManager] attachTarget: Invalid target or host HWND";
    return false;
  }

  // 1. Hide target during modification to prevent visual glitches
  ShowWindow(target, SW_HIDE);

  // 2. Prepare child styles
  LONG_PTR newStyle = original.style;
  newStyle &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
                WS_MAXIMIZEBOX | WS_SYSMENU);
  newStyle |= (WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

  SetLastError(0);
  LONG_PTR prevStyle = SetWindowLongPtrW(target, GWL_STYLE, newStyle);
  if (prevStyle == 0 && GetLastError() != 0) {
    DWORD err = GetLastError();
    qWarning() << "[StageManager] attachTarget: SetWindowLongPtr GWL_STYLE "
                  "failed, error:"
               << err;
    restoreTarget(target, original);
    return false;
  }

  // Remove WS_EX_TOPMOST while hosted
  LONG_PTR newExStyle = original.exStyle & ~WS_EX_TOPMOST;
  SetLastError(0);
  LONG_PTR prevExStyle = SetWindowLongPtrW(target, GWL_EXSTYLE, newExStyle);
  if (prevExStyle == 0 && GetLastError() != 0) {
    DWORD err = GetLastError();
    qWarning() << "[StageManager] attachTarget: SetWindowLongPtr GWL_EXSTYLE "
                  "failed, error:"
               << err;
    restoreTarget(target, original);
    return false;
  }

  // 3. SetParent to content host HWND
  SetLastError(0);
  HWND prevParent = SetParent(target, host);
  DWORD parentErr = GetLastError();
  if (!prevParent && parentErr != 0) {
    qWarning() << "[StageManager] attachTarget: SetParent failed, error:"
               << parentErr;
    restoreTarget(target, original);
    return false;
  }

  if (GetParent(target) != host) {
    qWarning() << "[StageManager] attachTarget: Parent verification failed";
    restoreTarget(target, original);
    return false;
  }

  // 4. Force frame change
  SetWindowPos(target, NULL, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                   SWP_FRAMECHANGED);

  // 5. Size to content host
  RECT rcHost;
  if (GetClientRect(host, &rcHost)) {
    int w = rcHost.right - rcHost.left;
    int h = rcHost.bottom - rcHost.top;
    SetWindowPos(target, NULL, 0, 0, w, h,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  }

  qDebug() << "[StageManager] attachTarget succeeded for HWND:" << target;
  return true;
}

bool WindowEmbedder::restoreTarget(HWND target,
                                   const OriginalWindowState &original) {
  if (!IsWindow(target)) {
    qDebug() << "[StageManager] restoreTarget: Target HWND no longer exists:"
             << target;
    return true;
  }

  // 1. Hide target during restoration
  ShowWindow(target, SW_HIDE);

  // 2. Restore parent to original owner or desktop
  HWND parentToRestore = original.owner ? original.owner : NULL;
  SetLastError(0);
  SetParent(target, parentToRestore);

  // 3. Restore style and exStyle
  SetLastError(0);
  SetWindowLongPtrW(target, GWL_STYLE, original.style);
  SetWindowLongPtrW(target, GWL_EXSTYLE, original.exStyle);

  // 4. Frame change
  SetWindowPos(target, NULL, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                   SWP_FRAMECHANGED);

  // 5. Restore placement and geometry
  SetWindowPlacement(target, &original.placement);

  // 6. Restore show state
  if (original.visible) {
    if (original.iconic) {
      ShowWindow(target, SW_MINIMIZE);
    } else if (original.zoomed) {
      ShowWindow(target, SW_MAXIMIZE);
    } else {
      ShowWindow(target, SW_SHOWNA);
    }
  } else {
    ShowWindow(target, SW_HIDE);
  }

  // 7. Re-apply topmost if it originally was topmost
  if (original.topMost) {
    SetWindowPos(target, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }

  qDebug() << "[StageManager] restoreTarget finished for HWND:" << target;
  return true;
}

void WindowEmbedder::setTargetVisible(HWND target, bool visible,
                                      const RECT &hostRect) {
  if (!IsWindow(target))
    return;

  if (visible) {
    int w = hostRect.right - hostRect.left;
    int h = hostRect.bottom - hostRect.top;
    SetWindowPos(target, NULL, 0, 0, w, h,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW |
                     SWP_FRAMECHANGED);
  } else {
    ShowWindow(target, SW_HIDE);
  }
}

void WindowEmbedder::resizeTarget(HWND target, const RECT &hostRect) {
  if (!IsWindow(target))
    return;

  int w = hostRect.right - hostRect.left;
  int h = hostRect.bottom - hostRect.top;
  SetWindowPos(target, NULL, 0, 0, w, h,
               SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void WindowEmbedder::focusTarget(HWND target, HWND stageHwnd) {
  if (!IsWindow(target))
    return;

  if (stageHwnd && IsWindow(stageHwnd)) {
    SetForegroundWindow(stageHwnd);
  }

  DWORD targetTid = GetWindowThreadProcessId(target, NULL);
  DWORD currentTid = GetCurrentThreadId();

  if (targetTid != 0 && targetTid != currentTid) {
    AttachThreadInput(currentTid, targetTid, TRUE);
    SetFocus(target);
    AttachThreadInput(currentTid, targetTid, FALSE);
  } else {
    SetFocus(target);
  }
}
