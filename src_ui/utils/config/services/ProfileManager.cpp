#include "ProfileManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>


ProfileManager::ProfileManager(QObject *parent)
    : QObject(parent), m_currentProfile("default") {
  ensureConfigsDir();

  // Setup file watcher
  m_watcher = new QFileSystemWatcher(this);
  m_watcher->addPath(configsDir());

  connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
          [this](const QString &path) {
            qDebug() << "[ProfileManager] Directory changed:" << path;
            scanProfiles();
          });

  scanProfiles();
}

void ProfileManager::ensureConfigsDir() {
  QDir dir(configsDir());
  if (!dir.exists()) {
    dir.mkpath(".");
    qDebug() << "[ProfileManager] Created configs directory:" << configsDir();

    // Create default.json by copying current config.json if it exists
    QString mainConfig = mainConfigPath();
    if (QFile::exists(mainConfig)) {
      copyFile(mainConfig, defaultProfilePath());
      qDebug() << "[ProfileManager] Created default.json from current config";
    }
  } else if (!QFile::exists(defaultProfilePath())) {
    // configs dir exists but no default.json - create it
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
    // Remove .json extension to get profile name
    QString profileName = file;
    profileName.chop(5); // Remove ".json"
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

  // Check if profile already exists
  if (m_profiles.contains(processName)) {
    qWarning() << "[ProfileManager] Profile already exists:" << processName;
    return false;
  }

  // Copy current config.json to the new profile
  QString mainConfig = mainConfigPath();
  QString newProfilePath = profilePath(processName);

  if (!QFile::exists(mainConfig)) {
    qWarning() << "[ProfileManager] No config.json to copy from";
    return false;
  }

  if (!copyFile(mainConfig, newProfilePath)) {
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

  // If this is the current profile, switch to default first
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

  // Check if already on this profile
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

  QString mainConfig = mainConfigPath();

  // Step 1: Save current config to old profile
  QString oldProfilePath;
  if (m_currentProfile == "default") {
    oldProfilePath = defaultProfilePath();
  } else {
    oldProfilePath = profilePath(m_currentProfile);
  }

  // Move (not copy) current config.json to old profile
  if (QFile::exists(mainConfig)) {
    // Remove old profile file first if it exists
    if (QFile::exists(oldProfilePath)) {
      QFile::remove(oldProfilePath);
    }
    if (!moveFile(mainConfig, oldProfilePath)) {
      qWarning() << "[ProfileManager] Failed to save current config to profile";
      return false;
    }
    qDebug() << "[ProfileManager] Saved current config to:" << oldProfilePath;
  }

  // Step 2: Copy target profile to config.json
  if (!copyFile(targetProfilePath, mainConfig)) {
    qWarning() << "[ProfileManager] Failed to activate profile:" << processName;
    // Try to restore
    moveFile(oldProfilePath, mainConfig);
    return false;
  }

  m_currentProfile = processName;
  qDebug() << "[ProfileManager] Switched to profile:" << processName;
  emit currentProfileChanged();
  emit configSwitched();
  return true;
}

void ProfileManager::switchToDefault() { switchToProfile("default"); }

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
