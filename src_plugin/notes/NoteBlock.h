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
  Callout, // New
  List,
  TaskList, // New
  Embed,    // New: ![[Note#Section]] as block
  Image,
  ThematicBreak, // Divider
  Reference,     // New: | text
  Math,
  Table // Future proofing
};

struct NoteBlock {
  int id = -1;
  BlockType type = BlockType::Paragraph;
  QString content;
  QVariantMap metadata;
  int level = 0;    // For Headings (1-6)
  QString language; // For Code blocks

  // UI Helpers
  int heightHint = 0;
};

#endif // NOTEBLOCK_H
