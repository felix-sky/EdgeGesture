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

signals:
  void pluginsChanged();
  void pluginsScanned(const QVariantList &plugins);

private:
  QVariantList m_plugins;
};

#endif // PLUGIN_H
