#pragma once

#include <QPixmap>
#include <QQuickImageProvider>
#include <windows.h>


class WinIconProvider : public QQuickImageProvider {
public:
  WinIconProvider();

  QPixmap requestPixmap(const QString &id, QSize *size,
                        const QSize &requestedSize) override;

private:
  QPixmap fromHICON(HICON icon);
};
