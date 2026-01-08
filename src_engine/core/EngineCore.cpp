#include "EngineCore.h"
#include <cmath>
#include <iostream>
#include <psapi.h>
#include <string>

EngineCore::EngineCore() {}

EngineCore::~EngineCore() {}

std::string EngineCore::GetActiveProcessName() {
  HWND hwnd = GetForegroundWindow();
  if (!hwnd)
    return "";

  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid == 0)
    return "";

  HANDLE hProcess = OpenProcess(
      PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  if (!hProcess)
    return "";

  char buffer[MAX_PATH];
  std::string result;
  if (GetModuleBaseNameA(hProcess, nullptr, buffer, MAX_PATH)) {
    result = buffer;
  }
  CloseHandle(hProcess);
  return result;
}

void EngineCore::Run() {
  ConfigManager::Get().Load();

  int screenW = GetSystemMetrics(SM_CXSCREEN);
  int screenH = GetSystemMetrics(SM_CYSCREEN);

  m_vis.Init(screenW, screenH);

  InputWindow::Get().SetCallbacks(
      [this](bool inZone, bool isLeft) { OnZoneState(inZone, isLeft); },
      [this](bool isLeft, int y) { OnGestureStart(isLeft, y); },
      [this](int x, int y) { OnGestureUpdate(x, y); },
      [this]() { OnGestureEnd(); });
  InputWindow::Get().Initialize();

  // SetTimer(NULL, m_profileTimerId, 500, nullptr);
  m_profileTimerId = SetTimer(NULL, 0, 500, nullptr);
  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);

    if (msg.message == (WM_USER + 101)) {
      ReloadConfig();
    }

    if (msg.message == WM_TIMER && msg.wParam == m_profileTimerId) {
      std::string appName = GetActiveProcessName();
      if (!appName.empty() && appName != m_lastAppName) {
        m_lastAppName = appName;
        ConfigManager::Get().LoadProfile(appName);
        InputWindow::Get().UpdateLayout();
      }
    }
  }
}

void EngineCore::ReloadConfig() {
  ConfigManager::Get().Load();
  InputWindow::Get().UpdateLayout();
  std::cout << "[Core] Config Reloaded" << std::endl;
}

void EngineCore::OnZoneState(bool inZone, bool isLeft) {
  if (m_isDragging)
    return;
}

void EngineCore::OnGestureStart(bool isLeft, int y) {
  if (IsBlacklistedAppActive()) {
    std::cout << "[Core] Blacklisted app active, ignoring gesture."
              << std::endl;
    return;
  }

  std::cout << "\n=== Gesture Start === Side: " << (isLeft ? "LEFT" : "RIGHT")
            << " | Y: " << y << std::endl;

  m_isDragging = true;
  m_isLeft = isLeft;
  m_anchorY = (float)y;

  m_targetX = 0;
  m_targetY = m_anchorY;

  m_currentX = 0;
  m_currentY = m_anchorY;
  m_velocityX = 0;
  m_velocityY = 0;

  m_vis.SetTimerCallback([this]() { this->PhysicsLoop(); });
  SetTimer(m_vis.GetHwnd(), m_timerId, 16, nullptr);
}

void EngineCore::OnGestureUpdate(int x, int y) {
  float absX = (float)x;
  int screenW = GetSystemMetrics(SM_CXSCREEN);

  if (m_isLeft) {
    m_targetX = absX;
  } else {
    m_targetX = (float)(screenW - absX);
  }

  m_targetY = (float)y;

  std::cout << "Update: X=" << x << " Y=" << y << " | targetX=" << m_targetX
            << " targetY=" << m_targetY << std::endl;

  DetermineGesture();
}

void EngineCore::OnGestureEnd() {
  m_isDragging = false;
  AppConfig &cfg = ConfigManager::Get().Current();
  std::cout << "=== Gesture End === currentX: " << m_currentX
            << " | threshold: " << cfg.triggerThreshold << std::endl;
  if (m_currentX > cfg.triggerThreshold) {
    std::cout << "  -> TRIGGERING ACTION: " << m_currentAction << std::endl;
    m_dispatcher.Trigger(m_currentAction);
  } else {
    std::cout << "  -> Below threshold, not triggering" << std::endl;
  }
}

void EngineCore::DetermineGesture() {
  float dy = m_targetY - m_anchorY;
  float dx = m_targetX;

  std::string base = m_isLeft ? "left" : "right";
  std::string direction = "_right";

  AppConfig &cfg = ConfigManager::Get().Current();

  if (cfg.splitMode > 0) {
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    float relY = m_anchorY / (float)screenH;
    std::string zone = "";

    if (cfg.splitMode == 1) {
      if (relY < 0.5f)
        zone = "_top";
      else
        zone = "_bottom";
    } else if (cfg.splitMode == 2) {
      if (relY < 0.33f)
        zone = "_top";
      else if (relY < 0.66f)
        zone = "_middle";
      else
        zone = "_bottom";
    }
    base += zone;
  }
  if (dx >= cfg.shortSwipeThreshold) {
    bool isLong = (dx > cfg.longSwipeThreshold);

    if (dy < -dx * 0.5f)
      direction = "_diag_up";
    else if (dy > dx * 0.5f)
      direction = "_diag_down";
    else {
      direction = m_isLeft ? "_right" : "_left";
      if (isLong) {
        direction = "_long" + direction;
      }
    }

    std::string key = base + direction;

    std::cout << "Gesture: " << key << " | dx:" << dx << " dy:" << dy
              << " | threshold met: " << (dx >= 30) << std::endl;

    AppConfig &cfg = ConfigManager::Get().Current();
    if (cfg.gestureMap.count(key)) {
      m_currentAction = cfg.gestureMap[key];
      std::cout << "  -> Action: " << m_currentAction << std::endl;
    } else {
      std::cout << "  -> Gesture key NOT found in map: " << key << std::endl;
      m_currentAction = "none";
    }
  }
}

void EngineCore::PhysicsLoop() {
  AppConfig &cfg = ConfigManager::Get().Current();

  float realTargetX = m_isDragging ? m_targetX : 0;
  if (realTargetX > cfg.maxWaveX)
    realTargetX = cfg.maxWaveX;

  float forceX = (realTargetX - m_currentX) * cfg.tension;
  m_velocityX += forceX;
  m_velocityX *= cfg.friction;
  m_currentX += m_velocityX;

  float realTargetY = m_isDragging ? m_targetY : m_anchorY;
  float forceY = (realTargetY - m_currentY) * cfg.tension;
  m_velocityY += forceY;
  m_velocityY *= cfg.friction;
  m_currentY += m_velocityY;

  if (!m_isDragging && std::abs(m_currentX) < 0.5f &&
      std::abs(m_velocityX) < 0.5f) {
    m_currentX = 0;
    KillTimer(m_vis.GetHwnd(), m_timerId);
  }

  bool triggered = (m_currentX > cfg.triggerThreshold);
  m_vis.Update(m_currentX, m_currentY, m_anchorY, m_isLeft, triggered, "");
  m_vis.Render();
}

bool EngineCore::IsBlacklistedAppActive() {
  std::string exeName = GetActiveProcessName();
  if (exeName.empty())
    return false;

  AppConfig &cfg = ConfigManager::Get().Current();
  for (const auto &blocked : cfg.blacklist) {
    if (_stricmp(exeName.c_str(), blocked.c_str()) == 0) {
      return true;
    }
  }

  return false;
}
