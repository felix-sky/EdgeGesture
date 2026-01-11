#ifndef MATHRENDER_H
#define MATHRENDER_H

#include <QObject>
#include <QQmlEngine>
#include <QString>

class MathRender : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  explicit MathRender(QObject *parent = nullptr);
  ~MathRender();

  static MathRender *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
  static MathRender *instance();

  Q_INVOKABLE QString renderToSvg(const QString &latex, float fontSize = 20.0f,
                                  const QString &colorName = "black");

private:
  static MathRender *s_instance;
  static bool s_initialized;

  void ensureInit();
};

#endif // MATHRENDER_H
