#pragma once

#include <QJsonObject>
#include <QObject>

class QFileSystemWatcher;

class SettingsManager : public QObject {
  Q_OBJECT
public:
  explicit SettingsManager(QObject *parent = nullptr);

  QJsonObject load();
  void save(const QJsonObject &config);

signals:
  void configChanged(const QJsonObject &newConfig);

private:
  void setupWatcher();
  void onFileChanged(const QString &path);

  QFileSystemWatcher *m_watcher = nullptr;
  QJsonObject m_lastConfig;
};
