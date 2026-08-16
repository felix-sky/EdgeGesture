#include "WindowController.h"
#include "SafeWindowReparenter.h"
#include <QDebug>
#include <QQuickWindow>

WindowController::WindowController(HWND hwnd, QObject *parent)
    : QObject(parent), m_hwnd(hwnd) {
  m_reparenter = SafeWindowReparenter::instance();
}

WindowController::~WindowController() {
  // Cleanup is handled by Manager or signals
}

QString WindowController::title() const {
  if (!IsWindow(m_hwnd))
    return "Invalid Window";
  int len = GetWindowTextLengthW(m_hwnd);
  if (len == 0)
    return "Unknown";
  std::vector<wchar_t> buf(len + 1);
  GetWindowTextW(m_hwnd, buf.data(), len + 1);
  return QString::fromWCharArray(buf.data());
}

qint64 WindowController::hwndConstant() const { return (qint64)m_hwnd; }

QQuickItem *WindowController::capsuleItem() const { return m_capsuleItem; }

void WindowController::setCapsuleItem(QQuickItem *item) {
  if (m_capsuleItem == item)
    return;

  if (m_capsuleItem) {
    disconnect(m_capsuleItem, nullptr, this, nullptr);
  } else {
    // If we are unbinding, hide the window so it doesn't float around
    // ghost-like
    if (IsWindow(m_hwnd)) {
      ShowWindow(m_hwnd, SW_HIDE);
    }
  }

  m_capsuleItem = item;

  if (m_capsuleItem) {
    // Connect changes to syncGeometry
    connect(m_capsuleItem, &QQuickItem::xChanged, this,
            &WindowController::onCapsuleXChanged);
    connect(m_capsuleItem, &QQuickItem::yChanged, this,
            &WindowController::onCapsuleYChanged);
    connect(m_capsuleItem, &QQuickItem::widthChanged, this,
            &WindowController::onCapsuleWidthChanged);
    connect(m_capsuleItem, &QQuickItem::heightChanged, this,
            &WindowController::onCapsuleHeightChanged);

    // Initial Reparent
    if (!m_reparenter)
      m_reparenter = SafeWindowReparenter::instance();
    if (m_reparenter) {
      m_reparenter->reparentWindow((qint64)m_hwnd, m_capsuleItem);

      // Connect to global signals safely
      connect(m_reparenter, &SafeWindowReparenter::windowActiveStateChanged,
              this, &WindowController::onWindowActiveStateChanged,
              Qt::UniqueConnection);
      connect(m_reparenter, &SafeWindowReparenter::windowDestroyed, this,
              &WindowController::onWindowDestroyed, Qt::UniqueConnection);
    } else {
      qWarning() << "SafeWindowReparenter instance not found in "
                    "WindowController::setCapsuleItem";
    }

    // Ensure it is visible when we switch to it
    if (IsWindow(m_hwnd)) {
      ShowWindow(m_hwnd, SW_SHOW);
    }
  }

  emit capsuleItemChanged();
}

void WindowController::onCapsuleXChanged() { syncGeometry(); }
void WindowController::onCapsuleYChanged() { syncGeometry(); }
void WindowController::onCapsuleWidthChanged() { syncGeometry(); }
void WindowController::onCapsuleHeightChanged() { syncGeometry(); }

void WindowController::syncGeometry() {
  if (!m_capsuleItem || !IsWindow(m_hwnd))
    return;

  // We need the Reparenter to update the position
  if (m_reparenter) {
    m_reparenter->updateWindowPosition((qint64)m_hwnd, m_capsuleItem);
  } else {
    // Retry getting instance just in case
    m_reparenter = SafeWindowReparenter::instance();
    if (m_reparenter) {
      m_reparenter->updateWindowPosition((qint64)m_hwnd, m_capsuleItem);
    }
  }
}

void WindowController::requestMove(int x, int y) {
  if (m_capsuleItem) {
    m_capsuleItem->setX(x);
    m_capsuleItem->setY(y);
    // Sync happens via signal
  }
}

void WindowController::requestResize(int w, int h) {
  if (m_capsuleItem) {
    m_capsuleItem->setWidth(w);
    m_capsuleItem->setHeight(h);
  }
}

void WindowController::requestFocus() {
  if (IsWindow(m_hwnd)) {
    SetForegroundWindow(m_hwnd);
    SetFocus(m_hwnd);
  }
}

void WindowController::requestClose() {
  if (IsWindow(m_hwnd)) {
    PostMessage(m_hwnd, WM_CLOSE, 0, 0);
  }
  emit closed();
}

void WindowController::requestRelease() {
  // Restore window to original state without killing it
  if (m_reparenter) {
    m_reparenter->restoreWindow((qint64)m_hwnd);
  }
  emit closed(); // Signal manager to remove this controller
}

float WindowController::getDevicePixelRatio() const {
  if (m_capsuleItem && m_capsuleItem->window()) {
    return m_capsuleItem->window()->devicePixelRatio();
  }
  return 1.0f;
}

void WindowController::onWindowActiveStateChanged(qint64 hwnd, bool active) {
  if (hwnd == (qint64)m_hwnd) {
    emit activeChanged(active);
  }
}

void WindowController::onWindowDestroyed(qint64 hwnd) {
  if (hwnd == (qint64)m_hwnd) {
    qWarning() << "Window destroyed unexpectedly (Zombie detection):" << m_hwnd;
    requestRelease(); // Or close
  }
}
