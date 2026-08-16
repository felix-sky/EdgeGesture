#include "WindowThumbnailProvider.h"
#include <QDebug>
#include <QImage>

WindowThumbnailProvider::WindowThumbnailProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

QPixmap WindowThumbnailProvider::requestPixmap(const QString &id, QSize *size,
                                               const QSize &requestedSize) {
  quint64 hwndVal = id.toULongLong();
  HWND hwnd = (HWND)hwndVal;

  if (!IsWindow(hwnd))
    return QPixmap();

  QPixmap pixmap = captureWindow(hwnd);

  if (size) {
    *size = pixmap.size();
  }

  // Resize if requested
  if (requestedSize.isValid() && !pixmap.isNull()) {
    pixmap = pixmap.scaled(requestedSize, Qt::KeepAspectRatio,
                           Qt::SmoothTransformation);
  }

  return pixmap;
}

QPixmap WindowThumbnailProvider::captureWindow(HWND hwnd) {
  RECT rc;
  if (!GetWindowRect(hwnd, &rc))
    return QPixmap();

  int w = rc.right - rc.left;
  int h = rc.bottom - rc.top;

  if (w <= 0 || h <= 0)
    return QPixmap();

  HDC hdcScreen = GetDC(NULL);
  HDC hdcMem = CreateCompatibleDC(hdcScreen);
  HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, w, h);
  HGDIOBJ oldBitmap = SelectObject(hdcMem, hBitmap);

  // PrintWindow is generally better than BitBlt for overlapping windows/DWM
  // PW_RENDERFULLCONTENT (0x00000002) is available on Windows 8.1+
  // PW_CLIENTONLY (0x00000001)

  bool result = PrintWindow(hwnd, hdcMem, 2); // 2 = PW_RENDERFULLCONTENT
  if (!result) {
    // Fallback to standard PrintWindow
    result = PrintWindow(hwnd, hdcMem, 0);
  }

  // If PrintWindow fails or returns black, sometimes BitBlt is needed (but
  // BitBlt captures passing windows)

  QPixmap pixmap;
  if (result) {
    // Convert HBITMAP to QPixmap
    // Qt's fromWinHBITMAP handles this
    pixmap = QPixmap::fromImage(QImage::fromHBITMAP(hBitmap));
  }

  SelectObject(hdcMem, oldBitmap);
  DeleteObject(hBitmap);
  DeleteDC(hdcMem);
  ReleaseDC(NULL, hdcScreen);

  return pixmap;
}
