#ifndef UNICODE
#define UNICODE
#endif

#include "core/EngineCore.h"
#include <iostream>

void InitConsole() {
  AllocConsole();
  FILE *stream;
  freopen_s(&stream, "CONOUT$", "w", stdout);
  freopen_s(&stream, "CONOUT$", "w", stderr);
  SetConsoleOutputCP(65001);
  std::cout << "[INFO] EdgeGesture Engine Starting..." << std::endl;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
  // Only allow one instance
  HANDLE hMutex = CreateMutex(NULL, TRUE, L"EdgeGestureEngineMutex");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    MessageBox(NULL, L"Engine is already running.", L"Error",
               MB_OK | MB_ICONERROR);
    return 1;
  }

  // InitConsole(); // Disabled for production
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  EngineCore engine;
  engine.Run();

  ReleaseMutex(hMutex);
  return 0;
}
