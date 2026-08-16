#include "QmlDwmThumbnail.h"
#include <QDebug>
#include <QQuickWindow>
#include <QScreen>

#pragma comment(lib, "dwmapi.lib")

QmlDwmThumbnail::QmlDwmThumbnail(QQuickItem *parent) : QQuickItem(parent) {
  setFlag(ItemHasContents, false); // We don't render with QSG
}

QmlDwmThumbnail::~QmlDwmThumbnail() { unregisterThumbnail(); }

quint64 QmlDwmThumbnail::sourceHwnd() const { return m_sourceHwnd; }

void QmlDwmThumbnail::setSourceHwnd(quint64 newSourceHwnd) {
  if (m_sourceHwnd == newSourceHwnd)
    return;
  m_sourceHwnd = newSourceHwnd;
  emit sourceHwndChanged();

  unregisterThumbnail();
  if (isComponentComplete() && window()) {
    registerThumbnail();
  }
}

bool QmlDwmThumbnail::interactive() const { return m_interactive; }

void QmlDwmThumbnail::setInteractive(bool newInteractive) {
  if (m_interactive == newInteractive)
    return;
  m_interactive = newInteractive;
  emit interactiveChanged();
  // interactive property might affect how we handle input or DWM properties
  // For now, DWM thumbnails are purely visual unless we set specific properties
  updateThumbnailRect();
}

void QmlDwmThumbnail::componentComplete() {
  QQuickItem::componentComplete();
  if (window()) {
    m_window = window();
    if (m_sourceHwnd) {
      registerThumbnail();
    }
  }
}

void QmlDwmThumbnail::geometryChange(const QRectF &newGeometry,
                                     const QRectF &oldGeometry) {
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  updateThumbnailRect();
}

void QmlDwmThumbnail::itemChange(ItemChange change,
                                 const ItemChangeData &value) {
  if (change == ItemSceneChange) {
    if (value.window) {
      m_window = value.window;
      if (m_sourceHwnd) {
        // Window changed (e.g. reparented or created)
        unregisterThumbnail();
        registerThumbnail();
      }
    } else {
      // Window lost
      unregisterThumbnail();
      m_window = nullptr;
    }
  } else if (change == ItemVisibleHasChanged) {
    if (isVisible()) {
      updateThumbnailRect();
    } else {
      // Optimization: hide thumbnail if item is invisible?
      // Currently DWM handles clipping, but we could update properties to
      // invisible
    }
  }

  QQuickItem::itemChange(change, value);
}

void QmlDwmThumbnail::registerThumbnail() {
  if (!m_sourceHwnd || !m_window)
    return;

  HRESULT hr = DwmRegisterThumbnail((HWND)m_window->winId(), (HWND)m_sourceHwnd,
                                    &m_hThumbnail);
  if (SUCCEEDED(hr)) {
    updateThumbnailRect();
  } else {
    qWarning() << "DwmRegisterThumbnail failed with HR:" << hr;
    m_hThumbnail = nullptr;
  }
}

void QmlDwmThumbnail::unregisterThumbnail() {
  if (m_hThumbnail) {
    DwmUnregisterThumbnail(m_hThumbnail);
    m_hThumbnail = nullptr;
  }
}

void QmlDwmThumbnail::updateThumbnailRect() {
  if (!m_hThumbnail || !m_window)
    return;

  // 1. Get Item position in Scene (Qt Window client area)
  QPointF itemPos = mapToScene(QPointF(0, 0));

  // 2. Get Qt Window position on screen (Global)
  // Note: window()->position() includes the frame if it exists.
  // However, DwmRegisterThumbnail destRect is relative to the client area of
  // the destination window (m_window->winId()). So we actually DON'T need the
  // global window position if targeting the window handle directly. WAIT! The
  // user instructions specifically mentioned: "QML x,y is relative... convert
  // to global... DWM... rcDestination" Let's re-verify
  // DwmUpdateThumbnailProperties documentation. "rcDestination: The destination
  // rectangle in the window coordinates of the hDstWnd window." So it IS
  // relative to the destination window client area, NOT global screen
  // coordinates. BUT, if the QQuickWindow is a child of another window or has
  // complex DPI, we need to be careful.

  // Qt's mapToScene returns coordinates relative to the QQuickWindow's content
  // item. QQuickWindow's winId() usually maps to the window itself.

  // 3. Handle HiDPI
  qreal dpr = m_window->devicePixelRatio();

  // Calculate coordinates relative to the backing window (hDstWnd)
  int finalX = (int)(itemPos.x() * dpr);
  int finalY = (int)(itemPos.y() * dpr);
  int finalW = (int)(width() * dpr);
  int finalH = (int)(height() * dpr);

  RECT destRect = {finalX, finalY, finalX + finalW, finalY + finalH};

  // 5. Optimization
  if (memcmp(&m_lastDestRect, &destRect, sizeof(RECT)) == 0) {
    return;
  }
  m_lastDestRect = destRect;

  DWM_THUMBNAIL_PROPERTIES props;
  props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE |
                  DWM_TNP_SOURCECLIENTAREAONLY | DWM_TNP_OPACITY;
  props.rcDestination = destRect;
  props.fVisible = isVisible();
  props.fSourceClientAreaOnly = FALSE; // Include non-client area (title bar)
                                       // for now, maybe expose as prop
  props.opacity = (BYTE)(opacity() * 255);

  // If sourceHwnd is minimized, this might show the icon or nothing depending
  // on Windows version and settings. We can assume the Manager will handle
  // restoration if needed, or we just show what we can.

  DwmUpdateThumbnailProperties(m_hThumbnail, &props);
}
