#include "utils/ExeIconProvider.h"
#include "utils/config/bridge/ConfigBridge.h"
#include "utils/config/bridge/FileBridge.h"
#include "utils/config/bridge/SystemBridge.h"
#include "utils/config/services/Plugin.h"
#include "utils/config/system/WindowsUtils.h"
#include <FluentUI.h>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dwmapi.h>
#include <windows.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")

int main(int argc, char *argv[]) {
  // Singleton check
  HANDLE hMutex = CreateMutexW(NULL, TRUE, L"EdgeGesture_SettingsUI_Singleton");
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
  engine.addImportPath("./bin");

  const QUrl url(u"qrc:/Main.qml"_qs);
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreated, &app,
      [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
          QCoreApplication::exit(-1);
        }
      },
      Qt::QueuedConnection);
  engine.load(url);

  int execResult = app.exec();

  // Ensure GestureEngine.exe is closed on exit
  WindowsUtils::killProcess("GestureEngine.exe");

  return execResult;
}
