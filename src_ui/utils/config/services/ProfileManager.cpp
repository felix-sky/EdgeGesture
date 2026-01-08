#include "ProfileManager.h"
#include "SettingsManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>

ProfileManager::ProfileManager(QObject *parent)
    : QObject(parent), m_currentProfile("default") {
  ensureConfigsDir();

  m_watcher = new QFileSystemWatcher(this);
  m_watcher->addPath(configsDir());

  connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
          [this](const QString &path) {
            qDebug() << "[ProfileManager] Directory changed:" << path;
            scanProfiles();
          });

  scanProfiles();
}

void ProfileManager::setSettingsManager(SettingsManager *manager) {
  m_settingsManager = manager;
}

void ProfileManager::ensureConfigsDir() {
  QDir dir(configsDir());
  if (!dir.exists()) {
    dir.mkpath(".");
    qDebug() << "[ProfileManager] Created configs directory:" << configsDir();

    QString mainConfig = mainConfigPath();
    if (QFile::exists(mainConfig)) {
      copyFile(mainConfig, defaultProfilePath());
      qDebug() << "[ProfileManager] Created default.json from current config";
    }
  } else if (!QFile::exists(defaultProfilePath())) {
    QString mainConfig = mainConfigPath();
    if (QFile::exists(mainConfig)) {
      copyFile(mainConfig, defaultProfilePath());
      qDebug() << "[ProfileManager] Created default.json from current config";
    }
  }
}

QString ProfileManager::configsDir() const {
  return QCoreApplication::applicationDirPath() + "/configs";
}

QString ProfileManager::profilePath(const QString &processName) const {
  return configsDir() + "/" + processName + ".json";
}

QString ProfileManager::defaultProfilePath() const {
  return configsDir() + "/default.json";
}

QString ProfileManager::mainConfigPath() const {
  return QCoreApplication::applicationDirPath() + "/config.json";
}

void ProfileManager::scanProfiles() {
  m_profiles.clear();

  QDir dir(configsDir());
  QStringList filters;
  filters << "*.json";
  QStringList files = dir.entryList(filters, QDir::Files);

  for (const QString &file : files) {
    QString profileName = file;
    profileName.chop(5);
    if (profileName != "default") {
      m_profiles.append(profileName);
    }
  }

  qDebug() << "[ProfileManager] Found profiles:" << m_profiles;
  emit profilesChanged();
}

bool ProfileManager::addProfile(const QString &processName) {
  if (processName.isEmpty()) {
    qWarning() << "[ProfileManager] Cannot add profile with empty name";
    return false;
  }

  if (m_profiles.contains(processName)) {
    qWarning() << "[ProfileManager] Profile already exists:" << processName;
    return false;
  }

  QString sourceConfig = m_settingsManager
                             ? m_settingsManager->currentConfigPath()
                             : mainConfigPath();
  QString newProfilePath = profilePath(processName);

  if (!QFile::exists(sourceConfig)) {
    qWarning() << "[ProfileManager] No source config to copy from";
    return false;
  }

  if (!copyFile(sourceConfig, newProfilePath)) {
    qWarning() << "[ProfileManager] Failed to create profile:" << processName;
    return false;
  }

  m_profiles.append(processName);
  qDebug() << "[ProfileManager] Added profile:" << processName;
  emit profilesChanged();
  return true;
}

bool ProfileManager::removeProfile(const QString &processName) {
  if (processName.isEmpty() || processName == "default") {
    qWarning() << "[ProfileManager] Cannot remove default or empty profile";
    return false;
  }

  QString path = profilePath(processName);
  if (!QFile::exists(path)) {
    qWarning() << "[ProfileManager] Profile does not exist:" << processName;
    return false;
  }

  if (m_currentProfile == processName) {
    switchToDefault();
  }

  if (!QFile::remove(path)) {
    qWarning() << "[ProfileManager] Failed to delete profile:" << processName;
    return false;
  }

  m_profiles.removeAll(processName);
  qDebug() << "[ProfileManager] Removed profile:" << processName;
  emit profilesChanged();
  return true;
}

bool ProfileManager::switchToProfile(const QString &processName) {
  if (processName.isEmpty()) {
    switchToDefault();
    return true;
  }

  if (m_currentProfile == processName) {
    qDebug() << "[ProfileManager] Already on profile:" << processName;
    return true;
  }

  QString targetProfilePath;
  if (processName == "default") {
    targetProfilePath = defaultProfilePath();
  } else {
    targetProfilePath = profilePath(processName);
  }

  if (!QFile::exists(targetProfilePath)) {
    qDebug() << "[ProfileManager] Profile not found, using default:"
             << processName;
    switchToDefault();
    return false;
  }

  m_currentProfile = processName;

  if (m_settingsManager) {
    m_settingsManager->setCurrentConfigPath(targetProfilePath);
  }

  qDebug() << "[ProfileManager] Switched to profile:" << processName;
  emit currentProfileChanged();
  emit configSwitched();
  return true;
}

void ProfileManager::switchToDefault() { switchToProfile("default"); }

void ProfileManager::setCurrentProfile(const QString &profileName) {
  if (m_currentProfile != profileName) {
    m_currentProfile = profileName;

    QString targetPath;
    if (profileName == "default") {
      targetPath = defaultProfilePath();
    } else {
      targetPath = profilePath(profileName);
    }

    if (m_settingsManager && QFile::exists(targetPath)) {
      m_settingsManager->setCurrentConfigPath(targetPath);
    }

    emit currentProfileChanged();
  }
}

bool ProfileManager::copyFile(const QString &src, const QString &dest) {
  if (QFile::exists(dest)) {
    QFile::remove(dest);
  }
  return QFile::copy(src, dest);
}

bool ProfileManager::moveFile(const QString &src, const QString &dest) {
  if (QFile::exists(dest)) {
    QFile::remove(dest);
  }
  return QFile::rename(src, dest);
}
