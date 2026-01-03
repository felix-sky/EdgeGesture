#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <windows.h>

using json = nlohmann::json;
#include <vector>

struct SideConfig {
  bool enabled = true;
  int width = 30;
  int size =
      100; // Vertical size % or px? Let's assume px for now or logic handles it
  int position = 50; // Top offset %
  std::string color = "#000000";
};

struct AppConfig {
  float tension = 0.35f;
  float friction = 0.65f;
  float triggerThreshold = 90.0f;
  float maxWaveX = 160.0f;
  int verticalRange = 50;

  // Split Mode: 0=None, 1=Two, 2=Three
  int splitMode = 0;

  // Swipe Thresholds
  float longSwipeThreshold = 450.0f;
  float shortSwipeThreshold = 30.0f;

  SideConfig left;
  SideConfig right;

  // Gestures key: "left_right", "right_diag_up", etc.
  // value: "back", "task_view", "quick_panel"
  // Gestures key: "left_right", "right_diag_up", etc.
  // value: "back", "task_view", "quick_panel"
  std::map<std::string, std::string> gestureMap;

  // Actions key: "Back", "TaskView"
  // value: "alt+left", "win+tab"
  std::map<std::string, std::string> actionMap;

  std::vector<std::string> blacklist;

  // Quick Panel IPC port or window name could go here
};

class ConfigManager {
public:
  static ConfigManager &Get() {
    static ConfigManager instance;
    return instance;
  }

  void Load();
  AppConfig &Current() { return m_config; }

  // Helper to get color ref from hex string
  COLORREF GetColorRef(const std::string &hexFunc);

private:
  ConfigManager() = default;
  AppConfig m_config;
  std::string m_configPath = "config.json";
};
