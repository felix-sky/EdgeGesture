#include "TestRoundTrip.h"
#include "../../src_plugin/notes/MarkdownParser.h"
#include "../../src_plugin/notes/NoteBlockModel.h"
#include <QTest>

void TestRoundTrip::testUnmodifiedFidelity() {
  QString complexDoc = QStringLiteral(
      "# Document Title\n\n"
      "Here is a paragraph with **bold** and *italic* and `code` and [[WikiLink]].\n\n"
      "> [!NOTE]- Collapsed Note\n"
      "> First line of callout\n"
      "> Second line\n\n"
      "```cpp\n"
      "int main() {\n"
      "    return 0;\n"
      "}\n"
      "```\n\n"
      "- [x] Completed task\n"
      "- [ ] Pending task\n\n"
      "| Column A | Column B |\n"
      "| --- | --- |\n"
      "| Value 1 | Value 2 |\n\n"
      "![[image.png|300x200]]"
  );

  NoteBlockModel model;
  model.loadMarkdown(complexDoc);

  // Since loadMarkdown runs concurrently on parser, wait for watcher
  QTest::qWait(300);

  QCOMPARE(model.isModified(), false);
  QString serialized = model.getMarkdown();
  QCOMPARE(serialized.trimmed(), complexDoc.trimmed());
}

void TestRoundTrip::testModifiedBlockRoundTrip() {
  QString original = QStringLiteral(
      "# First Heading\n\n"
      "Original paragraph text.\n\n"
      "## Second Heading"
  );

  NoteBlockModel model;
  model.loadMarkdown(original);
  QTest::qWait(300);

  QCOMPARE(model.rowCount(), 3);

  // Update middle paragraph
  model.updateBlock(1, QStringLiteral("Updated paragraph text."));
  QCOMPARE(model.isModified(), true);

  QString result = model.getMarkdown();
  QVERIFY(result.contains(QStringLiteral("# First Heading")));
  QVERIFY(result.contains(QStringLiteral("Updated paragraph text.")));
  QVERIFY(result.contains(QStringLiteral("## Second Heading")));
  QVERIFY(!result.contains(QStringLiteral("Original paragraph text.")));
}

void TestRoundTrip::testCalloutRoundTrip() {
  QString original = QStringLiteral("> [!FAQ]+ Questions\n> Answer 1\n> Answer 2");
  NoteBlockModel model;
  model.loadMarkdown(original);
  QTest::qWait(300);

  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.data(model.index(0, 0), NoteBlockModel::TypeRole).toString(), QStringLiteral("callout"));
  QCOMPARE(model.data(model.index(0, 0), NoteBlockModel::FoldStateRole).toString(), QStringLiteral("+"));
  QCOMPARE(model.data(model.index(0, 0), NoteBlockModel::IsFoldableRole).toBool(), true);
  QCOMPARE(model.data(model.index(0, 0), NoteBlockModel::IsCollapsedRole).toBool(), false);

  // Toggle fold state
  model.toggleCalloutFold(0);
  QCOMPARE(model.data(model.index(0, 0), NoteBlockModel::IsCollapsedRole).toBool(), true);
}

void TestRoundTrip::testTaskListRoundTrip() {
  QString original = QStringLiteral("- [ ] Task one\n- [x] Task two\n- [/] Task three");
  NoteBlockModel model;
  model.loadMarkdown(original);
  QTest::qWait(300);

  QCOMPARE(model.rowCount(), 3);
  QCOMPARE(model.data(model.index(0, 0), NoteBlockModel::TypeRole).toString(), QStringLiteral("tasklist"));
  QCOMPARE(model.data(model.index(1, 0), NoteBlockModel::TypeRole).toString(), QStringLiteral("tasklist"));
}
