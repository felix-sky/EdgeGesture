#include "SettingsManager.h"

#include <QDebug>
#include <QFile>
#include <QFileSystemWatcher>
#include <QJsonDocument>

SettingsManager::SettingsManager(QObject *parent) : QObject(parent) {
  setupWatcher();
}

QJsonObject SettingsManager::load() {
  QFile file("config.json");
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    m_lastConfig = doc.object();
    return m_lastConfig;
  }
  return QJsonObject();
}

void SettingsManager::save(const QJsonObject &config) {
  if (m_lastConfig == config)
    return;

  m_lastConfig = config;

  QFile file("config.json");
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QJsonDocument(config).toJson());
    qDebug() << "[SettingsManager] Saved config to disk.";
  }
}

void SettingsManager::setupWatcher() {
  m_watcher = new QFileSystemWatcher(this);
  if (QFile::exists("config.json")) {
    m_watcher->addPath("config.json");
  } else {
    m_watcher->addPath(".");
  }

  connect(m_watcher, &QFileSystemWatcher::fileChanged, this,
          &SettingsManager::onFileChanged);
  connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
          [this](const QString &path) {
            if (QFile::exists("config.json") &&
                !m_watcher->files().contains("config.json")) {
              m_watcher->addPath("config.json");
              onFileChanged("config.json");
            }
          });
}

void SettingsManager::onFileChanged(const QString &path) {
  QFile file("config.json");
  if (file.open(QIODevice::ReadOnly)) {
    QJsonObject newConfig = QJsonDocument::fromJson(file.readAll()).object();
    if (newConfig == m_lastConfig) {
      return; // Ignore our own saves
    }

    qDebug() << "[SettingsManager] External change detected.";
    m_lastConfig = newConfig;
    emit configChanged(newConfig);
  }
}
