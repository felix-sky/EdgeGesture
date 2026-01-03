#pragma once
#include <string>
#include <vector>
#include <windows.h>

enum class ActionType { None, Back, TaskView, ShowDesktop, QuickPanel };

class ActionDispatcher {
public:
  ActionDispatcher();
  ~ActionDispatcher();

  void Trigger(const std::string &actionName);

private:
  void SendKeySequence(const std::vector<INPUT> &inputs);
  std::vector<INPUT> ParseKeySequence(const std::string &sequence);
  WORD GetKeyCode(const std::string &keyName);
  void NotifyQtQuickPanel();
  void SendPluginCommand(const std::string &pluginName);
};
