#include <QCoreApplication>
#include <QTest>
#include <QFile>
#include <QTextStream>
#include "TestObsidianParser.h"
#include "TestNotesIndex.h"
#include "TestRoundTrip.h"

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  int status = 0;

  {
    QStringList args = {QString::fromUtf8(argv[0]), QStringLiteral("-o"), QStringLiteral("d:/SurfaceGesture/test_parser.txt,txt")};
    TestObsidianParser test;
    status |= QTest::qExec(&test, args);
  }

  {
    QStringList args = {QString::fromUtf8(argv[0]), QStringLiteral("-o"), QStringLiteral("d:/SurfaceGesture/test_index.txt,txt")};
    TestNotesIndex test;
    status |= QTest::qExec(&test, args);
  }

  {
    QStringList args = {QString::fromUtf8(argv[0]), QStringLiteral("-o"), QStringLiteral("d:/SurfaceGesture/test_roundtrip.txt,txt")};
    TestRoundTrip test;
    status |= QTest::qExec(&test, args);
  }

  return status;
}
