#include "InputHook.h"
#include "core/ConfigManager.h"
#include <iostream>

// Static trampoline
InputHook *g_HookInstance = nullptr;

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (g_HookInstance)
    return g_HookInstance->HookProc(nCode, wParam, lParam);
  return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void InputHook::Install() {
  g_HookInstance = this;
  m_hHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc,
                             GetModuleHandle(NULL), 0);
  std::cout << "[Hook] Installed" << std::endl;
}

void InputHook::Uninstall() {
  if (m_hHook) {
    UnhookWindowsHookEx(m_hHook);
    m_hHook = nullptr;
  }
}

void InputHook::SetCallbacks(ZoneStateCallback onZone,
                             GestureStartCallback onStart,
                             GestureUpdateCallback onUpdate,
                             GestureEndCallback onEnd) {
  OnZoneState = onZone;
  OnStart = onStart;
  OnUpdate = onUpdate;
  OnEnd = onEnd;
}

LRESULT InputHook::HookProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode >= 0) {
    MSLLHOOKSTRUCT *pMouse = (MSLLHOOKSTRUCT *)lParam;

    if (wParam == WM_MOUSEMOVE) {
      // Check dragging
      if (m_isDragging) {
        if (OnUpdate)
          OnUpdate(pMouse->pt.x, pMouse->pt.y);
        return 1; // Consume input while dragging
      }

      // Check Hover Logic (Zone State)
      bool isLeft = false;
      // Check if triggers
      bool inZone = IsSideTrigger(pMouse->pt.x, pMouse->pt.y, isLeft);

      // Debounce/Logic update
      // We just notify blindly and let Core handle logic or duplicate checks?
      // Better to check change here to reduce IPC/func calls
      if (OnZoneState)
        OnZoneState(inZone, isLeft);
    } else if (wParam == WM_LBUTTONDOWN) {
      bool isTouch = IsTouchInput(pMouse->dwExtraInfo);
      bool isLeft = false;
      if (IsSideTrigger(pMouse->pt.x, pMouse->pt.y, isLeft) && isTouch) {
        m_isDragging = true;
        m_startY = pMouse->pt.y;
        if (OnStart)
          OnStart(isLeft, pMouse->pt.y);
        return 1; // Consume click
      }
    } else if (wParam == WM_LBUTTONUP) {
      if (m_isDragging) {
        m_isDragging = false;
        if (OnEnd)
          OnEnd();
        // return 1; // Consume UP
      }
    }
  }
  return CallNextHookEx(m_hHook, nCode, wParam, lParam);
}

bool InputHook::IsTouchInput(ULONG_PTR extraInfo) {
  if (extraInfo == 0)
    return false;
  return ((extraInfo & 0xFF515700) == 0xFF515700);
}

bool InputHook::IsSideTrigger(int x, int y, bool &outIsLeft) {
  AppConfig &cfg = ConfigManager::Get().Current();
  int screenW = GetSystemMetrics(SM_CXSCREEN);

  // Check Left
  if (cfg.left.enabled) {
    if (x <= cfg.left.width) {
      // Check Y Range (Position & Size) ... Simplifying for now to full height
      // or ignore
      outIsLeft = true;
      return true;
    }
  }
  // Check Right
  if (cfg.right.enabled) {
    if (x >= screenW - cfg.right.width) {
      outIsLeft = false;
      return true;
    }
  }
  return false;
}
