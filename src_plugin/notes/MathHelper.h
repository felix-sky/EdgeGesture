#ifndef MATHHELPER_H
#define MATHHELPER_H

#include <QColor>
#include <QObject>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include <memory>

class JKQTMathText;
class QQmlEngine;
class QJSEngine;

/**
 * @brief QML Singleton helper for rendering LaTeX math to base64 image strings
 *
 * This class provides methods to render LaTeX math expressions to
 * base64-encoded PNG images that can be embedded in QML Text components using
 * RichText format.
 */
class MathHelper : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  explicit MathHelper(QObject *parent = nullptr);
  ~MathHelper() override;

  static MathHelper *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
  static MathHelper *instance();

  /**
   * @brief Render LaTeX math expression to a base64-encoded PNG string
   * @param latex The LaTeX math expression (without $ delimiters)
   * @param fontSize Font size in points
   * @param colorName Color name or hex code (e.g., "black", "#FFFFFF")
   * @return Base64-encoded PNG image data, or empty string on failure
   */
  Q_INVOKABLE QString renderToBase64(const QString &latex,
                                     double fontSize = 16.0,
                                     const QString &colorName = "black");

  /**
   * @brief Process text and replace math expressions with placeholders
   *
   * This method finds all $$...$$ (block) and $...$ (inline) math expressions
   * in the input text, renders them, and replaces them with placeholders.
   * Call restoreMathPlaceholders() after other markdown processing to restore.
   *
   * @param text The input text containing math expressions
   * @param fontSize Base font size for rendering
   * @param darkMode True for dark mode (white text), false for light mode
   * @return Text with math replaced by placeholders
   */
  Q_INVOKABLE QString processMathToPlaceholders(const QString &text,
                                                double fontSize, bool darkMode);

  /**
   * @brief Restore math placeholders with actual rendered content
   * @param text Text containing placeholders from processMathToPlaceholders()
   * @return Text with placeholders replaced by rendered math HTML
   */
  Q_INVOKABLE QString restoreMathPlaceholders(const QString &text);

  /**
   * @brief Clear stored placeholders (call at start of new text processing)
   */
  Q_INVOKABLE void clearPlaceholders();

private:
  void ensureInit();
  QString renderBlockMath(const QString &latex, double fontSize,
                          const QString &colorName);
  QString renderInlineMath(const QString &latex, double fontSize,
                           const QString &colorName);

  static MathHelper *s_instance;
  std::unique_ptr<JKQTMathText> m_mathText;

  // Placeholder storage
  QStringList m_placeholders;
  int m_placeholderIndex;

  // Static regex patterns for performance
  static const QRegularExpression s_blockMathRegex;
  static const QRegularExpression s_inlineMathRegex;
};

#endif // MATHHELPER_H
