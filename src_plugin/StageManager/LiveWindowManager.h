#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <dwmapi.h>
#include <windows.h>

#include <qqml.h>

struct WindowInfo {
  quint64 hwnd;
  QString title;
  // We can add icon path or other metadata here
};

class LiveWindowManager : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  enum Roles { HwndRole = Qt::UserRole + 1, TitleRole, IconRole };

  explicit LiveWindowManager(QObject *parent = nullptr);
  ~LiveWindowManager() override;

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE void refresh();
  Q_INVOKABLE void activateWindow(quint64 hwnd);

  // Static callback for EnumWindows
  static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);

  // Static callback for SetWinEventHook
  static void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event,
                                    HWND hwnd, LONG idObject, LONG idChild,
                                    DWORD dwEventThread, DWORD dwmsEventTime);

private:
  void addWindow(HWND hwnd);
  void removeWindow(HWND hwnd);
  bool isValidWindow(HWND hwnd);
  void onWindowEvent(DWORD event, HWND hwnd);

  QVector<WindowInfo> m_windows;
  DWORD m_myProcessId = 0;
  HWINEVENTHOOK m_hEventHook = nullptr;

  // Helper to queue updates to the main thread
  void queueRefresh();
};
