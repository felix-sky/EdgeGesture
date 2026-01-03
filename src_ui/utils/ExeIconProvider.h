#ifndef EXEICONPROVIDER_H
#define EXEICONPROVIDER_H

#include <QFileIconProvider>
#include <QFileInfo>
#include <QIcon>
#include <QPixmap>
#include <QQuickImageProvider>


class ExeIconProvider : public QQuickImageProvider {
public:
  ExeIconProvider();
  QPixmap requestPixmap(const QString &id, QSize *size,
                        const QSize &requestedSize) override;
};

#endif // EXEICONPROVIDER_H
