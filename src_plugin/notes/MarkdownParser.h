#ifndef MARKDOWNPARSER_H
#define MARKDOWNPARSER_H

#include "NoteBlock.h"
#include <QString>
#include <QVector>


class MarkdownParser {
public:
  /**
   * @brief Parses markdown text into a list of structured blocks.
   * @param markdown The raw markdown string.
   * @return A vector of NoteBlock objects.
   */
  static QVector<NoteBlock> parse(const QString &markdown);
};

#endif // MARKDOWNPARSER_H
