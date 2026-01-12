#pragma once

#include <QString>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

namespace WindowsUtils {

void setWindowDark(bool isDark);
void postMessageToWindow(const std::wstring &className,
                         const std::wstring &windowName, UINT msg,
                         WPARAM wParam, LPARAM lParam);
void killProcess(const QString &processName);
void applyMica(HWND hwnd);

} // namespace WindowsUtils
