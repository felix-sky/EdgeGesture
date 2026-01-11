#ifndef MATHRENDERITEM_H
#define MATHRENDERITEM_H

#include <QColor>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

// Forward declaration
class JKQTMathText;

/**
 * @brief QML component for rendering LaTeX math formulas using JKQTMathText
 *
 * This is a QQuickPaintedItem that uses JKQTMathText to render LaTeX math
 * expressions directly onto the QML scenegraph using native QPainter
 * instructions.
 *
 * Usage in QML:
 * @code
 * MathRenderItem {
 *     latex: "\\frac{a}{b}"
 *     fontSize: 20
 *     color: "#000000"
 * }
 * @endcode
 */
class MathRenderItem : public QQuickPaintedItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(QString latex READ latex WRITE setLatex NOTIFY latexChanged)
  Q_PROPERTY(
      double fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

public:
  explicit MathRenderItem(QQuickItem *parent = nullptr);
  ~MathRenderItem() override;

  void paint(QPainter *painter) override;

  QString latex() const;
  void setLatex(const QString &latex);

  double fontSize() const;
  void setFontSize(double size);

  QColor color() const;
  void setColor(const QColor &color);

signals:
  void latexChanged();
  void fontSizeChanged();
  void colorChanged();

private:
  void updateImplicitSize();
  void parseLaTeX();

  std::unique_ptr<JKQTMathText> m_mathText;
  QString m_latex;
  double m_fontSize = 16.0;
  QColor m_color = Qt::black;
  bool m_needsReparse = true;
};

#endif // MATHRENDERITEM_H
