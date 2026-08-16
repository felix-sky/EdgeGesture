#include "TestObsidianParser.h"
#include "../../src_plugin/notes/ObsidianParser.h"
#include <QTest>

void TestObsidianParser::testParseBasicWikiLink() {
  ObsidianLink link = ObsidianParser::parseLink(QStringLiteral("[[My Note]]"));
  QCOMPARE(link.target, QStringLiteral("My Note"));
  QCOMPARE(link.displayText, QStringLiteral("My Note"));
  QVERIFY(!link.isEmbed);
  QVERIFY(!link.isImage);
  QVERIFY(link.heading.isEmpty());
  QVERIFY(link.blockId.isEmpty());
  QVERIFY(link.alias.isEmpty());
}

void TestObsidianParser::testParseWikiLinkWithAlias() {
  ObsidianLink link = ObsidianParser::parseLink(QStringLiteral("[[My Note|Custom Title]]"));
  QCOMPARE(link.target, QStringLiteral("My Note"));
  QCOMPARE(link.alias, QStringLiteral("Custom Title"));
  QCOMPARE(link.displayText, QStringLiteral("Custom Title"));
  QVERIFY(!link.isEmbed);
}

void TestObsidianParser::testParseWikiLinkWithHeading() {
  ObsidianLink link = ObsidianParser::parseLink(QStringLiteral("[[My Note#Section 1]]"));
  QCOMPARE(link.target, QStringLiteral("My Note"));
  QCOMPARE(link.heading, QStringLiteral("Section 1"));
  QCOMPARE(link.displayText, QStringLiteral("My Note \u203A Section 1"));
}

void TestObsidianParser::testParseWikiLinkWithHeadingAndAlias() {
  ObsidianLink link = ObsidianParser::parseLink(QStringLiteral("[[My Note#Section 1|Alias Text]]"));
  QCOMPARE(link.target, QStringLiteral("My Note"));
  QCOMPARE(link.heading, QStringLiteral("Section 1"));
  QCOMPARE(link.alias, QStringLiteral("Alias Text"));
  QCOMPARE(link.displayText, QStringLiteral("Alias Text"));
}

void TestObsidianParser::testParseWikiLinkWithBlockId() {
  ObsidianLink link = ObsidianParser::parseLink(QStringLiteral("[[My Note#^block-123]]"));
  QCOMPARE(link.target, QStringLiteral("My Note"));
  QCOMPARE(link.blockId, QStringLiteral("block-123"));
  QCOMPARE(link.displayText, QStringLiteral("My Note \u203A block-123"));
}

void TestObsidianParser::testParseWikiLinkWithFolder() {
  ObsidianLink link = ObsidianParser::parseLink(QStringLiteral("[[folder/subfolder/My Note]]"));
  QCOMPARE(link.target, QStringLiteral("folder/subfolder/My Note"));
  QCOMPARE(link.displayText, QStringLiteral("folder/subfolder/My Note"));
}

void TestObsidianParser::testParseImageEmbedBasic() {
  ObsidianLink link = ObsidianParser::parseLink(QStringLiteral("![[photo.png]]"));
  QCOMPARE(link.target, QStringLiteral("photo.png"));
  QVERIFY(link.isEmbed);
  QVERIFY(link.isImage);
  QCOMPARE(link.imageWidth, 0);
  QCOMPARE(link.imageHeight, 0);
}

void TestObsidianParser::testParseImageEmbedWithDimensions() {
  ObsidianLink link1 = ObsidianParser::parseLink(QStringLiteral("![[diagram.svg|300]]"));
  QCOMPARE(link1.target, QStringLiteral("diagram.svg"));
  QVERIFY(link1.isImage);
  QCOMPARE(link1.imageWidth, 300);
  QCOMPARE(link1.imageHeight, 0);

  ObsidianLink link2 = ObsidianParser::parseLink(QStringLiteral("![[screenshot.jpg|640x480]]"));
  QCOMPARE(link2.target, QStringLiteral("screenshot.jpg"));
  QVERIFY(link2.isImage);
  QCOMPARE(link2.imageWidth, 640);
  QCOMPARE(link2.imageHeight, 480);
}

void TestObsidianParser::testSplitFrontmatterBasic() {
  QString doc = QStringLiteral("---\nedgegesture-color: '#624a73'\ntags:\n  - project\n  - urgent\n---\n# Main Body\nHello world");
  FrontmatterData fm = ObsidianParser::splitFrontmatter(doc);
  QCOMPARE(fm.color, QStringLiteral("#624a73"));
  QVERIFY(fm.tags.contains(QStringLiteral("project")));
  QVERIFY(fm.tags.contains(QStringLiteral("urgent")));
  QCOMPARE(fm.rawBody.trimmed(), QStringLiteral("# Main Body\nHello world"));
}

void TestObsidianParser::testSplitFrontmatterComplex() {
  QString doc = QStringLiteral("---\nauthor: 'Felix'\ncustom_field: 42\naliases:\n  - Alias One\n  - Alias Two\nedgegesture-pinned: true\n---\nBody here");
  FrontmatterData fm = ObsidianParser::splitFrontmatter(doc);
  QVERIFY(fm.isPinned);
  QCOMPARE(fm.aliases.size(), 2);
  QCOMPARE(fm.aliases[0], QStringLiteral("Alias One"));
  QCOMPARE(fm.aliases[1], QStringLiteral("Alias Two"));
  QCOMPARE(fm.rawBody.trimmed(), QStringLiteral("Body here"));
}

void TestObsidianParser::testMergeFrontmatterNonDestructive() {
  QString original = QStringLiteral("---\nauthor: 'Felix'\ncustom_field: 42\ntags:\n  - demo\n---\nOriginal content");
  QVariantMap updates;
  updates[QStringLiteral("edgegesture-color")] = QStringLiteral("#0078d4");

  QString merged = ObsidianParser::mergeFrontmatter(original, updates, QStringLiteral("Updated content"));
  QVERIFY(merged.contains(QStringLiteral("author: 'Felix'")));
  QVERIFY(merged.contains(QStringLiteral("custom_field: 42")));
  QVERIFY(merged.contains(QStringLiteral("tags:\n  - demo")));
  QVERIFY(merged.contains(QStringLiteral("edgegesture-color: #0078d4")));
  QVERIFY(merged.contains(QStringLiteral("Updated content")));
}

void TestObsidianParser::testParseCallouts() {
  CalloutInfo info1;
  bool parsed1 = ObsidianParser::parseCallout(QStringLiteral("> [!NOTE] This is note title\n> Line 1\n> Line 2"), info1);
  QVERIFY(parsed1);
  QCOMPARE(info1.type, QStringLiteral("note"));
  QCOMPARE(info1.title, QStringLiteral("This is note title"));
  QVERIFY(!info1.isFoldable);
  QCOMPARE(info1.body.trimmed(), QStringLiteral("Line 1\nLine 2"));

  CalloutInfo info2;
  bool parsed2 = ObsidianParser::parseCallout(QStringLiteral("> [!WARNING]- Collapsed warning\n> Danger!"), info2);
  QVERIFY(parsed2);
  QCOMPARE(info2.type, QStringLiteral("warning"));
  QCOMPARE(info2.foldState, QStringLiteral("-"));
  QVERIFY(info2.isFoldable);
  QVERIFY(info2.isCollapsed);
  QCOMPARE(info2.title, QStringLiteral("Collapsed warning"));
  QCOMPARE(info2.body.trimmed(), QStringLiteral("Danger!"));

  CalloutInfo info3;
  bool parsed3 = ObsidianParser::parseCallout(QStringLiteral("> [!tip]+ Expanded tip\n> Tip content"), info3);
  QVERIFY(parsed3);
  QCOMPARE(info3.type, QStringLiteral("tip"));
  QCOMPARE(info3.foldState, QStringLiteral("+"));
  QVERIFY(info3.isFoldable);
  QVERIFY(!info3.isCollapsed);
}
