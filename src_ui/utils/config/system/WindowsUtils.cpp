#include "WindowsUtils.h"

#include <QGuiApplication>
#include <QProcess>
#include <QWindow>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")

namespace WindowsUtils {

void setWindowDark(bool isDark) {
  QWindow *window = QGuiApplication::focusWindow();
  if (!window && !QGuiApplication::topLevelWindows().isEmpty()) {
    window = QGuiApplication::topLevelWindows().at(0);
  }

  if (window) {
    HWND hwnd = (HWND)window->winId();
    if (hwnd) {
      BOOL value = isDark ? TRUE : FALSE;
      // DWMWA_USE_IMMERSIVE_DARK_MODE = 20
      DwmSetWindowAttribute(hwnd, 20, &value, sizeof(value));

      SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
  }
}

void postMessageToWindow(const std::wstring &className,
                         const std::wstring &windowName, UINT msg,
                         WPARAM wParam, LPARAM lParam) {
  HWND hwnd = FindWindowW(className.empty() ? nullptr : className.c_str(),
                          windowName.empty() ? nullptr : windowName.c_str());

  if (hwnd) {
    PostMessageW(hwnd, msg, wParam, lParam);
  }
}

void killProcess(const QString &processName) {
  QProcess::execute("taskkill", QStringList() << "/F"
                                              << "/IM" << processName);
}

void applyMica(HWND hwnd) {
  if (!hwnd)
    return;

  // DWMWA_SYSTEMBACKDROP_TYPE = 38
  // DWMSBT_MAINWINDOW = 2 (Mica)
  int backdropValue = 2;
  DwmSetWindowAttribute(hwnd, 38, &backdropValue, sizeof(backdropValue));

  // Extend frame into client area to ensure transparency
  MARGINS margins = {-1, -1, -1, -1};
  DwmExtendFrameIntoClientArea(hwnd, &margins);
}

} // namespace WindowsUtils
