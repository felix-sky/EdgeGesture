#include "NotesFileHandler.h"
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>

NotesFileHandler::NotesFileHandler(QObject *parent) : QObject(parent) {
  // Constructor
}

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

QString NotesFileHandler::saveClipboardImage(const QString &folderPath) {
  QClipboard *clipboard = QGuiApplication::clipboard();
  const QMimeData *mimeData = clipboard->mimeData();

  if (mimeData->hasImage()) {
    QImage image = qvariant_cast<QImage>(mimeData->imageData());
    if (!image.isNull()) {
      QString fileName =
          "Pasted image " +
          QDateTime::currentDateTime().toString("yyyyMMddHHmmss") + ".png";
      QString fullPath = folderPath + "/" + fileName;

      // Ensure folder exists
      QDir dir(folderPath);
      if (!dir.exists()) {
        dir.mkpath(".");
      }

      if (image.save(fullPath, "PNG")) {
        return fileName;
      }
    }
  }
  return "";
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

  // FALLBACK: Try without "Pasted image " prefix (common Obsidian naming issue)
  if (imageName.startsWith(QStringLiteral("Pasted image "))) {
    QString altName = imageName.mid(13); // Remove "Pasted image " (13 chars)

    // Try direct path first
    QString altPath = noteFolder + QStringLiteral("/") + altName;
    if (QFile::exists(altPath)) {
      return QDir::cleanPath(altPath);
    }

    // Recursive search with alternate name
    QDirIterator altIt(normalizedRoot, QStringList() << altName, QDir::Files,
                       QDirIterator::Subdirectories);
    if (altIt.hasNext()) {
      return QDir::cleanPath(altIt.next());
    }
  }

  // Not found
  return QString();
}

QString NotesFileHandler::extractSection(const QString &notePath,
                                         const QString &sectionName) {
  QString normalizedPath = normalizePath(notePath);

  QFile file(normalizedPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning()
        << "NotesFileHandler: Failed to read file for section extraction:"
        << normalizedPath;
    return QString();
  }

  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  // Skip frontmatter if present
  if (content.startsWith(QLatin1String("---"))) {
    int endIndex = content.indexOf(QLatin1String("---"), 3);
    if (endIndex > 0) {
      content = content.mid(endIndex + 4);
    }
  }

  // Parse the markdown to find headings
  // We use a simple line-by-line approach for efficiency
  QStringList lines = content.split(QLatin1Char('\n'));

  int startLine = -1;
  int startLevel = 0;
  QString sectionContent;

  // Normalize search term
  QString searchTerm = sectionName.trimmed();

  for (int i = 0; i < lines.size(); ++i) {
    QString line = lines[i];

    // Check if this line is a heading (# Title, ## Title, etc.)
    QRegularExpression headingRegex(QStringLiteral("^(#{1,6})\\s+(.*)$"));
    QRegularExpressionMatch match = headingRegex.match(line);

    if (match.hasMatch()) {
      int level = match.captured(1).length();
      QString headingText = match.captured(2).trimmed();

      if (startLine < 0) {
        // Looking for the target heading
        if (headingText.compare(searchTerm, Qt::CaseInsensitive) == 0) {
          startLine = i + 1; // Start collecting from next line
          startLevel = level;
        }
      } else {
        // Already found target, check if this heading ends the section
        if (level <= startLevel) {
          // This heading is at same or higher level - stop here
          break;
        }
        // Otherwise include this heading in the section content
        sectionContent += line + QLatin1Char('\n');
      }
    } else if (startLine >= 0) {
      // We're collecting content after the target heading
      sectionContent += line + QLatin1Char('\n');
    }
  }

  return sectionContent.trimmed();
}

QString NotesFileHandler::extractBlock(const QString &notePath,
                                       const QString &blockId) {
  QString normalizedPath = normalizePath(notePath);

  QFile file(normalizedPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "NotesFileHandler: Failed to read file for block extraction:"
               << normalizedPath;
    return QString();
  }

  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  // Skip frontmatter if present
  if (content.startsWith(QLatin1String("---"))) {
    int endIndex = content.indexOf(QLatin1String("---"), 3);
    if (endIndex > 0) {
      content = content.mid(endIndex + 4);
    }
  }

  // Search for line/paragraph ending with ^blockId
  // Pattern: any content followed by ^blockId at end of line
  QString searchPattern = QStringLiteral("\\^") +
                          QRegularExpression::escape(blockId) +
                          QStringLiteral("\\s*$");
  QRegularExpression blockRegex(searchPattern,
                                QRegularExpression::MultilineOption);

  QStringList lines = content.split(QLatin1Char('\n'));

  for (int i = 0; i < lines.size(); ++i) {
    QString line = lines[i];

    if (blockRegex.match(line).hasMatch()) {
      // Found the block - return the content without the ^blockId marker
      QString result = line;
      result.replace(QRegularExpression(QStringLiteral("\\s*\\^") +
                                        QRegularExpression::escape(blockId) +
                                        QStringLiteral("\\s*$")),
                     QString());
      return result.trimmed();
    }
  }

  // Block not found
  return QString();
}
