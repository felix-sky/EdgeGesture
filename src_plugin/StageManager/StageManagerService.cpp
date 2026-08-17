#include "StageManagerService.h"
#include "WindowEmbedder.h"
#include <QDebug>
#include <dwmapi.h>

#pragma comment(lib, "Dwmapi.lib")

StageManagerService *StageManagerService::s_instance = nullptr;

StageManagerService *StageManagerService::instance() { return s_instance; }

StageManagerService::StageManagerService(QObject *parent) : QObject(parent) {
  s_instance = this;
}

StageManagerService::~StageManagerService() {
  closeAll();
  s_instance = nullptr;
}

quint64 StageManagerService::createContainer() {
  ContainerId cid = m_nextContainerId++;
  auto *controller = new StageContainerController(cid, this);
  m_containers.insert(cid, controller);

  connect(controller, &StageContainerController::containerCloseRequested, this,
          [this, cid]() { closeContainer(cid); });

  m_activeContainerId = cid;
  emit containerCreated(cid, controller);
  emit containerCountChanged(m_containers.count());
  emit activeContainerChanged(m_activeContainerId);

  return cid;
}

void StageManagerService::createContainerWithWindow(qint64 hwnd) {
  HWND nativeHwnd = (HWND)hwnd;
  if (!IsWindow(nativeHwnd) || isManaged(hwnd))
    return;

  quint64 cid = createContainer();
  addWindowToContainer(hwnd, cid);
}

bool StageManagerService::addWindowToActiveContainer(qint64 hwnd) {
  HWND nativeHwnd = (HWND)hwnd;
  if (!IsWindow(nativeHwnd) || isManaged(hwnd))
    return false;

  if (m_activeContainerId == 0 || !m_containers.contains(m_activeContainerId)) {
    createContainerWithWindow(hwnd);
    return true;
  }

  return addWindowToContainer(hwnd, m_activeContainerId);
}

bool StageManagerService::addWindowToContainer(qint64 hwnd,
                                               quint64 containerId) {
  HWND nativeHwnd = (HWND)hwnd;
  if (!IsWindow(nativeHwnd))
    return false;

  if (isManaged(hwnd)) {
    qWarning() << "[StageManager] Window is already managed:" << nativeHwnd;
    return false;
  }

  if (!m_containers.contains(containerId)) {
    qWarning() << "[StageManager] Container not found:" << containerId;
    return false;
  }

  StageContainerController *controller = m_containers[containerId];

  // Capture original state
  PageRecord page;
  page.id = m_nextPageId++;
  page.containerId = containerId;
  WindowEmbedder::getWindowIdentity(nativeHwnd, page.identity);
  page.title = page.identity.title;

  if (!WindowEmbedder::captureOriginalState(nativeHwnd, page.original)) {
    qWarning() << "[StageManager] Failed to capture original state for HWND:"
               << nativeHwnd;
    return false;
  }

  if (!controller->addPage(page)) {
    qWarning() << "[StageManager] Failed to add page to controller";
    return false;
  }

  ManagedEntry entry;
  entry.pageId = page.id;
  entry.containerId = containerId;
  entry.pid = page.identity.pid;
  entry.tid = page.identity.tid;
  m_managedWindows.insert(nativeHwnd, entry);

  m_activeContainerId = containerId;
  emit activeContainerChanged(m_activeContainerId);
  return true;
}

void StageManagerService::activatePage(quint64 containerId, int pageIndex) {
  if (m_containers.contains(containerId)) {
    m_containers[containerId]->activatePage(pageIndex);
    setActiveDestination(containerId);
  }
}

void StageManagerService::releasePage(quint64 containerId, int pageIndex) {
  if (m_containers.contains(containerId)) {
    auto *ctrl = m_containers[containerId];
    if (pageIndex >= 0 && pageIndex < ctrl->pages().count()) {
      HWND targetHwnd = ctrl->pages()[pageIndex].identity.hwnd;
      m_managedWindows.remove(targetHwnd);
      ctrl->releasePage(pageIndex);
    }
  }
}

void StageManagerService::closePage(quint64 containerId, int pageIndex) {
  if (m_containers.contains(containerId)) {
    auto *ctrl = m_containers[containerId];
    if (pageIndex >= 0 && pageIndex < ctrl->pages().count()) {
      HWND targetHwnd = ctrl->pages()[pageIndex].identity.hwnd;
      m_managedWindows.remove(targetHwnd);
      ctrl->closePage(pageIndex);
    }
  }
}

void StageManagerService::closeContainer(quint64 containerId) {
  if (!m_containers.contains(containerId))
    return;

  auto *ctrl = m_containers.take(containerId);
  for (const auto &p : ctrl->pages()) {
    m_managedWindows.remove(p.identity.hwnd);
  }

  ctrl->restoreAllPages();
  ctrl->deleteLater();

  if (m_activeContainerId == containerId) {
    if (!m_containers.isEmpty()) {
      m_activeContainerId = m_containers.firstKey();
    } else {
      m_activeContainerId = 0;
    }
    emit activeContainerChanged(m_activeContainerId);
  }

  emit containerClosed(containerId);
  emit containerCountChanged(m_containers.count());
}

void StageManagerService::setPinned(quint64 containerId, bool pinned) {
  if (m_containers.contains(containerId)) {
    m_containers[containerId]->setPinned(pinned);
  }
}

void StageManagerService::setActiveDestination(quint64 containerId) {
  if (m_containers.contains(containerId) &&
      m_activeContainerId != containerId) {
    m_activeContainerId = containerId;
    emit activeContainerChanged(m_activeContainerId);
  }
}

bool StageManagerService::isManaged(qint64 hwnd) const {
  HWND nativeHwnd = (HWND)hwnd;
  if (!m_managedWindows.contains(nativeHwnd))
    return false;

  if (!IsWindow(nativeHwnd)) {
    const_cast<StageManagerService *>(this)->m_managedWindows.remove(nativeHwnd);
    return false;
  }

  DWORD pid = 0;
  GetWindowThreadProcessId(nativeHwnd, &pid);
  const auto &entry = m_managedWindows.value(nativeHwnd);
  if (pid == 0 || pid != entry.pid) {
    const_cast<StageManagerService *>(this)->m_managedWindows.remove(nativeHwnd);
    return false;
  }

  if (!m_containers.contains(entry.containerId)) {
    const_cast<StageManagerService *>(this)->m_managedWindows.remove(nativeHwnd);
    return false;
  }

  return true;
}

StageContainerController *
StageManagerService::getController(quint64 containerId) const {
  return m_containers.value(containerId, nullptr);
}

void StageManagerService::applyWin11RoundedCorners(QQuickWindow *window) {
  if (!window)
    return;

  HWND hwnd = (HWND)window->winId();
  if (!IsWindow(hwnd))
    return;

  const DWORD DWMWCP_ROUND = 2;
  const DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;
  DWORD preference = DWMWCP_ROUND;

  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference,
                        sizeof(preference));
}

void StageManagerService::closeAll() {
  auto keys = m_containers.keys();
  for (quint64 cid : keys) {
    closeContainer(cid);
  }
  m_managedWindows.clear();
}

void StageManagerService::onTargetDestroyed(HWND hwnd) {
  if (!m_managedWindows.contains(hwnd))
    return;

  m_managedWindows.remove(hwnd);

  for (auto *ctrl : m_containers.values()) {
    ctrl->onTargetDestroyed(hwnd);
  }
}

QString StageManagerService::dumpState() const {
  QString res;
  res += QString("ActiveContainer: %1, ContainerCount: %2\n")
             .arg(m_activeContainerId)
             .arg(m_containers.count());

  for (auto it = m_containers.cbegin(); it != m_containers.cend(); ++it) {
    auto *ctrl = it.value();
    res += QString("Container %1: pages=%2, activeIdx=%3, pinned=%4\n")
               .arg(it.key())
               .arg(ctrl->pageCount())
               .arg(ctrl->activeIndex())
               .arg(ctrl->isPinned());

    for (int i = 0; i < ctrl->pages().count(); ++i) {
      const auto &p = ctrl->pages()[i];
      res += QString("  Page %1: HWND=0x%2, title='%3', state=%4\n")
                 .arg(i)
                 .arg((quint64)p.identity.hwnd, 0, 16)
                 .arg(p.title)
                 .arg((int)p.state);
    }
  }
  return res;
}
