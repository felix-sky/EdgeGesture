#include "SafeWindowReparenter.h"
#include <QDebug>
#include <QQuickWindow>
#include <commctrl.h>
#include <dwmapi.h>
#include <math.h>
#include <mutex>
#include <tchar.h>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Dwmapi.lib")

SafeWindowReparenter *SafeWindowReparenter::s_instance = nullptr;

static const TCHAR *CONTAINER_CLASS_NAME = _T("EdgeGesture_StageContainer");

static const UINT_PTR SUBCLASS_ID = 12345;
LRESULT CALLBACK LockTargetWndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                   LPARAM lParam, UINT_PTR uIdSubclass,
                                   DWORD_PTR dwRefData);

SafeWindowReparenter *SafeWindowReparenter::instance() { return s_instance; }

SafeWindowReparenter::SafeWindowReparenter(QObject *parent) : QObject(parent) {
  s_instance = this;
  // Registration done lazily or in constructor
}

SafeWindowReparenter::~SafeWindowReparenter() {
  auto keys = m_managedWindows.keys();
  for (HWND hwnd : std::as_const(keys)) {
    restoreWindow((qint64)hwnd);
  }
  s_instance = nullptr;
}

bool RegisterContainerClass() {
  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.lpfnWndProc = DefWindowProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.lpszClassName = CONTAINER_CLASS_NAME;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.hbrBackground = (HBRUSH)GetStockObject(
      NULL_BRUSH); // Fix: Was BLACK_BRUSH, masking content
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);

  ATOM atom = RegisterClassEx(&wc);
  if (!atom) {
    DWORD err = GetLastError();
    if (err == ERROR_CLASS_ALREADY_EXISTS)
      return true;
    qWarning() << "RegisterClassEx failed. Error:" << err;
    return false;
  }
  return true;
}

bool SafeWindowReparenter::reparentWindow(qint64 hwndVal,
                                          QQuickItem *capsuleItem) {
  static std::once_flag flag;
  std::call_once(flag, []() { RegisterContainerClass(); });

  if (!capsuleItem || !capsuleItem->window())
    return false;
  HWND targetHwnd = (HWND)hwndVal;
  if (!IsWindow(targetHwnd))
    return false;
  if (m_managedWindows.contains(targetHwnd))
    return true;

  // Get Qt Window
  QQuickWindow *qWin = capsuleItem->window();
  HWND qtHwnd = (HWND)qWin->winId();
  if (!qtHwnd)
    return false;

  WindowState state;
  state.hwnd = targetHwnd;
  state.originalParent = GetParent(targetHwnd);
  state.originalStyle = GetWindowLongPtr(targetHwnd, GWL_STYLE);
  state.originalExStyle = GetWindowLongPtr(targetHwnd, GWL_EXSTYLE);
  GetWindowRect(targetHwnd, &state.originalRect);
  state.isReparented = true;

  //  Intermediate Container
  // Fix: Create with initial size 1x1 or actual size to avoid 100x100 flicker?
  // We'll trust updateWindowPosition to resize it immediately.
  state.containerHwnd = CreateWindowEx(
      0, CONTAINER_CLASS_NAME, _T(""), WS_CHILD | WS_CLIPCHILDREN, 0, 0, 100,
      100, qtHwnd, NULL, GetModuleHandle(NULL), NULL);

  if (!state.containerHwnd) {
    DWORD err = GetLastError();
    qWarning() << "Failed to create container window. Error:" << err;
    return false;
  }

  SetWindowLongPtr(state.containerHwnd, GWLP_USERDATA, (LONG_PTR)this);

  // Prepare Target Window
  // Get title bar height for cropping
  // Use SM_CYFRAME * 2 if WS_THICKFRAME was there, or just standard caption
  // metrics
  state.titleBarHeight = GetSystemMetrics(SM_CYCAPTION) +
                         GetSystemMetrics(SM_CYFRAME) * 2 +
                         20; // Added 20px extra crop

  LONG_PTR style = state.originalStyle;
  style &= ~(WS_THICKFRAME | WS_POPUP | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
             WS_SYSMENU);

  style |= WS_CHILD;
  SetWindowLongPtr(targetHwnd, GWL_STYLE, style);

  SetParent(targetHwnd, state.containerHwnd);
  SetWindowSubclass(targetHwnd, LockTargetWndProc, SUBCLASS_ID,
                    (DWORD_PTR)this);

  // Store State
  m_managedWindows.insert(targetHwnd, state);

  updateWindowPosition(hwndVal, capsuleItem);
  ShowWindow(targetHwnd, SW_SHOW);

  return true;
}

bool SafeWindowReparenter::restoreWindow(qint64 hwndVal) {
  HWND hwnd = (HWND)hwndVal;
  if (!m_managedWindows.contains(hwnd))
    return false;

  WindowState state = m_managedWindows.take(hwnd);

  // Remove Hook first
  RemoveWindowSubclass(hwnd, LockTargetWndProc, SUBCLASS_ID);

  // Hide to avoid glitch
  ShowWindow(hwnd, SW_HIDE);

  // Restore Parent
  // Restore Parent
  HWND parentToRestore = state.originalParent ? state.originalParent : NULL;
  if (!SetParent(hwnd, parentToRestore)) {
    qWarning()
        << "SetParent failed during restore. Fallback to NULL (Desktop). error:"
        << GetLastError();
    SetParent(hwnd, NULL); // Fallback
  }

  // Restore Styles
  SetWindowLongPtr(hwnd, GWL_STYLE, state.originalStyle);
  SetWindowLongPtr(hwnd, GWL_EXSTYLE, state.originalExStyle);

  // Restore Rect
  SetWindowPos(hwnd, NULL, state.originalRect.left, state.originalRect.top,
               state.originalRect.right - state.originalRect.left,
               state.originalRect.bottom - state.originalRect.top,
               SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

  // Force style update
  SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                   SWP_FRAMECHANGED);

  ShowWindow(hwnd, SW_SHOW);

  if (state.containerHwnd && IsWindow(state.containerHwnd)) {
    DestroyWindow(state.containerHwnd);
  }
  return true;
}

void SafeWindowReparenter::updateWindowPosition(qint64 hwndVal,
                                                QQuickItem *item) {
  HWND targetHwnd = (HWND)hwndVal;
  if (!m_managedWindows.contains(targetHwnd))
    return;
  WindowState &state = m_managedWindows[targetHwnd];

  if (!item || !item->window())
    return;

  // Coordinate Conversion
  qreal dpr = item->window()->effectiveDevicePixelRatio();
  QPointF pos = item->mapToScene(QPointF(0, 0));

  int x = (int)round(pos.x() * dpr);
  int y = (int)round(pos.y() * dpr);
  int w = (int)round(item->width() * dpr);
  int h = (int)round(item->height() * dpr);

  // 1. Move Container
  // Add SWP_SHOWWINDOW to ensure container is visible
  SetWindowPos(state.containerHwnd, NULL, x, y, w, h,
               SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

  // 2. Move Target inside Container (Cropping)
  // Add SWP_SHOWWINDOW and SWP_FRAMECHANGED to force redraw
  SetWindowPos(
      targetHwnd, NULL, 0, -state.titleBarHeight, w, h + state.titleBarHeight,
      SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);

  // Force redraw of target
  RedrawWindow(targetHwnd, NULL, NULL,
               RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

void SafeWindowReparenter::makeWindowClickThrough(qint64 hwndVal) {
  HWND hwnd = (HWND)hwndVal;
  if (!IsWindow(hwnd))
    return;

  // Add WS_EX_TRANSPARENT and WS_EX_LAYERED to allow clicks to pass through
  // transparent areas
  LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
  exStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT;
  SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

  // Make the window 100% opaque but with layered attributes for transparency
  // hit-testing
  SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

  qDebug() << "Made window click-through:" << hwnd;
}

void SafeWindowReparenter::applyWin11RoundedCorners(QQuickWindow *window) {
  if (!window)
    return;

  HWND hwnd = (HWND)window->winId();
  if (!IsWindow(hwnd))
    return;

  // Windows 11 Build 22000+ supports this attribute
  // DWMWCP_ROUND (2) = Standard rounded corners (for larger windows)
  // DWMWCP_ROUNDSMALL (3) = Small rounded corners (for tool windows)
  const DWORD DWMWCP_ROUND = 2;
  const DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;

  DWORD preference = DWMWCP_ROUND;

  HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                                     &preference, sizeof(preference));

  if (FAILED(hr)) {
    qWarning() << "Failed to set Win11 rounded corners, hr:" << hr;
  } else {
    qDebug() << "Applied Win11 rounded corners to window:" << hwnd;
  }
}

void SafeWindowReparenter::batchMoveWindows(const QList<int> &hwnds, int dx,
                                            int dy) {
  if (hwnds.isEmpty())
    return;

  // Use DeferWindowPos for atomic movement of multiple windows
  HDWP hdwp = BeginDeferWindowPos(hwnds.size());
  if (!hdwp)
    return;

  for (int hwndVal : hwnds) {
    HWND hwnd = (HWND)(qint64)hwndVal;
    if (IsWindow(hwnd)) {
      RECT rect;
      if (GetWindowRect(hwnd, &rect)) {
        hdwp = DeferWindowPos(hdwp, hwnd, NULL, rect.left + dx, rect.top + dy,
                              0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
      }
    }
  }

  EndDeferWindowPos(hdwp);
}

// Subclass Proc
LRESULT CALLBACK LockTargetWndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                   LPARAM lParam, UINT_PTR uIdSubclass,
                                   DWORD_PTR dwRefData) {

  switch (uMsg) {
  case WM_WINDOWPOSCHANGING: {

    break;
  }
  case WM_NCHITTEST: {
    LRESULT hit = DefSubclassProc(hWnd, uMsg, wParam, lParam);
    // Fool the app into thinking caption is client
    if (hit == HTCAPTION || hit == HTTOP || hit == HTBOTTOM || hit == HTLEFT ||
        hit == HTRIGHT) {
      return HTCLIENT;
    }
    return hit;
  }
  case WM_SETFOCUS: {
    SafeWindowReparenter *self = (SafeWindowReparenter *)dwRefData;
    if (self)
      self->notifyWindowActiveState(hWnd, true);
    break;
  }
  case WM_KILLFOCUS: {
    SafeWindowReparenter *self = (SafeWindowReparenter *)dwRefData;
    if (self)
      self->notifyWindowActiveState(hWnd, false);
    break;
  }
  case WM_NCDESTROY:
    RemoveWindowSubclass(hWnd, LockTargetWndProc, uIdSubclass);
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
  }
  return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void SafeWindowReparenter::stripWindowStyles(HWND hwnd, WindowState &state) {}
void SafeWindowReparenter::restoreWindowStyles(HWND hwnd,
                                               const WindowState &state) {}
HWND SafeWindowReparenter::createContainerWindow(HWND parentHwnd) {
  return NULL;
}
void SafeWindowReparenter::onTargetDestroyed(HWND container, HWND target) {
  if (m_managedWindows.contains(target)) {
    m_managedWindows.remove(target);
    emit windowDestroyed((qint64)target);
    if (IsWindow(container))
      DestroyWindow(container);
  }
}
LRESULT CALLBACK SafeWindowReparenter::ContainerWndProc(HWND hwnd, UINT uMsg,
                                                        WPARAM wParam,
                                                        LPARAM lParam) {
  // Detect child death via WM_PARENTNOTIFY
  if (uMsg == WM_PARENTNOTIFY && LOWORD(wParam) == WM_DESTROY) {
    // Notify zombie
    SafeWindowReparenter *self = SafeWindowReparenter::instance();
    if (self) {
      self->onTargetDestroyed(hwnd, (HWND)lParam);
    }
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
