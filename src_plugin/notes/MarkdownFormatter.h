#pragma once

#include <QString>

/**
 * @brief High-performance Markdown to HTML formatter.
 *
 * Replaces QML JavaScript regex processing with C++ QRegularExpression
 * for 10-50x faster text formatting. Used by NoteBlockModel to pre-render
 * content for display.
 *
 * Supported syntax:
 * - **bold** or __bold__
 * - *italic* or _italic_
 * - ~~strikethrough~~
 * - `inline code`
 * - ==highlight==
 * - [[wiki links]] with optional |alias and #anchor
 */
class MarkdownFormatter {
public:
  /**
   * @brief Format markdown content to HTML.
   * @param content Raw markdown text.
   * @param darkMode Whether dark theme is active (affects highlight/code
   * colors).
   * @return HTML-formatted string for display in Text/RichText.
   */
  static QString format(const QString &content, bool darkMode);

  /**
   * @brief Format wiki links only.
   * Useful when other formatting is handled separately.
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
