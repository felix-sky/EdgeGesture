#include "NotesFileHandler.h"
#include <QDir>
#include <QDirIterator>
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

  // Read existing file to preserve frontmatter fields
  QString existingTags;
  QString existingPinned;

  QFile readFile(normalizedPath);
  if (readFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream in(&readFile);
    QString existingContent = in.readAll();
    readFile.close();

    // Parse existing frontmatter to preserve tags and pinned
    if (existingContent.startsWith(QLatin1String("---"))) {
      int endIndex = existingContent.indexOf(QLatin1String("---"), 3);
      if (endIndex > 0) {
        QString frontmatter = existingContent.mid(3, endIndex - 3);

        // Extract tags
        QRegularExpression tagsRegex(
            QStringLiteral("tags:\\s*\\[([^\\]]*)\\]"));
        QRegularExpressionMatch tagsMatch = tagsRegex.match(frontmatter);
        if (tagsMatch.hasMatch()) {
          existingTags = QStringLiteral("tags: [") + tagsMatch.captured(1) +
                         QStringLiteral("]\n");
        }

        // Extract pinned
        QRegularExpression pinnedRegex(
            QStringLiteral("pinned:\\s*(true|false)"));
        QRegularExpressionMatch pinnedMatch = pinnedRegex.match(frontmatter);
        if (pinnedMatch.hasMatch()) {
          existingPinned = QStringLiteral("pinned: ") +
                           pinnedMatch.captured(1) + QStringLiteral("\n");
        }
      }
    }
  }

  // Build content with frontmatter, preserving existing fields
  QString fileContent =
      QStringLiteral("---\ncolor: ") + color + QStringLiteral("\n");
  if (!existingTags.isEmpty()) {
    fileContent += existingTags;
  }
  if (!existingPinned.isEmpty()) {
    fileContent += existingPinned;
  }
  fileContent += QStringLiteral("---\n") + content;

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
  return info.completeBaseName();
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

bool NotesFileHandler::updateFrontmatter(const QString &path,
                                         const QString &key,
                                         const QVariant &value) {
  QString normalizedPath = normalizePath(path);

  QFile file(normalizedPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning()
        << "NotesFileHandler: Failed to read file for frontmatter update:"
        << normalizedPath;
    return false;
  }

  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  // Format the value
  QString valueStr;
  if (value.typeId() == QMetaType::Bool) {
    valueStr =
        value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  } else if (value.typeId() == QMetaType::QStringList) {
    QStringList list = value.toStringList();
    valueStr = QStringLiteral("[") + list.join(QStringLiteral(", ")) +
               QStringLiteral("]");
  } else {
    valueStr = value.toString();
  }

  // Check if frontmatter exists
  if (content.startsWith(QLatin1String("---"))) {
    int endIdx = content.indexOf(QLatin1String("---"), 3);
    if (endIdx > 0) {
      QString frontmatter = content.mid(3, endIdx - 3);
      QString afterFrontmatter = content.mid(endIdx);

      // Check if key already exists
      QRegularExpression keyRegex(key + QStringLiteral(":\\s*[^\\n]+"));
      if (keyRegex.match(frontmatter).hasMatch()) {
        // Replace existing key
        frontmatter.replace(keyRegex, key + QStringLiteral(": ") + valueStr);
      } else {
        // Add new key before closing ---
        frontmatter +=
            key + QStringLiteral(": ") + valueStr + QStringLiteral("\n");
      }

      content = QStringLiteral("---") + frontmatter + afterFrontmatter;
    }
  } else {
    // No frontmatter, create one
    content = QStringLiteral("---\n") + key + QStringLiteral(": ") + valueStr +
              QStringLiteral("\n---\n") + content;
  }

  // Write back
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "NotesFileHandler: Failed to write frontmatter update:"
               << normalizedPath;
    return false;
  }

  QTextStream out(&file);
  out << content;
  file.close();

  return true;
}

QStringList NotesFileHandler::getTags(const QString &path) {
  QString normalizedPath = normalizePath(path);
  QStringList tags;

  QFile file(normalizedPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return tags;
  }

  // Read only header for efficiency
  char buffer[1024];
  qint64 bytesRead = file.read(buffer, sizeof(buffer));
  QString header = QString::fromUtf8(buffer, static_cast<int>(bytesRead));
  file.close();

  if (header.startsWith(QLatin1String("---"))) {
    int endIdx = header.indexOf(QLatin1String("---"), 3);
    if (endIdx > 0) {
      QString frontmatter = header.mid(3, endIdx - 3);

      // Match tags: [tag1, tag2, tag3]
      QRegularExpression tagsRegex(QStringLiteral("tags:\\s*\\[([^\\]]+)\\]"));
      QRegularExpressionMatch match = tagsRegex.match(frontmatter);

      if (match.hasMatch()) {
        QString tagsStr = match.captured(1);
        const QStringList rawTags = tagsStr.split(QLatin1Char(','));
        for (const QString &tag : rawTags) {
          QString cleaned = tag.trimmed();
          cleaned.remove(QLatin1Char('"'));
          cleaned.remove(QLatin1Char('\''));
          if (!cleaned.isEmpty()) {
            tags.append(cleaned);
          }
        }
      }
    }
  }

  return tags;
}

QString NotesFileHandler::findImage(const QString &imageName,
                                    const QString &notePath,
                                    const QString &rootPath) {
  QString normalizedRoot = normalizePath(rootPath);
  QString normalizedNotePath = normalizePath(notePath);

  // Get the folder containing the note
  QFileInfo noteInfo(normalizedNotePath);
  QString noteFolder = noteInfo.absolutePath();

  // List of places to check (in order of preference)
  QStringList searchPaths;

  // 1. Same folder as the note
  searchPaths << noteFolder + QStringLiteral("/") + imageName;

  // 2. Attachments folder (common Obsidian pattern)
  searchPaths << noteFolder + QStringLiteral("/attachments/") + imageName;
  searchPaths << normalizedRoot + QStringLiteral("/attachments/") + imageName;

  // 3. Images folder (common pattern)
  searchPaths << noteFolder + QStringLiteral("/images/") + imageName;
  searchPaths << normalizedRoot + QStringLiteral("/images/") + imageName;

  // 4. Assets folder (another common pattern)
  searchPaths << noteFolder + QStringLiteral("/assets/") + imageName;
  searchPaths << normalizedRoot + QStringLiteral("/assets/") + imageName;

  // 5. Root folder
  searchPaths << normalizedRoot + QStringLiteral("/") + imageName;

  // Check each path
  for (const QString &path : std::as_const(searchPaths)) {
    if (QFile::exists(path)) {
      return QDir::cleanPath(path);
    }
  }

  // If not found in common locations, do a recursive search (expensive but
  // thorough)
  QDirIterator it(normalizedRoot, QStringList() << imageName, QDir::Files,
                  QDirIterator::Subdirectories);
  if (it.hasNext()) {
    return QDir::cleanPath(it.next());
  }

  // Not found
  return QString();
}
