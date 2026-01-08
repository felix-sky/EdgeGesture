#pragma once
#include "actions/ActionDispatcher.h"
#include "core/ConfigManager.h"
#include "input/InputWindow.h"
#include "ui/Visualizer.h"
#include <memory>
#include <string>
#include <windows.h>

class EngineCore {
public:
  EngineCore();
  ~EngineCore();

  void Run();
  void ReloadConfig();

private:
  void OnZoneState(bool inZone, bool isLeft);
  void OnGestureStart(bool isLeft, int y);
  void OnGestureUpdate(int x, int y);
  void OnGestureEnd();

  void PhysicsLoop();

  bool IsBlacklistedAppActive();
  std::string GetActiveProcessName();

  void DetermineGesture();

  Visualizer m_vis;
  ActionDispatcher m_dispatcher;

  float m_currentX = 0;
  float m_currentY = 0;
  float m_velocityX = 0;
  float m_velocityY = 0;
  float m_targetX = 0;
  float m_targetY = 0;
  float m_anchorY = 0;

  bool m_isDragging = false;
  bool m_isAnimating = false;
  bool m_isLeft = true;

  UINT_PTR m_timerId = 1001;
  UINT_PTR m_profileTimerId = 2001;

  std::string m_currentAction;
  std::string m_currentGestureName;
  std::string m_lastAppName;
};
