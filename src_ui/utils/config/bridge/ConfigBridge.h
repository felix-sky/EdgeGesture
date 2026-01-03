#ifndef CONFIGBRIDGE_H
#define CONFIGBRIDGE_H

#include <QObject>
#include <QVariantMap>

#include "../models/HandleSettings.h"
#include "../models/PhysicsSettings.h"
#include "../services/ActionRegistry.h"
#include "../services/Plugin.h"
#include "../system/SystemEventListener.h"

// Forward declarations for other classes
class EngineControl;
class SettingsManager;
class QTimer;

class ConfigBridge : public QObject {
  Q_OBJECT

  // Sub-components
  Q_PROPERTY(Plugin *plugin READ plugin CONSTANT)
  Q_PROPERTY(ActionRegistry *actionRegistry READ actionRegistry CONSTANT)

  // Config Groups
  Q_PROPERTY(PhysicsSettings *physics READ physics CONSTANT)
  Q_PROPERTY(HandleSettings *leftHandle READ leftHandle CONSTANT)
  Q_PROPERTY(HandleSettings *rightHandle READ rightHandle CONSTANT)

  // Others
  Q_PROPERTY(QStringList enabledPlugins READ enabledPlugins WRITE
                 setEnabledPlugins NOTIFY enabledPluginsChanged)
  Q_PROPERTY(bool engineEnabled READ engineEnabled WRITE setEngineEnabled NOTIFY
                 engineEnabledChanged)
  Q_PROPERTY(
      int splitMode READ splitMode WRITE setSplitMode NOTIFY splitModeChanged)

public:
  explicit ConfigBridge(QObject *parent = nullptr);
  ~ConfigBridge() override;

  // Accessors for QML
  Plugin *plugin() const { return m_plugin; }
  ActionRegistry *actionRegistry() const { return m_actionRegistry; }

  PhysicsSettings *physics() const { return m_physics; }
  HandleSettings *leftHandle() const { return m_leftHandle; }
  HandleSettings *rightHandle() const { return m_rightHandle; }

  // Other Properties
  QStringList enabledPlugins() const;
  void setEnabledPlugins(const QStringList &list);

  bool engineEnabled() const;
  void setEngineEnabled(bool enabled);

  int splitMode() const;
  void setSplitMode(int mode);

  // Methods
  Q_INVOKABLE void requestSave();
  Q_INVOKABLE void applySettings();
  Q_INVOKABLE void setPreviewHandle(bool pressed, bool isLeft);
  Q_INVOKABLE void setWindowDark(bool dark);

signals:
  void enabledPluginsChanged();
  void engineEnabledChanged();
  void splitModeChanged();
  void showPlugin(QString name);
  void settingsChanged(); // Restored signal

private slots:
  void onPluginsScanned(const QVariantList &plugins);
  void delayedSave();

private:
  void loadConfig();
  void generateConfig();

  // Sub-objects
  Plugin *m_plugin;
  ActionRegistry *m_actionRegistry;

  PhysicsSettings *m_physics;
  HandleSettings *m_leftHandle;
  HandleSettings *m_rightHandle;

  // System
  EngineControl *m_engineControl;
  SettingsManager *m_settingsManager;
  SystemEventListener *m_systemListener;

  // Internal state
  QVariantMap m_configCache;
  QTimer *m_saveTimer;

  bool m_engineEnabled;
  int m_splitMode;
  QStringList m_enabledPlugins;
};

#endif // CONFIGBRIDGE_H
