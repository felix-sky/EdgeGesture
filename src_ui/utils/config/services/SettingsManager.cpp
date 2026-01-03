#include "SettingsManager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonDocument>

SettingsManager::SettingsManager(QObject *parent) : QObject(parent) {
  setupWatcher();
}

QJsonObject SettingsManager::load() {
  QString configPath = QFileInfo("config.json").absoluteFilePath();
  QFile file(configPath);
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isNull()) {
      m_lastConfig = doc.object();
      return m_lastConfig;
    }
  }
  return QJsonObject();
}

void SettingsManager::save(const QJsonObject &config) {
  if (m_lastConfig == config)
    return;

  m_lastConfig = config;

  QString configPath = QFileInfo("config.json").absoluteFilePath();
  QFile file(configPath);
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QJsonDocument(config).toJson());
    qDebug() << "[SettingsManager] Saved config to disk.";
  }
}

void SettingsManager::setupWatcher() {
  m_watcher = new QFileSystemWatcher(this);
  QString configPath = QFileInfo("config.json").absoluteFilePath();
  QString dirPath = QFileInfo(configPath).absolutePath();

  if (QFile::exists(configPath)) {
    m_watcher->addPath(configPath);
  } else {
    m_watcher->addPath(dirPath);
  }

  connect(m_watcher, &QFileSystemWatcher::fileChanged, this,
          &SettingsManager::onFileChanged);
  connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
          [this, configPath](const QString &path) {
            Q_UNUSED(path);
            if (QFile::exists(configPath) &&
                !m_watcher->files().contains(configPath)) {
              m_watcher->addPath(configPath);
              onFileChanged(configPath);
            }
          });
}

void SettingsManager::onFileChanged(const QString &path) {
  QString configPath = QFileInfo("config.json").absoluteFilePath();
  if (path != configPath && QFileInfo(path).fileName() != "config.json") {
    return;
  }

  QFile file(configPath);
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject newConfig = doc.object();

    if (newConfig.isEmpty() && !doc.isObject()) {
      return;
    }

    if (newConfig == m_lastConfig) {
      return; // Ignore our own saves
    }

    qDebug() << "[SettingsManager] External change detected.";
    m_lastConfig = newConfig;
    emit configChanged(newConfig);
  }
}
