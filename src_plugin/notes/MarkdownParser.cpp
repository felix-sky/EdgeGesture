#include "MarkdownParser.h"
#include "ObsidianParser.h"
#include "md4c/src/md4c.h"
#include <QDebug>
#include <QRegularExpression>
#include <QStack>

// Context to pass to md4c callbacks
struct ParseContext {
  QVector<NoteBlock> blocks;
  NoteBlock currentBlock;
  bool inBlock = false;
  int inQuoteLevel = 0;   // Track nesting for Quotes/Callouts
  QStack<bool> listStack; // True = Ordered, False = Unordered
  QString currentTextBuffer;

  // Table parsing state
  bool inTable = false;
  bool inTableHeader = false;
  QVector<QVector<QString>> tableRows;
  QVector<QString> currentTableRow;
  QString currentCellBuffer;
};

// MD4C Callbacks
static int enter_block_callback(MD_BLOCKTYPE type, void *detail,
                                void *userdata) {
  ParseContext *ctx = static_cast<ParseContext *>(userdata);

  // If inside a quote, track nesting and keep collecting in quote buffer
  if (ctx->inQuoteLevel > 0) {
    if (type == MD_BLOCK_QUOTE) {
      ctx->inQuoteLevel++;
    }
    if (type == MD_BLOCK_P) {
      if (!ctx->currentTextBuffer.isEmpty() &&
          !ctx->currentTextBuffer.endsWith(QLatin1Char('\n'))) {
        ctx->currentTextBuffer.append(QStringLiteral("\n"));
      }
    }
    return 0;
  }

  // Start of a Top-Level Block
  NoteBlock newBlock;
  ctx->inBlock = true;
  ctx->currentTextBuffer.clear();

  switch (type) {
  case MD_BLOCK_H: {
    newBlock.type = BlockType::Heading;
    MD_BLOCK_H_DETAIL *h_detail = (MD_BLOCK_H_DETAIL *)detail;
    newBlock.level = h_detail->level;
    break;
  }
  case MD_BLOCK_CODE: {
    newBlock.type = BlockType::Code;
    MD_BLOCK_CODE_DETAIL *c_detail = (MD_BLOCK_CODE_DETAIL *)detail;
    if (c_detail->lang.text != NULL) {
      newBlock.language =
          QString::fromUtf8(c_detail->lang.text, c_detail->lang.size);
    }
    break;
  }
  case MD_BLOCK_QUOTE:
    newBlock.type = BlockType::Quote;
    ctx->inQuoteLevel = 1; // Start Quote Mode
    break;
  case MD_BLOCK_LI: {
    MD_BLOCK_LI_DETAIL *li_detail = (MD_BLOCK_LI_DETAIL *)detail;
    if (li_detail->is_task) {
      newBlock.type = BlockType::TaskList;
      QChar mark = QChar(li_detail->task_mark);
      newBlock.metadata[QStringLiteral("taskStatus")] = QString(mark);
      newBlock.metadata[QStringLiteral("checked")] = (mark == QLatin1Char('x') || mark == QLatin1Char('X'));
    } else {
      newBlock.type = BlockType::List;
    }
    break;
  }
  case MD_BLOCK_UL:
    ctx->listStack.push(false); // Unordered
    break;
  case MD_BLOCK_OL:
    ctx->listStack.push(true); // Ordered
    break;
  case MD_BLOCK_HR:
    newBlock.type = BlockType::ThematicBreak;
    break;
  case MD_BLOCK_TABLE:
    ctx->inTable = true;
    ctx->tableRows.clear();
    ctx->inBlock = false;
    break;
  case MD_BLOCK_THEAD:
    ctx->inTableHeader = true;
    break;
  case MD_BLOCK_TBODY:
    ctx->inTableHeader = false;
    break;
  case MD_BLOCK_TR:
    ctx->currentTableRow.clear();
    ctx->inBlock = false;
    break;
  case MD_BLOCK_TH:
  case MD_BLOCK_TD:
    ctx->currentCellBuffer.clear();
    ctx->inBlock = false;
    break;
  case MD_BLOCK_P:
    if (ctx->inTable) {
      ctx->inBlock = false;
      break;
    }
    newBlock.type = BlockType::Paragraph;
    break;
  default:
    ctx->inBlock = false;
    break;
  }

  if (ctx->inBlock) {
    ctx->currentBlock = newBlock;
  }
  return 0;
}

static int leave_block_callback(MD_BLOCKTYPE type, void *detail,
                                void *userdata) {
  Q_UNUSED(detail);
  ParseContext *ctx = static_cast<ParseContext *>(userdata);

  if (type == MD_BLOCK_UL || type == MD_BLOCK_OL) {
    if (!ctx->listStack.isEmpty())
      ctx->listStack.pop();
    return 0;
  }

  // Table cell/row/table ending handling
  if (type == MD_BLOCK_TH || type == MD_BLOCK_TD) {
    ctx->currentTableRow.append(ctx->currentCellBuffer.trimmed());
    ctx->currentCellBuffer.clear();
    return 0;
  }

  if (type == MD_BLOCK_TR) {
    if (!ctx->currentTableRow.isEmpty()) {
      ctx->tableRows.append(ctx->currentTableRow);
      ctx->currentTableRow.clear();
    }
    return 0;
  }

  if (type == MD_BLOCK_THEAD || type == MD_BLOCK_TBODY) {
    return 0;
  }

  if (type == MD_BLOCK_TABLE) {
    if (!ctx->tableRows.isEmpty()) {
      NoteBlock tableBlock;
      tableBlock.type = BlockType::Table;

      QVariantList rowsVariant;
      for (const auto &row : ctx->tableRows) {
        QVariantList cellsVariant;
        for (const QString &cell : row) {
          cellsVariant.append(cell);
        }
        rowsVariant.append(QVariant(cellsVariant));
      }
      tableBlock.metadata[QStringLiteral("rows")] = rowsVariant;

      // Reconstruct clean markdown representation for raw
      QString rawTable;
      for (int i = 0; i < ctx->tableRows.size(); ++i) {
        const auto &row = ctx->tableRows[i];
        rawTable.append(QLatin1Char('|'));
        for (const QString &cell : row) {
          rawTable.append(QStringLiteral(" ") + cell + QStringLiteral(" |"));
        }
        rawTable.append(QLatin1Char('\n'));
        if (i == 0 && !row.isEmpty()) {
          rawTable.append(QLatin1Char('|'));
          for (int j = 0; j < row.size(); ++j) {
            rawTable.append(QStringLiteral(" --- |"));
          }
          rawTable.append(QLatin1Char('\n'));
        }
      }
      tableBlock.raw = rawTable.trimmed();
      tableBlock.content = tableBlock.raw;

      ctx->blocks.append(tableBlock);
    }
    ctx->inTable = false;
    ctx->tableRows.clear();
    return 0;
  }

  // Quote / Callout logic
  if (type == MD_BLOCK_QUOTE) {
    if (ctx->inQuoteLevel > 0) {
      ctx->inQuoteLevel--;
      if (ctx->inQuoteLevel == 0) {
        QString fullQuoteText = ctx->currentTextBuffer.trimmed();
        CalloutInfo calloutInfo;

        if (ObsidianParser::parseCallout(fullQuoteText, calloutInfo)) {
          ctx->currentBlock.type = BlockType::Callout;
          ctx->currentBlock.content = calloutInfo.body;
          ctx->currentBlock.metadata[QStringLiteral("calloutType")] = calloutInfo.type;
          ctx->currentBlock.metadata[QStringLiteral("title")] = calloutInfo.title;
          ctx->currentBlock.metadata[QStringLiteral("foldState")] = calloutInfo.foldState;
          ctx->currentBlock.metadata[QStringLiteral("isFoldable")] = calloutInfo.isFoldable;
          ctx->currentBlock.metadata[QStringLiteral("isCollapsed")] = calloutInfo.isCollapsed;

          QString headerPrefix = QStringLiteral("> [!") + calloutInfo.type.toUpper() + QStringLiteral("]") +
                                 calloutInfo.foldState + QStringLiteral(" ") + calloutInfo.title;
          QString bodyPrefix = calloutInfo.body.isEmpty() ? QString() : (QStringLiteral("\n") +
                               QStringList(calloutInfo.body.split(QLatin1Char('\n'))).join(QStringLiteral("\n> ")));
          if (!calloutInfo.body.isEmpty()) {
            bodyPrefix = QStringLiteral("\n> ") + calloutInfo.body;
            bodyPrefix.replace(QStringLiteral("\n"), QStringLiteral("\n> "));
          }
          ctx->currentBlock.raw = headerPrefix + bodyPrefix;
        } else {
          ctx->currentBlock.type = BlockType::Quote;
          ctx->currentBlock.content = fullQuoteText;
          QStringList lines = fullQuoteText.split(QLatin1Char('\n'));
          QString rawQuote;
          for (const QString &line : lines) {
            rawQuote.append(QStringLiteral("> ") + line + QStringLiteral("\n"));
          }
          ctx->currentBlock.raw = rawQuote.trimmed();
        }

        ctx->blocks.append(ctx->currentBlock);
        ctx->inBlock = false;
        ctx->currentTextBuffer.clear();
      }
    }
    return 0;
  }

  if (ctx->inQuoteLevel > 0) {
    return 0;
  }

  // Standard Blocks
  bool shouldPush = false;
  switch (type) {
  case MD_BLOCK_LI:
    shouldPush = true;
    if (ctx->inBlock) {
      if (ctx->currentBlock.type == BlockType::List) {
        bool isOrdered = !ctx->listStack.isEmpty() && ctx->listStack.top();
        ctx->currentBlock.metadata[QStringLiteral("listType")] =
            isOrdered ? QStringLiteral("ordered") : QStringLiteral("bullet");
      }

      // Check for extended task statuses md4c might miss
      if (ctx->currentBlock.type == BlockType::List) {
        QString cleanContent = ctx->currentTextBuffer.trimmed();
        static const QRegularExpression taskRegex(QStringLiteral("^\\[(.)\\]\\s+(.*)$"));
        QRegularExpressionMatch match = taskRegex.match(cleanContent);
        if (match.hasMatch()) {
          ctx->currentBlock.type = BlockType::TaskList;
          QString mark = match.captured(1);
          ctx->currentBlock.metadata[QStringLiteral("taskStatus")] = mark;
          ctx->currentBlock.metadata[QStringLiteral("checked")] = (mark == QStringLiteral("x") || mark == QStringLiteral("X"));
          ctx->currentTextBuffer = match.captured(2).trimmed();
        }
      }

      if (ctx->currentBlock.type == BlockType::TaskList) {
        QString mark = ctx->currentBlock.metadata[QStringLiteral("taskStatus")].toString();
        if (mark.isEmpty()) {
          mark = ctx->currentBlock.metadata[QStringLiteral("checked")].toBool() ? QStringLiteral("x") : QStringLiteral(" ");
        }
        ctx->currentBlock.raw = QStringLiteral("- [") + mark + QStringLiteral("] ") + ctx->currentTextBuffer;
      } else {
        bool isOrdered = (ctx->currentBlock.metadata[QStringLiteral("listType")].toString() == QStringLiteral("ordered"));
        ctx->currentBlock.raw = (isOrdered ? QStringLiteral("1. ") : QStringLiteral("- ")) + ctx->currentTextBuffer;
      }
    }
    break;

  case MD_BLOCK_P: {
    QString clean = ctx->currentTextBuffer.trimmed();

    // Check for standalone Obsidian embed or markdown image
    static const QRegularExpression embedRegex(QStringLiteral("^!(\\[\\[[^\\]]+\\]\\])$"));
    QRegularExpressionMatch embedMatch = embedRegex.match(clean);

    static const QRegularExpression mdImgRegex(QStringLiteral("^!\\[(.*?)\\]\\((.*?)\\)$"));
    QRegularExpressionMatch mdImgMatch = mdImgRegex.match(clean);

    if (embedMatch.hasMatch()) {
      QString innerRaw = clean; // e.g. "![[image.png|100]]" or "![[Note#Heading]]"
      ObsidianLink link = ObsidianParser::parseLink(innerRaw);

      if (link.isImage) {
        ctx->currentBlock.type = BlockType::Image;
        ctx->currentBlock.content = link.target;
        ctx->currentBlock.metadata[QStringLiteral("target")] = link.target;
        ctx->currentBlock.metadata[QStringLiteral("alias")] = link.alias;
        ctx->currentBlock.metadata[QStringLiteral("width")] = link.imageWidth;
        ctx->currentBlock.metadata[QStringLiteral("height")] = link.imageHeight;
      } else {
        ctx->currentBlock.type = BlockType::Embed;
        ctx->currentBlock.content = link.target + (link.heading.isEmpty() ? QString() : (QStringLiteral("#") + link.heading))
                                                + (link.blockId.isEmpty() ? QString() : (QStringLiteral("#^") + link.blockId));
        ctx->currentBlock.metadata[QStringLiteral("target")] = link.target;
        ctx->currentBlock.metadata[QStringLiteral("heading")] = link.heading;
        ctx->currentBlock.metadata[QStringLiteral("blockId")] = link.blockId;
        ctx->currentBlock.metadata[QStringLiteral("alias")] = link.alias;
      }
      ctx->currentBlock.raw = clean;
    } else if (mdImgMatch.hasMatch()) {
      ctx->currentBlock.type = BlockType::Image;
      ctx->currentBlock.content = mdImgMatch.captured(2);
      ctx->currentBlock.metadata[QStringLiteral("target")] = mdImgMatch.captured(2);
      ctx->currentBlock.metadata[QStringLiteral("alt")] = mdImgMatch.captured(1);
      ctx->currentBlock.raw = clean;
    } else {
      ctx->currentBlock.type = BlockType::Paragraph;
      ctx->currentBlock.raw = ctx->currentTextBuffer;
    }

    shouldPush = true;
    break;
  }

  case MD_BLOCK_HR:
    ctx->currentBlock.type = BlockType::ThematicBreak;
    ctx->currentBlock.raw = QStringLiteral("---");
    ctx->currentBlock.content = QString();
    shouldPush = true;
    break;

  case MD_BLOCK_H:
    ctx->currentBlock.raw = QString(ctx->currentBlock.level, QLatin1Char('#')) + QStringLiteral(" ") + ctx->currentTextBuffer;
    shouldPush = true;
    break;

  case MD_BLOCK_CODE: {
    QString code = ctx->currentTextBuffer;
    ctx->currentBlock.raw = QStringLiteral("```") + ctx->currentBlock.language + QStringLiteral("\n") + code + QStringLiteral("\n```");
    shouldPush = true;
    break;
  }

  default:
    break;
  }

  if (shouldPush && ctx->inBlock) {
    if (ctx->currentBlock.type != BlockType::Embed &&
        ctx->currentBlock.type != BlockType::Image &&
        ctx->currentBlock.type != BlockType::ThematicBreak) {
      ctx->currentBlock.content = ctx->currentTextBuffer;
    }

    ctx->blocks.append(ctx->currentBlock);
    ctx->currentTextBuffer.clear();
    ctx->inBlock = false;
  }

  return 0;
}

static int enter_span_callback(MD_SPANTYPE type, void *detail, void *userdata) {
  Q_UNUSED(detail);
  ParseContext *ctx = static_cast<ParseContext *>(userdata);

  QString &buffer =
      ctx->inTable ? ctx->currentCellBuffer : ctx->currentTextBuffer;

  if (type == MD_SPAN_STRONG)
    buffer.append(QStringLiteral("**"));
  if (type == MD_SPAN_EM)
    buffer.append(QStringLiteral("*"));
  if (type == MD_SPAN_CODE)
    buffer.append(QStringLiteral("`"));
  if (type == MD_SPAN_WIKILINK)
    buffer.append(QStringLiteral("[["));
  if (type == MD_SPAN_IMG)
    buffer.append(QStringLiteral("!["));
  if (type == MD_SPAN_LATEXMATH)
    buffer.append(QStringLiteral("$"));
  if (type == MD_SPAN_LATEXMATH_DISPLAY)
    buffer.append(QStringLiteral("$$"));

  return 0;
}

static int leave_span_callback(MD_SPANTYPE type, void *detail, void *userdata) {
  ParseContext *ctx = static_cast<ParseContext *>(userdata);

  QString &buffer =
      ctx->inTable ? ctx->currentCellBuffer : ctx->currentTextBuffer;

  if (type == MD_SPAN_STRONG)
    buffer.append(QStringLiteral("**"));
  if (type == MD_SPAN_EM)
    buffer.append(QStringLiteral("*"));
  if (type == MD_SPAN_CODE)
    buffer.append(QStringLiteral("`"));
  if (type == MD_SPAN_WIKILINK)
    buffer.append(QStringLiteral("]]"));
  if (type == MD_SPAN_IMG) {
    MD_SPAN_IMG_DETAIL *img_detail = (MD_SPAN_IMG_DETAIL *)detail;
    QString src = QString::fromUtf8(img_detail->src.text, img_detail->src.size);
    buffer.append(QStringLiteral("](%1)").arg(src));
  }
  if (type == MD_SPAN_LATEXMATH)
    buffer.append(QStringLiteral("$"));
  if (type == MD_SPAN_LATEXMATH_DISPLAY)
    buffer.append(QStringLiteral("$$"));

  return 0;
}

static int text_callback(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size,
                         void *userdata) {
  Q_UNUSED(type);
  ParseContext *ctx = static_cast<ParseContext *>(userdata);
  QString textStr = QString::fromUtf8(text, static_cast<int>(size));

  if (ctx->inTable) {
    ctx->currentCellBuffer.append(textStr);
  } else {
    ctx->currentTextBuffer.append(textStr);
  }
  return 0;
}

QVector<NoteBlock> MarkdownParser::parse(const QString &markdown) {
  ParseContext ctx;

  MD_PARSER parser = {
      0, // abi_version
      MD_DIALECT_GITHUB | MD_FLAG_WIKILINKS | MD_FLAG_TASKLISTS |
          MD_FLAG_LATEXMATHSPANS | MD_FLAG_TABLES, // flags
      enter_block_callback,
      leave_block_callback,
      enter_span_callback,
      leave_span_callback,
      text_callback,
      nullptr // debug_log
  };

  QByteArray data = markdown.toUtf8();
  md_parse(data.constData(), data.size(), &parser, &ctx);

  return ctx.blocks;
}
