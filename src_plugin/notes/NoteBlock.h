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
  List,
  Image,
  ThematicBreak,
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

// Helper for QML enum exposure (if needed, or just use strings/ints)
// We'll likely map these to role names or integer types in the Model.

#endif // NOTEBLOCK_H
