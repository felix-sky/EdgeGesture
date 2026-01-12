#include "NotesIndex.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>
#include <QtConcurrent>

NotesIndex *NotesIndex::s_instance = nullptr;

NotesIndex *NotesIndex::instance() {
  if (!s_instance) {
    s_instance = new NotesIndex();
  }
  return s_instance;
}

NotesIndex *NotesIndex::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine) {
  Q_UNUSED(qmlEngine);
  Q_UNUSED(jsEngine);
  return instance();
}

NotesIndex::NotesIndex(QObject *parent)
    : QObject(parent), m_fsWatcher(new QFileSystemWatcher(this)),
      m_watcher(new QFutureWatcher<QVector<NoteMetadata>>(this)) {
  qCritical() << "NotesIndex::NotesIndex (Constructor) - Instance created:"
              << this;

  connect(m_watcher, &QFutureWatcher<QVector<NoteMetadata>>::finished, this,
          &NotesIndex::onScanFinished);
  connect(m_fsWatcher, &QFileSystemWatcher::directoryChanged, this,
          &NotesIndex::onDirectoryChanged);
}

NotesIndex::~NotesIndex() {
  if (m_watcher->isRunning()) {
    m_watcher->cancel();
    m_watcher->waitForFinished();
  }
}

void NotesIndex::setRootPath(const QString &path) {
  qDebug() << "NotesIndex::setRootPath called with:" << path;
  QString normalizedPath = path;
  if (normalizedPath.startsWith(QLatin1String("file:///"))) {
    normalizedPath = QUrl(normalizedPath).toLocalFile();
  }
  qDebug() << "NotesIndex::setRootPath normalized to:" << normalizedPath;
  qDebug() << "NotesIndex::setRootPath current m_rootPath:" << m_rootPath;

  if (m_rootPath != normalizedPath) {
    m_rootPath = normalizedPath;
    qDebug() << "NotesIndex::setRootPath path changed, calling rebuildIndex";
    rebuildIndex();
  } else {
    qDebug() << "NotesIndex::setRootPath path unchanged, skipping rebuild";
  }
}

void NotesIndex::rebuildIndex() {
  qDebug() << "NotesIndex::rebuildIndex called, m_rootPath:" << m_rootPath;
  if (m_rootPath.isEmpty()) {
    qDebug() << "NotesIndex::rebuildIndex - rootPath is empty, returning";
    return;
  }

  if (m_watcher->isRunning()) {
    qDebug() << "NotesIndex::rebuildIndex - canceling previous scan";
    m_watcher->cancel();
    m_watcher->waitForFinished();
  }

  m_indexing = true;
  qDebug() << "NotesIndex: Rebuilding index for path:" << m_rootPath;
  emit indexingChanged();

  // Count files first for progress
  QString rootPath = m_rootPath;
  qDebug() << "NotesIndex::rebuildIndex - starting async scan for:" << rootPath;
  m_watcher->setFuture(QtConcurrent::run([rootPath]() {
    qDebug() << "NotesIndex: Async scan starting in thread for:" << rootPath;
    QVector<NoteMetadata> results;
    QDirIterator it(rootPath, {"*.md"}, QDir::Files,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
      QString filePath = QDir::cleanPath(it.next()); // Normalize path
      NoteMetadata meta = parseFileHeader(filePath);
      meta.filePath = filePath; // Ensure meta has normalized path
      results.append(meta);
    }

    // Also add folders
    QDirIterator dirIt(rootPath, QDir::Dirs | QDir::NoDotAndDotDot,
                       QDirIterator::Subdirectories);
    while (dirIt.hasNext()) {
      QString dirPath = QDir::cleanPath(dirIt.next()); // Normalize path
      NoteMetadata meta;
      meta.filePath = dirPath;
      meta.title = QFileInfo(dirPath).fileName();
      meta.isFolder = true;
      meta.lastModified = QFileInfo(dirPath).lastModified();
      meta.color = QStringLiteral("#FFB900");
      results.append(meta);
    }

    qDebug() << "NotesIndex: Async scan completed, found" << results.size()
             << "items";
    return results;
  }));
}

void NotesIndex::onScanFinished() {
  qDebug() << "NotesIndex::onScanFinished called";
  QVector<NoteMetadata> results = m_watcher->result();
  qDebug() << "NotesIndex::onScanFinished - processing" << results.size()
           << "results";
  processIndexResults(results);

  m_indexing = false;
  emit indexingChanged();
  emit indexReady();
  qDebug() << "NotesIndex::onScanFinished - index ready, titleToPath size:"
           << m_titleToPath.size();
}

void NotesIndex::processIndexResults(const QVector<NoteMetadata> &results) {
  // Clear existing indices
  m_index.clear();
  m_tagIndex.clear();
  m_backlinks.clear();
  m_titleToPath.clear();

  m_totalFiles = results.size();
  emit totalFilesChanged();

  int progress = 0;
  for (const NoteMetadata &meta : results) {
    m_index.insert(meta.filePath, meta);
    m_titleToPath.insert(meta.title, meta.filePath);

    // Build tag index
    for (const QString &tag : meta.tags) {
      m_tagIndex.insert(tag.toLower(), meta.filePath);
    }

    progress++;
    if (progress % 100 == 0) {
      m_indexProgress = progress;
      emit indexProgressChanged();
    }
  }

  m_indexProgress = m_totalFiles;
  emit indexProgressChanged();

  // Now scan for WikiLinks and build backlinks
  // This is done separately since we need all titles indexed first
  for (const NoteMetadata &meta : results) {
    if (meta.isFolder)
      continue;

    // Re-read header to get WikiLinks
    QFile file(meta.filePath);
    if (file.open(QIODevice::ReadOnly)) {
      char buffer[2048];
      qint64 bytesRead = file.read(buffer, sizeof(buffer));
      QString content = QString::fromUtf8(buffer, static_cast<int>(bytesRead));
      file.close();

      QStringList links = parseWikiLinks(content);
      for (const QString &linkedTitle : links) {
        m_backlinks[linkedTitle].append(meta.filePath);
      }
    }
  }

  // Watch root directory
  watchDirectory(m_rootPath);

  emit indexUpdated();
}

void NotesIndex::watchDirectory(const QString &path) {
  if (!m_fsWatcher->directories().contains(path)) {
    m_fsWatcher->addPath(path);
  }

  // Watch subdirectories
  QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot);
  while (it.hasNext()) {
    QString subDir = it.next();
    if (!m_fsWatcher->directories().contains(subDir)) {
      m_fsWatcher->addPath(subDir);
    }
  }
}

void NotesIndex::onDirectoryChanged(const QString &path) {
  Q_UNUSED(path);
  // Don't do a full rebuild on every change - it causes race conditions
  // with updateEntry() calls. The filesystem watcher fires when files are
  // saved, but updateEntry() is already called explicitly after saves.
  // Only emit a signal so views can refresh if needed.
  emit indexUpdated();
}

void NotesIndex::updateEntry(const QString &path) {
  // If called on a non-singleton instance (QML may create extra instances),
  // delegate to the actual singleton
  if (this != s_instance && s_instance != nullptr) {
    s_instance->updateEntry(path);
    return;
  }

  QString normalizedPath = path;
  if (normalizedPath.startsWith(QLatin1String("file:///"))) {
    normalizedPath = QUrl(normalizedPath).toLocalFile();
  }
  // Ensure path consistency (separators and absolute path)
  normalizedPath =
      QDir::cleanPath(QFileInfo(normalizedPath).absoluteFilePath());

  if (QFileInfo::exists(normalizedPath)) {
    NoteMetadata meta = parseFileHeader(normalizedPath);
    // Ensure the metadata also stores the normalized path
    meta.filePath = normalizedPath;

    // Remove old tag entries
    if (m_index.contains(normalizedPath)) {
      const NoteMetadata &oldMeta = m_index[normalizedPath];
      for (const QString &tag : oldMeta.tags) {
        m_tagIndex.remove(tag.toLower(), normalizedPath);
      }
      m_titleToPath.remove(oldMeta.title);
    }

    // Update index
    m_index.insert(normalizedPath, meta);
    m_titleToPath.insert(meta.title, normalizedPath);

    // Update tag index
    for (const QString &tag : meta.tags) {
      m_tagIndex.insert(tag.toLower(), normalizedPath);
    }

    emit entryUpdated(normalizedPath);
    emit indexUpdated();
  }
}

void NotesIndex::removeEntry(const QString &path) {
  QString normalizedPath = path;
  if (normalizedPath.startsWith(QLatin1String("file:///"))) {
    normalizedPath = QUrl(normalizedPath).toLocalFile();
  }
  normalizedPath =
      QDir::cleanPath(QFileInfo(normalizedPath).absoluteFilePath());

  if (m_index.contains(normalizedPath)) {
    const NoteMetadata &meta = m_index[normalizedPath];

    // Remove from tag index
    for (const QString &tag : meta.tags) {
      m_tagIndex.remove(tag.toLower(), normalizedPath);
    }

    // Remove from title map
    m_titleToPath.remove(meta.title);

    // Remove from backlinks (as source)
    for (auto it = m_backlinks.begin(); it != m_backlinks.end(); ++it) {
      it.value().removeAll(normalizedPath);
    }

    m_index.remove(normalizedPath);
    emit indexUpdated();
  }
}

void NotesIndex::clear() {
  m_index.clear();
  m_tagIndex.clear();
  m_backlinks.clear();
  m_titleToPath.clear();
  m_indexProgress = 0;
  m_totalFiles = 0;
  emit indexProgressChanged();
  emit totalFilesChanged();
  emit indexUpdated();
}

bool NotesIndex::isIndexing() const { return m_indexing; }

int NotesIndex::indexProgress() const { return m_indexProgress; }

int NotesIndex::totalFiles() const { return m_totalFiles; }

NoteMetadata NotesIndex::getMetadata(const QString &path) const {
  QString normalizedPath = path;
  if (normalizedPath.startsWith(QLatin1String("file:///"))) {
    normalizedPath = QUrl(normalizedPath).toLocalFile();
  }
  normalizedPath =
      QDir::cleanPath(QFileInfo(normalizedPath).absoluteFilePath());
  return m_index.value(normalizedPath);
}

QVector<NoteMetadata>
NotesIndex::getItemsInFolder(const QString &folderPath) const {
  QString normalizedPath = folderPath;
  if (normalizedPath.startsWith(QLatin1String("file:///"))) {
    normalizedPath = QUrl(normalizedPath).toLocalFile();
  }
  normalizedPath =
      QDir::cleanPath(QFileInfo(normalizedPath).absoluteFilePath());

  QVector<NoteMetadata> items;
  QDir dir(normalizedPath);

  if (!dir.exists())
    return items;

  // Get direct children only
  dir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
  const QFileInfoList entries = dir.entryInfoList();

  for (const QFileInfo &info : entries) {
    QString itemPath = QDir::cleanPath(info.absoluteFilePath());

    if (info.isDir()) {
      // Create folder metadata on the fly if not indexed
      NoteMetadata meta;
      meta.filePath = itemPath;
      meta.title = info.fileName();
      meta.isFolder = true;
      meta.lastModified = info.lastModified();
      meta.color = QStringLiteral("#FFB900");
      items.append(meta);
    } else if (info.suffix().compare(QLatin1String("md"),
                                     Qt::CaseInsensitive) == 0) {
      if (m_index.contains(itemPath)) {
        items.append(m_index.value(itemPath));
      } else {
        // Parse on demand if not yet indexed
        items.append(parseFileHeader(itemPath));
      }
    }
  }

  // Sort: pinned first, then folders, then by date
  std::sort(items.begin(), items.end(),
            [](const NoteMetadata &a, const NoteMetadata &b) {
              if (a.isPinned != b.isPinned)
                return a.isPinned > b.isPinned;
              if (a.isFolder != b.isFolder)
                return a.isFolder > b.isFolder;
              return a.lastModified > b.lastModified;
            });

  return items;
}

QVector<NoteMetadata> NotesIndex::getNotesByTag(const QString &tag) const {
  QVector<NoteMetadata> results;
  QString lowerTag = tag.toLower();

  const QList<QString> paths = m_tagIndex.values(lowerTag);
  for (const QString &path : paths) {
    if (m_index.contains(path)) {
      results.append(m_index.value(path));
    }
  }

  return results;
}

QVector<NoteMetadata> NotesIndex::searchByTitle(const QString &query) const {
  QVector<NoteMetadata> results;
  QString lowerQuery = query.toLower();

  for (auto it = m_index.constBegin(); it != m_index.constEnd(); ++it) {
    const NoteMetadata &meta = it.value();
    if (meta.title.toLower().contains(lowerQuery)) {
      results.append(meta);
    }
    // Also search in tags
    else {
      for (const QString &tag : meta.tags) {
        if (tag.toLower().contains(lowerQuery)) {
          results.append(meta);
          break;
        }
      }
    }
  }

  return results;
}

QStringList NotesIndex::getBacklinks(const QString &title) const {
  return m_backlinks.value(title);
}

QStringList NotesIndex::getAllTags() const {
  QStringList tags = m_tagIndex.uniqueKeys();
  tags.sort(Qt::CaseInsensitive);
  return tags;
}

QString NotesIndex::findPathByTitle(const QString &title) const {
  // If called on a non-singleton instance (QML may create extra instances),
  // delegate to the actual singleton
  if (this != s_instance && s_instance != nullptr) {
    qDebug() << "NotesIndex::findPathByTitle - delegating to singleton";
    return s_instance->findPathByTitle(title);
  }

  qDebug() << "NotesIndex::findPathByTitle looking for:" << title;

  if (m_titleToPath.contains(title)) {
    QString result = m_titleToPath.value(title);
    qDebug() << "NotesIndex::findPathByTitle Found:" << result;
    return result;
  }

  qDebug() << "NotesIndex::findPathByTitle - NOT FOUND in index of size:"
           << m_titleToPath.size();
  // Debug: print all titles in index if small enough or just summary?
  // Let's print top 5 to see if index is populated at all
  int count = 0;
  for (auto it = m_titleToPath.constBegin(); it != m_titleToPath.constEnd();
       ++it) {
    if (count++ < 5) {
      qDebug() << "   Index sample:" << it.key();
    } else {
      break;
    }
  }

  return QString();
}

// Static parsing methods

NoteMetadata NotesIndex::parseFileHeader(const QString &path) {
  NoteMetadata meta;
  meta.filePath = path;
  meta.title = QFileInfo(path).completeBaseName();
  meta.lastModified = QFileInfo(path).lastModified();
  meta.isFolder = false;
  meta.color = QStringLiteral("#624a73"); // Default

  QFile file(path);
  if (file.open(QIODevice::ReadOnly)) {
    // Read only first 1024 bytes for efficiency
    char buffer[1024];
    qint64 bytesRead = file.read(buffer, sizeof(buffer));
    QString header = QString::fromUtf8(buffer, static_cast<int>(bytesRead));
    file.close();

    // Parse frontmatter
    if (header.startsWith(QLatin1String("---"))) {
      int endIdx = header.indexOf(QLatin1String("---"), 3);
      if (endIdx > 0) {
        QString fm = header.mid(3, endIdx - 3);
        meta.color = parseColor(fm);
        meta.tags = parseTags(fm);
        meta.isPinned = parsePinned(fm);
      }
    }
  }

  return meta;
}

QString NotesIndex::parseColor(const QString &frontmatter) {
  QRegularExpression colorRegex(QStringLiteral("color:\\s*(#[a-fA-F0-9]+)"));
  QRegularExpressionMatch match = colorRegex.match(frontmatter);
  if (match.hasMatch()) {
    return match.captured(1);
  }
  return QStringLiteral("#624a73");
}

QStringList NotesIndex::parseTags(const QString &frontmatter) {
  QStringList tags;

  // 1. Try JSON-style array: tags: [tag1, tag2]
  QRegularExpression jsonTagsRegex(QStringLiteral("tags:\\s*\\[([^\\]]+)\\]"));
  QRegularExpressionMatch jsonMatch = jsonTagsRegex.match(frontmatter);

  if (jsonMatch.hasMatch()) {
    QString tagsStr = jsonMatch.captured(1);
    const QStringList rawTags = tagsStr.split(QLatin1Char(','));
    for (const QString &tag : rawTags) {
      QString cleaned = tag.trimmed();
      cleaned.remove(QLatin1Char('"'));
      cleaned.remove(QLatin1Char('\''));
      if (!cleaned.isEmpty()) {
        tags.append(cleaned);
      }
    }
    return tags;
  }

  // 2. Try YAML list style:
  // tags:
  //   - tag1
  //   - tag2
  // Or simple single line: tags: tag1

  // Find "tags:" line
  QRegularExpression tagsLineRegex(QStringLiteral("^tags:\\s*(.*)$"),
                                   QRegularExpression::MultilineOption);
  QRegularExpressionMatch lineMatch = tagsLineRegex.match(frontmatter);

  if (lineMatch.hasMatch()) {
    QString lineValue = lineMatch.captured(1).trimmed();

    // If there is value on the same line and it's not a list start
    if (!lineValue.isEmpty()) {
      // Single tag: tags: mytag
      // Or comma separated without brackets (less common but possible): tags:
      // tag1, tag2 ? For now assume single value if not empty
      tags.append(lineValue);
    } else {
      // Look for subsequent lines starting with "- "
      // We need to find where "tags:" starts
      int startOffset = lineMatch.capturedEnd();

      // Split frontmatter into lines starting from tags:
      QString remaining = frontmatter.mid(startOffset);
      QTextStream stream(&remaining);
      QString line;
      while (stream.readLineInto(&line)) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1String("-"))) {
          QString tag = trimmed.mid(1).trimmed();
          tag.remove(QLatin1Char('"'));
          tag.remove(QLatin1Char('\''));
          if (!tag.isEmpty()) {
            tags.append(tag);
          }
        } else if (trimmed.contains(QLatin1String(":"))) {
          // Next key found, stop
          break;
        } else if (trimmed.isEmpty()) {
          continue;
        } else {
          // Indentation or continuation? If not starting with -, maybe stop
          // But YAML is indentation based. For simplicity, we stop if we hit
          // something that looks like a key or doesn't start with -
          break;
        }
      }
    }
  }

  return tags;
}

bool NotesIndex::parsePinned(const QString &frontmatter) {
  QRegularExpression pinnedRegex(QStringLiteral("pinned:\\s*(true|false)"),
                                 QRegularExpression::CaseInsensitiveOption);
  QRegularExpressionMatch match = pinnedRegex.match(frontmatter);
  if (match.hasMatch()) {
    return match.captured(1).toLower() == QLatin1String("true");
  }
  return false;
}

QStringList NotesIndex::parseWikiLinks(const QString &content) {
  QStringList links;
  QRegularExpression wikiRegex(QStringLiteral("\\[\\[([^\\]]+)\\]\\]"));
  QRegularExpressionMatchIterator it = wikiRegex.globalMatch(content);

  while (it.hasNext()) {
    QRegularExpressionMatch match = it.next();
    QString link = match.captured(1).trimmed();
    if (!link.isEmpty() && !links.contains(link)) {
      links.append(link);
    }
  }

  return links;
}
