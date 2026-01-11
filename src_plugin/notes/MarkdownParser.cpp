#include "MarkdownParser.h"
#include "md4c/src/md4c.h"
#include <QDebug>
#include <QRegularExpression>

// Context to pass to md4c callbacks
struct ParseContext {
  QVector<NoteBlock> blocks;
  NoteBlock currentBlock;
  bool inBlock = false;
  int inQuoteLevel = 0; // Track nesting for flattening Quotes/Callouts
  QString currentTextBuffer;
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
      newBlock.type = BlockType::TaskList;
      // Capture the exact char: 'x', 'X', ' ', '-', '/'
      QChar mark = QChar(li_detail->task_mark);
      newBlock.metadata["taskStatus"] = QString(mark);
      newBlock.metadata["checked"] = (mark == 'x' || mark == 'X');
    } else {
      newBlock.type = BlockType::List;
    }
    break;
  }
  case MD_BLOCK_P:
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

  // Quote Logic
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
      // Manual check for extended task statuses that md4c missed
      if (ctx->currentBlock.type == BlockType::List) {
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

  if (type == MD_SPAN_STRONG)
    ctx->currentTextBuffer.append("**");
  if (type == MD_SPAN_EM)
    ctx->currentTextBuffer.append("*");
  if (type == MD_SPAN_CODE)
    ctx->currentTextBuffer.append("`");

  if (type == MD_SPAN_WIKILINK) {
    ctx->currentTextBuffer.append("[[");
  }

  if (type == MD_SPAN_IMG) {
    ctx->currentTextBuffer.append("![");
  }

  if (type == MD_SPAN_LATEXMATH) {
    ctx->currentTextBuffer.append("$");
  }
  if (type == MD_SPAN_LATEXMATH_DISPLAY) {
    ctx->currentTextBuffer.append("$$");
  }

  return 0;
}

static int leave_span_callback(MD_SPANTYPE type, void *detail, void *userdata) {
  ParseContext *ctx = static_cast<ParseContext *>(userdata);
  if (type == MD_SPAN_STRONG)
    ctx->currentTextBuffer.append("**");
  if (type == MD_SPAN_EM)
    ctx->currentTextBuffer.append("*");
  if (type == MD_SPAN_CODE)
    ctx->currentTextBuffer.append("`");

  if (type == MD_SPAN_WIKILINK) {
    ctx->currentTextBuffer.append("]]");
  }

  if (type == MD_SPAN_IMG) {
    MD_SPAN_IMG_DETAIL *img_detail = (MD_SPAN_IMG_DETAIL *)detail;
    QString src = QString::fromUtf8(img_detail->src.text, img_detail->src.size);
    ctx->currentTextBuffer.append(QString("](%1)").arg(src));
  }

  if (type == MD_SPAN_LATEXMATH) {
    ctx->currentTextBuffer.append("$");
  }
  if (type == MD_SPAN_LATEXMATH_DISPLAY) {
    ctx->currentTextBuffer.append("$$");
  }

  return 0;
}

static int text_callback(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size,
                         void *userdata) {
  ParseContext *ctx = static_cast<ParseContext *>(userdata);
  ctx->currentTextBuffer.append(
      QString::fromUtf8(text, static_cast<int>(size)));
  return 0;
}

QVector<NoteBlock> MarkdownParser::parse(const QString &markdown) {
  ParseContext ctx;

  MD_PARSER parser = {
      0, // abi_version
      MD_DIALECT_GITHUB | MD_FLAG_WIKILINKS | MD_FLAG_TASKLISTS |
          MD_FLAG_LATEXMATHSPANS, // flags
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
