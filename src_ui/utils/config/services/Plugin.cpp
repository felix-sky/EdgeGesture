#include "Plugin.h"
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

Plugin::Plugin(QObject *parent) : QObject(parent) {}

bool isItemRoot(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return false;

  QTextStream in(&file);
  QString content = in.readAll();

  // Simple heuristic: Remove comments and find first brace
  // Note: This is a basic check.
  QRegularExpression re("//.*|/\\*.*?\\*/");
  content.replace(re, "");

  QRegularExpression rootRe("^\\s*(\\w+)\\s*\\{",
                            QRegularExpression::MultilineOption);
  QRegularExpressionMatch match = rootRe.match(content);
  if (match.hasMatch()) {
    QString type = match.captured(1);
    return type == "Item";
  }
  return false;
}

void Plugin::scanPlugins() {
  m_plugins.clear();
  QVariantList callablePlugins;

  // Scan component plugins from plugin/components/ (for PluginSettingsPage)
  QStringList componentPaths;
  componentPaths << QCoreApplication::applicationDirPath() +
                        "/plugin/components";
  componentPaths << QDir::currentPath() + "/plugin/components";

  for (const QString &path : componentPaths) {
    QDir dir(path);
    if (dir.exists()) {
      const QFileInfoList list =
          dir.entryInfoList(QStringList() << "*.qml", QDir::Files);
      for (const QFileInfo &info : list) {
        if (!isItemRoot(info.absoluteFilePath()))
          continue;

        QVariantMap plugin;
        plugin["name"] = info.baseName();
        plugin["path"] = "file:///" + info.absoluteFilePath();

        // Check if already exists (avoid duplicates from both paths)
        bool exists = false;
        for (int i = 0; i < m_plugins.size(); ++i) {
          if (m_plugins[i].toMap()["name"] == plugin["name"]) {
            exists = true;
            break;
          }
        }
        if (!exists) {
          m_plugins.append(plugin);
        }
      }
    }
  }

  // Also scan qrc:/plugin/components for embedded components
  QDir qrcComponentDir(":/plugin/components");
  if (qrcComponentDir.exists()) {
    const QFileInfoList list =
        qrcComponentDir.entryInfoList(QStringList() << "*.qml", QDir::Files);
    for (const QFileInfo &info : list) {
      if (!isItemRoot(info.absoluteFilePath()))
        continue;

      QString name = info.baseName();
      // Check if already exists (external overrides qrc)
      bool exists = false;
      for (int i = 0; i < m_plugins.size(); ++i) {
        if (m_plugins[i].toMap()["name"] == name) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        QVariantMap plugin;
        plugin["name"] = name;
        plugin["path"] = "qrc:/plugin/components/" + info.fileName();
        m_plugins.append(plugin);
      }
    }
  }

  // Scan callable plugins from plugin/ root (for ActionRegistry)
  // These are the ones that can be triggered by gestures
  QStringList callablePaths;
  callablePaths << QCoreApplication::applicationDirPath() + "/plugin";
  callablePaths << QDir::currentPath() + "/plugin";

  for (const QString &path : callablePaths) {
    QDir dir(path);
    if (dir.exists()) {
      const QFileInfoList list =
          dir.entryInfoList(QStringList() << "*.qml", QDir::Files);
      for (const QFileInfo &info : list) {
        if (!isItemRoot(info.absoluteFilePath()))
          continue;

        QVariantMap plugin;
        plugin["name"] = info.baseName();
        plugin["path"] = "file:///" + info.absoluteFilePath();

        // Check if already exists in callable list
        bool exists = false;
        for (int i = 0; i < callablePlugins.size(); ++i) {
          if (callablePlugins[i].toMap()["name"] == plugin["name"]) {
            exists = true;
            break;
          }
        }
        if (!exists) {
          callablePlugins.append(plugin);
        }
      }
    }
  }

  // Also scan qrc:/plugin root for embedded callable plugins
  QDir qrcDir(":/plugin");
  if (qrcDir.exists()) {
    const QFileInfoList list =
        qrcDir.entryInfoList(QStringList() << "*.qml", QDir::Files);
    for (const QFileInfo &info : list) {
      if (!isItemRoot(info.absoluteFilePath()))
        continue;

      QString name = info.baseName();
      bool exists = false;
      for (int i = 0; i < callablePlugins.size(); ++i) {
        if (callablePlugins[i].toMap()["name"] == name) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        QVariantMap plugin;
        plugin["name"] = name;
        plugin["path"] = "qrc:/plugin/" + info.fileName();
        callablePlugins.append(plugin);
      }
    }
  }

  qDebug() << "[Plugin] Scanned component plugins:" << m_plugins.size();
  qDebug() << "[Plugin] Scanned callable plugins:" << callablePlugins.size();

  emit pluginsChanged();
  // Only emit callable plugins to ActionRegistry
  emit pluginsScanned(callablePlugins);
}

QString Plugin::getPluginPath(const QString &name) {
  const auto &list = m_plugins;
  for (const auto &v : list) {
    QVariantMap map = v.toMap();
    if (map["name"].toString() == name) {
      return map["path"].toString();
    }
  }
  return "";
}

QVariantList Plugin::plugins() const { return m_plugins; }

QJsonArray Plugin::saveToConfig() const {
  QJsonArray pluginsArray;
  const auto &list = m_plugins;
  for (const auto &var : list) {
    QVariantMap map = var.toMap();
    QJsonObject plugin;
    plugin.insert("name", map["name"].toString());
    plugin.insert("path", map["path"].toString());
    pluginsArray.append(plugin);
  }
  return pluginsArray;
}
