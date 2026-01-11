#ifndef MATHITEM_H
#define MATHITEM_H

#include <QColor>
#include <QPainter>
#include <QQuickPaintedItem>


namespace tex {
class TeXRender;
}

class MathItem : public QQuickPaintedItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QString latex READ latex WRITE setLatex NOTIFY latexChanged)
  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(
      qreal fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)

public:
  explicit MathItem(QQuickItem *parent = nullptr);
  ~MathItem() override;

  void paint(QPainter *painter) override;

  QString latex() const;
  void setLatex(const QString &latex);

  QColor color() const;
  void setColor(const QColor &color);

  qreal fontSize() const;
  void setFontSize(qreal size);

signals:
  void latexChanged();
  void colorChanged();
  void fontSizeChanged();

private:
  void updateRender();
  void checkInit();

  QString m_latex;
  QColor m_color = Qt::black;
  qreal m_fontSize = 20.0;
  tex::TeXRender *m_render = nullptr;
  static bool s_initialized;
};

#endif // MATHITEM_H
