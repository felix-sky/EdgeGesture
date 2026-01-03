#ifndef PLUGIN_H
#define PLUGIN_H

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QVariantList>

class Plugin : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)

public:
  explicit Plugin(QObject *parent = nullptr);

  Q_INVOKABLE void scanPlugins();
  Q_INVOKABLE QString getPluginPath(const QString &name);
  QVariantList plugins() const;

  // For loading/saving config
  QJsonArray saveToConfig() const;

  // Note: loadFromConfig might not be strictly necessary if scanPlugins
  // determines the state, but if we persist some plugin metadata it would be.
  // The original code loads plugins from config, but scanPlugins overwrites
  // m_plugins largely based on filesystem. However, scanPlugins is called in
  // constructor of ConfigBridge. Let's copy the load logic if there is any
  // specific persistence logic. Looking at ConfigBridge::load(), it doesn't
  // actually load 'plugins' array into m_plugins. It only saves it.
  // scanPlugins() populates m_plugins. So we don't need loadFromConfig for
  // plugins unless we want to cache them. Logic: scanPlugins() is called in
  // ConfigBridge constructor.

signals:
  void pluginsChanged();
  void pluginsScanned(const QVariantList &plugins);

private:
  QVariantList m_plugins;
};

#endif // PLUGIN_H
