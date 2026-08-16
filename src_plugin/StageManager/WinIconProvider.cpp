#include "WinIconProvider.h"
#include <QDebug>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <psapi.h>


#pragma comment(lib, "psapi.lib")

WinIconProvider::WinIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

QPixmap WinIconProvider::requestPixmap(const QString &id, QSize *size,
                                       const QSize &requestedSize) {
  quint64 hwndVal = id.toULongLong();
  HWND hwnd = (HWND)hwndVal;

  if (!IsWindow(hwnd))
    return QPixmap();

  int width = requestedSize.width() > 0 ? requestedSize.width() : 32;
  int height = requestedSize.height() > 0 ? requestedSize.height() : 32;

  if (size) {
    *size = QSize(width, height);
  }

  HICON hIcon = nullptr;

  // Try sending WM_GETICON
  DWORD_PTR result;
  if (SendMessageTimeout(hwnd, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG, 100,
                         &result)) {
    hIcon = (HICON)result;
  }

  if (!hIcon) {
    if (SendMessageTimeout(hwnd, WM_GETICON, ICON_SMALL, 0, SMTO_ABORTIFHUNG,
                           100, &result)) {
      hIcon = (HICON)result;
    }
  }

  // Try GetClassLong
  if (!hIcon) {
    hIcon = (HICON)GetClassLongPtr(hwnd, GCLP_HICON);
  }
  if (!hIcon) {
    hIcon = (HICON)GetClassLongPtr(hwnd, GCLP_HICONSM);
  }

  if (!hIcon) {
    // Fallback: Get EXE Path
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE hProcess =
        OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess) {
      WCHAR path[MAX_PATH];
      if (GetModuleFileNameExW(hProcess, NULL, path, MAX_PATH)) {
        QFileInfo fi(QString::fromWCharArray(path));
        QFileIconProvider provider;
        QIcon icon = provider.icon(fi);
        if (!icon.isNull()) {
          CloseHandle(hProcess);
          return icon.pixmap(width, height);
        }
      }
      CloseHandle(hProcess);
    }

    return QPixmap();
  }

  return fromHICON(hIcon);
}

QPixmap WinIconProvider::fromHICON(HICON icon) {
  if (!icon)
    return QPixmap();

  ICONINFO ii;
  if (!GetIconInfo(icon, &ii))
    return QPixmap();

  int w = 0, h = 0;
  BITMAP bm;
  if (GetObject(ii.hbmMask, sizeof(bm), &bm)) {
    w = bm.bmWidth;
    h = bm.bmHeight;
    if (!ii.hbmColor)
      h /= 2;
  }

  if (ii.hbmMask)
    DeleteObject(ii.hbmMask);
  if (ii.hbmColor)
    DeleteObject(ii.hbmColor);

  if (w <= 0 || h <= 0)
    return QPixmap();

  // Create a DC and a bitmap to draw the icon into
  HDC screenDC = GetDC(nullptr);
  HDC memDC = CreateCompatibleDC(screenDC);

  // Create a 32-bit bitmap for alpha channel support
  BITMAPINFO bmi;
  memset(&bmi, 0, sizeof(bmi));
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h; // Top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void *bits = nullptr;
  HBITMAP hBitmap =
      CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);

  HGDIOBJ oldBitmap = SelectObject(memDC, hBitmap);

  // Clear background to transparent black
  memset(bits, 0, w * h * 4);

  DrawIconEx(memDC, 0, 0, icon, w, h, 0, nullptr, DI_NORMAL);

  // QImage from bits
  // QImage::Format_ARGB32_Premultiplied expects B G R A
  QImage image((uchar *)bits, w, h, QImage::Format_ARGB32_Premultiplied);

  // Make a deep copy because we are about to destroy the DIB
  QPixmap pixmap = QPixmap::fromImage(image.copy());

  SelectObject(memDC, oldBitmap);
  DeleteObject(hBitmap);
  DeleteDC(memDC);
  ReleaseDC(nullptr, screenDC);

  return pixmap;
}
