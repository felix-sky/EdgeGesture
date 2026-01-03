#ifndef SYSTEMBRIDGE_H
#define SYSTEMBRIDGE_H

#include <QObject>

class SystemBridge : public QObject {
  Q_OBJECT

  // System State Properties
  Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY wifiEnabledChanged)
  Q_PROPERTY(bool bluetoothEnabled READ bluetoothEnabled NOTIFY
                 bluetoothEnabledChanged)
  Q_PROPERTY(bool nightLightEnabled READ nightLightEnabled NOTIFY
                 nightLightEnabledChanged)
  Q_PROPERTY(int brightnessLevel READ brightnessLevel WRITE setBrightnessLevel
                 NOTIFY brightnessLevelChanged)
  Q_PROPERTY(int volumeLevel READ volumeLevel WRITE setVolumeLevel NOTIFY
                 volumeLevelChanged)

public:
  explicit SystemBridge(QObject *parent = nullptr);

  // Getters
  bool wifiEnabled() const { return m_wifiEnabled; }
  bool bluetoothEnabled() const { return m_bluetoothEnabled; }
  bool nightLightEnabled() const { return m_nightLightEnabled; }
  int brightnessLevel() const { return m_brightnessLevel; }
  int volumeLevel() const { return m_volumeLevel; }

  // Setters
  void setBrightnessLevel(int level);
  void setVolumeLevel(int level);

  // System Control Methods
  Q_INVOKABLE void toggleWifi();
  Q_INVOKABLE void toggleBluetooth();
  Q_INVOKABLE void toggleNightLight();
  Q_INVOKABLE void toggleFocusAssist();

  Q_INVOKABLE void lockScreen();
  Q_INVOKABLE void launchTaskManager();
  Q_INVOKABLE void launchCalculator();
  Q_INVOKABLE void launchSnippingTool();
  Q_INVOKABLE void openSettings(const QString &page = "");
  Q_INVOKABLE void launchProcess(const QString &executable,
                                 const QStringList &args = QStringList());

  Q_INVOKABLE QString saveClipboardImage(const QString &saveDir);

signals:
  void wifiEnabledChanged();
  void bluetoothEnabledChanged();
  void nightLightEnabledChanged();
  void brightnessLevelChanged();
  void volumeLevelChanged();

private:
  bool m_wifiEnabled = false;
  bool m_bluetoothEnabled = false;
  bool m_nightLightEnabled = false;
  int m_brightnessLevel = 75;
  int m_volumeLevel = 50;

  void updateWifiState();
  void updateBluetoothState();
  void updateNightLightState();
  void updateBrightnessLevel();
  void updateVolumeLevel();
};

#endif // SYSTEMBRIDGE_H
