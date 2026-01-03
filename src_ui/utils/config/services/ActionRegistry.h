#pragma once

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

class ActionRegistry : public QObject {
  Q_OBJECT
  Q_PROPERTY(QStringList blacklist READ blacklist NOTIFY blacklistChanged)

public:
  explicit ActionRegistry(QObject *parent = nullptr);

  // Gestures
  Q_INVOKABLE QString getGesture(const QString &key) const;
  Q_INVOKABLE void setGesture(const QString &key, const QString &action);
  const QMap<QString, QString> &gestures() const { return m_gestureMap; }

  // Actions
  Q_INVOKABLE QVariantMap getActions() const;
  Q_INVOKABLE QStringList getActionNames() const;
  Q_INVOKABLE void addAction(const QString &name, const QString &command);
  Q_INVOKABLE void removeAction(const QString &name);
  Q_INVOKABLE bool hasAction(const QString &name) const;

  // Blacklist
  Q_INVOKABLE QStringList blacklist() const;
  Q_INVOKABLE void addBlacklistApp(const QString &exeName);
  Q_INVOKABLE void removeBlacklistApp(int index);

  // IO
  void loadFromConfig(const QJsonObject &root);
  void saveToConfig(QJsonObject &root) const;

signals:
  void gesturesChanged();
  void actionsChanged();
  void blacklistChanged();

private:
  QMap<QString, QString> m_gestureMap;
  QMap<QString, QString> m_actionMap;
  QStringList m_blacklist;
};
