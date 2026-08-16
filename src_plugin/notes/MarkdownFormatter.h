#pragma once

#include <QString>

class MarkdownFormatter {
public:
  /**
   * @brief Format markdown content to HTML for display in RichText.
   * @param content Raw markdown text.
   * @param darkMode Whether dark theme is active.
   * @return HTML-formatted string.
   */
  static QString format(const QString &content, bool darkMode);

  /**
   * @brief Format wiki links only.
   */
  static QString formatWikiLinks(const QString &content);

private:
  static QString formatBold(const QString &text);
  static QString formatItalic(const QString &text);
  static QString formatStrikethrough(const QString &text);
  static QString formatInlineCode(const QString &text, bool darkMode);
  static QString formatHighlight(const QString &text, bool darkMode);
  static QString formatWikiLinksInternal(const QString &text);
};
