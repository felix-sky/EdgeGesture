#include "SettingsManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonDocument>

SettingsManager::SettingsManager(QObject *parent) : QObject(parent) {
  m_currentPath = QCoreApplication::applicationDirPath() + "/config.json";
  setupWatcher();
}

QString SettingsManager::currentConfigPath() const { return m_currentPath; }

void SettingsManager::setCurrentConfigPath(const QString &path) {
  if (m_currentPath != path) {
    m_currentPath = path;
    updateWatcher();
    qDebug() << "[SettingsManager] Config path changed to:" << m_currentPath;
  }
}

QJsonObject SettingsManager::load() { return loadFromPath(m_currentPath); }

QJsonObject SettingsManager::loadFromPath(const QString &path) {
  QFile file(path);
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

  QFile file(m_currentPath);
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QJsonDocument(config).toJson());
    qDebug() << "[SettingsManager] Saved config to:" << m_currentPath;
  }
}

void SettingsManager::setupWatcher() {
  m_watcher = new QFileSystemWatcher(this);
  updateWatcher();

  connect(m_watcher, &QFileSystemWatcher::fileChanged, this,
          &SettingsManager::onFileChanged);
  connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
          [this](const QString &path) {
            Q_UNUSED(path);
            if (QFile::exists(m_currentPath) &&
                !m_watcher->files().contains(m_currentPath)) {
              m_watcher->addPath(m_currentPath);
              onFileChanged(m_currentPath);
            }
          });
}

void SettingsManager::updateWatcher() {
  QStringList files = m_watcher->files();
  if (!files.isEmpty()) {
    m_watcher->removePaths(files);
  }

  if (QFile::exists(m_currentPath)) {
    m_watcher->addPath(m_currentPath);
  } else {
    QString dirPath = QFileInfo(m_currentPath).absolutePath();
    if (!m_watcher->directories().contains(dirPath)) {
      m_watcher->addPath(dirPath);
    }
  }
}

void SettingsManager::onFileChanged(const QString &path) {
  if (path != m_currentPath &&
      QFileInfo(path).absoluteFilePath() != m_currentPath) {
    return;
  }

  QFile file(m_currentPath);
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject newConfig = doc.object();

    if (newConfig.isEmpty() && !doc.isObject()) {
      return;
    }

    if (newConfig == m_lastConfig) {
      return;
    }

    qDebug() << "[SettingsManager] External change detected.";
    m_lastConfig = newConfig;
    emit configChanged(newConfig);
  }
}
