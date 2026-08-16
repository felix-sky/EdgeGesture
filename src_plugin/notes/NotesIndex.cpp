#include "NotesIndex.h"
#include "ObsidianParser.h"
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>
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
      m_watcher(new QFutureWatcher<ScanResult>(this)) {
  connect(m_watcher, &QFutureWatcher<ScanResult>::finished, this,
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
  QString normalizedPath = path;
  if (normalizedPath.startsWith(QLatin1String("file:///"))) {
    normalizedPath = QUrl(normalizedPath).toLocalFile();
  }
  normalizedPath = QDir::cleanPath(normalizedPath);

  if (m_rootPath != normalizedPath) {
    m_rootPath = normalizedPath;
    rebuildIndex();
  }
}

void NotesIndex::rebuildIndex() {
  if (m_rootPath.isEmpty()) {
    return;
  }

  if (m_watcher->isRunning()) {
    m_watcher->cancel();
    m_watcher->waitForFinished();
  }

  m_indexing = true;
  emit indexingChanged();

  QString rootPath = m_rootPath;
  m_watcher->setFuture(QtConcurrent::run([rootPath]() {
    ScanResult result;

    // Scan all Markdown notes
    QDirIterator it(rootPath, {QStringLiteral("*.md")}, QDir::Files,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
      QString filePath = QDir::cleanPath(it.next());
      QStringList links;
      NoteMetadata meta = parseNoteFile(filePath, &links);
      result.notes.append(meta);

      // Register backlinks
      for (const QString &target : links) {
        result.backlinks[target.toLower()].append(filePath);
      }
    }

    // Scan all folder items
    QDirIterator dirIt(rootPath, QDir::Dirs | QDir::NoDotAndDotDot,
                       QDirIterator::Subdirectories);
    while (dirIt.hasNext()) {
      QString dirPath = QDir::cleanPath(dirIt.next());
      NoteMetadata meta;
      meta.filePath = dirPath;
      meta.title = QFileInfo(dirPath).fileName();
      meta.isFolder = true;
      meta.lastModified = QFileInfo(dirPath).lastModified();
      meta.color = QStringLiteral("#FFB900");
      result.notes.append(meta);
    }

    // Scan attachment files (.png, .jpg, .svg, .webp, .pdf, etc.)
    static const QStringList attachmentFilters = {
        QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
        QStringLiteral("*.gif"), QStringLiteral("*.bmp"), QStringLiteral("*.svg"),
        QStringLiteral("*.webp"), QStringLiteral("*.ico"), QStringLiteral("*.tiff"),
        QStringLiteral("*.pdf")};
    QDirIterator attachIt(rootPath, attachmentFilters, QDir::Files,
                          QDirIterator::Subdirectories);
    while (attachIt.hasNext()) {
      QString attachPath = QDir::cleanPath(attachIt.next());
      QString filename = QFileInfo(attachPath).fileName().toLower();
      result.attachments.insert(filename, attachPath);
    }

    return result;
  }));
}

void NotesIndex::onScanFinished() {
  ScanResult result = m_watcher->result();
  processScanResults(result);

  m_indexing = false;
  emit indexingChanged();
  emit indexReady();
  emit indexUpdated();
}

void NotesIndex::processScanResults(const ScanResult &result) {
  m_pathIndex.clear();
  m_titleIndex.clear();
  m_aliasIndex.clear();
  m_tagIndex.clear();
  m_attachmentIndex = result.attachments;
  m_backlinks = result.backlinks;

  m_totalFiles = result.notes.size();
  emit totalFilesChanged();

  int progress = 0;
  for (const NoteMetadata &meta : result.notes) {
    m_pathIndex.insert(meta.filePath, meta);

    if (!meta.isFolder) {
      m_titleIndex.insert(meta.title.toLower(), meta.filePath);

      for (const QString &alias : meta.aliases) {
        m_aliasIndex.insert(alias.toLower(), meta.filePath);
      }

      for (const QString &tag : meta.tags) {
        m_tagIndex.insert(tag.toLower(), meta.filePath);
      }
    }

    progress++;
    if (progress % 100 == 0) {
      m_indexProgress = progress;
      emit indexProgressChanged();
    }
  }

  m_indexProgress = m_totalFiles;
  emit indexProgressChanged();

  watchDirectoryRecursively(m_rootPath);
}

void NotesIndex::watchDirectoryRecursively(const QString &path) {
  if (path.isEmpty() || !QDir(path).exists())
    return;

  if (!m_fsWatcher->directories().contains(path)) {
    m_fsWatcher->addPath(path);
  }

  QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    QString subDir = it.next();
    if (!m_fsWatcher->directories().contains(subDir)) {
      m_fsWatcher->addPath(subDir);
    }
  }
}

void NotesIndex::onDirectoryChanged(const QString &path) {
  Q_UNUSED(path);
  // Perform controlled incremental scan or emit update
  emit indexUpdated();
}

void NotesIndex::updateEntry(const QString &path) {
  if (this != s_instance && s_instance != nullptr) {
    s_instance->updateEntry(path);
    return;
  }

  QString normalizedPath = path;
  if (normalizedPath.startsWith(QLatin1String("file:///"))) {
    normalizedPath = QUrl(normalizedPath).toLocalFile();
  }
  normalizedPath = QDir::cleanPath(QFileInfo(normalizedPath).absoluteFilePath());

  if (QFileInfo::exists(normalizedPath)) {
    // Remove old mappings
    removeEntry(normalizedPath);

    // Re-parse
    QStringList links;
    NoteMetadata meta = parseNoteFile(normalizedPath, &links);

    m_pathIndex.insert(normalizedPath, meta);
    if (!meta.isFolder) {
      m_titleIndex.insert(meta.title.toLower(), normalizedPath);

      for (const QString &alias : meta.aliases) {
        m_aliasIndex.insert(alias.toLower(), normalizedPath);
      }

      for (const QString &tag : meta.tags) {
        m_tagIndex.insert(tag.toLower(), normalizedPath);
      }

      for (const QString &target : links) {
        m_backlinks[target.toLower()].append(normalizedPath);
      }
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
  normalizedPath = QDir::cleanPath(QFileInfo(normalizedPath).absoluteFilePath());

  if (m_pathIndex.contains(normalizedPath)) {
    const NoteMetadata &oldMeta = m_pathIndex[normalizedPath];

    m_titleIndex.remove(oldMeta.title.toLower(), normalizedPath);

    for (const QString &alias : oldMeta.aliases) {
      m_aliasIndex.remove(alias.toLower(), normalizedPath);
    }

    for (const QString &tag : oldMeta.tags) {
      m_tagIndex.remove(tag.toLower(), normalizedPath);
    }

    for (auto it = m_backlinks.begin(); it != m_backlinks.end(); ++it) {
      it.value().removeAll(normalizedPath);
    }

    m_pathIndex.remove(normalizedPath);
    emit indexUpdated();
  }
}

void NotesIndex::clear() {
  m_pathIndex.clear();
  m_titleIndex.clear();
  m_aliasIndex.clear();
  m_tagIndex.clear();
  m_attachmentIndex.clear();
  m_backlinks.clear();
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
  normalizedPath = QDir::cleanPath(QFileInfo(normalizedPath).absoluteFilePath());
  return m_pathIndex.value(normalizedPath);
}

QVector<NoteMetadata> NotesIndex::getItemsInFolder(const QString &folderPath) const {
  QString normalizedPath = folderPath;
  if (normalizedPath.startsWith(QLatin1String("file:///"))) {
    normalizedPath = QUrl(normalizedPath).toLocalFile();
  }
  normalizedPath = QDir::cleanPath(QFileInfo(normalizedPath).absoluteFilePath());

  QVector<NoteMetadata> items;
  QDir dir(normalizedPath);
  if (!dir.exists())
    return items;

  dir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
  const QFileInfoList entries = dir.entryInfoList();

  for (const QFileInfo &info : entries) {
    QString itemPath = QDir::cleanPath(info.absoluteFilePath());

    if (info.isDir()) {
      NoteMetadata meta;
      meta.filePath = itemPath;
      meta.title = info.fileName();
      meta.isFolder = true;
      meta.lastModified = info.lastModified();
      meta.color = QStringLiteral("#FFB900");
      items.append(meta);
    } else if (info.suffix().compare(QLatin1String("md"), Qt::CaseInsensitive) == 0) {
      if (m_pathIndex.contains(itemPath)) {
        items.append(m_pathIndex.value(itemPath));
      } else {
        items.append(parseNoteFile(itemPath));
      }
    }
  }

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
  const QList<QString> paths = m_tagIndex.values(tag.toLower());
  for (const QString &path : paths) {
    if (m_pathIndex.contains(path)) {
      results.append(m_pathIndex.value(path));
    }
  }
  return results;
}

QVector<NoteMetadata> NotesIndex::searchByTitle(const QString &query) const {
  QVector<NoteMetadata> results;
  QString lowerQuery = query.toLower();

  for (auto it = m_pathIndex.constBegin(); it != m_pathIndex.constEnd(); ++it) {
    const NoteMetadata &meta = it.value();
    if (meta.title.toLower().contains(lowerQuery)) {
      results.append(meta);
    } else {
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
  return m_backlinks.value(title.toLower());
}

QStringList NotesIndex::getAllTags() const {
  QStringList tags = m_tagIndex.uniqueKeys();
  tags.sort(Qt::CaseInsensitive);
  return tags;
}

QString NotesIndex::findPathByTitle(const QString &title) const {
  LinkResolution res = resolveLink(title);
  if (res.kind == LinkResolutionKind::Found || res.kind == LinkResolutionKind::Ambiguous) {
    return res.matches.isEmpty() ? QString() : res.matches.first();
  }
  return QString();
}

QString NotesIndex::findAttachment(const QString &name, const QString &currentNotePath) const {
  QString lowerName = name.toLower();

  // 1. Direct O(1) index lookup
  if (m_attachmentIndex.contains(lowerName)) {
    return m_attachmentIndex.value(lowerName);
  }

  // 2. Relative to current note
  if (!currentNotePath.isEmpty()) {
    QFileInfo noteInfo(currentNotePath);
    QString noteFolder = noteInfo.absolutePath();
    QString direct = QDir::cleanPath(noteFolder + QStringLiteral("/") + name);
    if (QFile::exists(direct)) {
      return direct;
    }
    QString inAttachments = QDir::cleanPath(noteFolder + QStringLiteral("/attachments/") + name);
    if (QFile::exists(inAttachments)) {
      return inAttachments;
    }
  }

  return QString();
}

LinkResolution NotesIndex::resolveLink(const QString &linkText, const QString &currentNotePath) const {
  ObsidianLink parsed = ObsidianParser::parseLink(linkText);
  LinkResolution res;
  res.target = parsed.target;
  res.heading = parsed.heading;
  res.blockId = parsed.blockId;
  res.alias = parsed.alias;

  // 1. Check if target is empty (Self-reference like [[#Heading]] or [[#^block-id]])
  if (res.target.isEmpty()) {
    if (!currentNotePath.isEmpty()) {
      res.kind = LinkResolutionKind::Found;
      res.matches.append(currentNotePath);
    } else {
      res.kind = LinkResolutionKind::Missing;
    }
    return res;
  }

  // 2. Check if target is an exact existing file path
  QString currentFolder;
  if (!currentNotePath.isEmpty()) {
    currentFolder = QFileInfo(currentNotePath).absolutePath();
    QString relativeCandidate = QDir::cleanPath(currentFolder + QStringLiteral("/") + res.target);
    if (!relativeCandidate.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive)) {
      relativeCandidate += QStringLiteral(".md");
    }
    if (QFile::exists(relativeCandidate)) {
      res.kind = LinkResolutionKind::Found;
      res.matches.append(relativeCandidate);
      return res;
    }
  }

  if (!m_rootPath.isEmpty()) {
    QString vaultCandidate = QDir::cleanPath(m_rootPath + QStringLiteral("/") + res.target);
    if (!vaultCandidate.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive)) {
      vaultCandidate += QStringLiteral(".md");
    }
    if (QFile::exists(vaultCandidate)) {
      res.kind = LinkResolutionKind::Found;
      res.matches.append(vaultCandidate);
      return res;
    }
  }

  // 3. Match against Title Index
  QString cleanTitle = res.target;
  if (cleanTitle.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive)) {
    cleanTitle.chop(3);
  }
  QString lowerTitle = cleanTitle.toLower();

  QList<QString> titleMatches = m_titleIndex.values(lowerTitle);
  if (!titleMatches.isEmpty()) {
    if (titleMatches.size() == 1) {
      res.kind = LinkResolutionKind::Found;
      res.matches.append(titleMatches.first());
      return res;
    } else {
      // Ambiguous matches: if one match is in the same folder as currentNotePath, put it first
      if (!currentFolder.isEmpty()) {
        for (int i = 0; i < titleMatches.size(); ++i) {
          if (QFileInfo(titleMatches[i]).absolutePath() == currentFolder) {
            titleMatches.swapItemsAt(0, i);
            break;
          }
        }
      }
      res.kind = LinkResolutionKind::Ambiguous;
      res.matches = titleMatches;
      return res;
    }
  }

  // 4. Match against Alias Index
  QList<QString> aliasMatches = m_aliasIndex.values(lowerTitle);
  if (!aliasMatches.isEmpty()) {
    if (aliasMatches.size() == 1) {
      res.kind = LinkResolutionKind::Found;
      res.matches.append(aliasMatches.first());
      return res;
    } else {
      res.kind = LinkResolutionKind::Ambiguous;
      res.matches = aliasMatches;
      return res;
    }
  }

  // 5. Unresolved / Missing
  res.kind = LinkResolutionKind::Missing;
  return res;
}

QVariantMap NotesIndex::resolveLinkInfo(const QString &linkText, const QString &currentNotePath) const {
  LinkResolution res = resolveLink(linkText, currentNotePath);
  QVariantMap map;

  switch (res.kind) {
  case LinkResolutionKind::Found:
    map[QStringLiteral("kind")] = QStringLiteral("found");
    break;
  case LinkResolutionKind::Ambiguous:
    map[QStringLiteral("kind")] = QStringLiteral("ambiguous");
    break;
  case LinkResolutionKind::Missing:
  default:
    map[QStringLiteral("kind")] = QStringLiteral("missing");
    break;
  }

  map[QStringLiteral("target")] = res.target;
  map[QStringLiteral("heading")] = res.heading;
  map[QStringLiteral("blockId")] = res.blockId;
  map[QStringLiteral("alias")] = res.alias;
  map[QStringLiteral("matches")] = res.matches;
  map[QStringLiteral("bestMatch")] = res.matches.isEmpty() ? QString() : res.matches.first();

  return map;
}

NoteMetadata NotesIndex::parseNoteFile(const QString &path, QStringList *outLinks) {
  NoteMetadata meta;
  meta.filePath = path;
  meta.title = QFileInfo(path).completeBaseName();
  meta.lastModified = QFileInfo(path).lastModified();
  meta.isFolder = false;
  meta.color = QStringLiteral("#624a73");

  QFile file(path);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    FrontmatterData fm = ObsidianParser::splitFrontmatter(content);
    meta.color = fm.color;
    meta.isPinned = fm.isPinned;
    meta.tags = fm.tags;
    meta.aliases = fm.aliases;

    if (outLinks) {
      QVector<ObsidianLink> links = ObsidianParser::extractAllLinks(fm.rawBody);
      for (const ObsidianLink &link : links) {
        if (!link.target.isEmpty() && !outLinks->contains(link.target)) {
          outLinks->append(link.target);
        }
      }
    }
  }

  return meta;
}
