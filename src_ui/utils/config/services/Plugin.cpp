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

  // 1. Scan QRC (Built-in)
  QDir qrcDir(":/plugin");
  if (qrcDir.exists()) {
    const QFileInfoList list =
        qrcDir.entryInfoList(QStringList() << "*.qml", QDir::Files);
    for (const QFileInfo &info : list) {
      if (!isItemRoot(info.absoluteFilePath()))
        continue;

      QVariantMap plugin;
      plugin["name"] = info.baseName();
      plugin["path"] = "qrc:/plugin/" + info.fileName();
      m_plugins.append(plugin);
    }
  }

  // 2. Scan Filesystem (Overrides QRC if name matches)
  // Look in plugin/components folder
  QStringList searchPaths;
  searchPaths << QCoreApplication::applicationDirPath() + "/plugin/components";
  searchPaths << QDir::currentPath() + "/plugin/components";

  for (const QString &path : searchPaths) {
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

        // Check if exists and override
        bool replaced = false;
        for (int i = 0; i < m_plugins.size(); ++i) {
          if (m_plugins[i].toMap()["name"] == plugin["name"]) {
            m_plugins[i] = plugin;
            replaced = true;
            break;
          }
        }
        if (!replaced) {
          m_plugins.append(plugin);
        }
      }
    }
  }

  qDebug() << "[Plugin] Scanned plugins, found:" << m_plugins.size();

  emit pluginsChanged();
  emit pluginsScanned(m_plugins);
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
