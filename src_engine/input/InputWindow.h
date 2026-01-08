#pragma once
#include <functional>
#include <windows.h>

using ZoneStateCallback = std::function<void(bool inZone, bool isLeft)>;
using GestureStartCallback = std::function<void(bool isLeft, int y)>;
using GestureUpdateCallback = std::function<void(int x, int y)>;
using GestureEndCallback = std::function<void()>;

class InputWindow {
public:
  static InputWindow &Get() {
    static InputWindow instance;
    return instance;
  }

  // Creates the overlay windows
  void Initialize();
  // Destroys windows
  void Shutdown();

  void SetCallbacks(ZoneStateCallback onZone, GestureStartCallback onStart,
                    GestureUpdateCallback onUpdate, GestureEndCallback onEnd);

  // Updates window positions/sizes based on current config and screen
  // resolution
  void UpdateLayout();

private:
  InputWindow();
  ~InputWindow();

  void CreateWindows();
  void SetupWindow(HWND &hwnd, bool isLeft);

  static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                                  LPARAM lParam);

  // Check if the current message event comes from Touch
  bool IsTouchInput();

  HWND m_hwndLeft = nullptr;
  HWND m_hwndRight = nullptr;
  int m_previewMode = 0; // 0=None, 1=Left, 2=Right

  ZoneStateCallback OnZoneState;
  GestureStartCallback OnStart;
  GestureUpdateCallback OnUpdate;
  GestureEndCallback OnEnd;

  bool m_isDragging = false;

  // --- Dynamic Adjustment Members ---
  bool m_isHovering = false;
  bool m_isSuppressed = false; // True if window is currently hidden due to hover

  // Timer Identifiers
  static const UINT_PTR TIMER_HOVER_CHECK = 1001;
  static const UINT_PTR TIMER_RECOVERY = 1002;
  static const UINT_PTR TIMER_WATCHDOG = 1003;

  // Configuration Constants (Consider moving to ConfigManager later)
  const UINT HOVER_THRESHOLD_MS = 400;    // Time before window hides
  const UINT RECOVERY_DELAY_MS = 2000;    // Time before window reappears
  const UINT WATCHDOG_INTERVAL_MS = 2000; // Watchdog check interval
};
