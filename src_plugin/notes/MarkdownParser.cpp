#include "MarkdownParser.h"
#include "md4c/src/md4c.h"
#include <QDebug>
#include <QRegularExpression>
#include <QStack>

// Context to pass to md4c callbacks
struct ParseContext {
  QVector<NoteBlock> blocks;
  NoteBlock currentBlock;
  bool inBlock = false;
  int inQuoteLevel = 0;   // Track nesting for flattening Quotes/Callouts
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

  // If we are deeper in a quote (nested), we don't start new blocks yet.
  // We flatten everything inside the top-level quote into one block.
  if (ctx->inQuoteLevel > 0) {
    if (type == MD_BLOCK_QUOTE) {
      ctx->inQuoteLevel++;
    }
    // Add separator for paragraphs inside quotes
    if (type == MD_BLOCK_P) {
      if (!ctx->currentTextBuffer.isEmpty() &&
          !ctx->currentTextBuffer.endsWith('\n')) {
        ctx->currentTextBuffer.append("\n\n");
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
      // Capture the exact char: 'x', 'X', ' ', '-', '/'
      QChar mark = QChar(li_detail->task_mark);
      newBlock.metadata["taskStatus"] = QString(mark);
      newBlock.metadata["checked"] = (mark == 'x' || mark == 'X');
    } else {
      newBlock.type = BlockType::List;
      // MD4C doesn't explicitly expose parent OL/UL type here easily without
      // context But we can check the list char or infer from context if we
      // tracked it. For now, let's look at the source text logic or just use
      // metadata if we can't get it. Wait, MD4C separates UL and OL blocks
      // wrapping LI. We need to track the parent block type in context? Or we
      // can cheat and look at the first char of the content if we have offsets?
      // Better: let's track nesting in `enter_block` for UL/OL.
    }
    break;
  }
  case MD_BLOCK_UL:
    // We could use this to set a context flag "inUL"
    ctx->listStack.push(false); // Unordered
    break;
  case MD_BLOCK_OL:
    ctx->listStack.push(true); // Ordered
    break;
  case MD_BLOCK_HR:
    newBlock.type = BlockType::ThematicBreak;
    break;
  case MD_BLOCK_TABLE:
    // Start of a table - initialize table state
    qDebug() << "MarkdownParser: MD_BLOCK_TABLE enter - starting table parsing";
    ctx->inTable = true;
    ctx->tableRows.clear();
    ctx->inBlock = false; // Don't create block yet, wait for all rows
    break;
  case MD_BLOCK_THEAD:
    ctx->inTableHeader = true;
    break;
  case MD_BLOCK_TBODY:
    ctx->inTableHeader = false;
    break;
  case MD_BLOCK_TR:
    // Start a new row
    ctx->currentTableRow.clear();
    ctx->inBlock = false;
    break;
  case MD_BLOCK_TH:
  case MD_BLOCK_TD:
    // Start a cell - clear the cell buffer
    ctx->currentCellBuffer.clear();
    ctx->inBlock = false;
    break;
  case MD_BLOCK_P:
    // Skip paragraph blocks inside tables - cell content is handled separately
    if (ctx->inTable) {
      ctx->inBlock = false;
      break;
    }
    newBlock.type = BlockType::Paragraph;
    break;
  default:
    // For others (UL, OL, etc.), we don't necessarily start a *content* block
    // but we update state. However, for our flat list, we mostly ignore UL/OL
    // containers acting only on LIs.
    ctx->inBlock = false; // Don't flag this as an active content block
    break;
  }

  if (ctx->inBlock) {
    ctx->currentBlock = newBlock;
  }
  return 0;
}

static int leave_block_callback(MD_BLOCKTYPE type, void *detail,
                                void *userdata) {
  ParseContext *ctx = static_cast<ParseContext *>(userdata);

  if (type == MD_BLOCK_UL || type == MD_BLOCK_OL) {
    if (!ctx->listStack.isEmpty())
      ctx->listStack.pop();
    return 0;
  }

  // Table cell/row/table ending handling
  if (type == MD_BLOCK_TH || type == MD_BLOCK_TD) {
    // End of a cell - add cell content to the current row
    ctx->currentTableRow.append(ctx->currentCellBuffer.trimmed());
    ctx->currentCellBuffer.clear();
    return 0;
  }

  if (type == MD_BLOCK_TR) {
    // End of a row - add the row to table rows
    if (!ctx->currentTableRow.isEmpty()) {
      ctx->tableRows.append(ctx->currentTableRow);
      ctx->currentTableRow.clear();
    }
    return 0;
  }

  if (type == MD_BLOCK_THEAD || type == MD_BLOCK_TBODY) {
    // Just state tracking, already handled in enter
    return 0;
  }

  if (type == MD_BLOCK_TABLE) {
    // End of table - create the Table block with all cells as metadata
    qDebug()
        << "MarkdownParser: MD_BLOCK_TABLE leave - creating table block with"
        << ctx->tableRows.size() << "rows";
    if (!ctx->tableRows.isEmpty()) {
      NoteBlock tableBlock;
      tableBlock.type = BlockType::Table;

      // Convert QVector<QVector<QString>> to QVariantList for metadata
      QVariantList rowsVariant;
      for (const auto &row : ctx->tableRows) {
        QVariantList cellsVariant;
        for (const QString &cell : row) {
          cellsVariant.append(cell);
        }
        rowsVariant.append(QVariant(cellsVariant));
      }
      tableBlock.metadata["rows"] = rowsVariant;

      ctx->blocks.append(tableBlock);
      qDebug() << "MarkdownParser: Table block appended with metadata";
    }
    ctx->inTable = false;
    ctx->tableRows.clear();
    return 0;
  }

  // Quote Logic
  // ... (rest of function)
  if (type == MD_BLOCK_QUOTE) {
    if (ctx->inQuoteLevel > 0) {
      ctx->inQuoteLevel--;
      if (ctx->inQuoteLevel == 0) {
        // Finalize Quote Block
        ctx->currentBlock.content = ctx->currentTextBuffer;

        // Check for Callout Standard: [!INFO] Title
        // The text buffer usually doesn't have the '>' markers.
        // Regex: ^\[!(\w+)\](.*?)(\n|$)
        // Note: md4c might leave leading spaces? content is usually trimmed by
        // logic? Let's trim whitespace first.
        QString cleanContent = ctx->currentTextBuffer.trimmed();

        QRegularExpression regex(R"(^\[!(\w+)\][ \t]*(.*?)(\n|$))");
        QRegularExpressionMatch match = regex.match(cleanContent);
        if (match.hasMatch()) {
          ctx->currentBlock.type = BlockType::Callout;
          ctx->currentBlock.metadata["calloutType"] =
              match.captured(1).toLower();
          ctx->currentBlock.metadata["title"] = match.captured(2).trimmed();

          // Remove the definition line from content if strict?
          // Obsidian usually hides it.
          // Start index of content: match end.
          int matchEnd = match.capturedEnd(0); // End of first line
          if (matchEnd < cleanContent.length()) {
            ctx->currentBlock.content = cleanContent.mid(matchEnd).trimmed();
          } else {
            ctx->currentBlock.content = "";
          }
        }

        ctx->blocks.append(ctx->currentBlock);
        ctx->inBlock = false;
      }
    }
    return 0;
  }

  // If we are inside a quote, we ignore leave events of children (flattening)
  if (ctx->inQuoteLevel > 0) {
    return 0;
  }

  // Standard Blocks
  bool shouldPush = false;
  switch (type) {
  case MD_BLOCK_LI:
    shouldPush = true;
    if (ctx->inBlock) {
      // List Type Handling
      if (ctx->currentBlock.type == BlockType::List) {
        bool isOrdered = !ctx->listStack.isEmpty() && ctx->listStack.top();
        ctx->currentBlock.metadata["listType"] =
            isOrdered ? "ordered" : "bullet";
      }

      // Manual check for extended task statuses that md4c missed
      if (ctx->currentBlock.type == BlockType::List) {
        // ... (existing task check)
        QString cleanContent = ctx->currentTextBuffer.trimmed();
        QRegularExpression regex(R"(^\[(.)\]\s+(.*)$)");
        QRegularExpressionMatch match = regex.match(cleanContent);
        if (match.hasMatch()) {
          ctx->currentBlock.type = BlockType::TaskList;
          QString mark = match.captured(1);
          ctx->currentBlock.metadata["taskStatus"] = mark;
          ctx->currentBlock.metadata["checked"] = (mark == "x" || mark == "X");
          ctx->currentTextBuffer =
              match.captured(2).trimmed(); // Update content to remove hook
        }
      }
    }
    break;
  case MD_BLOCK_P: {
    // Check if it's a standalone Embed `![[...]]` or Image
    // `![[...]]`/`![...](...)`
    QString clean = ctx->currentTextBuffer.trimmed();
    qDebug() << "MarkdownParser: P Block. Clean content:" << clean;

    // 1. Check for `![[...]]`
    QRegularExpression embedRegex(R"(^!\[\[(.*?)\]\]$)");
    QRegularExpressionMatch embedMatch = embedRegex.match(clean);

    // 2. Check for `![...](...)`
    QRegularExpression mdImgRegex(R"(^!\[.*?\]\((.*?)\)$)");
    QRegularExpressionMatch mdImgMatch = mdImgRegex.match(clean);

    if (embedMatch.hasMatch()) {
      QString inner = embedMatch.captured(1);
      qDebug() << "MarkdownParser: Matched Embed Syntax. Inner content:"
               << inner;

      // Check extension to decide Embed vs Image
      static const QRegularExpression imgExt(
          R"(\.(png|jpg|jpeg|gif|bmp|svg|webp|ico|tiff?)$)",
          QRegularExpression::CaseInsensitiveOption);
      if (inner.contains(imgExt)) {
        // It's an image block
        ctx->currentBlock.type = BlockType::Image;
        ctx->currentBlock.content = inner;
        qDebug() << "MarkdownParser: Classified as Image";
      } else {
        // It's a note embed
        ctx->currentBlock.type = BlockType::Embed;
        ctx->currentBlock.content = inner;
        qDebug() << "MarkdownParser: Classified as Embed";
      }
    } else if (mdImgMatch.hasMatch()) {
      // Standard markdown image is always image
      ctx->currentBlock.type = BlockType::Image;
      ctx->currentBlock.content = mdImgMatch.captured(1); // The URL/Path
      qDebug() << "MarkdownParser: Matched MD Image Syntax. Path:"
               << ctx->currentBlock.content;
    } else {
      qDebug() << "MarkdownParser: No embed/image match. Standard Paragraph.";
    }

    shouldPush = true;
    break;
  }
  case MD_BLOCK_HR: // Ensure we push HR blocks
    shouldPush = true;
    break;
  case MD_BLOCK_H:
  case MD_BLOCK_CODE:
    shouldPush = true;
    break;
  default:
    break;
  }

  if (shouldPush && ctx->inBlock) {
    // Only update content from buffer if it wasn't already set by a specific
    // handler (like Embed/Image)
    if (ctx->currentBlock.type != BlockType::Embed &&
        ctx->currentBlock.type != BlockType::Image) {
      ctx->currentBlock.content = ctx->currentTextBuffer;
    }
    // If it is Embed/Image, we already set the content to the inner text

    ctx->blocks.append(ctx->currentBlock);

    ctx->currentTextBuffer.clear();
    ctx->inBlock = false;
  }

  return 0;
}

static int enter_span_callback(MD_SPANTYPE type, void *detail, void *userdata) {
  ParseContext *ctx = static_cast<ParseContext *>(userdata);

  // Use the appropriate buffer based on table state
  QString &buffer =
      ctx->inTable ? ctx->currentCellBuffer : ctx->currentTextBuffer;

  if (type == MD_SPAN_STRONG)
    buffer.append("**");
  if (type == MD_SPAN_EM)
    buffer.append("*");
  if (type == MD_SPAN_CODE)
    buffer.append("`");

  if (type == MD_SPAN_WIKILINK) {
    buffer.append("[[");
  }

  if (type == MD_SPAN_IMG) {
    buffer.append("![");
  }

  if (type == MD_SPAN_LATEXMATH) {
    buffer.append("$");
  }
  if (type == MD_SPAN_LATEXMATH_DISPLAY) {
    buffer.append("$$");
  }

  return 0;
}

static int leave_span_callback(MD_SPANTYPE type, void *detail, void *userdata) {
  ParseContext *ctx = static_cast<ParseContext *>(userdata);

  // Use the appropriate buffer based on table state
  QString &buffer =
      ctx->inTable ? ctx->currentCellBuffer : ctx->currentTextBuffer;

  if (type == MD_SPAN_STRONG)
    buffer.append("**");
  if (type == MD_SPAN_EM)
    buffer.append("*");
  if (type == MD_SPAN_CODE)
    buffer.append("`");

  if (type == MD_SPAN_WIKILINK) {
    buffer.append("]]");
  }

  if (type == MD_SPAN_IMG) {
    MD_SPAN_IMG_DETAIL *img_detail = (MD_SPAN_IMG_DETAIL *)detail;
    QString src = QString::fromUtf8(img_detail->src.text, img_detail->src.size);
    buffer.append(QString("](%1)").arg(src));
  }

  if (type == MD_SPAN_LATEXMATH) {
    buffer.append("$");
  }
  if (type == MD_SPAN_LATEXMATH_DISPLAY) {
    buffer.append("$$");
  }

  return 0;
}

static int text_callback(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size,
                         void *userdata) {
  ParseContext *ctx = static_cast<ParseContext *>(userdata);
  QString textStr = QString::fromUtf8(text, static_cast<int>(size));

  // If we're in a table, append to the cell buffer instead
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
