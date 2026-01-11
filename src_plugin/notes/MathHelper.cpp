#include "MathHelper.h"
#include "jkqtmathtext/jkqtmathtext.h"

#include <QBuffer>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QQmlEngine>
#include <QRegularExpression>

// Static regex patterns initialized once
const QRegularExpression
    MathHelper::s_blockMathRegex(QStringLiteral("\\$\\$([\\s\\S]+?)\\$\\$"));
const QRegularExpression
    MathHelper::s_inlineMathRegex(QStringLiteral("\\$([^\\$\\n]+?)\\$"));

MathHelper *MathHelper::s_instance = nullptr;

MathHelper::MathHelper(QObject *parent)
    : QObject(parent), m_placeholderIndex(0) {
  ensureInit();
}

MathHelper::~MathHelper() = default;

MathHelper *MathHelper::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine) {
  Q_UNUSED(qmlEngine);
  Q_UNUSED(jsEngine);
  return instance();
}

MathHelper *MathHelper::instance() {
  if (!s_instance) {
    s_instance = new MathHelper();
  }
  return s_instance;
}

void MathHelper::ensureInit() {
  if (!m_mathText) {
    m_mathText = std::make_unique<JKQTMathText>();
    m_mathText->useXITS();
  }
}

QString MathHelper::renderToBase64(const QString &latex, double fontSize,
                                   const QString &colorName) {
  if (latex.trimmed().isEmpty()) {
    return QString();
  }

  ensureInit();

  m_mathText->setFontSize(fontSize);
  QColor color(colorName);
  if (!color.isValid()) {
    color = Qt::black;
  }
  m_mathText->setFontColor(color);

  bool success = m_mathText->parse(
      latex, JKQTMathText::LatexParser,
      JKQTMathText::ParseOptions{JKQTMathText::StartWithMathMode,
                                 JKQTMathText::AddSpaceBeforeAndAfter});

  if (!success) {
    qWarning() << "MathHelper: Failed to parse LaTeX:" << latex;
    return QString();
  }

  QImage image =
      m_mathText->drawIntoImage(false, QColor(Qt::transparent), 4, 2.0, 96);

  if (image.isNull()) {
    qWarning() << "MathHelper: Failed to render LaTeX to image:" << latex;
    return QString();
  }

  QByteArray byteArray;
  QBuffer buffer(&byteArray);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");

  return QString::fromLatin1(byteArray.toBase64());
}

QString MathHelper::renderBlockMath(const QString &latex, double fontSize,
                                    const QString &colorName) {
  double blockRenderSize = fontSize * 1.5;
  QString base64 = renderToBase64(latex.trimmed(), blockRenderSize, colorName);
  if (base64.isEmpty()) {
    return QStringLiteral("$$%1$$").arg(latex);
  }
  return QStringLiteral(
             "<br><img src=\"data:image/png;base64,%1\" "
             "style=\"display:block;margin-left:auto;margin-right:auto;max-"
             "width:300px;\" /><br>")
      .arg(base64);
}

QString MathHelper::renderInlineMath(const QString &latex, double fontSize,
                                     const QString &colorName) {
  double renderSize = fontSize * 2.0;
  double displayHeight = fontSize * 1.1;
  QString base64 = renderToBase64(latex.trimmed(), renderSize, colorName);
  if (base64.isEmpty()) {
    return QStringLiteral("$%1$").arg(latex);
  }
  return QStringLiteral("<img src=\"data:image/png;base64,%1\" height=\"%2\" "
                        "style=\"vertical-align:-5px;\" />")
      .arg(base64)
      .arg(displayHeight);
}

void MathHelper::clearPlaceholders() {
  m_placeholders.clear();
  m_placeholderIndex = 0;
}

QString MathHelper::processMathToPlaceholders(const QString &text,
                                              double fontSize, bool darkMode) {
  clearPlaceholders();

  QString colorName =
      darkMode ? QStringLiteral("white") : QStringLiteral("black");
  QString result = text;

  // Process block math first ($$...$$)
  QRegularExpressionMatchIterator blockIt =
      s_blockMathRegex.globalMatch(result);
  QList<QPair<int, int>> blockRanges;
  QStringList blockReplacements;

  while (blockIt.hasNext()) {
    QRegularExpressionMatch match = blockIt.next();
    QString latex = match.captured(1);
    QString rendered = renderBlockMath(latex, fontSize, colorName);
    QString placeholder =
        QStringLiteral("%%MATHTOKEN:%1%%").arg(m_placeholderIndex++);
    m_placeholders.append(rendered);
    blockRanges.append(
        qMakePair(match.capturedStart(), match.capturedLength()));
    blockReplacements.append(placeholder);
  }

  // Apply block replacements in reverse order to preserve indices
  for (int i = blockRanges.size() - 1; i >= 0; --i) {
    result.replace(blockRanges[i].first, blockRanges[i].second,
                   blockReplacements[i]);
  }

  // Process inline math ($...$)
  QRegularExpressionMatchIterator inlineIt =
      s_inlineMathRegex.globalMatch(result);
  QList<QPair<int, int>> inlineRanges;
  QStringList inlineReplacements;

  while (inlineIt.hasNext()) {
    QRegularExpressionMatch match = inlineIt.next();
    QString latex = match.captured(1);
    QString rendered = renderInlineMath(latex, fontSize, colorName);
    QString placeholder =
        QStringLiteral("%%MATHTOKEN:%1%%").arg(m_placeholderIndex++);
    m_placeholders.append(rendered);
    inlineRanges.append(
        qMakePair(match.capturedStart(), match.capturedLength()));
    inlineReplacements.append(placeholder);
  }

  // Apply inline replacements in reverse order
  for (int i = inlineRanges.size() - 1; i >= 0; --i) {
    result.replace(inlineRanges[i].first, inlineRanges[i].second,
                   inlineReplacements[i]);
  }

  return result;
}

QString MathHelper::restoreMathPlaceholders(const QString &text) {
  QString result = text;
  for (int i = 0; i < m_placeholders.size(); ++i) {
    QString placeholder = QStringLiteral("%%MATHTOKEN:%1%%").arg(i);
    result.replace(placeholder, m_placeholders[i]);
  }
  return result;
}
