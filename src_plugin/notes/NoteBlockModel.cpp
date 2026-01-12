#include "NoteBlockModel.h"
#include "MarkdownFormatter.h"
#include "MarkdownParser.h"
#include <QtConcurrent>

NoteBlockModel::NoteBlockModel(QObject *parent)
    : QAbstractListModel(parent),
      m_watcher(new QFutureWatcher<QVector<NoteBlock>>(this)) {
  connect(m_watcher, &QFutureWatcher<QVector<NoteBlock>>::finished, this,
          &NoteBlockModel::onParseFinished);
}

int NoteBlockModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_blocks.size();
}

QVariant NoteBlockModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_blocks.size())
    return QVariant();

  const NoteBlock &block = m_blocks.at(index.row());

  switch (role) {
  case TypeRole: {
    switch (block.type) {
    case BlockType::Heading:
      return "heading";
    case BlockType::Code:
      return "code";
    case BlockType::Quote:
      return "quote";
    case BlockType::Callout:
      return "callout";
    case BlockType::TaskList:
      return "tasklist";
    case BlockType::List:
      return "list";
    case BlockType::Embed:
      return "embed";
    case BlockType::Image:
      return "image";
    case BlockType::ThematicBreak:
      return "divider";
    case BlockType::Reference:
      return "reference";
    case BlockType::Paragraph:
    default:
      return "paragraph";
    }
  }
  case ContentRole:
    return block.content;
  case MetadataRole:
    return block.metadata;
  case LevelRole:
    return QVariant(block.level);
  case LanguageRole:
    return block.language;
  case HeightHintRole:
    return QVariant(block.heightHint);
  case FormattedContentRole:
    // Pre-render markdown to HTML for display (uses C++ regex for speed)
    return MarkdownFormatter::format(block.content, m_darkMode);
  }

  return QVariant();
}

QHash<int, QByteArray> NoteBlockModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[TypeRole] = "type";
  roles[ContentRole] = "content";
  roles[MetadataRole] = "metadata";
  roles[LevelRole] = "level";
  roles[LanguageRole] = "language";
  roles[HeightHintRole] = "heightHint";
  roles[FormattedContentRole] = "formattedContent";
  return roles;
}

void NoteBlockModel::loadMarkdown(const QString &content) {
  if (m_watcher->isRunning()) {
    m_watcher->cancel();
    m_watcher->waitForFinished();
  }

  m_loading = true;
  emit loadingChanged();

  // Run parser in background thread
  // Note: In strict QtConcurrent, we accept by value to avoid race conditions
  // on 'content'
  m_watcher->setFuture(QtConcurrent::run(
      [content]() { return MarkdownParser::parse(content); }));
}

void NoteBlockModel::onParseFinished() {
  beginResetModel();
  m_blocks = m_watcher->result();
  endResetModel();

  m_loading = false;
  emit loadingChanged();
}

void NoteBlockModel::updateBlock(int row, const QString &text) {
  if (row < 0 || row >= m_blocks.size())
    return;

  QString finalContent = text;

  // Check if this is a paragraph being converted to a code block
  // Pattern: ```language (e.g., ```python, ```cpp, ```js)
  if (m_blocks[row].type == BlockType::Paragraph) {
    static QRegularExpression codeBlockTrigger(R"(^```(\w*)$)");
    QRegularExpressionMatch match = codeBlockTrigger.match(text.trimmed());
    if (match.hasMatch()) {
      // Convert to code block
      m_blocks[row].type = BlockType::Code;
      m_blocks[row].language = match.captured(1); // Language (may be empty)
      m_blocks[row].content = ""; // Empty content, ready for code
      QVector<int> roles = {TypeRole, ContentRole, LanguageRole};
      emit dataChanged(createIndex(row, 0), createIndex(row, 0), roles);
      return;
    }
  }

  // Check for conversion from Paragraph to other types
  if (m_blocks[row].type == BlockType::Paragraph) {
    QString trimmed = text.trimmed();

    // Reference: | text
    if (trimmed.startsWith("| ")) {
      m_blocks[row].type = BlockType::Reference;
      m_blocks[row].content = trimmed.mid(2); // Strip "| "
      QVector<int> roles = {TypeRole, ContentRole};
      emit dataChanged(createIndex(row, 0), createIndex(row, 0), roles);
      return;
    }

    // Divider: ---
    if (trimmed == "---") {
      m_blocks[row].type = BlockType::ThematicBreak;
      m_blocks[row].content = "";
      QVector<int> roles = {TypeRole, ContentRole};
      emit dataChanged(createIndex(row, 0), createIndex(row, 0), roles);
      return;
    }

    // Unordered List: * text or - text
    static QRegularExpression ulRegex(R"(^[\*\-]\s(.*)$)");
    QRegularExpressionMatch ulMatch = ulRegex.match(trimmed);
    if (ulMatch.hasMatch()) {
      m_blocks[row].type = BlockType::List;
      m_blocks[row].content = ulMatch.captured(1);
      m_blocks[row].metadata["listType"] = "bullet";
      QVector<int> roles = {TypeRole, ContentRole, MetadataRole};
      emit dataChanged(createIndex(row, 0), createIndex(row, 0), roles);
      return;
    }

    // Ordered List: 1. text
    static QRegularExpression olRegex(R"(^(\d+)\.\s(.*)$)");
    QRegularExpressionMatch olMatch = olRegex.match(trimmed);
    if (olMatch.hasMatch()) {
      m_blocks[row].type = BlockType::List;
      m_blocks[row].content = olMatch.captured(2);
      m_blocks[row].metadata["listType"] = "ordered";
      QVector<int> roles = {TypeRole, ContentRole, MetadataRole};
      emit dataChanged(createIndex(row, 0), createIndex(row, 0), roles);
      return;
    }

    // Heading: # Text
    static QRegularExpression hRegex(R"(^(#{1,6})\s(.*)$)");
    QRegularExpressionMatch hMatch = hRegex.match(trimmed);
    if (hMatch.hasMatch()) {
      m_blocks[row].type = BlockType::Heading;
      m_blocks[row].level = hMatch.captured(1).length();
      m_blocks[row].content = hMatch.captured(2);
      QVector<int> roles = {TypeRole, ContentRole, LevelRole};
      emit dataChanged(createIndex(row, 0), createIndex(row, 0), roles);
      return;
    }
  } else if (m_blocks[row].type == BlockType::ThematicBreak) {
    if (text.trimmed() != "---") {
      m_blocks[row].type = BlockType::Paragraph;
    }
  } else if (m_blocks[row].type == BlockType::Reference) {
    // If updating a reference block, ensure we don't duplicate the pipe prefix
    // and always use trimmed content to prevent whitespace accumulation
    QString trimmed = text.trimmed();
    qDebug() << "ReferenceBlock: updateBlock called. Row:" << row;
    qDebug() << "ReferenceBlock: Input text:" << text;
    qDebug() << "ReferenceBlock: Trimmed text:" << trimmed;
    if (trimmed.startsWith("| ")) {
      finalContent = trimmed.mid(2).trimmed();
      qDebug() << "ReferenceBlock: Stripped '| ' prefix. finalContent:"
               << finalContent;
    } else if (trimmed.startsWith("|")) {
      finalContent = trimmed.mid(1).trimmed();
      qDebug() << "ReferenceBlock: Stripped '|' prefix. finalContent:"
               << finalContent;
    } else {
      // No pipe found, but still use trimmed content to avoid whitespace issues
      finalContent = trimmed;
      qDebug() << "ReferenceBlock: No pipe prefix, using trimmed. finalContent:"
               << finalContent;
    }
  }

  // Direct update for responsiveness
  m_blocks[row].content = finalContent;
  QVector<int> roles = {ContentRole};
  emit dataChanged(createIndex(row, 0), createIndex(row, 0), roles);
}

void NoteBlockModel::insertBlock(int row, const QString &typeString,
                                 const QString &content) {
  if (row < 0 || row > m_blocks.size())
    return;

  beginInsertRows(QModelIndex(), row, row);

  NoteBlock block;
  block.content = content;
  // block.metadata is already default constructed as empty QVariantMap
  block.level = 1;

  if (typeString == "heading")
    block.type = BlockType::Heading;
  else if (typeString == "code")
    block.type = BlockType::Code;
  else if (typeString == "quote")
    block.type = BlockType::Quote;
  else if (typeString == "callout")
    block.type = BlockType::Callout;
  else if (typeString == "tasklist")
    block.type = BlockType::TaskList;
  else if (typeString == "list")
    block.type = BlockType::List;
  else if (typeString == "embed")
    block.type = BlockType::Embed;
  else if (typeString == "image")
    block.type = BlockType::Image;
  else if (typeString == "divider")
    block.type = BlockType::ThematicBreak;
  else if (typeString == "reference")
    block.type = BlockType::Reference;
  else
    block.type = BlockType::Paragraph;

  m_blocks.insert(row, block);
  endInsertRows();
}

void NoteBlockModel::replaceBlock(int row, const QString &text) {
  if (row < 0 || row >= m_blocks.size())
    return;

  // Parse the new text
  // We use the static parse method directly
  QVector<NoteBlock> newBlocks = MarkdownParser::parse(text);

  if (newBlocks.isEmpty()) {
    // If text is empty or meaningless, maybe we should just remove the block?
    // Or replace with empty paragraph?
    // Let's assume empty text -> empty paragraph
    NoteBlock emptyBlock;
    emptyBlock.type = BlockType::Paragraph;
    emptyBlock.content = "";
    newBlocks.append(emptyBlock);
  }

  // Optimize for 1-to-1 replacement (common case)
  if (newBlocks.size() == 1) {
    m_blocks[row] = newBlocks.first();
    QVector<int> roles = {TypeRole, ContentRole, LevelRole, MetadataRole,
                          LanguageRole};
    emit dataChanged(createIndex(row, 0), createIndex(row, 0), roles);
    return;
  }

  // Remove old block
  beginRemoveRows(QModelIndex(), row, row);
  m_blocks.removeAt(row);
  endRemoveRows();

  // Insert new blocks
  if (!newBlocks.isEmpty()) {
    beginInsertRows(QModelIndex(), row, row + newBlocks.size() - 1);
    for (int i = 0; i < newBlocks.size(); ++i) {
      m_blocks.insert(row + i, newBlocks[i]);
    }
    endInsertRows();
  }
}

void NoteBlockModel::removeBlock(int row) {
  if (row < 0 || row >= m_blocks.size())
    return;

  beginRemoveRows(QModelIndex(), row, row);
  m_blocks.removeAt(row);
  endRemoveRows();
}

QString NoteBlockModel::getMarkdown() const {
  QString result;
  for (const NoteBlock &block : m_blocks) {
    // Reconstruct based on type
    switch (block.type) {
    case BlockType::Heading:
      result.append(QString(block.level, '#') + " " + block.content + "\n\n");
      break;
    case BlockType::Code:
      result.append("```" + block.language + "\n" + block.content +
                    "\n```\n\n");
      break;
    case BlockType::Quote: {
      QStringList lines = block.content.split('\n');
      for (const QString &line : lines) {
        result.append("> " + line + "\n");
      }
      result.append("\n");
    } break;
    case BlockType::Callout: {
      result.append("> [!" +
                    block.metadata["calloutType"].toString().toUpper() + "] " +
                    block.metadata["title"].toString() + "\n");
      QStringList lines = block.content.split('\n');
      for (const QString &line : lines) {
        result.append("> " + line + "\n");
      }
      result.append("\n");
    } break;
    case BlockType::TaskList: {
      QString mark = " ";
      if (block.metadata.contains("taskStatus")) {
        mark = block.metadata["taskStatus"].toString();
      } else {
        mark = block.metadata["checked"].toBool() ? "x" : " ";
      }
      result.append(QString("- [%1] %2\n").arg(mark).arg(block.content));
    } break;
    case BlockType::Embed:
      // Reconstruct ![[Note#Section]]
      result.append(QString("![[%1]]\n\n").arg(block.content));
      break;
    case BlockType::Image:

      // Identify if it was likely an obsidian link (no path separation) or md
      // link
      result.append(QString("![[%1]]\n\n").arg(block.content));
      break;
    case BlockType::List: {
      // Check metadata for ordered vs unordered
      bool isOrdered = block.metadata["listType"].toString() == "ordered";
      if (isOrdered) {
        // We could track index, but for now simple reconstruction:
        result.append("1. " + block.content + "\n");
      } else {
        result.append("* " + block.content + "\n");
      }
    } break;
    case BlockType::Reference: {
        QString cleanContent = block.content;
        while (cleanContent.startsWith("|")) {
            cleanContent = cleanContent.mid(1).trimmed();
        }
        result.append("| " + cleanContent + "\n\n");
        break;
    }
    case BlockType::ThematicBreak:
      result.append("---\n\n");
      break;
    default:
      result.append(block.content + "\n\n");
      break;
    }
  }
  return result.trimmed();
}

void NoteBlockModel::setDarkMode(bool dark) {
  if (m_darkMode != dark) {
    m_darkMode = dark;
    // Notify all rows that formatted content has changed
    if (!m_blocks.isEmpty()) {
      emit dataChanged(createIndex(0, 0), createIndex(m_blocks.size() - 1, 0),
                       {FormattedContentRole});
    }
  }
}
