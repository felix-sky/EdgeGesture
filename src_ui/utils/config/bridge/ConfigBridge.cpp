#include "ConfigBridge.h"

#include "../services/ActionRegistry.h"
#include "../services/Plugin.h"
#include "../services/ProfileManager.h"
#include "../services/SettingsManager.h"
#include "../system/EngineControl.h"
#include "../system/SystemEventListener.h"
#include "../system/WindowsUtils.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

ConfigBridge::ConfigBridge(QObject *parent)
    : QObject(parent), m_loading(false) {
  // Initialize Sub-components
  m_plugin = new Plugin(this);

  m_engineControl = new EngineControl(this);
  m_actionRegistry = new ActionRegistry(this);
  m_settingsManager = new SettingsManager(this);
  m_profileManager = new ProfileManager(this);

  // Connect profile manager - reload config when profile switches
  connect(m_profileManager, &ProfileManager::configSwitched, this, [this]() {
    loadConfig();
    m_engineControl->notifyChanges();
  });

  // Initialize System Event Listener
  m_systemListener = new SystemEventListener(this);
  QCoreApplication::instance()->installNativeEventFilter(m_systemListener);
  connect(m_systemListener, &SystemEventListener::showPluginRequest, this,
          &ConfigBridge::showPlugin);

  // Auto-save timer
  m_saveTimer = new QTimer(this);
  m_saveTimer->setSingleShot(true);
  m_saveTimer->setInterval(500);
  connect(m_saveTimer, &QTimer::timeout, this, &ConfigBridge::delayedSave);

  // Initialize Config Objects
  m_physics = new PhysicsSettings(this);
  connect(m_physics, &PhysicsSettings::settingsChanged, this,
          &ConfigBridge::requestSave);

  m_leftHandle = new HandleSettings(this);
  connect(m_leftHandle, &HandleSettings::settingsChanged, this,
          &ConfigBridge::requestSave);

  m_rightHandle = new HandleSettings(this);
  connect(m_rightHandle, &HandleSettings::settingsChanged, this,
          &ConfigBridge::requestSave);

  connect(m_actionRegistry, &ActionRegistry::gesturesChanged, this,
          &ConfigBridge::requestSave);
  connect(m_actionRegistry, &ActionRegistry::actionsChanged, this,
          &ConfigBridge::requestSave);

  connect(m_plugin, &Plugin::pluginsScanned, this,
          &ConfigBridge::onPluginsScanned);
  connect(m_actionRegistry, &ActionRegistry::blacklistChanged, this,
          &ConfigBridge::requestSave);

  connect(m_settingsManager, &SettingsManager::configChanged, this,
          [this](const QJsonObject &root) {
            Q_UNUSED(root);
            loadConfig();
          });

  // Load config
  m_engineControl->cleanUp();
  loadConfig();
  m_plugin->scanPlugins();
}

ConfigBridge::~ConfigBridge() {
  QCoreApplication::instance()->removeNativeEventFilter(m_systemListener);
}

// Accessors for QML are defined inline in header.

// Other Properties
QStringList ConfigBridge::enabledPlugins() const { return m_enabledPlugins; }
void ConfigBridge::setEnabledPlugins(const QStringList &list) {
  if (m_enabledPlugins != list) {
    m_enabledPlugins = list;
    emit enabledPluginsChanged();
    requestSave();
  }
}

bool ConfigBridge::engineEnabled() const { return m_engineEnabled; }
void ConfigBridge::setEngineEnabled(bool enabled) {
  if (m_engineEnabled != enabled) {
    m_engineEnabled = enabled;
    emit engineEnabledChanged();
    requestSave();

    m_engineControl->setEnabled(enabled);
  }
}

int ConfigBridge::splitMode() const { return m_splitMode; }
void ConfigBridge::setSplitMode(int mode) {
  if (m_splitMode != mode) {
    m_splitMode = mode;
    emit splitModeChanged();
    requestSave();
  }
}

// Methods
void ConfigBridge::requestSave() {
  if (m_loading)
    return;

  if (!m_saveTimer->isActive()) {
    m_saveTimer->start();
  }
}

void ConfigBridge::delayedSave() {
  generateConfig();
  QJsonObject root = QJsonObject::fromVariantMap(m_configCache);
  m_settingsManager->save(root);
  if (m_engineControl)
    m_engineControl->notifyChanges();
}

void ConfigBridge::applySettings() {
  requestSave();
  if (m_engineControl) {
    m_engineControl->notifyChanges();
  }
}

void ConfigBridge::setPreviewHandle(bool pressed, bool isLeft) {
  if (m_engineControl) {
    m_engineControl->setPreviewHandle(pressed, isLeft);
  }
}

void ConfigBridge::setWindowDark(bool dark) {
  WindowsUtils::setWindowDark(dark);
}

void ConfigBridge::loadConfig() {
  m_loading = true;

  QJsonObject root = m_settingsManager->load();
  m_configCache = root.toVariantMap();

  // Load Engine Status
  setEngineEnabled(root["engine_enabled"].toBool(true));

  // Load Sub-objects
  m_physics->loadFromConfig(root);
  m_leftHandle->loadFromConfig(root);
  m_rightHandle->loadFromConfig(root);
  m_actionRegistry->loadFromConfig(root);

  // Load Split Mode from general (after physics loaded general)
  if (root.contains("general")) {
    setSplitMode(root["general"].toObject()["split_mode"].toInt(0));
  } else {
    setSplitMode(0);
  }

  QJsonArray pluginsArr = root["enabled_plugins"].toArray();
  QStringList pluginsList;
  for (const auto &val : pluginsArr)
    pluginsList.append(val.toString());
  setEnabledPlugins(pluginsList);

  // Explicitly set engine state
  m_engineControl->setEnabled(m_engineEnabled);

  m_loading = false;
  emit settingsChanged();
}

void ConfigBridge::generateConfig() {
  QJsonObject root;

  root["engine_enabled"] = m_engineEnabled;
  root["enabled_plugins"] = QJsonArray::fromStringList(m_enabledPlugins);

  // Save Sub-objects
  m_physics->saveToConfig(root); // Creates 'general' object
  m_leftHandle->saveToConfig(root);
  m_rightHandle->saveToConfig(root);
  m_actionRegistry->saveToConfig(root);

  // Save Split Mode to general
  QJsonObject general = root["general"].toObject();
  general["split_mode"] = m_splitMode;
  root["general"] = general;

  m_configCache = root.toVariantMap();
}

void ConfigBridge::onPluginsScanned(const QVariantList &plugins) {
  bool actionsUpdated = false;
  for (const auto &var : plugins) {
    QString name = var.toMap()["name"].toString();
    if (!name.isEmpty() && !m_actionRegistry->hasAction(name)) {
      QString command = "plugin:" + name;
      m_actionRegistry->addAction(name, command);
      actionsUpdated = true;
    }
  }

  if (actionsUpdated) {
    requestSave();
  }
}
