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
  // Background brush doesn't matter much for layered, but let's set it
  wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  RegisterClassExW(&wcex);

  CreateWindows();
  UpdateLayout();
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

  // Make them "Input Opaque" but "Visually Transparent"
  // Alpha = 1 (out of 255) is virtually invisible but captures input.
  if (m_hwndLeft)
    SetLayeredWindowAttributes(m_hwndLeft, 0, 1, LWA_ALPHA);
  if (m_hwndRight)
    SetLayeredWindowAttributes(m_hwndRight, 0, 1, LWA_ALPHA);
}

void InputWindow::UpdateLayout() {
  int screenW = GetSystemMetrics(SM_CXSCREEN);
  int screenH = GetSystemMetrics(SM_CYSCREEN);

  AppConfig &cfg = ConfigManager::Get().Current();

  // Create brushes if needed or just rely on background color?
  // For simple preview, let's just make the window visible.
  // Standard background is BLACK.
  // If we want color matches, we'd need to handle WM_ERASEBKGND or WM_PAINT.
  // Let's stick to simple semi-transparent black for alignment first.

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
  // Also check standard flag (0x80) if needed, but the mask above is what
  // InputHook used. Let's stick to the user's previous successful logic
  // signature if possible.
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
    case WM_LBUTTONDOWN: {
      if (pThis->IsTouchInput()) {
        pThis->m_isDragging = true;
        SetCapture(hWnd);

        bool isLeft = (hWnd == pThis->m_hwndLeft);

        // Convert client to screen coordinates
        POINT pt;
        pt.x = (short)LOWORD(lParam);
        pt.y = (short)HIWORD(lParam);
        ClientToScreen(hWnd, &pt);

        if (pThis->OnStart) {
          pThis->OnStart(isLeft, pt.y);
        }
        return 0;
      }
      break;
    }
    case WM_MOUSEMOVE: {
      if (pThis->m_isDragging) {
        POINT pt;
        pt.x = (short)LOWORD(lParam);
        pt.y = (short)HIWORD(lParam);
        ClientToScreen(hWnd, &pt);

        if (pThis->OnUpdate) {
          pThis->OnUpdate(pt.x, pt.y);
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
