#include "FileBridge.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

FileBridge::FileBridge(QObject *parent) : QObject(parent) {}

QString FileBridge::normalizePath(const QString &path) {
  QString localPath = path;
  if (path.startsWith(QLatin1String("file://"))) {
    localPath = QUrl(path).toLocalFile();
  }
  // Handle Windows specific issues if any, usually toLocalFile is enough
  return localPath;
}

QString FileBridge::readFile(const QString &path) {
  QString localPath = normalizePath(path);
  if (!QFile::exists(localPath)) {
    qWarning() << "FileBridge: File does not exist:" << localPath;
    return QString();
  }

  QFile file(localPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "FileBridge: Failed to open file for reading:" << localPath;
    return QString();
  }

  QTextStream in(&file);
  // Set codec if needed, usually UTF-8 is default for Qt6
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  in.setCodec("UTF-8");
#endif
  QString content = in.readAll();
  file.close();
  return content;
}

bool FileBridge::writeFile(const QString &path, const QString &content) {
  QString localPath = normalizePath(path);
  QFile file(localPath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "FileBridge: Failed to open file for writing:" << localPath;
    return false;
  }

  QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  out.setCodec("UTF-8");
#endif
  out << content;
  file.close();
  return true;
}

bool FileBridge::appendFile(const QString &path, const QString &content) {
  QString localPath = normalizePath(path);
  QFile file(localPath);
  if (!file.open(QIODevice::Append | QIODevice::Text)) {
    qWarning() << "FileBridge: Failed to open file for appending:" << localPath;
    return false;
  }

  QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  out.setCodec("UTF-8");
#endif
  out << content;
  file.close();
  return true;
}

bool FileBridge::exists(const QString &path) {
  return QFile::exists(normalizePath(path));
}

bool FileBridge::removeFile(const QString &path) {
  return QFile::remove(normalizePath(path));
}

bool FileBridge::createDirectory(const QString &path) {
  QDir dir;
  return dir.mkpath(normalizePath(path));
}

QString FileBridge::getFileName(const QString &path) {
  QFileInfo fileInfo(normalizePath(path));
  return fileInfo.fileName();
}

QString FileBridge::urlToPath(const QString &urlString) {
  return normalizePath(urlString);
}

QString FileBridge::getAppPath() {
  return QCoreApplication::applicationDirPath();
}

QString FileBridge::listDirectory(const QString &path) {
  QString localPath = normalizePath(path);
  QDir dir(localPath);

  if (!dir.exists()) {
    qWarning() << "FileBridge: Directory does not exist:" << localPath;
    return QStringLiteral("[]");
  }

  QJsonArray result;
  dir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
  dir.setSorting(QDir::DirsFirst | QDir::Name);
  QFileInfoList entries = dir.entryInfoList();

  for (const QFileInfo &info : entries) {
    QJsonObject obj;
    obj.insert(QStringLiteral("name"), info.fileName());
    obj.insert(QStringLiteral("path"), info.absoluteFilePath());
    obj.insert(QStringLiteral("isDir"), info.isDir());
    obj.insert(
        QStringLiteral("modifiedDate"),
        info.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
    obj.insert(QStringLiteral("baseName"), info.baseName());
    result.append(obj);
  }

  return QString::fromUtf8(
      QJsonDocument(result).toJson(QJsonDocument::Compact));
}

bool FileBridge::removeDirectory(const QString &path) {
  QString localPath = normalizePath(path);
  QDir dir(localPath);
  if (!dir.exists()) {
    return true;
  }
  return dir.removeRecursively();
}

bool FileBridge::isDirectory(const QString &path) {
  QString localPath = normalizePath(path);
  QFileInfo info(localPath);
  return info.isDir();
}

QString FileBridge::getBaseName(const QString &path) {
  QFileInfo fileInfo(normalizePath(path));
  return fileInfo.baseName();
}
