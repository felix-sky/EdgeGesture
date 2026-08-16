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
      return QStringLiteral("heading");
    case BlockType::Code:
      return QStringLiteral("code");
    case BlockType::Quote:
      return QStringLiteral("quote");
    case BlockType::Callout:
      return QStringLiteral("callout");
    case BlockType::TaskList:
      return QStringLiteral("tasklist");
    case BlockType::List:
      return QStringLiteral("list");
    case BlockType::Embed:
      return QStringLiteral("embed");
    case BlockType::Image:
      return QStringLiteral("image");
    case BlockType::ThematicBreak:
      return QStringLiteral("divider");
    case BlockType::Table:
      return QStringLiteral("table");
    case BlockType::Paragraph:
    default:
      return QStringLiteral("paragraph");
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
    return MarkdownFormatter::format(block.content, m_darkMode);
  case RawRole:
    return block.raw;
  case FoldStateRole:
    return block.metadata.value(QStringLiteral("foldState"), QString());
  case IsFoldableRole:
    return block.metadata.value(QStringLiteral("isFoldable"), false);
  case IsCollapsedRole:
    return block.metadata.value(QStringLiteral("isCollapsed"), false);
  }

  return QVariant();
}

bool NoteBlockModel::setData(const QModelIndex &index, const QVariant &value, int role) {
  if (!index.isValid() || index.row() >= m_blocks.size())
    return false;

  if (role == IsCollapsedRole) {
    m_blocks[index.row()].metadata[QStringLiteral("isCollapsed")] = value.toBool();
    emit dataChanged(index, index, {IsCollapsedRole});
    return true;
  }
  return false;
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
  roles[RawRole] = "raw";
  roles[FoldStateRole] = "foldState";
  roles[IsFoldableRole] = "isFoldable";
  roles[IsCollapsedRole] = "isCollapsed";
  return roles;
}

void NoteBlockModel::loadMarkdown(const QString &content) {
  if (m_watcher->isRunning()) {
    m_watcher->cancel();
    m_watcher->waitForFinished();
  }

  m_originalMarkdown = content;
  m_isModified = false;
  emit isModifiedChanged();

  m_loading = true;
  emit loadingChanged();

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

  m_isModified = true;
  emit isModifiedChanged();

  m_blocks[row].isModified = true;
  QString finalContent = text;

  // Paragraph type conversions
  if (m_blocks[row].type == BlockType::Paragraph) {
    static const QRegularExpression codeBlockTrigger(QStringLiteral("^```(\\w*)$"));
    QRegularExpressionMatch match = codeBlockTrigger.match(text.trimmed());
    if (match.hasMatch()) {
      m_blocks[row].type = BlockType::Code;
      m_blocks[row].language = match.captured(1);
      m_blocks[row].content = QString();
      m_blocks[row].raw = QStringLiteral("```") + m_blocks[row].language + QStringLiteral("\n\n```");
      QVector<int> roles = {TypeRole, ContentRole, LanguageRole, RawRole};
      emit dataChanged(createIndex(row, 0), createIndex(row, 0), roles);
      return;
    }

    QString trimmed = text.trimmed();
    if (trimmed == QStringLiteral("---")) {
      m_blocks[row].type = BlockType::ThematicBreak;
      m_blocks[row].content = QString();
      m_blocks[row].raw = QStringLiteral("---");
      QVector<int> roles = {TypeRole, ContentRole, RawRole};
      emit dataChanged(createIndex(row, 0), createIndex(row, 0), roles);
      return;
    }

    static const QRegularExpression ulRegex(QStringLiteral("^[\\*\\-]\\s(.*)$"));
    QRegularExpressionMatch ulMatch = ulRegex.match(trimmed);
    if (ulMatch.hasMatch()) {
      m_blocks[row].type = BlockType::List;
      m_blocks[row].content = ulMatch.captured(1);
      m_blocks[row].metadata[QStringLiteral("listType")] = QStringLiteral("bullet");
      m_blocks[row].raw = trimmed;
      QVector<int> roles = {TypeRole, ContentRole, MetadataRole, RawRole};
      emit dataChanged(createIndex(row, 0), createIndex(row, 0), roles);
      return;
    }

    static const QRegularExpression olRegex(QStringLiteral("^(\\d+)\\.\\s(.*)$"));
    QRegularExpressionMatch olMatch = olRegex.match(trimmed);
    if (olMatch.hasMatch()) {
      m_blocks[row].type = BlockType::List;
      m_blocks[row].content = olMatch.captured(2);
      m_blocks[row].metadata[QStringLiteral("listType")] = QStringLiteral("ordered");
      m_blocks[row].raw = trimmed;
      QVector<int> roles = {TypeRole, ContentRole, MetadataRole, RawRole};
      emit dataChanged(createIndex(row, 0), createIndex(row, 0), roles);
      return;
    }

    static const QRegularExpression hRegex(QStringLiteral("^(#{1,6})\\s(.*)$"));
    QRegularExpressionMatch hMatch = hRegex.match(trimmed);
    if (hMatch.hasMatch()) {
      m_blocks[row].type = BlockType::Heading;
      m_blocks[row].level = hMatch.captured(1).length();
      m_blocks[row].content = hMatch.captured(2);
      m_blocks[row].raw = trimmed;
      QVector<int> roles = {TypeRole, ContentRole, LevelRole, RawRole};
      emit dataChanged(createIndex(row, 0), createIndex(row, 0), roles);
      return;
    }
  } else if (m_blocks[row].type == BlockType::ThematicBreak) {
    if (text.trimmed() != QStringLiteral("---")) {
      m_blocks[row].type = BlockType::Paragraph;
    }
  }

  m_blocks[row].content = finalContent;
  m_blocks[row].raw = finalContent;
  QVector<int> roles = {ContentRole, RawRole, FormattedContentRole};
  emit dataChanged(createIndex(row, 0), createIndex(row, 0), roles);
}

void NoteBlockModel::insertBlock(int row, const QString &typeString,
                                 const QString &content) {
  if (row < 0 || row > m_blocks.size())
    return;

  m_isModified = true;
  emit isModifiedChanged();

  beginInsertRows(QModelIndex(), row, row);

  NoteBlock block;
  block.content = content;
  block.level = 1;
  block.isModified = true;

  if (typeString == QStringLiteral("heading")) {
    block.type = BlockType::Heading;
    block.raw = QStringLiteral("# ") + content;
  } else if (typeString == QStringLiteral("code")) {
    block.type = BlockType::Code;
    block.raw = QStringLiteral("```\n") + content + QStringLiteral("\n```");
  } else if (typeString == QStringLiteral("quote")) {
    block.type = BlockType::Quote;
    block.raw = QStringLiteral("> ") + content;
  } else if (typeString == QStringLiteral("callout")) {
    block.type = BlockType::Callout;
    block.metadata[QStringLiteral("calloutType")] = QStringLiteral("note");
    block.metadata[QStringLiteral("title")] = QStringLiteral("Note");
    block.raw = QStringLiteral("> [!NOTE]\n> ") + content;
  } else if (typeString == QStringLiteral("tasklist")) {
    block.type = BlockType::TaskList;
    block.metadata[QStringLiteral("checked")] = false;
    block.metadata[QStringLiteral("taskStatus")] = QStringLiteral(" ");
    block.raw = QStringLiteral("- [ ] ") + content;
  } else if (typeString == QStringLiteral("list")) {
    block.type = BlockType::List;
    block.metadata[QStringLiteral("listType")] = QStringLiteral("bullet");
    block.raw = QStringLiteral("- ") + content;
  } else if (typeString == QStringLiteral("embed") || typeString == QStringLiteral("image")) {
    block.type = (typeString == QStringLiteral("image")) ? BlockType::Image : BlockType::Embed;
    block.raw = QStringLiteral("![[") + content + QStringLiteral("]]");
  } else if (typeString == QStringLiteral("divider")) {
    block.type = BlockType::ThematicBreak;
    block.raw = QStringLiteral("---");
  } else {
    block.type = BlockType::Paragraph;
    block.raw = content;
  }

  m_blocks.insert(row, block);
  endInsertRows();
}

void NoteBlockModel::replaceBlock(int row, const QString &text) {
  if (row < 0 || row >= m_blocks.size())
    return;

  m_isModified = true;
  emit isModifiedChanged();

  QVector<NoteBlock> newBlocks = MarkdownParser::parse(text);

  if (newBlocks.isEmpty()) {
    NoteBlock emptyBlock;
    emptyBlock.type = BlockType::Paragraph;
    emptyBlock.content = QString();
    emptyBlock.raw = QString();
    emptyBlock.isModified = true;
    newBlocks.append(emptyBlock);
  }

  for (auto &b : newBlocks) {
    b.isModified = true;
  }

  if (newBlocks.size() == 1) {
    m_blocks[row] = newBlocks.first();
    QVector<int> roles = {TypeRole, ContentRole, LevelRole, MetadataRole,
                          LanguageRole, RawRole, FormattedContentRole,
                          FoldStateRole, IsFoldableRole, IsCollapsedRole};
    emit dataChanged(createIndex(row, 0), createIndex(row, 0), roles);
    return;
  }

  beginRemoveRows(QModelIndex(), row, row);
  m_blocks.removeAt(row);
  endRemoveRows();

  beginInsertRows(QModelIndex(), row, row + newBlocks.size() - 1);
  for (int i = 0; i < newBlocks.size(); ++i) {
    m_blocks.insert(row + i, newBlocks[i]);
  }
  endInsertRows();
}

void NoteBlockModel::removeBlock(int row) {
  if (row < 0 || row >= m_blocks.size())
    return;

  m_isModified = true;
  emit isModifiedChanged();

  beginRemoveRows(QModelIndex(), row, row);
  m_blocks.removeAt(row);
  endRemoveRows();
}

void NoteBlockModel::toggleCalloutFold(int row) {
  if (row < 0 || row >= m_blocks.size())
    return;

  if (m_blocks[row].type == BlockType::Callout) {
    bool current = m_blocks[row].metadata.value(QStringLiteral("isCollapsed"), false).toBool();
    m_blocks[row].metadata[QStringLiteral("isCollapsed")] = !current;
    emit dataChanged(createIndex(row, 0), createIndex(row, 0), {IsCollapsedRole});
  }
}

QString NoteBlockModel::getMarkdown() const {
  // If nothing in the document was modified, return exact original markdown (100% roundtrip fidelity)
  if (!m_isModified) {
    return m_originalMarkdown;
  }

  QString result;
  for (int i = 0; i < m_blocks.size(); ++i) {
    const NoteBlock &block = m_blocks.at(i);

    // If block was unmodified and has raw source, use it directly
    if (!block.isModified && !block.raw.isEmpty()) {
      result.append(block.raw);
      result.append(QStringLiteral("\n\n"));
      continue;
    }

    // Reconstruct modified block
    switch (block.type) {
    case BlockType::Heading:
      result.append(QString(block.level, QLatin1Char('#')) + QStringLiteral(" ") + block.content + QStringLiteral("\n\n"));
      break;

    case BlockType::Code: {
      QString code = block.content;
      while (code.startsWith(QLatin1Char('\n')) || code.startsWith(QLatin1Char('\r'))) {
        code = code.mid(1);
      }
      while (code.endsWith(QLatin1Char('\n')) || code.endsWith(QLatin1Char('\r'))) {
        code.chop(1);
      }
      result.append(QStringLiteral("```") + block.language + QStringLiteral("\n") + code + QStringLiteral("\n```\n\n"));
      break;
    }

    case BlockType::Quote: {
      QStringList lines = block.content.split(QLatin1Char('\n'));
      for (const QString &line : lines) {
        result.append(QStringLiteral("> ") + line + QStringLiteral("\n"));
      }
      result.append(QStringLiteral("\n"));
      break;
    }

    case BlockType::Callout: {
      QString calloutType = block.metadata.value(QStringLiteral("calloutType"), QStringLiteral("NOTE")).toString().toUpper();
      QString foldState = block.metadata.value(QStringLiteral("foldState"), QString()).toString();
      QString title = block.metadata.value(QStringLiteral("title"), QString()).toString();

      result.append(QStringLiteral("> [!") + calloutType + QStringLiteral("]") + foldState +
                    (title.isEmpty() ? QString() : (QStringLiteral(" ") + title)) + QStringLiteral("\n"));

      QStringList lines = block.content.split(QLatin1Char('\n'));
      for (const QString &line : lines) {
        result.append(QStringLiteral("> ") + line + QStringLiteral("\n"));
      }
      result.append(QStringLiteral("\n"));
      break;
    }

    case BlockType::TaskList: {
      QString mark = block.metadata.value(QStringLiteral("taskStatus"), QString()).toString();
      if (mark.isEmpty()) {
        mark = block.metadata.value(QStringLiteral("checked"), false).toBool() ? QStringLiteral("x") : QStringLiteral(" ");
      }
      result.append(QStringLiteral("- [") + mark + QStringLiteral("] ") + block.content + QStringLiteral("\n"));
      break;
    }

    case BlockType::Embed:
    case BlockType::Image:
      result.append(QStringLiteral("![[") + block.content + QStringLiteral("]]\n\n"));
      break;

    case BlockType::List: {
      bool isOrdered = (block.metadata.value(QStringLiteral("listType")).toString() == QStringLiteral("ordered"));
      result.append((isOrdered ? QStringLiteral("1. ") : QStringLiteral("- ")) + block.content + QStringLiteral("\n"));
      break;
    }

    case BlockType::ThematicBreak:
      result.append(QStringLiteral("---\n\n"));
      break;

    case BlockType::Table: {
      QVariantList rows = block.metadata.value(QStringLiteral("rows")).toList();
      for (int r = 0; r < rows.size(); ++r) {
        QVariantList cells = rows[r].toList();
        result.append(QLatin1Char('|'));
        for (const QVariant &cell : cells) {
          result.append(QStringLiteral(" ") + cell.toString() + QStringLiteral(" |"));
        }
        result.append(QLatin1Char('\n'));
        if (r == 0 && !cells.isEmpty()) {
          result.append(QLatin1Char('|'));
          for (int c = 0; c < cells.size(); ++c) {
            result.append(QStringLiteral(" --- |"));
          }
          result.append(QLatin1Char('\n'));
        }
      }
      result.append(QStringLiteral("\n"));
      break;
    }

    case BlockType::Paragraph:
    default:
      result.append(block.content + QStringLiteral("\n\n"));
      break;
    }
  }

  return result.trimmed();
}

void NoteBlockModel::setDarkMode(bool dark) {
  if (m_darkMode != dark) {
    m_darkMode = dark;
    if (!m_blocks.isEmpty()) {
      emit dataChanged(createIndex(0, 0), createIndex(m_blocks.size() - 1, 0),
                       {FormattedContentRole});
    }
  }
}
