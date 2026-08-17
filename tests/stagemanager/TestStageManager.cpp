#include <QSignalSpy>
#include <QTest>
#include "../../src_plugin/StageManager/StageContainerController.h"
#include "../../src_plugin/StageManager/StageManagerService.h"
#include "../../src_plugin/StageManager/StageManagerTypes.h"
#include "../../src_plugin/StageManager/WindowEmbedder.h"

class TestStageManager : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {}

  void testCandidateValidation() {
    // NULL HWND is not candidate
    QVERIFY(!WindowEmbedder::isCandidate(nullptr, GetCurrentProcessId()));

    // Invalid HWND is not candidate
    HWND fakeHwnd = (HWND)(intptr_t)0x12345678;
    QVERIFY(!WindowEmbedder::isCandidate(fakeHwnd, GetCurrentProcessId()));
  }

  void testServiceContainerManagement() {
    StageManagerService service;
    QCOMPARE(service.containerCount(), 0);
    QCOMPARE(service.activeContainerId(), 0ULL);

    // Create container
    quint64 c1 = service.createContainer();
    QVERIFY(c1 > 0);
    QCOMPARE(service.containerCount(), 1);
    QCOMPARE(service.activeContainerId(), c1);

    StageContainerController *ctrl1 = service.getController(c1);
    QVERIFY(ctrl1 != nullptr);
    QCOMPARE(ctrl1->containerId(), c1);
    QCOMPARE(ctrl1->pageCount(), 0);

    // Create second container
    quint64 c2 = service.createContainer();
    QVERIFY(c2 > 0 && c2 != c1);
    QCOMPARE(service.containerCount(), 2);
    QCOMPARE(service.activeContainerId(), c2);

    // Switch active destination
    service.setActiveDestination(c1);
    QCOMPARE(service.activeContainerId(), c1);

    // Close container
    service.closeContainer(c1);
    QCOMPARE(service.containerCount(), 1);
    QCOMPARE(service.activeContainerId(), c2);

    service.closeAll();
    QCOMPARE(service.containerCount(), 0);
    QCOMPARE(service.activeContainerId(), 0ULL);
  }

  void testContainerControllerPages() {
    StageContainerController controller(100);
    QCOMPARE(controller.containerId(), 100ULL);
    QCOMPARE(controller.pageCount(), 0);
    QCOMPARE(controller.activeIndex(), -1);

    // Pinning test
    QCOMPARE(controller.isPinned(), false);
    controller.setPinned(true);
    QCOMPARE(controller.isPinned(), true);
    controller.setPinned(false);
    QCOMPARE(controller.isPinned(), false);
  }

  void testOriginalWindowStateDefaults() {
    OriginalWindowState state;
    QCOMPARE(state.style, 0LL);
    QCOMPARE(state.exStyle, 0LL);
    QCOMPARE(state.visible, false);
    QCOMPARE(state.iconic, false);
    QCOMPARE(state.zoomed, false);
    QCOMPARE(state.topMost, false);
    QCOMPARE((int)state.placement.length, (int)sizeof(WINDOWPLACEMENT));
  }
};

QTEST_MAIN(TestStageManager)
#include "TestStageManager.moc"
