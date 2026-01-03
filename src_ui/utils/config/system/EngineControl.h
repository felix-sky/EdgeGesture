#pragma once

#include <QObject>

class QProcess;

class EngineControl : public QObject {
  Q_OBJECT
public:
  explicit EngineControl(QObject *parent = nullptr);
  ~EngineControl();

  void setEnabled(bool enabled);
  bool isEnabled() const { return m_enabled; }

  // Clean up existing instances
  void cleanUp();

  // Notify engine components of config changes
  void notifyChanges();

  // Input overlay preview control
  void setPreviewHandle(bool enabled, bool isLeft);

private:
  void updateState();

  bool m_enabled = false;
  QProcess *m_process = nullptr;
};
