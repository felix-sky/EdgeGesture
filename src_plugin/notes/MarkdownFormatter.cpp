#include "MarkdownFormatter.h"
#include "ObsidianParser.h"
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
  static const QRegularExpression boldAsterisk(QStringLiteral("\\*\\*([^*]+)\\*\\*"));
  result.replace(boldAsterisk, QStringLiteral("<b>\\1</b>"));

  // __bold__ syntax
  static const QRegularExpression boldUnderscore(QStringLiteral("__([^_]+)__"));
  result.replace(boldUnderscore, QStringLiteral("<b>\\1</b>"));

  return result;
}

QString MarkdownFormatter::formatItalic(const QString &text) {
  QString result = text;

  // *italic* syntax
  static const QRegularExpression italicAsterisk(QStringLiteral("\\*([^*]+)\\*"));
  result.replace(italicAsterisk, QStringLiteral("<i>\\1</i>"));

  // _italic_ syntax
  static const QRegularExpression italicUnderscore(QStringLiteral("_([^_]+)_"));
  result.replace(italicUnderscore, QStringLiteral("<i>\\1</i>"));

  return result;
}

QString MarkdownFormatter::formatStrikethrough(const QString &text) {
  QString result = text;

  // ~~strikethrough~~ syntax
  static const QRegularExpression strikethrough(QStringLiteral("~~([^~]+)~~"));
  result.replace(strikethrough, QStringLiteral("<s>\\1</s>"));

  return result;
}

QString MarkdownFormatter::formatInlineCode(const QString &text, bool darkMode) {
  QString result = text;

  // Theme-aware styling:
  // Dark mode: readable light foreground on dark-tinted background
  // Light mode: readable dark foreground on light-tinted background
  QString bgColor = darkMode ? QStringLiteral("rgba(255, 255, 255, 0.12)")
                             : QStringLiteral("rgba(0, 0, 0, 0.06)");
  QString textColor = darkMode ? QStringLiteral("#e6edf3")
                               : QStringLiteral("#24292f");

  QString replacement =
      QStringLiteral(
          "<code style=\"background-color: %1; color: %2; padding: 2px 4px; "
          "border-radius: 4px; font-family: monospace;\">\\1</code>")
          .arg(bgColor, textColor);

  static const QRegularExpression inlineCode(QStringLiteral("`([^`]+)`"));
  result.replace(inlineCode, replacement);

  return result;
}

QString MarkdownFormatter::formatHighlight(const QString &text, bool darkMode) {
  QString result = text;

  // ==highlight== syntax with theme-aware color
  QString highlightColor = darkMode ? QStringLiteral("rgba(255, 215, 0, 0.35)")
                                    : QStringLiteral("rgba(255, 255, 0, 0.45)");

  QString replacement =
      QStringLiteral("<span style=\"background-color: %1; padding: 1px 2px; border-radius: 2px;\">\\1</span>")
          .arg(highlightColor);

  static const QRegularExpression highlight(QStringLiteral("==(.*?)=="));
  result.replace(highlight, replacement);

  return result;
}

QString MarkdownFormatter::formatWikiLinksInternal(const QString &text) {
  QString result = text;

  static const QRegularExpression wikiLink(QStringLiteral("\\[\\[([^\\]]+)\\]\\]"));
  QRegularExpressionMatchIterator it = wikiLink.globalMatch(text);
  QVector<QPair<int, int>> replacements;
  QStringList htmlLinks;

  while (it.hasNext()) {
    QRegularExpressionMatch match = it.next();
    QString rawInner = match.captured(0);
    ObsidianLink link = ObsidianParser::parseLink(rawInner);

    QString encodedTarget = QUrl::toPercentEncoding(link.target);
    if (!link.heading.isEmpty()) {
      encodedTarget += QStringLiteral("#") + QUrl::toPercentEncoding(link.heading);
    } else if (!link.blockId.isEmpty()) {
      encodedTarget += QStringLiteral("#^") + QUrl::toPercentEncoding(link.blockId);
    }

    QString htmlLink = QStringLiteral("<a href=\"%1\">%2</a>")
                           .arg(encodedTarget, link.displayText.toHtmlEscaped());

    replacements.append({static_cast<int>(match.capturedStart()), static_cast<int>(match.capturedLength())});
    htmlLinks.append(htmlLink);
  }

  // Replace in reverse order to preserve match offsets
  for (int i = replacements.size() - 1; i >= 0; --i) {
    result.replace(replacements[i].first, replacements[i].second, htmlLinks[i]);
  }

  return result;
}
