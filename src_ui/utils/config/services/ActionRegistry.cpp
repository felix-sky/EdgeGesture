#include "ActionRegistry.h"
#include <QJsonArray>
#include <QJsonObject>

ActionRegistry::ActionRegistry(QObject *parent) : QObject(parent) {}

QString ActionRegistry::getGesture(const QString &key) const {
  return m_gestureMap.value(key, "none");
}

void ActionRegistry::setGesture(const QString &key, const QString &action) {
  if (m_gestureMap.value(key) != action) {
    m_gestureMap.insert(key, action);
    emit gesturesChanged();
  }
}

QVariantMap ActionRegistry::getActions() const {
  QVariantMap map;
  for (auto it = m_actionMap.begin(); it != m_actionMap.end(); ++it) {
    map.insert(it.key(), it.value());
  }
  return map;
}

QStringList ActionRegistry::getActionNames() const {
  return m_actionMap.keys();
}

void ActionRegistry::addAction(const QString &name, const QString &command) {
  if (!name.isEmpty() && !command.isEmpty()) {
    m_actionMap.insert(name, command);
    emit actionsChanged();
  }
}

void ActionRegistry::removeAction(const QString &name) {
  if (m_actionMap.contains(name)) {
    m_actionMap.remove(name);
    emit actionsChanged();
  }
}

bool ActionRegistry::hasAction(const QString &name) const {
  return m_actionMap.contains(name);
}

QStringList ActionRegistry::blacklist() const { return m_blacklist; }

void ActionRegistry::addBlacklistApp(const QString &exeName) {
  if (!exeName.isEmpty() &&
      !m_blacklist.contains(exeName, Qt::CaseInsensitive)) {
    m_blacklist.append(exeName);
    emit blacklistChanged();
  }
}

void ActionRegistry::removeBlacklistApp(int index) {
  if (index >= 0 && index < m_blacklist.size()) {
    m_blacklist.removeAt(index);
    emit blacklistChanged();
  }
}

void ActionRegistry::loadFromConfig(const QJsonObject &root) {
  // Gestures
  m_gestureMap.clear();
  QJsonObject gestures = root.value("gestures").toObject();
  for (auto it = gestures.begin(); it != gestures.end(); ++it) {
    m_gestureMap.insert(it.key(), it.value().toString());
  }

  // Defaults
  if (m_gestureMap.isEmpty()) {
    m_gestureMap.insert("left_right", "back");
    m_gestureMap.insert("left_diag_up", "TaskView");
    m_gestureMap.insert("left_diag_down", "ShowDesktop");
    m_gestureMap.insert("left_long_right", "QuickPanel");
  }

  // Actions
  m_actionMap.clear();
  if (root.contains("actions")) {
    QJsonObject actions = root.value("actions").toObject();
    for (auto it = actions.begin(); it != actions.end(); ++it) {
      m_actionMap.insert(it.key(), it.value().toString());
    }
  }

  // Blacklist
  m_blacklist.clear();
  QJsonArray blacklist = root.value("blacklist").toArray();
  for (const auto &val : std::as_const(blacklist)) {
    m_blacklist.append(val.toString());
  }
}

void ActionRegistry::saveToConfig(QJsonObject &root) const {
  QJsonObject gestures;
  for (auto it = m_gestureMap.begin(); it != m_gestureMap.end(); ++it) {
    gestures.insert(it.key(), it.value());
  }
  root.insert("gestures", gestures);

  QJsonObject actions;
  for (auto it = m_actionMap.begin(); it != m_actionMap.end(); ++it) {
    actions.insert(it.key(), it.value());
  }
  root.insert("actions", actions);

  QJsonArray blacklist;
  for (const QString &app : m_blacklist) {
    blacklist.append(app);
  }
  root.insert("blacklist", blacklist);
}
