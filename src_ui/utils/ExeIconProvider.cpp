#include "ExeIconProvider.h"
#include <QDebug>

ExeIconProvider::ExeIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

QPixmap ExeIconProvider::requestPixmap(const QString &id, QSize *size,
                                       const QSize &requestedSize) {
  // id is the path to the exe
  int width = requestedSize.width() > 0 ? requestedSize.width() : 48;
  int height = requestedSize.height() > 0 ? requestedSize.height() : 48;

  if (size) {
    *size = QSize(width, height);
  }

  QFileInfo fileInfo(id);
  if (!fileInfo.exists()) {
    qDebug() << "[ImageProvider] File not found:" << id;
    return QPixmap();
  }

  QFileIconProvider provider;
  QIcon icon = provider.icon(fileInfo);

  // Fallback/Check if icon is empty (some svgs or special files might fail)
  if (icon.isNull()) {
    qDebug() << "[ImageProvider] Failed to extract icon:" << id;
    return QPixmap();
  }

  return icon.pixmap(width, height);
}
