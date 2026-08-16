#include "TestNotesIndex.h"
#include "../../src_plugin/notes/NotesIndex.h"
#include <QDir>
#include <QFile>
#include <QTest>
#include <QTextStream>

void TestNotesIndex::initTestCase() {
  QVERIFY(m_vaultDir.isValid());
  QString root = m_vaultDir.path();

  // Create test note 1: "Project A.md"
  QFile note1(root + QStringLiteral("/Project A.md"));
  QVERIFY(note1.open(QIODevice::WriteOnly | QIODevice::Text));
  QTextStream out1(&note1);
  out1 << "---\n"
       << "tags:\n"
       << "  - work\n"
       << "  - active\n"
       << "aliases:\n"
       << "  - Alpha\n"
       << "---\n"
       << "# Project A\n"
       << "Linking to [[Task List]] and [[Project B#Milestones]].\n";
  note1.close();

  // Create test note 2: "Task List.md"
  QFile note2(root + QStringLiteral("/Task List.md"));
  QVERIFY(note2.open(QIODevice::WriteOnly | QIODevice::Text));
  QTextStream out2(&note2);
  out2 << "---\n"
       << "tags:\n"
       << "  - work\n"
       << "---\n"
       << "# Tasks\n"
       << "- [ ] Work on [[Alpha]]\n";
  note2.close();

  // Create a subfolder with attachment
  QDir(root).mkpath(QStringLiteral("subfolder/attachments"));
  QFile img(root + QStringLiteral("/subfolder/attachments/diagram.png"));
  QVERIFY(img.open(QIODevice::WriteOnly));
  img.write("PNG_DATA_DUMMY");
  img.close();

  // Run index
  NotesIndex::instance()->setRootPath(root);
  QTest::qWait(400);
}

void TestNotesIndex::testStructuredLinkResolution() {
  NotesIndex *index = NotesIndex::instance();

  // 1. Direct title match
  LinkResolution res1 = index->resolveLink(QStringLiteral("[[Project A]]"));
  QCOMPARE(res1.kind, LinkResolutionKind::Found);
  QVERIFY(res1.matches.first().endsWith(QStringLiteral("Project A.md")));

  // 2. Heading match
  LinkResolution res2 = index->resolveLink(QStringLiteral("[[Project A#Overview|Go to Project A]]"));
  QCOMPARE(res2.kind, LinkResolutionKind::Found);
  QCOMPARE(res2.heading, QStringLiteral("Overview"));
  QCOMPARE(res2.alias, QStringLiteral("Go to Project A"));

  // 3. Alias match
  LinkResolution res3 = index->resolveLink(QStringLiteral("[[Alpha]]"));
  QCOMPARE(res3.kind, LinkResolutionKind::Found);
  QVERIFY(res3.matches.first().endsWith(QStringLiteral("Project A.md")));

  // 4. Missing link
  LinkResolution res4 = index->resolveLink(QStringLiteral("[[Nonexistent Note]]"));
  QCOMPARE(res4.kind, LinkResolutionKind::Missing);
}

void TestNotesIndex::testAttachmentLookup() {
  NotesIndex *index = NotesIndex::instance();
  QString found = index->findAttachment(QStringLiteral("diagram.png"));
  QVERIFY(!found.isEmpty());
  QVERIFY(found.endsWith(QStringLiteral("diagram.png")));
}

void TestNotesIndex::testBacklinks() {
  NotesIndex *index = NotesIndex::instance();
  QStringList backlinks = index->getBacklinks(QStringLiteral("Task List"));
  QVERIFY(!backlinks.isEmpty());
  QVERIFY(backlinks.first().endsWith(QStringLiteral("Project A.md")));
}

void TestNotesIndex::testTagQuery() {
  NotesIndex *index = NotesIndex::instance();
  QVector<NoteMetadata> workNotes = index->getNotesByTag(QStringLiteral("work"));
  QCOMPARE(workNotes.size(), 2);

  QStringList allTags = index->getAllTags();
  QVERIFY(allTags.contains(QStringLiteral("work")));
  QVERIFY(allTags.contains(QStringLiteral("active")));
}
