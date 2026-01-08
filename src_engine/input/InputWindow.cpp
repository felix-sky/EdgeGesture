#include "InputWindow.h"
#include "core/ConfigManager.h"
#include <iostream>

#define WINDOW_CLASS_NAME L"OHOInputOverlay"

InputWindow::InputWindow() {}

InputWindow::~InputWindow() { Shutdown(); }

void InputWindow::Initialize() {
  WNDCLASSEXW wcex = {sizeof(WNDCLASSEX)};
  wcex.lpfnWndProc = WndProc;
  wcex.hInstance = GetModuleHandle(NULL);
  wcex.lpszClassName = WINDOW_CLASS_NAME;
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  RegisterClassExW(&wcex);

  CreateWindows();
  UpdateLayout();

  // Start watchdog timer to restore windows if hidden by system/win+D
  if (m_hwndLeft) {
    SetTimer(m_hwndLeft, TIMER_WATCHDOG, WATCHDOG_INTERVAL_MS, nullptr);
  }
}

void InputWindow::Shutdown() {
  if (m_hwndLeft) {
    DestroyWindow(m_hwndLeft);
    m_hwndLeft = nullptr;
  }
  if (m_hwndRight) {
    DestroyWindow(m_hwndRight);
    m_hwndRight = nullptr;
  }
  UnregisterClassW(WINDOW_CLASS_NAME, GetModuleHandle(NULL));
}

void InputWindow::SetCallbacks(ZoneStateCallback onZone,
                               GestureStartCallback onStart,
                               GestureUpdateCallback onUpdate,
                               GestureEndCallback onEnd) {
  OnZoneState = onZone;
  OnStart = onStart;
  OnUpdate = onUpdate;
  OnEnd = onEnd;
}

void InputWindow::CreateWindows() {
  // Create Left Window
  m_hwndLeft = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
      WINDOW_CLASS_NAME, L"OHO_Left",
      WS_POPUP, // Popup, no borders
      0, 0, 0, 0, nullptr, nullptr, GetModuleHandle(NULL), this);

  // Create Right Window
  m_hwndRight = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
      WINDOW_CLASS_NAME, L"OHO_Right", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
      GetModuleHandle(NULL), this);

  // Core logic is to create a visually transparent window that captures input
  // however 0 will cause the window to disappear
  if (m_hwndLeft)
    SetLayeredWindowAttributes(m_hwndLeft, 0, 1, LWA_ALPHA);
  if (m_hwndRight)
    SetLayeredWindowAttributes(m_hwndRight, 0, 1, LWA_ALPHA);
}

void InputWindow::UpdateLayout() {
  int screenW = GetSystemMetrics(SM_CXSCREEN);
  int screenH = GetSystemMetrics(SM_CYSCREEN);

  AppConfig &cfg = ConfigManager::Get().Current();

  BYTE alpha = m_previewMode ? 100 : 1;

  // Left Logic
  if (m_hwndLeft) {
    if (cfg.left.enabled) {
      int h = (int)((cfg.left.size / 100.0f) * screenH);
      int top = (int)((cfg.left.position / 100.0f) * screenH) - (h / 2);

      // Clamp
      if (top < 0)
        top = 0;
      if (top + h > screenH)
        top = screenH - h;

      MoveWindow(m_hwndLeft, 0, top, cfg.left.width, h, TRUE);
      SetLayeredWindowAttributes(
          m_hwndLeft, 0, (m_previewMode == 1 || m_previewMode == 3) ? 128 : 1,
          LWA_ALPHA);
      ShowWindow(m_hwndLeft, SW_SHOWNOACTIVATE);
    } else {
      ShowWindow(m_hwndLeft, SW_HIDE);
    }
  }

  // Right Logic
  if (m_hwndRight) {
    if (cfg.right.enabled) {
      int h = (int)((cfg.right.size / 100.0f) * screenH);
      int top = (int)((cfg.right.position / 100.0f) * screenH) - (h / 2);

      if (top < 0)
        top = 0;
      if (top + h > screenH)
        top = screenH - h;

      MoveWindow(m_hwndRight, screenW - cfg.right.width, top, cfg.right.width,
                 h, TRUE);
      SetLayeredWindowAttributes(
          m_hwndRight, 0, (m_previewMode == 2 || m_previewMode == 3) ? 128 : 1,
          LWA_ALPHA);
      ShowWindow(m_hwndRight, SW_SHOWNOACTIVATE);
    } else {
      ShowWindow(m_hwndRight, SW_HIDE);
    }
  }
}

bool InputWindow::IsTouchInput() {
  ULONG_PTR extraInfo = GetMessageExtraInfo();
  // Common signature for Pen/Touch input vs Mouse injection
  if ((extraInfo & 0xFF515700) == 0xFF515700) {
    return true;
  }

  return false;
}

LRESULT CALLBACK InputWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam,
                                      LPARAM lParam) {
  InputWindow *pThis = nullptr;

  if (message == WM_NCCREATE) {
    CREATESTRUCT *pCreate = (CREATESTRUCT *)lParam;
    pThis = (InputWindow *)pCreate->lpCreateParams;
    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
  } else {
    pThis = (InputWindow *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
  }

  if (pThis) {
    switch (message) {

    // --- 1. Gesture Initiation (The "Fast Action") ---
    case WM_LBUTTONDOWN: {
      // If the user clicks immediately, we cancel any suppression logic
      if (pThis->m_isHovering) {
        KillTimer(hWnd, TIMER_HOVER_CHECK);
        pThis->m_isHovering = false;
      }

      pThis->m_isDragging = true;
      SetCapture(hWnd);

      bool isLeft = (hWnd == pThis->m_hwndLeft);
      POINT pt;
      pt.x = (short)LOWORD(lParam);
      pt.y = (short)HIWORD(lParam);
      ClientToScreen(hWnd, &pt);

      if (pThis->OnStart) {
        pThis->OnStart(isLeft, pt.y);
      }
      return 0;
    }

    //  Hover Detection、
    case WM_MOUSEMOVE: {
      if (pThis->m_isDragging) {
        // Standard Dragging Logic
        POINT pt;
        pt.x = (short)LOWORD(lParam);
        pt.y = (short)HIWORD(lParam);
        ClientToScreen(hWnd, &pt);
        if (pThis->OnUpdate)
          pThis->OnUpdate(pt.x, pt.y);
      } else {
        // Logic: User is hovering but hasn't clicked yet
        if (!pThis->m_isHovering) {
          pThis->m_isHovering = true;

          // Request notification when mouse leaves this window
          TRACKMOUSEEVENT tme;
          tme.cbSize = sizeof(TRACKMOUSEEVENT);
          tme.dwFlags = TME_LEAVE;
          tme.hwndTrack = hWnd;
          TrackMouseEvent(&tme);

          // Start the countdown: If user doesn't click in X ms, hide window
          SetTimer(hWnd, TIMER_HOVER_CHECK, pThis->HOVER_THRESHOLD_MS, nullptr);
        }
      }
      break;
    }

    // --- 3. Mouse Left the Window (Cancel Check) ---
    case WM_MOUSELEAVE: {
      pThis->m_isHovering = false;
      KillTimer(hWnd, TIMER_HOVER_CHECK);
      break;
    }

    // --- 4. Timer Logic (Suppression & Recovery) ---
    case WM_TIMER: {
      if (wParam == TIMER_HOVER_CHECK) {
        // Timeout reached: User is staring at the edge/aiming for scrollbar
        KillTimer(hWnd, TIMER_HOVER_CHECK);
        pThis->m_isHovering = false;

        // ACTION: Suppress the window (Hide it)
        ShowWindow(hWnd, SW_HIDE);
        pThis->m_isSuppressed = true;

        // Schedule Recovery
        SetTimer(hWnd, TIMER_RECOVERY, pThis->RECOVERY_DELAY_MS, nullptr);

        std::cout << "[InputWindow] Auto-yield triggered (Suppressed)"
                  << std::endl;
      } else if (wParam == TIMER_RECOVERY) {
        // Timeout reached: Bring the window back
        KillTimer(hWnd, TIMER_RECOVERY);
        pThis->m_isSuppressed = false;

        // Restore visibility (No Activate to prevent stealing focus)
        ShowWindow(hWnd, SW_SHOWNOACTIVATE);

        std::cout << "[InputWindow] Window recovered" << std::endl;
      } else if (wParam == TIMER_WATCHDOG) {
        // Watchdog: Check if windows are unexpectedly hidden (e.g., by Win+D)
        // Only restore if not intentionally suppressed by hover logic
        AppConfig &cfg = ConfigManager::Get().Current();

        if (!pThis->m_isSuppressed) {
          if (pThis->m_hwndLeft && cfg.left.enabled &&
              !IsWindowVisible(pThis->m_hwndLeft)) {
            ShowWindow(pThis->m_hwndLeft, SW_SHOWNOACTIVATE);
            std::cout << "[InputWindow] Watchdog restored left window"
                      << std::endl;
          }
          if (pThis->m_hwndRight && cfg.right.enabled &&
              !IsWindowVisible(pThis->m_hwndRight)) {
            ShowWindow(pThis->m_hwndRight, SW_SHOWNOACTIVATE);
            std::cout << "[InputWindow] Watchdog restored right window"
                      << std::endl;
          }
        }
      }
      break;
    }

    case WM_LBUTTONUP: {
      if (pThis->m_isDragging) {
        pThis->m_isDragging = false;
        ReleaseCapture();
        if (pThis->OnEnd) {
          pThis->OnEnd();
        }
      }
      break;
    }

    case WM_DISPLAYCHANGE: {
      pThis->UpdateLayout();
      break;
    }
    case WM_USER + 101: {
      ConfigManager::Get().Load();
      pThis->UpdateLayout();
      break;
    }
    case WM_USER + 102: {
      // Preview Mode: wParam = 0 (Off), 1 (Left), 2 (Right)
      pThis->m_previewMode = (int)wParam;
      pThis->UpdateLayout();
      break;
    }
    }
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}
