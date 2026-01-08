#pragma once

#include <QJsonObject>
#include <QObject>

class QFileSystemWatcher;

class SettingsManager : public QObject {
  Q_OBJECT
public:
  explicit SettingsManager(QObject *parent = nullptr);

  QJsonObject load();
  QJsonObject loadFromPath(const QString &path);
  void save(const QJsonObject &config);

  void setCurrentConfigPath(const QString &path);
  QString currentConfigPath() const;

signals:
  void configChanged(const QJsonObject &newConfig);

private:
  void setupWatcher();
  void updateWatcher();
  void onFileChanged(const QString &path);

  QString m_currentPath;
  QFileSystemWatcher *m_watcher = nullptr;
  QJsonObject m_lastConfig;
};
