#include "MathRenderItem.h"
#include "jkqtmathtext/jkqtmathtext.h"
#include <QDebug>
#include <QPainter>

MathRenderItem::MathRenderItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
  setAntialiasing(true);

  // Render to a Framebuffer Object (FBO) for caching to prevent from re-redner
  // when scrolling
  setRenderTarget(QQuickPaintedItem::FramebufferObject);

  m_mathText = std::make_unique<JKQTMathText>();
  m_mathText->useXITS();

  m_mathText->setFontSize(m_fontSize);
}

MathRenderItem::~MathRenderItem() = default;

void MathRenderItem::paint(QPainter *painter) {
  if (m_latex.isEmpty() || !m_mathText) {
    return;
  }

  // Re-parse if needed
  if (m_needsReparse) {
    parseLaTeX();
  }

  // High-quality rendering hints
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setRenderHint(QPainter::TextAntialiasing);
  painter->setRenderHint(QPainter::SmoothPixmapTransform);

  // Draw the math expression centered in the item's bounding rectangle
  m_mathText->draw(*painter, Qt::AlignCenter, QRectF(0, 0, width(), height()),
                   false);
}

QString MathRenderItem::latex() const { return m_latex; }

void MathRenderItem::setLatex(const QString &latex) {
  if (m_latex == latex) {
    return;
  }

  m_latex = latex;
  m_needsReparse = true;

  // Parse immediately to calculate size
  parseLaTeX();

  emit latexChanged();
  update();
}

double MathRenderItem::fontSize() const { return m_fontSize; }

void MathRenderItem::setFontSize(double size) {
  if (qFuzzyCompare(m_fontSize, size)) {
    return;
  }

  m_fontSize = size;
  m_mathText->setFontSize(size);
  m_needsReparse = true;
  parseLaTeX();

  emit fontSizeChanged();
  update();
}

QColor MathRenderItem::color() const { return m_color; }

void MathRenderItem::setColor(const QColor &color) {
  if (m_color == color) {
    return;
  }

  m_color = color;
  m_mathText->setFontColor(color);
  m_needsReparse = true;

  emit colorChanged();
  update();
}

void MathRenderItem::parseLaTeX() {
  if (m_latex.isEmpty() || !m_mathText) {
    setImplicitWidth(0);
    setImplicitHeight(0);
    return;
  }

  // Parse the LaTeX string
  // JKQTMathText expects math mode by default with $ delimiters,
  // but we want to parse raw LaTeX, so we use StartWithMathMode option
  bool success = m_mathText->parse(
      m_latex, JKQTMathText::LatexParser,
      JKQTMathText::ParseOptions{JKQTMathText::StartWithMathMode,
                                 JKQTMathText::AddSpaceBeforeAndAfter});

  if (!success) {
    qWarning() << "MathRenderItem: Failed to parse LaTeX:" << m_latex;
    // Try parsing as regular text as fallback
    m_mathText->parse(m_latex);
  }

  m_needsReparse = false;
  updateImplicitSize();
}

void MathRenderItem::updateImplicitSize() {
  if (!m_mathText) {
    return;
  }

  // Create a temporary painter to measure size
  // JKQTMathText requires a painter to calculate size
  QImage tempImage(1, 1, QImage::Format_ARGB32);
  QPainter tempPainter(&tempImage);

  QSizeF size = m_mathText->getSize(tempPainter);

  const double padding = 4.0;

  // Update QML layout engine with implicit size
  setImplicitWidth(size.width() + padding * 2);
  setImplicitHeight(size.height() + padding * 2);
}
