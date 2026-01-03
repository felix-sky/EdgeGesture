#include "ActionDispatcher.h"
#include "core/ConfigManager.h"
#include <iostream>
#include <sstream>

ActionDispatcher::ActionDispatcher() {}

ActionDispatcher::~ActionDispatcher() {}

void ActionDispatcher::Trigger(const std::string &actionName) {
  std::cout << "[Action] Triggering: " << actionName << std::endl;

  // 1. Check for ConfigManager managed action first
  AppConfig &cfg = ConfigManager::Get().Current();
  std::string command = actionName;

  // If the actionName exists in the map, use the mapped command string
  if (cfg.actionMap.count(actionName)) {
    command = cfg.actionMap[actionName];
    std::cout << "  -> Resolved to: " << command << std::endl;
  }

  // 2. Plugin Commands
  // Support legacy "quick_panel" for older configs, but map to plugin
  if (command == "quick_panel") {
    SendPluginCommand("QuickPanel");
    return;
  }

  if (command.rfind("plugin:", 0) == 0) {
    // Starts with "plugin:"
    std::string pluginName = command.substr(7);
    if (!pluginName.empty()) {
      SendPluginCommand(pluginName);
    }
    return;
  }

  if (command == "none" || command.empty()) {
    return;
  }

  // 3. Parse and Execute Key Sequence
  std::vector<INPUT> inputs = ParseKeySequence(command);
  if (!inputs.empty()) {
    SendKeySequence(inputs);
  } else {
    std::cerr << "[Action] Parse failed or empty sequence for: " << command
              << std::endl;
  }
}

std::vector<INPUT>
ActionDispatcher::ParseKeySequence(const std::string &sequence) {
  std::vector<INPUT> inputs;
  std::vector<WORD> keysDown; // Track keys to release them in reverse order

  std::stringstream ss(sequence);
  std::string segment;

  while (std::getline(ss, segment, '+')) {
    // Trim? (Assuming simple format for now)
    WORD vk = GetKeyCode(segment);
    if (vk == 0) {
      std::cerr << "  -> Unknown key: " << segment << std::endl;
      continue;
    }

    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    inputs.push_back(input);
    keysDown.push_back(vk);
  }

  // Add Key Ups in reverse
  for (auto it = keysDown.rbegin(); it != keysDown.rend(); ++it) {
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = *it;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    inputs.push_back(input);
  }

  return inputs;
}

WORD ActionDispatcher::GetKeyCode(const std::string &keyName) {
  std::string k = keyName;
  // lower case for comparison?
  // Let's do simple map or checks
  if (k == "ctrl" || k == "control")
    return VK_CONTROL;
  if (k == "alt" || k == "menu")
    return VK_MENU;
  if (k == "shift")
    return VK_SHIFT;
  if (k == "win" || k == "cmd" || k == "super")
    return VK_LWIN;
  if (k == "tab")
    return VK_TAB;
  if (k == "esc")
    return VK_ESCAPE;
  if (k == "enter" || k == "return")
    return VK_RETURN;
  if (k == "space")
    return VK_SPACE;
  if (k == "backspace")
    return VK_BACK;
  if (k == "delete" || k == "del")
    return VK_DELETE;

  // Arrows
  if (k == "left" || k == "key_left")
    return VK_LEFT;
  if (k == "right" || k == "key_right")
    return VK_RIGHT;
  if (k == "up" || k == "key_up")
    return VK_UP;
  if (k == "down" || k == "key_down")
    return VK_DOWN;

  // Letters / Numbers
  if (k.length() == 1) {
    char c = toupper(k[0]);
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      return (WORD)c;
    }
  }

  // F-keys
  if (k.length() >= 2 && (k[0] == 'f' || k[0] == 'F')) {
    try {
      int num = std::stoi(k.substr(1));
      if (num >= 1 && num <= 24)
        return (WORD)(VK_F1 + (num - 1));
    } catch (...) {
    }
  }

  // Multimedia Keys
  if (k == "volume_up" || k == "vol_up")
    return VK_VOLUME_UP;
  if (k == "volume_down" || k == "vol_down")
    return VK_VOLUME_DOWN;
  if (k == "volume_mute" || k == "mute")
    return VK_VOLUME_MUTE;
  if (k == "media_next" || k == "next")
    return VK_MEDIA_NEXT_TRACK;
  if (k == "media_prev" || k == "prev")
    return VK_MEDIA_PREV_TRACK;
  if (k == "media_play_pause" || k == "play_pause")
    return VK_MEDIA_PLAY_PAUSE;
  if (k == "media_stop")
    return VK_MEDIA_STOP;

  return 0; // Unknown
}

void ActionDispatcher::SendKeySequence(const std::vector<INPUT> &inputs) {
  if (!inputs.empty())
    SendInput((UINT)inputs.size(), (LPINPUT)inputs.data(), sizeof(INPUT));
}

void ActionDispatcher::SendPluginCommand(const std::string &pluginName) {
  HWND hwnd = FindWindowW(NULL, L"EdgeGesture Config");
  if (hwnd) {
    // prepare COPYDATASTRUCT
    COPYDATASTRUCT cds;
    cds.dwData = 1; // ID for Plugin Command
    cds.cbData = (DWORD)(pluginName.size() + 1);
    cds.lpData = (PVOID)pluginName.c_str();

    SendMessage(hwnd, WM_COPYDATA, 0, (LPARAM)&cds);
    std::cout << "[Action] Sent plugin command: " << pluginName << std::endl;
  } else {
    std::cerr << "[Action] Qt Window not found for Plugin Command" << std::endl;
  }
}

void ActionDispatcher::NotifyQtQuickPanel() { SendPluginCommand("QuickPanel"); }
