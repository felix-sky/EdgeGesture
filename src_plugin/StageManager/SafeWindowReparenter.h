#pragma once

#include <QMap>
#include <QObject>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>
#include <windows.h>

struct WindowState {
  HWND hwnd;
  HWND containerHwnd; // The intermediate child window
  HWND originalParent;
  LONG_PTR originalStyle;
  LONG_PTR originalExStyle;
  RECT originalRect;
  int titleBarHeight; // For cropping
  bool isReparented;
};

class SafeWindowReparenter : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  static SafeWindowReparenter *instance();
  explicit SafeWindowReparenter(QObject *parent = nullptr);
  ~SafeWindowReparenter();

  Q_INVOKABLE bool reparentWindow(qint64 hwndVal, QQuickItem *container);
  Q_INVOKABLE bool restoreWindow(qint64 hwndVal);
  Q_INVOKABLE void updateWindowPosition(qint64 hwndVal, QQuickItem *container);
  Q_INVOKABLE void makeWindowClickThrough(qint64 hwndVal);

  Q_INVOKABLE void applyWin11RoundedCorners(QQuickWindow *window);
  Q_INVOKABLE void batchMoveWindows(const QList<int> &hwnds, int dx, int dy);

signals:
  void windowDestroyed(qint64 hwnd);
  void windowActiveStateChanged(qint64 hwnd, bool active);

public:
  void notifyWindowActiveState(HWND hwnd, bool active) {
    emit windowActiveStateChanged((qint64)hwnd, active);
  }

private:
  QMap<HWND, WindowState> m_managedWindows;

  void stripWindowStyles(HWND hwnd, WindowState &state);
  void restoreWindowStyles(HWND hwnd, const WindowState &state);
  HWND createContainerWindow(HWND parentHwnd);
  void onTargetDestroyed(HWND container, HWND target);
  static LRESULT CALLBACK ContainerWndProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                           LPARAM lParam);

private:
  static SafeWindowReparenter *s_instance;
};
