#include "ConfigManager.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

void ConfigManager::Load() {
  if (!std::filesystem::exists(m_configPath)) {
    json j;
    j["physics"] = {{"tension", 0.35}, {"friction", 0.65}};
    j["general"] = {{"trigger_threshold", 90.0},
                    {"max_wave_x", 160.0},
                    {"long_swipe_threshold", 450.0},
                    {"short_swipe_threshold", 30.0}};

    j["left_handle"] = {{"enabled", true},
                        {"width", 4},
                        {"size", 80},
                        {"position", 50},
                        {"color", "#000000"}};
    j["right_handle"] = {{"enabled", true},
                         {"width", 4},
                         {"size", 80},
                         {"position", 50},
                         {"color", "#000000"}};

    j["gestures"] = {{"left_right", "Back"},
                     {"left_diag_up", "TaskView"},
                     {"left_diag_down", "ShowDesktop"},
                     {"left_long_right", "QuickPanel"}};

    j["actions"] = {
        {"Back", "alt+left"},        {"TaskView", "win+tab"},
        {"ShowDesktop", "win+d"},    {"QuickPanel", "plugin:QuickPanel"},
        {"VolumeUp", "volume_up"},   {"VolumeDown", "volume_down"},
        {"Mute", "volume_mute"},     {"PlayPause", "media_play_pause"},
        {"NextTrack", "media_next"}, {"PrevTrack", "media_prev"}};

    std::ofstream o(m_configPath);
    o << std::setw(4) << j << std::endl;
  }

  LoadFromPath(m_configPath);
  m_currentProfile = "default";
}

void ConfigManager::LoadProfile(const std::string &appName) {
  if (appName == m_currentProfile) {
    return;
  }

  std::string profilePath = m_configsDir + "/" + appName + ".json";
  std::string defaultPath = m_configsDir + "/default.json";

  if (std::filesystem::exists(profilePath)) {
    LoadFromPath(profilePath);
    m_currentProfile = appName;
    std::cout << "[Config] Loaded profile: " << appName << std::endl;
  } else if (std::filesystem::exists(defaultPath)) {
    LoadFromPath(defaultPath);
    m_currentProfile = "default";
    std::cout << "[Config] Profile not found, using default" << std::endl;
  } else {
    std::cout << "[Config] No profile or default found, keeping current"
              << std::endl;
    return;
  }

  if (m_profileChangeCb) {
    m_profileChangeCb(m_currentProfile);
  }
}

void ConfigManager::LoadFromPath(const std::string &path) {
  try {
    std::ifstream i(path);
    json j;
    i >> j;

    bool modified = false;
    if (!j.contains("actions")) {
      j["actions"] = json::object();
      modified = true;
    }

    std::map<std::string, std::string> defaults = {
        {"Back", "alt+left"},        {"TaskView", "win+tab"},
        {"ShowDesktop", "win+d"},    {"QuickPanel", "plugin:QuickPanel"},
        {"VolumeUp", "volume_up"},   {"VolumeDown", "volume_down"},
        {"Mute", "volume_mute"},     {"PlayPause", "media_play_pause"},
        {"NextTrack", "media_next"}, {"PrevTrack", "media_prev"}};

    for (const auto &[key, val] : defaults) {
      if (!j["actions"].contains(key)) {
        j["actions"][key] = val;
        modified = true;
      }
    }

    if (modified) {
      std::ofstream o(path);
      o << std::setw(4) << j << std::endl;
      std::cout << "[Config] Updated config with missing actions." << std::endl;
    }

    if (j.contains("physics")) {
      m_config.tension = j["physics"].value("tension", 0.35f);
      m_config.friction = j["physics"].value("friction", 0.65f);
    }

    if (j.contains("general")) {
      m_config.triggerThreshold =
          j["general"].value("trigger_threshold", 90.0f);
      m_config.maxWaveX = j["general"].value("max_wave_x", 160.0f);
      m_config.verticalRange = j["general"].value("vertical_range", 50);
      m_config.splitMode = j["general"].value("split_mode", 0);
      m_config.longSwipeThreshold =
          j["general"].value("long_swipe_threshold", 450.0f);
      m_config.shortSwipeThreshold =
          j["general"].value("short_swipe_threshold", 30.0f);
    }

    if (j.contains("left_handle")) {
      auto &l = j["left_handle"];
      m_config.left.enabled = l.value("enabled", true);
      m_config.left.width = l.value("width", 30);
      m_config.left.size = l.value("size", 100);
      m_config.left.position = l.value("position", 50);
      m_config.left.color = l.value("color", "#000000");
    }

    if (j.contains("right_handle")) {
      auto &r = j["right_handle"];
      m_config.right.enabled = r.value("enabled", true);
      m_config.right.width = r.value("width", 30);
      m_config.right.size = r.value("size", 100);
      m_config.right.position = r.value("position", 50);
      m_config.right.color = r.value("color", "#000000");
    }

    if (j.contains("actions")) {
      m_config.actionMap.clear();
      for (auto &el : j["actions"].items()) {
        m_config.actionMap[el.key()] = el.value();
      }
    }

    if (j.contains("gestures")) {
      m_config.gestureMap.clear();
      for (auto &el : j["gestures"].items()) {
        m_config.gestureMap[el.key()] = el.value();
      }
    }

    m_config.blacklist.clear();
    if (j.contains("blacklist")) {
      for (const auto &val : j["blacklist"]) {
        m_config.blacklist.push_back(val.get<std::string>());
      }
    }

    std::cout << "[Config] Loaded from: " << path << std::endl;

  } catch (std::exception &e) {
    std::cerr << "[Config] Error loading config: " << e.what() << std::endl;
  }
}

COLORREF ConfigManager::GetColorRef(const std::string &hex) {
  if (hex.length() < 7 || hex[0] != '#')
    return RGB(0, 0, 0);
  int r, g, b;
  sscanf_s(hex.c_str(), "#%02x%02x%02x", &r, &g, &b);
  return RGB(r, g, b);
}
