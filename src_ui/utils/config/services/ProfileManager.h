#pragma once

#include <QObject>
#include <QStringList>

class QFileSystemWatcher;

class ProfileManager : public QObject {
  Q_OBJECT

  Q_PROPERTY(QStringList profiles READ profiles NOTIFY profilesChanged)
  Q_PROPERTY(
      QString currentProfile READ currentProfile NOTIFY currentProfileChanged)

public:
  explicit ProfileManager(QObject *parent = nullptr);

  // Get list of available profiles (process names)
  QStringList profiles() const { return m_profiles; }
  QString currentProfile() const { return m_currentProfile; }

  // Profile management - Q_INVOKABLE for QML access
  Q_INVOKABLE bool addProfile(const QString &processName);
  Q_INVOKABLE bool removeProfile(const QString &processName);
  Q_INVOKABLE bool switchToProfile(const QString &processName);
  Q_INVOKABLE void switchToDefault();

  // Scan for existing profiles
  Q_INVOKABLE void scanProfiles();

signals:
  void profilesChanged();
  void currentProfileChanged();
  void configSwitched(); // Emitted when config file is swapped

private:
  void ensureConfigsDir();
  QString configsDir() const;
  QString profilePath(const QString &processName) const;
  QString defaultProfilePath() const;
  QString mainConfigPath() const;

  bool copyFile(const QString &src, const QString &dest);
  bool moveFile(const QString &src, const QString &dest);

  QStringList m_profiles;
  QString m_currentProfile; // "default" or process name like "notepad.exe"
  QFileSystemWatcher *m_watcher;
};
