#pragma once

#include <QObject>

class TestRoundTrip : public QObject {
  Q_OBJECT

private slots:
  void testUnmodifiedFidelity();
  void testModifiedBlockRoundTrip();
  void testCalloutRoundTrip();
  void testTaskListRoundTrip();
};
