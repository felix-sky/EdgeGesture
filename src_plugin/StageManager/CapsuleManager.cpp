#include "CapsuleManager.h"
#include <QDebug>

CapsuleManager *CapsuleManager::s_instance = nullptr;

CapsuleManager *CapsuleManager::instance() { return s_instance; }

CapsuleManager::CapsuleManager(QObject *parent) : QAbstractListModel(parent) {
  s_instance = this;
}

CapsuleManager::~CapsuleManager() {
  closeAll();
  s_instance = nullptr;
}

int CapsuleManager::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_controllers.count();
}

QVariant CapsuleManager::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_controllers.count())
    return QVariant();

  WindowController *controller = m_controllers[index.row()];

  switch (role) {
  case HwndRole:
    return QVariant::fromValue(controller->hwndConstant());
  case TitleRole:
    return controller->title();
  case ControllerRole:
    return QVariant::fromValue(controller);
  }
  return QVariant();
}

QHash<int, QByteArray> CapsuleManager::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[HwndRole] = "hwnd";
  roles[TitleRole] = "title";
  roles[ControllerRole] = "controller";
  return roles;
}

void CapsuleManager::addWindow(qint64 hwndVal) {
  HWND hwnd = (HWND)hwndVal;

  // Check duplicates
  for (auto *c : m_controllers) {
    if (c->hwndConstant() == hwndVal)
      return;
  }

  beginInsertRows(QModelIndex(), m_controllers.count(), m_controllers.count());
  auto *controller = new WindowController(hwnd, this);
  connect(controller, &WindowController::closed, this,
          [this, hwndVal]() { removeWindow(hwndVal); });
  m_controllers.append(controller);
  endInsertRows();
}

void CapsuleManager::removeWindow(qint64 hwndVal) {
  for (int i = 0; i < m_controllers.count(); ++i) {
    if (m_controllers[i]->hwndConstant() == hwndVal) {
      beginRemoveRows(QModelIndex(), i, i);
      auto *c = m_controllers.takeAt(i);
      // Before deleting, restore window logic should happen (in controller
      // destructor or explicit) Ideally controller destructor calls Restore.
      delete c;
      endRemoveRows();
      return;
    }
  }
}

void CapsuleManager::closeAll() {
  beginResetModel();
  qDeleteAll(m_controllers);
  m_controllers.clear();
  endResetModel();
}
