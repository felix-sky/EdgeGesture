#pragma once

#include <QObject>
#include <QTemporaryDir>

class TestNotesIndex : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void testStructuredLinkResolution();
  void testAttachmentLookup();
  void testBacklinks();
  void testTagQuery();

private:
  QTemporaryDir m_vaultDir;
};
