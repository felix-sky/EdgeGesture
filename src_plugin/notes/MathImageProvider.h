#ifndef MATHIMAGEPROVIDER_H
#define MATHIMAGEPROVIDER_H

#include <QImage>
#include <QQuickImageProvider>


class MathImageProvider : public QQuickImageProvider {
public:
  MathImageProvider();
  ~MathImageProvider() override;

  QImage requestImage(const QString &id, QSize *size,
                      const QSize &requestedSize) override;

private:
  void ensureInit();
  static bool s_initialized;
};

#endif // MATHIMAGEPROVIDER_H
