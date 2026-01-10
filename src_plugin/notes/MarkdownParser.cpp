#include "MarkdownParser.h"
#include "md4c/src/md4c.h"
#include <QDebug>
#include <QStack>

// Context to pass to md4c callbacks
struct ParseContext {
  QVector<NoteBlock> blocks;
  NoteBlock currentBlock;
  bool inBlock = false;
  // Helper to buffer text for the current block
  QString currentTextBuffer;
};

// MD4C Callbacks
static int enter_block_callback(MD_BLOCKTYPE type, void *detail,
                                void *userdata) {
  ParseContext *ctx = static_cast<ParseContext *>(userdata);

  // If we were already in a block, we might need to flush it or handle nesting.
  // md4c handles nesting, but for our flat list of visual blocks, we mostly
  // care about top-level or specific blocks. For simplicity in Phase 1, we
  // treat nested blocks as part of the parent or separate blocks depending on
  // type.

  // Commit previous block if valid and we are starting a new major block
  // (This logic might need refinement for nested lists, but good start)
  if (ctx->inBlock) {
    // Recursive/nested constraint: MD4C might fire P inside LI.
    // We might want to flush the LI start?
  }

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
    break;
  case MD_BLOCK_UL:
  case MD_BLOCK_OL:
  case MD_BLOCK_LI:
    newBlock.type = BlockType::List;
    // Logic for lists is tricky in a flat list model.
    // We might treat LI as individual blocks.
    break;
  case MD_BLOCK_P:
    newBlock.type = BlockType::Paragraph;
    break;
  default:
    newBlock.type = BlockType::Paragraph; // Fallback
    break;
  }

  // Prepare current block
  // Since md4c is recursive, this "currentBlock" notion is simplistic.
  // A proper implementation uses a stack.
  // For Phase 1, let's assuming we accumulate text into the current "Leaf"
  // block.

  ctx->currentBlock = newBlock;
  return 0;
}

static int leave_block_callback(MD_BLOCKTYPE type, void *detail,
                                void *userdata) {
  ParseContext *ctx = static_cast<ParseContext *>(userdata);

  // When leaving a block, we finalize it.
  // Flattening: We only append "leaf" blocks or major structural blocks.
  // E.g. Leave P -> append block.
  // Leave LI -> append block.

  // Filter: DONT append container blocks like UL/OL if they just contain LIs
  // (which we also append). Actually, we want to append the block ONLY if it
  // has content, OR if it's a structural separator.

  if (type == MD_BLOCK_DOC)
    return 0; // Ignore doc root

  // Simplistic flattening:
  // If we are leaving a P, H, CODE, QUOTE, LI -> push it.
  // If we are leaving UL/OL -> do nothing (the LIs were pushed).

  bool shouldPush = false;
  switch (type) {
  case MD_BLOCK_P:
  case MD_BLOCK_H:
  case MD_BLOCK_CODE:
  case MD_BLOCK_QUOTE:
  case MD_BLOCK_LI:
    shouldPush = true;
    break;
  default:
    break;
  }

  if (shouldPush) {
    ctx->currentBlock.content = ctx->currentTextBuffer;

    // Simple heuristic: If it's a list item, prefix with bullet?
    // Or handle in QML. Let's handle in QML.

    ctx->blocks.append(ctx->currentBlock);

    // Reset
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

  // Convert WikiLinks to Standard Markdown Links
  // [[Target]] -> [Target](Target)
  // ![[Image.png]] -> ![Image.png](Image.png)
  if (type == MD_SPAN_WIKILINK) {
    // PRESERVE syntax: [[Target]]
    ctx->currentTextBuffer.append("[[");
  }

  if (type == MD_SPAN_IMG) {
    // PRESERVE syntax: ![[Image.png]] if it was a wikilink image?
    // MD4C parses ![[...]] as IMG if the dialect allows it?
    // Actually MD4C MD_FLAG_WIKILINKS parses [[...]] as WikiLink.
    // It does not necessarily parse ![[...]] as IMG automatically unless it's
    // standard syntax ![...](...). Wait, User says ![[Pasted image...]] If MD4C
    // parses that as text, fine. If it parses as IMG, we preserve. Standard
    // CommonMark IMG is ![alt](src).
    ctx->currentTextBuffer.append("![");
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
      0,                                     // abi_version
      MD_DIALECT_GITHUB | MD_FLAG_WIKILINKS, // flags
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
