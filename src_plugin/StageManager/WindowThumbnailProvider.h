#pragma once

#include <QPixmap>
#include <QQuickImageProvider>
#include <windows.h>


class WindowThumbnailProvider : public QQuickImageProvider {
public:
  WindowThumbnailProvider();

  QPixmap requestPixmap(const QString &id, QSize *size,
                        const QSize &requestedSize) override;

private:
  QPixmap captureWindow(HWND hwnd);
};
