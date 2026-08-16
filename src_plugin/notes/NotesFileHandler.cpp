#include "NotesFileHandler.h"
#include "ObsidianParser.h"
#include "NotesIndex.h"
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
#include <QSaveFile>
#include <QTextStream>
#include <QUrl>
#include <QtConcurrent>

NotesFileHandler::NotesFileHandler(QObject *parent) : QObject(parent) {}

QString NotesFileHandler::createNote(const QString &folderPath,
                                     const QString &title,
                                     const QString &content,
                                     const QString &color) {
  QString normalizedFolder = normalizePath(folderPath);
  QString safeTitle = sanitizeFileName(title);
  QString filePath = normalizedFolder + QStringLiteral("/") + safeTitle +
                     QStringLiteral(".md");

  // Build content with namespaced frontmatter
  QVariantMap fields;
  fields[QStringLiteral("edgegesture-color")] = color.isEmpty() ? QStringLiteral("#624a73") : color;
  QString fileContent = ObsidianParser::mergeFrontmatter(QString(), fields, content);

  QSaveFile saveFile(filePath);
  if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "NotesFileHandler: Failed to create note:" << filePath;
    return QString();
  }

  QTextStream out(&saveFile);
  out << fileContent;
  if (!saveFile.commit()) {
    qWarning() << "NotesFileHandler: Failed to commit new note:" << filePath;
    return QString();
  }

  return filePath;
}

bool NotesFileHandler::saveNote(const QString &filePath, const QString &content,
                                const QString &color) {
  QString normalizedPath = normalizePath(filePath);

  QString originalContent;
  QFile readFile(normalizedPath);
  if (readFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream in(&readFile);
    originalContent = in.readAll();
    readFile.close();
  }

  // Preserve ALL existing frontmatter properties, only updating color
  QVariantMap fields;
  if (!color.isEmpty()) {
    fields[QStringLiteral("edgegesture-color")] = color;
  }
  QString fileContent = ObsidianParser::mergeFrontmatter(originalContent, fields, content);

  QSaveFile saveFile(normalizedPath);
  if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "NotesFileHandler: Failed to open save file for note:" << normalizedPath;
    return false;
  }

  QTextStream out(&saveFile);
  out << fileContent;
  return saveFile.commit();
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

  FrontmatterData fm = ObsidianParser::splitFrontmatter(content);
  result[QStringLiteral("color")] = fm.color;
  result[QStringLiteral("content")] = fm.rawBody;

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
    newPath = parentDir + QStringLiteral("/") + safeName + QStringLiteral(".md");

    // Read old content
    QFile oldFile(normalizedOldPath);
    if (!oldFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      return QString();
    }
    QString content = QTextStream(&oldFile).readAll();
    oldFile.close();

    // Atomic write to new file
    QSaveFile newFile(newPath);
    if (!newFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
      return QString();
    }
    QTextStream out(&newFile);
    out << content;
    if (!newFile.commit()) {
      return QString();
    }

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
  result.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")),
                 QStringLiteral("_"));
  return result;
}

QString NotesFileHandler::normalizePath(const QString &path) {
  QString result = path;
  if (result.startsWith(QLatin1String("file:///"))) {
    result = QUrl(result).toLocalFile();
  }
  return QDir::cleanPath(result);
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

  // Merge frontmatter non-destructively
  QVariantMap fields;
  fields[key] = value;
  QString updatedContent = ObsidianParser::mergeFrontmatter(content, fields);

  QSaveFile saveFile(normalizedPath);
  if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "NotesFileHandler: Failed to open save file for frontmatter update:"
               << normalizedPath;
    return false;
  }

  QTextStream out(&saveFile);
  out << updatedContent;
  return saveFile.commit();
}

QStringList NotesFileHandler::getTags(const QString &path) {
  QString normalizedPath = normalizePath(path);
  QFile file(normalizedPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QStringList();
  }

  // Read full content to avoid missing tags
  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  return ObsidianParser::splitFrontmatter(content).tags;
}

QString NotesFileHandler::saveClipboardImage(const QString &folderPath) {
  QClipboard *clipboard = QGuiApplication::clipboard();
  const QMimeData *mimeData = clipboard->mimeData();

  if (mimeData->hasImage()) {
    QImage image = qvariant_cast<QImage>(mimeData->imageData());
    if (!image.isNull()) {
      QString fileName =
          QStringLiteral("Pasted image ") +
          QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddHHmmss")) +
          QStringLiteral(".png");
      QString fullPath = folderPath + QStringLiteral("/") + fileName;

      QDir dir(folderPath);
      if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
      }

      if (image.save(fullPath, "PNG")) {
        return fileName;
      }
    }
  }
  return QString();
}

QString NotesFileHandler::findImage(const QString &imageName,
                                    const QString &notePath,
                                    const QString &rootPath) {
  // 1. Check memory cache
  if (m_imageCache.contains(imageName)) {
    QString cachedPath = m_imageCache[imageName];
    if (QFile::exists(cachedPath)) {
      return cachedPath;
    }
    m_imageCache.remove(imageName);
  }

  // 2. Check NotesIndex attachment index if available
  QString fromIndex = NotesIndex::instance()->findAttachment(imageName, notePath);
  if (!fromIndex.isEmpty() && QFile::exists(fromIndex)) {
    m_imageCache.insert(imageName, fromIndex);
    return fromIndex;
  }

  QString normalizedRoot = normalizePath(rootPath);
  QString normalizedNotePath = normalizePath(notePath);

  QFileInfo noteInfo(normalizedNotePath);
  QString noteFolder = noteInfo.absolutePath();

  // Search common locations
  QStringList searchPaths;
  searchPaths << noteFolder + QStringLiteral("/") + imageName;
  searchPaths << noteFolder + QStringLiteral("/attachments/") + imageName;
  searchPaths << normalizedRoot + QStringLiteral("/attachments/") + imageName;
  searchPaths << noteFolder + QStringLiteral("/images/") + imageName;
  searchPaths << normalizedRoot + QStringLiteral("/images/") + imageName;
  searchPaths << noteFolder + QStringLiteral("/assets/") + imageName;
  searchPaths << normalizedRoot + QStringLiteral("/assets/") + imageName;
  searchPaths << normalizedRoot + QStringLiteral("/") + imageName;

  for (const QString &path : std::as_const(searchPaths)) {
    if (QFile::exists(path)) {
      QString foundPath = QDir::cleanPath(path);
      m_imageCache.insert(imageName, foundPath);
      return foundPath;
    }
  }

  // Fallback recursive search
  QDirIterator it(normalizedRoot, QStringList() << imageName, QDir::Files,
                  QDirIterator::Subdirectories);
  if (it.hasNext()) {
    QString foundPath = QDir::cleanPath(it.next());
    m_imageCache.insert(imageName, foundPath);
    return foundPath;
  }

  return QString();
}

void NotesFileHandler::findImageAsync(const QString &imageName,
                                      const QString &notePath,
                                      const QString &rootPath) {
  QString imgName = imageName;
  QString noteP = notePath;
  QString rootP = rootPath;

  QtConcurrent::run([this, imgName, noteP, rootP]() {
    QString resultPath = this->findImage(imgName, noteP, rootP);
    emit this->imagePathFound(imgName, resultPath);
  });
}

QString NotesFileHandler::extractSection(const QString &notePath,
                                         const QString &sectionName) {
  QString normalizedPath = normalizePath(notePath);
  QFile file(normalizedPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString();
  }

  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  FrontmatterData fm = ObsidianParser::splitFrontmatter(content);
  QStringList lines = fm.rawBody.split(QLatin1Char('\n'));

  int startLine = -1;
  int startLevel = 0;
  QString sectionContent;
  QString searchTerm = sectionName.trimmed();

  for (int i = 0; i < lines.size(); ++i) {
    QString line = lines[i];
    static const QRegularExpression headingRegex(QStringLiteral("^(#{1,6})\\s+(.*)$"));
    QRegularExpressionMatch match = headingRegex.match(line);

    if (match.hasMatch()) {
      int level = match.captured(1).length();
      QString headingText = match.captured(2).trimmed();

      if (startLine < 0) {
        if (headingText.compare(searchTerm, Qt::CaseInsensitive) == 0) {
          startLine = i + 1;
          startLevel = level;
        }
      } else {
        if (level <= startLevel) {
          break;
        }
        sectionContent += line + QLatin1Char('\n');
      }
    } else if (startLine >= 0) {
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
    return QString();
  }

  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  FrontmatterData fm = ObsidianParser::splitFrontmatter(content);
  QString searchPattern = QStringLiteral("\\^") +
                          QRegularExpression::escape(blockId) +
                          QStringLiteral("\\s*$");
  QRegularExpression blockRegex(searchPattern,
                                QRegularExpression::MultilineOption);

  QStringList lines = fm.rawBody.split(QLatin1Char('\n'));
  for (int i = 0; i < lines.size(); ++i) {
    QString line = lines[i];
    if (blockRegex.match(line).hasMatch()) {
      QString result = line;
      result.replace(QRegularExpression(QStringLiteral("\\s*\\^") +
                                        QRegularExpression::escape(blockId) +
                                        QStringLiteral("\\s*$")),
                     QString());
      return result.trimmed();
    }
  }

  return QString();
}
