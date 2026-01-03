#pragma once
#include "core/ConfigManager.h"
#include <functional>
#include <windows.h>

// Callback types
using ZoneStateCallback = std::function<void(bool inZone, bool isLeft)>;
using GestureStartCallback = std::function<void(bool isLeft, int y)>;
using GestureUpdateCallback = std::function<void(int x, int y)>;
using GestureEndCallback = std::function<void()>;

class InputHook {
public:
  static InputHook &Get() {
    static InputHook instance;
    return instance;
  }

  void Install();
  void Uninstall();

  void SetCallbacks(ZoneStateCallback onZone, GestureStartCallback onStart,
                    GestureUpdateCallback onUpdate, GestureEndCallback onEnd);

  // Called from static hook proc
  LRESULT HookProc(int nCode, WPARAM wParam, LPARAM lParam);

private:
  InputHook() = default;
  HHOOK m_hHook = nullptr;

  ZoneStateCallback OnZoneState;
  GestureStartCallback OnStart;
  GestureUpdateCallback OnUpdate;
  GestureEndCallback OnEnd;

  int m_startY = 0;
  bool m_isDragging = false;
  bool m_isMouseInZone = false;

  // Helpers
  bool IsTouchInput(ULONG_PTR extraInfo);
  bool IsSideTrigger(int x, int y, bool &outIsLeft);
};
