#ifndef NOTEBLOCK_H
#define NOTEBLOCK_H

#include <QHash>
#include <QString>
#include <QVariantMap>

enum class BlockType {
  Paragraph,
  Heading,
  Code,
  Quote,
  Callout,
  List,
  TaskList,
  Embed,    // ![[Note#Section]] or ![[Note]] as block
  Image,    // ![[image.png]] or ![alt](url)
  ThematicBreak, // Divider ---
  Reference,     // | text
  Math,
  Table
};

struct NoteBlock {
  int id = -1;
  BlockType type = BlockType::Paragraph;
  QString raw;      // Exact raw source markdown for non-destructive roundtrip
  QString content;  // Parsed content text
  QVariantMap metadata;
  int level = 0;    // For Headings (1-6)
  QString language; // For Code blocks
  bool isModified = false;

  // UI Helpers
  int heightHint = 0;
};

#endif // NOTEBLOCK_H
