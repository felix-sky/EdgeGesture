#include "LiveWindowManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

// Global pointer for the hook callback to access the instance
static LiveWindowManager *g_instance = nullptr;

LiveWindowManager::LiveWindowManager(QObject *parent)
    : QAbstractListModel(parent) {
  g_instance = this;
  m_myProcessId = GetCurrentProcessId();

  // Initial scan
  refresh();

  // Install hooks
  // EVENT_OBJECT_CREATE (0x8000) to EVENT_OBJECT_DESTROY (0x8001) covers window
  // creation/destruction EVENT_SYSTEM_FOREGROUND (0x0003) covers focus changes
  // (optional for reordering) EVENT_OBJECT_NAMECHANGE (0x800C) covers title
  // changes
  m_hEventHook =
      SetWinEventHook(EVENT_MIN, EVENT_MAX, nullptr, WinEventProc, 0, 0,
                      WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
}

LiveWindowManager::~LiveWindowManager() {
  if (m_hEventHook) {
    UnhookWinEvent(m_hEventHook);
  }
  g_instance = nullptr;
}

int LiveWindowManager::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_windows.count();
}

QVariant LiveWindowManager::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_windows.count())
    return QVariant();

  const WindowInfo &window = m_windows[index.row()];

  switch (role) {
  case HwndRole:
    return QVariant::fromValue(window.hwnd);
  case TitleRole:
    return window.title;
  case IconRole:
    return QString("image://windowIcons/%1").arg(window.hwnd);
  }

  return QVariant();
}

QHash<int, QByteArray> LiveWindowManager::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[HwndRole] = "hwnd";
  roles[TitleRole] = "title";
  roles[IconRole] = "iconSource";
  return roles;
}

void LiveWindowManager::refresh() {
  beginResetModel();
  m_windows.clear();
  EnumWindows(EnumWindowsProc, (LPARAM)this);
  endResetModel();
}

void LiveWindowManager::activateWindow(quint64 hwnd) {
  HWND nativeHwnd = (HWND)hwnd;
  if (IsWindow(nativeHwnd)) {
    // If minimized, restore it
    if (IsIconic(nativeHwnd)) {
      ShowWindow(nativeHwnd, SW_RESTORE);
    }
    SetForegroundWindow(nativeHwnd);
  }
}

BOOL CALLBACK LiveWindowManager::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
  LiveWindowManager *manager = (LiveWindowManager *)lParam;
  if (manager->isValidWindow(hwnd)) {
    manager->addWindow(hwnd);
  }
  return TRUE;
}

void LiveWindowManager::addWindow(HWND hwnd) {
  // Get title
  int length = GetWindowTextLengthW(hwnd);
  if (length == 0)
    return;

  WCHAR *buffer = new WCHAR[length + 1];
  GetWindowTextW(hwnd, buffer, length + 1);
  QString title = QString::fromWCharArray(buffer);
  delete[] buffer;

  // Check if already exists (shouldn't happen during EnumWindow, but might in
  // events)
  for (const auto &win : m_windows) {
    if ((HWND)win.hwnd == hwnd)
      return;
  }

  // In EnumWindows, we are building the list, so we might want to append
  // directly Ideally we shouldn't modify m_windows directly if not inside
  // beginResetModel/endResetModel BUT EnumWindowsProc is called synchronously
  // within refresh() which calls beginResetModel. However, for single window
  // additions via events, we need beginInsertRows.

  // We need to differentiate bulk load vs single add
  // For now, let's assume this is called inside refresh() OR carefully wrapped.
  // Actually, let's make addWindow handle the single-insertion case if we are
  // not resetting. Complexity: knowing if we are resetting.

  // Simplification for V1: separate internalAdd (for bulk) and public/event
  // add. FOR NOW: Let's assume addWindow is used by EnumWindows which clears
  // list first.

  WindowInfo info;
  info.hwnd = (quint64)hwnd;
  info.title = title;

  // Since we cleared the list in refresh(), we just append.
  m_windows.append(info);
}

bool LiveWindowManager::isValidWindow(HWND hwnd) {
  if (!IsWindowVisible(hwnd))
    return false;

  // Check if it has a title (some tool windows don't)
  if (GetWindowTextLengthW(hwnd) == 0)
    return false;

  // Exclude tool windows
  LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
  if (exStyle & WS_EX_TOOLWINDOW)
    return false;

  // Exclude our own process
  DWORD processId;
  GetWindowThreadProcessId(hwnd, &processId);
  if (processId == m_myProcessId)
    return false;

  // Check for Cloaked windows (Windows 10/11 Virtual Desktop ghosts)
  int cloakedVal = 0;
  HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloakedVal,
                                     sizeof(cloakedVal));
  if (SUCCEEDED(hr) && cloakedVal != 0) {
    return false;
  }

  // Additional check: Is it the Program Manager? (Desktop)
  WCHAR className[256];
  GetClassNameW(hwnd, className, 256);
  if (wcscmp(className, L"Progman") == 0 ||
      wcscmp(className, L"WorkerW") == 0) {
    return false;
  }

  // Exclude application frame host if it is empty/cloaked (often handled by
  // cloaked check but good to note)

  return true;
}

void CALLBACK LiveWindowManager::WinEventProc(HWINEVENTHOOK hWinEventHook,
                                              DWORD event, HWND hwnd,
                                              LONG idObject, LONG idChild,
                                              DWORD dwEventThread,
                                              DWORD dwmsEventTime) {
  if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF)
    return;
  if (!g_instance)
    return;

  // We should process this on the main thread to be safe with Qt
  // Use QMetaObject::invokeMethod
  QMetaObject::invokeMethod(
      g_instance,
      [event, hwnd]() {
        if (g_instance)
          g_instance->onWindowEvent(event, hwnd);
      },
      Qt::QueuedConnection);
}

void LiveWindowManager::onWindowEvent(DWORD event, HWND hwnd) {
  if (!IsWindow(hwnd)) {
    removeWindow(hwnd);
    return;
  }

  if (event == EVENT_OBJECT_DESTROY) {
    removeWindow(hwnd);
  } else if (event == EVENT_OBJECT_CREATE || event == EVENT_OBJECT_SHOW) {
    if (isValidWindow(hwnd)) {
      // Check existence first
      bool exists = false;
      for (const auto &w : m_windows) {
        if ((HWND)w.hwnd == hwnd) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        // Single insert
        // Get title
        int length = GetWindowTextLengthW(hwnd);
        if (length > 0) {
          WCHAR *buffer = new WCHAR[length + 1];
          GetWindowTextW(hwnd, buffer, length + 1);
          QString title = QString::fromWCharArray(buffer);
          delete[] buffer;

          WindowInfo info;
          info.hwnd = (quint64)hwnd;
          info.title = title;

          beginInsertRows(QModelIndex(), m_windows.count(), m_windows.count());
          m_windows.append(info);
          endInsertRows();
        }
      }
    }
  } else if (event == EVENT_OBJECT_NAMECHANGE) {
    // Update title
    for (int i = 0; i < m_windows.count(); ++i) {
      if ((HWND)m_windows[i].hwnd == hwnd) {
        int length = GetWindowTextLengthW(hwnd);
        if (length > 0) {
          WCHAR *buffer = new WCHAR[length + 1];
          GetWindowTextW(hwnd, buffer, length + 1);
          QString newTitle = QString::fromWCharArray(buffer);
          delete[] buffer;
          if (m_windows[i].title != newTitle) {
            m_windows[i].title = newTitle;
            emit dataChanged(index(i), index(i), {TitleRole});
          }
        }
        break;
      }
    }
  } else if (event == EVENT_OBJECT_HIDE) {
    removeWindow(hwnd);
  }
}

void LiveWindowManager::removeWindow(HWND hwnd) {
  for (int i = 0; i < m_windows.count(); ++i) {
    if ((HWND)m_windows[i].hwnd == hwnd) {
      beginRemoveRows(QModelIndex(), i, i);
      m_windows.removeAt(i);
      endRemoveRows();
      break;
    }
  }
}
