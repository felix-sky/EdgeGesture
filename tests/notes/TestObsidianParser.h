#pragma once

#include <QObject>

class TestObsidianParser : public QObject {
  Q_OBJECT

private slots:
  void testParseBasicWikiLink();
  void testParseWikiLinkWithAlias();
  void testParseWikiLinkWithHeading();
  void testParseWikiLinkWithHeadingAndAlias();
  void testParseWikiLinkWithBlockId();
  void testParseWikiLinkWithFolder();
  void testParseImageEmbedBasic();
  void testParseImageEmbedWithDimensions();
  void testSplitFrontmatterBasic();
  void testSplitFrontmatterComplex();
  void testMergeFrontmatterNonDestructive();
  void testParseCallouts();
};
