#include "NotesFileHandler.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>

NotesFileHandler::NotesFileHandler(QObject *parent) : QObject(parent) {}

QString NotesFileHandler::createNote(const QString &folderPath,
                                     const QString &title,
                                     const QString &content,
                                     const QString &color) {
  QString normalizedFolder = normalizePath(folderPath);
  QString safeTitle = sanitizeFileName(title);
  QString filePath = normalizedFolder + QStringLiteral("/") + safeTitle +
                     QStringLiteral(".md");

  // Build content with frontmatter
  QString fileContent = QStringLiteral("---\ncolor: ") + color +
                        QStringLiteral("\n---\n") + content;

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "NotesFileHandler: Failed to create note:" << filePath;
    return QString();
  }

  QTextStream out(&file);
  out << fileContent;
  file.close();

  return filePath;
}

bool NotesFileHandler::saveNote(const QString &filePath, const QString &content,
                                const QString &color) {
  QString normalizedPath = normalizePath(filePath);

  // Build content with frontmatter
  QString fileContent = QStringLiteral("---\ncolor: ") + color +
                        QStringLiteral("\n---\n") + content;

  QFile file(normalizedPath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "NotesFileHandler: Failed to save note:" << normalizedPath;
    return false;
  }

  QTextStream out(&file);
  out << fileContent;
  file.close();

  return true;
}

QVariantMap NotesFileHandler::readNote(const QString &filePath) {
  QVariantMap result;
  result[QStringLiteral("content")] = QString();
  result[QStringLiteral("color")] = QStringLiteral("#624a73");

  QString normalizedPath = normalizePath(filePath);
  QFile file(normalizedPath);

  if (!file.exists()) {
    qWarning() << "NotesFileHandler: File does not exist:" << normalizedPath;
    return result;
  }

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "NotesFileHandler: Failed to read note:" << normalizedPath;
    return result;
  }

  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  // Parse frontmatter
  if (content.startsWith(QLatin1String("---"))) {
    int endIndex = content.indexOf(QLatin1String("---"), 3);
    if (endIndex > 0) {
      QString frontmatter = content.mid(3, endIndex - 3);

      // Extract color
      QRegularExpression colorRegex(
          QStringLiteral("color:\\s*(#[a-fA-F0-9]+)"));
      QRegularExpressionMatch match = colorRegex.match(frontmatter);
      if (match.hasMatch()) {
        result[QStringLiteral("color")] = match.captured(1);
      }

      // Content after frontmatter
      result[QStringLiteral("content")] = content.mid(endIndex + 4).trimmed();
    } else {
      result[QStringLiteral("content")] = content;
    }
  } else {
    result[QStringLiteral("content")] = content;
  }

  return result;
}

bool NotesFileHandler::createFolder(const QString &parentPath,
                                    const QString &name) {
  QString normalizedParent = normalizePath(parentPath);
  QString safeName = sanitizeFileName(name);
  QString folderPath = normalizedParent + QStringLiteral("/") + safeName;

  QDir dir;
  return dir.mkpath(folderPath);
}

bool NotesFileHandler::deleteItem(const QString &path, bool isFolder) {
  QString normalizedPath = normalizePath(path);

  if (isFolder) {
    QDir dir(normalizedPath);
    return dir.removeRecursively();
  } else {
    return QFile::remove(normalizedPath);
  }
}

QString NotesFileHandler::renameItem(const QString &oldPath,
                                     const QString &newName, bool isFolder) {
  QString normalizedOldPath = normalizePath(oldPath);
  QFileInfo info(normalizedOldPath);
  QString parentDir = info.absolutePath();
  QString safeName = sanitizeFileName(newName);

  QString newPath;
  if (isFolder) {
    newPath = parentDir + QStringLiteral("/") + safeName;
    QDir dir(normalizedOldPath);
    if (dir.rename(normalizedOldPath, newPath)) {
      return newPath;
    }
  } else {
    newPath =
        parentDir + QStringLiteral("/") + safeName + QStringLiteral(".md");

    // Read old content
    QFile oldFile(normalizedOldPath);
    if (!oldFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      return QString();
    }
    QString content = QTextStream(&oldFile).readAll();
    oldFile.close();

    // Write to new file
    QFile newFile(newPath);
    if (!newFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
      return QString();
    }
    QTextStream out(&newFile);
    out << content;
    newFile.close();

    // Remove old file
    QFile::remove(normalizedOldPath);
    return newPath;
  }

  return QString();
}

bool NotesFileHandler::exists(const QString &path) {
  return QFile::exists(normalizePath(path));
}

QString NotesFileHandler::getFileName(const QString &path) {
  QFileInfo info(normalizePath(path));
  return info.fileName();
}

QString NotesFileHandler::urlToPath(const QString &urlString) {
  return normalizePath(urlString);
}

QString NotesFileHandler::getBaseName(const QString &path) {
  QFileInfo info(normalizePath(path));
  return info.baseName();
}

QString NotesFileHandler::sanitizeFileName(const QString &name) {
  QString result = name;
  // Remove invalid filename characters
  result.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")),
                 QStringLiteral("_"));
  return result;
}

QString NotesFileHandler::normalizePath(const QString &path) {
  QString result = path;
  if (result.startsWith(QLatin1String("file:///"))) {
    result = QUrl(result).toLocalFile();
  }
  return result;
}
