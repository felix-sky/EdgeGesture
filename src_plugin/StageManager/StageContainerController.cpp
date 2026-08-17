#include "StageContainerController.h"
#include "WindowEmbedder.h"
#include <QDebug>
#include <QVariantMap>
#include <cmath>
#include <mutex>

static const wchar_t *STAGE_HOST_CLASS_NAME =
    L"EdgeGesture_StageContainerHost";

static void RegisterStageHostClass() {
  static std::once_flag flag;
  std::call_once(flag, []() {
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = STAGE_HOST_CLASS_NAME;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);
  });
}

StageContainerController::StageContainerController(ContainerId id,
                                                   QObject *parent)
    : QObject(parent), m_containerId(id) {
  RegisterStageHostClass();
}

StageContainerController::~StageContainerController() {
  restoreAllPages();
  if (m_contentHostHwnd && IsWindow(m_contentHostHwnd)) {
    DestroyWindow(m_contentHostHwnd);
    m_contentHostHwnd = nullptr;
  }
}

QString StageContainerController::title() const {
  if (m_activeIndex >= 0 && m_activeIndex < m_pages.count()) {
    return m_pages[m_activeIndex].title;
  }
  return "Stage Container";
}

void StageContainerController::bindHostWindow(QQuickWindow *window,
                                              QQuickItem *anchorItem) {
  m_qwindow = window;
  m_anchorItem = anchorItem;

  if (m_qwindow) {
    m_qtHwnd = (HWND)m_qwindow->winId();
    createContentHostHwnd();

    connect(m_qwindow, &QQuickWindow::xChanged, this,
            &StageContainerController::syncGeometry);
    connect(m_qwindow, &QQuickWindow::yChanged, this,
            &StageContainerController::syncGeometry);
    connect(m_qwindow, &QQuickWindow::widthChanged, this,
            &StageContainerController::syncGeometry);
    connect(m_qwindow, &QQuickWindow::heightChanged, this,
            &StageContainerController::syncGeometry);
  }

  if (m_anchorItem) {
    connect(m_anchorItem, &QQuickItem::xChanged, this,
            &StageContainerController::syncGeometry);
    connect(m_anchorItem, &QQuickItem::yChanged, this,
            &StageContainerController::syncGeometry);
    connect(m_anchorItem, &QQuickItem::widthChanged, this,
            &StageContainerController::syncGeometry);
    connect(m_anchorItem, &QQuickItem::heightChanged, this,
            &StageContainerController::syncGeometry);
  }

  syncGeometry();
}

void StageContainerController::createContentHostHwnd() {
  if (!m_qtHwnd || m_contentHostHwnd)
    return;

  m_contentHostHwnd = CreateWindowExW(
      0, STAGE_HOST_CLASS_NAME, L"", WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE,
      0, 0, 100, 100, m_qtHwnd, NULL, GetModuleHandle(NULL), NULL);

  if (!m_contentHostHwnd) {
    qWarning()
        << "[StageManager] Failed to create StageContainer contentHostHwnd:"
        << GetLastError();
  }
}

RECT StageContainerController::getAnchorPhysicalRect() const {
  RECT rc = {0, 0, 800, 500};
  if (!m_qwindow || !m_anchorItem)
    return rc;

  qreal dpr = m_qwindow->effectiveDevicePixelRatio();
  QPointF pos = m_anchorItem->mapToScene(QPointF(0, 0));

  rc.left = (int)std::round(pos.x() * dpr);
  rc.top = (int)std::round(pos.y() * dpr);
  rc.right = rc.left + (int)std::round(m_anchorItem->width() * dpr);
  rc.bottom = rc.top + (int)std::round(m_anchorItem->height() * dpr);

  return rc;
}

void StageContainerController::syncGeometry() {
  if (!m_contentHostHwnd || !IsWindow(m_contentHostHwnd))
    return;

  RECT anchorRect = getAnchorPhysicalRect();
  int hostX = anchorRect.left;
  int hostY = anchorRect.top;
  int hostW = anchorRect.right - anchorRect.left;
  int hostH = anchorRect.bottom - anchorRect.top;

  SetWindowPos(m_contentHostHwnd, NULL, hostX, hostY, hostW, hostH,
               SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

  if (m_activeIndex >= 0 && m_activeIndex < m_pages.count()) {
    HWND activeHwnd = m_pages[m_activeIndex].identity.hwnd;
    RECT rcClient = {0, 0, hostW, hostH};
    WindowEmbedder::resizeTarget(activeHwnd, rcClient);
  }
}

bool StageContainerController::addPage(PageRecord page) {
  if (!m_contentHostHwnd) {
    createContentHostHwnd();
  }
  if (!m_contentHostHwnd) {
    qWarning() << "[StageManager] Cannot addPage: contentHostHwnd not ready";
    return false;
  }

  page.state = PageState::Attaching;
  if (!WindowEmbedder::attachTarget(page.identity.hwnd, m_contentHostHwnd,
                                    page.original)) {
    page.state = PageState::Failed;
    return false;
  }

  int newIndex = m_pages.count();
  page.containerId = m_containerId;
  page.state = PageState::AttachedInactive;
  m_pages.append(page);

  updatePageListModel();
  emit pageCountChanged(m_pages.count());
  emit pageAdded(page.id);

  // Activate the newly added page
  activatePage(newIndex);
  return true;
}

void StageContainerController::activatePage(int index) {
  if (index < 0 || index >= m_pages.count())
    return;

  RECT anchorRect = getAnchorPhysicalRect();
  RECT hostClient = {0, 0, anchorRect.right - anchorRect.left,
                     anchorRect.bottom - anchorRect.top};

  // Hide old page
  if (m_activeIndex >= 0 && m_activeIndex < m_pages.count() &&
      m_activeIndex != index) {
    m_pages[m_activeIndex].state = PageState::AttachedInactive;
    WindowEmbedder::setTargetVisible(m_pages[m_activeIndex].identity.hwnd,
                                     false, hostClient);
  }

  // Show and focus new page
  m_activeIndex = index;
  m_pages[m_activeIndex].state = PageState::AttachedActive;
  HWND newTarget = m_pages[m_activeIndex].identity.hwnd;
  WindowEmbedder::setTargetVisible(newTarget, true, hostClient);
  WindowEmbedder::focusTarget(newTarget, m_qtHwnd);

  updatePageListModel();
  emit activeIndexChanged(m_activeIndex);
  emit titleChanged();
}

bool StageContainerController::removePage(PageId pageId, bool restore) {
  int targetIdx = -1;
  for (int i = 0; i < m_pages.count(); ++i) {
    if (m_pages[i].id == pageId) {
      targetIdx = i;
      break;
    }
  }
  if (targetIdx == -1)
    return false;

  PageRecord page = m_pages.takeAt(targetIdx);
  if (restore && IsWindow(page.identity.hwnd)) {
    page.state = PageState::Restoring;
    WindowEmbedder::restoreTarget(page.identity.hwnd, page.original);
    page.state = PageState::Detached;
  }

  emit pageRemoved(page.id);
  emit pageCountChanged(m_pages.count());

  if (m_pages.isEmpty()) {
    m_activeIndex = -1;
    updatePageListModel();
    emit activeIndexChanged(m_activeIndex);
    emit titleChanged();
    requestContainerClose();
  } else {
    if (m_activeIndex >= m_pages.count()) {
      m_activeIndex = m_pages.count() - 1;
    }
    activatePage(m_activeIndex);
  }

  return true;
}

void StageContainerController::releasePage(int index) {
  if (index < 0 || index >= m_pages.count())
    return;
  removePage(m_pages[index].id, true);
}

void StageContainerController::closePage(int index) {
  if (index < 0 || index >= m_pages.count())
    return;

  HWND target = m_pages[index].identity.hwnd;
  if (IsWindow(target)) {
    PostMessageW(target, WM_CLOSE, 0, 0);
  }
}

void StageContainerController::onTargetDestroyed(HWND target) {
  for (int i = 0; i < m_pages.count(); ++i) {
    if (m_pages[i].identity.hwnd == target) {
      m_pages[i].state = PageState::Destroyed;
      removePage(m_pages[i].id, false);
      break;
    }
  }
}

void StageContainerController::restoreAllPages() {
  for (auto &page : m_pages) {
    if (IsWindow(page.identity.hwnd)) {
      page.state = PageState::Restoring;
      WindowEmbedder::restoreTarget(page.identity.hwnd, page.original);
      page.state = PageState::Detached;
    }
  }
  m_pages.clear();
  m_activeIndex = -1;
  updatePageListModel();
}

void StageContainerController::setPinned(bool pinned) {
  if (m_isPinned == pinned)
    return;
  m_isPinned = pinned;

  if (m_qtHwnd && IsWindow(m_qtHwnd)) {
    SetWindowPos(m_qtHwnd, pinned ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
  emit isPinnedChanged(m_isPinned);
}

void StageContainerController::minimizeContainer() {
  if (m_qwindow) {
    m_qwindow->showMinimized();
  }
}

void StageContainerController::requestContainerClose() {
  restoreAllPages();
  emit containerCloseRequested();
  if (m_qwindow) {
    m_qwindow->close();
  }
}

void StageContainerController::updatePageListModel() {
  QVariantList list;
  for (int i = 0; i < m_pages.count(); ++i) {
    const auto &p = m_pages[i];
    QVariantMap map;
    map["pageId"] = (quint64)p.id;
    map["hwnd"] = (quint64)p.identity.hwnd;
    map["title"] = p.title.isEmpty() ? p.identity.title : p.title;
    map["iconSource"] = QString("image://windowIcons/%1").arg((quint64)p.identity.hwnd);
    map["isActive"] = (i == m_activeIndex);
    map["index"] = i;
    list.append(map);
  }
  m_pageList = list;
  emit pagesModelChanged();
}
