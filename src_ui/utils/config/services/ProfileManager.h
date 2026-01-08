#pragma once

#include <QObject>
#include <QStringList>

class QFileSystemWatcher;
class SettingsManager;

class ProfileManager : public QObject {
  Q_OBJECT

  Q_PROPERTY(QStringList profiles READ profiles NOTIFY profilesChanged)
  Q_PROPERTY(
      QString currentProfile READ currentProfile NOTIFY currentProfileChanged)

public:
  explicit ProfileManager(QObject *parent = nullptr);

  void setSettingsManager(SettingsManager *manager);

  QStringList profiles() const { return m_profiles; }
  QString currentProfile() const { return m_currentProfile; }

  Q_INVOKABLE bool addProfile(const QString &processName);
  Q_INVOKABLE bool removeProfile(const QString &processName);
  Q_INVOKABLE bool switchToProfile(const QString &processName);
  Q_INVOKABLE void switchToDefault();

  Q_INVOKABLE void scanProfiles();

  void setCurrentProfile(const QString &profileName);

signals:
  void profilesChanged();
  void currentProfileChanged();
  void configSwitched();

private:
  void ensureConfigsDir();
  QString configsDir() const;
  QString profilePath(const QString &processName) const;
  QString defaultProfilePath() const;
  QString mainConfigPath() const;

  bool copyFile(const QString &src, const QString &dest);
  bool moveFile(const QString &src, const QString &dest);

  QStringList m_profiles;
  QString m_currentProfile;
  QFileSystemWatcher *m_watcher;
  SettingsManager *m_settingsManager = nullptr;
};
