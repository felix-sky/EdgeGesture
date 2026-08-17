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

  if (requestedSize.isValid() && !pixmap.isNull()) {
    pixmap = pixmap.scaled(requestedSize, Qt::KeepAspectRatio,
                           Qt::SmoothTransformation);
  } else if (!pixmap.isNull() && (pixmap.width() > 640 || pixmap.height() > 480)) {
    pixmap = pixmap.scaled(640, 480, Qt::KeepAspectRatio,
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

  // PW_RENDERFULLCONTENT (0x00000002) for Win 8.1+
  bool result = PrintWindow(hwnd, hdcMem, 2);
  if (!result) {
    result = PrintWindow(hwnd, hdcMem, 0);
  }

  QPixmap pixmap;
  if (result) {
    pixmap = QPixmap::fromImage(QImage::fromHBITMAP(hBitmap));
  }

  SelectObject(hdcMem, oldBitmap);
  DeleteObject(hBitmap);
  DeleteDC(hdcMem);
  ReleaseDC(NULL, hdcScreen);

  return pixmap;
}
