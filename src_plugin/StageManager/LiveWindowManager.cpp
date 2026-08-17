#include "LiveWindowManager.h"
#include "StageManagerService.h"
#include "WindowEmbedder.h"
#include <QCoreApplication>
#include <QDebug>
#include <vector>

static LiveWindowManager *g_instance = nullptr;

LiveWindowManager::LiveWindowManager(QObject *parent)
    : QAbstractListModel(parent) {
  g_instance = this;
  m_myProcessId = GetCurrentProcessId();

  refresh();

  // Install focused WinEvent hooks
  m_hEventHook = SetWinEventHook(
      EVENT_OBJECT_CREATE, EVENT_OBJECT_NAMECHANGE, nullptr, WinEventProc, 0, 0,
      WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
}

LiveWindowManager::~LiveWindowManager() {
  if (m_hEventHook) {
    UnhookWinEvent(m_hEventHook);
    m_hEventHook = nullptr;
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
    if (IsIconic(nativeHwnd)) {
      ShowWindow(nativeHwnd, SW_RESTORE);
    }
    SetForegroundWindow(nativeHwnd);
  }
}

BOOL CALLBACK LiveWindowManager::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
  LiveWindowManager *manager = (LiveWindowManager *)lParam;
  if (manager && manager->isValidWindow(hwnd)) {
    manager->addWindow(hwnd);
  }
  return TRUE;
}

void LiveWindowManager::addWindow(HWND hwnd) {
  int length = GetWindowTextLengthW(hwnd);
  if (length == 0)
    return;

  std::vector<wchar_t> buffer(length + 1);
  GetWindowTextW(hwnd, buffer.data(), length + 1);
  QString title = QString::fromWCharArray(buffer.data());

  for (const auto &win : m_windows) {
    if ((HWND)win.hwnd == hwnd)
      return;
  }

  WindowInfo info;
  info.hwnd = (quint64)hwnd;
  info.title = title;
  m_windows.append(info);
}

bool LiveWindowManager::isValidWindow(HWND hwnd) {
  if (!WindowEmbedder::isCandidate(hwnd, m_myProcessId))
    return false;

  // Managed windows must not appear in the sidebar
  if (StageManagerService::instance() &&
      StageManagerService::instance()->isManaged((qint64)hwnd)) {
    return false;
  }

  return true;
}

void CALLBACK LiveWindowManager::WinEventProc(HWINEVENTHOOK hWinEventHook,
                                              DWORD event, HWND hwnd,
                                              LONG idObject, LONG idChild,
                                              DWORD dwEventThread,
                                              DWORD dwmsEventTime) {
  Q_UNUSED(hWinEventHook);
  Q_UNUSED(dwEventThread);
  Q_UNUSED(dwmsEventTime);

  if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF)
    return;
  if (!g_instance)
    return;

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
    if (StageManagerService::instance()) {
      StageManagerService::instance()->onTargetDestroyed(hwnd);
    }
    removeWindow(hwnd);
    return;
  }

  if (event == EVENT_OBJECT_DESTROY) {
    if (StageManagerService::instance()) {
      StageManagerService::instance()->onTargetDestroyed(hwnd);
    }
    removeWindow(hwnd);
  } else if (event == EVENT_OBJECT_CREATE || event == EVENT_OBJECT_SHOW) {
    if (isValidWindow(hwnd)) {
      bool exists = false;
      for (const auto &w : m_windows) {
        if ((HWND)w.hwnd == hwnd) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        int length = GetWindowTextLengthW(hwnd);
        if (length > 0) {
          std::vector<wchar_t> buffer(length + 1);
          GetWindowTextW(hwnd, buffer.data(), length + 1);
          QString title = QString::fromWCharArray(buffer.data());

          WindowInfo info;
          info.hwnd = (quint64)hwnd;
          info.title = title;

          beginInsertRows(QModelIndex(), m_windows.count(), m_windows.count());
          m_windows.append(info);
          endInsertRows();
        }
      }
    } else {
      // If it became managed or invalid, remove from sidebar
      removeWindow(hwnd);
    }
  } else if (event == EVENT_OBJECT_NAMECHANGE) {
    for (int i = 0; i < m_windows.count(); ++i) {
      if ((HWND)m_windows[i].hwnd == hwnd) {
        int length = GetWindowTextLengthW(hwnd);
        if (length > 0) {
          std::vector<wchar_t> buffer(length + 1);
          GetWindowTextW(hwnd, buffer.data(), length + 1);
          QString newTitle = QString::fromWCharArray(buffer.data());
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
