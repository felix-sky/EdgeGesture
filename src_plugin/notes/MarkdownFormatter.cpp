#include "MarkdownFormatter.h"
#include <QRegularExpression>
#include <QUrl>

QString MarkdownFormatter::format(const QString &content, bool darkMode) {
  QString result = content;
  
  result = formatBold(result);

  result = formatItalic(result);

  result = formatStrikethrough(result);

  result = formatHighlight(result, darkMode);

  result = formatInlineCode(result, darkMode);

  result = formatWikiLinksInternal(result);

  return result;
}

QString MarkdownFormatter::formatWikiLinks(const QString &content) {
  return formatWikiLinksInternal(content);
}

QString MarkdownFormatter::formatBold(const QString &text) {
  QString result = text;

  // **bold** syntax
  static QRegularExpression boldAsterisk(R"(\*\*([^*]+)\*\*)");
  result.replace(boldAsterisk, QStringLiteral("<b>\\1</b>"));

  // __bold__ syntax
  static QRegularExpression boldUnderscore(R"(__([^_]+)__)");
  result.replace(boldUnderscore, QStringLiteral("<b>\\1</b>"));

  return result;
}

QString MarkdownFormatter::formatItalic(const QString &text) {
  QString result = text;

  // *italic* syntax (single asterisk, not double)
  static QRegularExpression italicAsterisk(R"(\*([^*]+)\*)");
  result.replace(italicAsterisk, QStringLiteral("<i>\\1</i>"));

  // _italic_ syntax (single underscore, not double)
  static QRegularExpression italicUnderscore(R"(_([^_]+)_)");
  result.replace(italicUnderscore, QStringLiteral("<i>\\1</i>"));

  return result;
}

QString MarkdownFormatter::formatStrikethrough(const QString &text) {
  QString result = text;

  // ~~strikethrough~~ syntax
  static QRegularExpression strikethrough(R"(~~([^~]+)~~)");
  result.replace(strikethrough, QStringLiteral("<s>\\1</s>"));

  return result;
}

QString MarkdownFormatter::formatInlineCode(const QString &text,
                                            bool darkMode) {
  QString result = text;

  // `code` syntax with theme-aware styling
  QString bgColor = darkMode ? QStringLiteral("rgba(255, 255, 255, 0.12)")
                             : QStringLiteral("rgba(0, 0, 0, 0.05)");
  QString textColor = darkMode ? QStringLiteral("rgba(0, 0, 0)")
                               : QStringLiteral("rgba(255, 255, 255)");

  QString replacement =
      QStringLiteral(
          "<code style=\"background-color: %1; color: %2; padding: 2px 4px; "
          "border-radius: 5px;\">\\1</code>")
          .arg(bgColor, textColor);

  static QRegularExpression inlineCode(R"(`([^`]+)`)");
  result.replace(inlineCode, replacement);

  return result;
}

QString MarkdownFormatter::formatHighlight(const QString &text, bool darkMode) {
  QString result = text;

  // ==highlight== syntax with theme-aware color
  QString highlightColor = darkMode ? QStringLiteral("rgba(255, 215, 0, 0.4)")
                                    : QStringLiteral("rgba(255, 255, 0, 0.5)");

  QString replacement =
      QStringLiteral("<span style=\"background-color: %1;\">\\1</span>")
          .arg(highlightColor);

  static QRegularExpression highlight(R"(==(.*?)==)");
  result.replace(highlight, replacement);

  return result;
}

QString MarkdownFormatter::formatWikiLinksInternal(const QString &text) {
  QString result = text;

  // [[Link|Alias]], [[Link#anchor]], [[Link#anchor|Alias]], [[Link]]
  static QRegularExpression wikiLink(R"(\[\[([^\]]+)\]\])");

  QRegularExpressionMatchIterator it = wikiLink.globalMatch(text);
  QVector<QPair<int, int>> replacements; // (start, length)
  QStringList htmlLinks;

  while (it.hasNext()) {
    QRegularExpressionMatch match = it.next();
    QString p1 = match.captured(1);
    QString linkTarget = p1;
    QString displayText = p1;

    // Check for alias syntax: [[target|alias]]
    int pipeIndex = p1.indexOf('|');
    if (pipeIndex != -1) {
      linkTarget = p1.left(pipeIndex);
      displayText = p1.mid(pipeIndex + 1);
    } else {
      // No alias - check for anchor and format display
      int hashIndex = p1.indexOf('#');
      if (hashIndex != -1) {
        QString pagePart = p1.left(hashIndex);
        QString anchorPart = p1.mid(hashIndex + 1);

        if (pagePart.isEmpty()) {
          // Self-reference like [[#heading]] or [[#^block-id]]
          if (anchorPart.startsWith('^')) {
            displayText = anchorPart.mid(1) + QStringLiteral(" (block)");
          } else {
            displayText = anchorPart;
          }
        } else {
          // Reference to another page with anchor
          if (anchorPart.startsWith('^')) {
            displayText = pagePart + QStringLiteral(" › ") + anchorPart.mid(1);
          } else {
            displayText = pagePart + QStringLiteral(" › ") + anchorPart;
          }
        }
      }
    }

    QString htmlLink = QStringLiteral("<a href=\"%1\">%2</a>")
                           .arg(QUrl::toPercentEncoding(linkTarget),
                                displayText.toHtmlEscaped());

    replacements.append({match.capturedStart(), match.capturedLength()});
    htmlLinks.append(htmlLink);
  }

  // Replace in reverse order to preserve positions
  for (int i = replacements.size() - 1; i >= 0; --i) {
    result.replace(replacements[i].first, replacements[i].second, htmlLinks[i]);
  }

  return result;
}
