#include "utils/FileBridge.h"
#include "utils/SystemBridge.h"
#include "utils/config/bridge/ConfigBridge.h"
#include "utils/config/services/Plugin.h"
#include <FluentUI.h>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include "utils/ExeIconProvider.h"
#include "utils/config/system/WindowsUtils.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dwmapi.h>
#include <windows.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")

void applyMica(QWindow *window) {
  if (!window)
    return;
  HWND hwnd = (HWND)window->winId();
  if (!hwnd)
    return;

  // DWMWA_SYSTEMBACKDROP_TYPE = 38
  // DWMSBT_MAINWINDOW = 2 (Mica)
  int backdropValue = 2;
  HRESULT result =
      DwmSetWindowAttribute(hwnd, 38, &backdropValue, sizeof(backdropValue));

  // Extend frame into client area to ensure transparency
  MARGINS margins = {-1, -1, -1, -1};
  DwmExtendFrameIntoClientArea(hwnd, &margins);
}



int main(int argc, char *argv[]) {
  // Singleton check
  HANDLE hMutex =
      CreateMutexW(NULL, TRUE, L"EdgeGesture_SettingsUI_Singleton");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    HWND existingWindow = FindWindowW(NULL, L"EdgeGesture Config");
    if (existingWindow) {
      ShowWindow(existingWindow, SW_RESTORE);
      SetForegroundWindow(existingWindow);
    }
    return 0;
  }


  QGuiApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);
  app.setWindowIcon(QIcon(":/icon.ico"));

  // Initialize FluentUI
  QQmlApplicationEngine engine;
  engine.addImageProvider("exe_icon", new ExeIconProvider);

  // FluentUI::registerTypes(&engine); // Not needed for shared/plugin build

  ConfigBridge configBridge;
  SystemBridge systemBridge;
  FileBridge fileBridge;

  engine.rootContext()->setContextProperty("ConfigBridge", &configBridge);
  engine.rootContext()->setContextProperty("SystemBridge", &systemBridge);
  engine.rootContext()->setContextProperty("FileBridge", &fileBridge);
  // Register decoupled components
  engine.rootContext()->setContextProperty("Plugin", configBridge.plugin());
  engine.addImportPath("./plugin/additional/");

  const QUrl url(u"qrc:/Main.qml"_qs);
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreated, &app,
      [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
          QCoreApplication::exit(-1);
        } else if (obj) {
          // Check if it's a window and apply Mica
          QWindow *window = qobject_cast<QWindow *>(obj);
          if (window) {
            applyMica(window);
          }
        }
      },
      Qt::QueuedConnection);
  engine.load(url);

  int execResult = app.exec();

  // Ensure GestureEngine.exe is closed on exit
  WindowsUtils::killProcess("GestureEngine.exe");

  return execResult;
}
