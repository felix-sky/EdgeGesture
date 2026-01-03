#include "EngineControl.h"
#include "WindowsUtils.h"

#include <QCoreApplication>
#include <QDebug>
#include <QProcess>

EngineControl::EngineControl(QObject *parent) : QObject(parent) {
  m_process = new QProcess(this);
  m_process->setProgram(QCoreApplication::applicationDirPath() +
                        "/GestureEngine.exe");
}

EngineControl::~EngineControl() {
  if (m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(1000);
  }
}

void EngineControl::setEnabled(bool enabled) {
  if (m_enabled != enabled) {
    m_enabled = enabled;
    updateState();
  }
}

void EngineControl::cleanUp() {
  WindowsUtils::killProcess("GestureEngine.exe");
}

void EngineControl::notifyChanges() {
  // Notify Visualizer: WM_USER + 101
  WindowsUtils::postMessageToWindow(L"OHOVisualizer", L"", 1125, 0, 0);

  // Notify InputWindow: WM_USER + 101
  WindowsUtils::postMessageToWindow(L"OHOInputOverlay", L"OHO_Left", 1125, 0,
                                    0);

  qDebug() << "[EngineControl] Notified engines of changes.";
}

void EngineControl::setPreviewHandle(bool enabled, bool isLeft) {
  // WM_USER + 102
  WPARAM wMode = 0;
  if (enabled) {
    wMode = isLeft ? 1 : 2;
  }
  WindowsUtils::postMessageToWindow(L"OHOInputOverlay", L"OHO_Left", 1126,
                                    wMode, 0);
}

void EngineControl::updateState() {
  if (m_enabled) {
    if (m_process->state() == QProcess::NotRunning) {
      m_process->start();
    }
  } else {
    if (m_process->state() != QProcess::NotRunning) {
      m_process->terminate();
      if (!m_process->waitForFinished(500)) {
        m_process->kill();
      }
    }
  }
}
