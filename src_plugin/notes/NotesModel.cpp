#include "NotesModel.h"
#include "NotesIndex.h"
#include <QDateTime>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <QtConcurrent>

NotesModel::NotesModel(QObject *parent)
    : QAbstractListModel(parent),
      m_watcher(new QFutureWatcher<QVector<NoteItem>>(this)),
      m_contentSearchWatcher(new QFutureWatcher<QVector<NoteItem>>(this)),
      m_fsWatcher(new QFileSystemWatcher(this)) {
  connect(m_watcher, &QFutureWatcher<QVector<NoteItem>>::finished, this,
          &NotesModel::onScanFinished);
  connect(m_contentSearchWatcher, &QFutureWatcher<QVector<NoteItem>>::finished,
          this, &NotesModel::onContentSearchFinished);
  connect(m_fsWatcher, &QFileSystemWatcher::directoryChanged, this,
          &NotesModel::onDirectoryChanged);

  // Connect to NotesIndex
  connect(NotesIndex::instance(), &NotesIndex::indexReady, this,
          &NotesModel::onIndexReady);
  connect(NotesIndex::instance(), &NotesIndex::indexUpdated, this,
          &NotesModel::onIndexReady);
  connect(NotesIndex::instance(), &NotesIndex::entryUpdated, this,
          &NotesModel::onEntryUpdated);
}

NotesModel::~NotesModel() {
  if (m_watcher->isRunning()) {
    m_watcher->cancel();
    m_watcher->waitForFinished();
  }
  if (m_contentSearchWatcher->isRunning()) {
    m_contentSearchWatcher->cancel();
    m_contentSearchWatcher->waitForFinished();
  }
}

int NotesModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return static_cast<int>(m_items.size());
}

QVariant NotesModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 ||
      index.row() >= static_cast<int>(m_items.size()))
    return QVariant();

  const NoteItem &item = m_items.at(index.row());

  switch (role) {
  case TypeRole:
    return item.type;
  case TitleRole:
    return item.title;
  case PathRole:
    return item.path;
  case DateRole:
    return item.date;
  case ColorRole:
    return item.color;
  case PreviewRole:
    // Lazy load preview if not yet loaded
    if (item.preview.isEmpty() && item.type == QLatin1String("note")) {
      // Note: In a real scenario, we might want to make this async
      // For now, we load it on demand
      const_cast<NoteItem &>(item).preview = createPreview(item.path);
    }
    return item.preview;
  case TagsRole:
    return item.tags;
  case IsPinnedRole:
    return item.isPinned;
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> NotesModel::roleNames() const {
  return {{TypeRole, "type"},   {TitleRole, "title"},
          {PathRole, "path"},   {DateRole, "date"},
          {ColorRole, "color"}, {PreviewRole, "preview"},
          {TagsRole, "tags"},   {IsPinnedRole, "isPinned"}};
}

QString NotesModel::currentPath() const { return m_currentPath; }

void NotesModel::setCurrentPath(const QString &path) {
  QString normalizedPath = path;
  if (normalizedPath.startsWith(QLatin1String("file:///"))) {
    normalizedPath = QUrl(normalizedPath).toLocalFile();
  }
  normalizedPath =
      QDir::cleanPath(QFileInfo(normalizedPath).absoluteFilePath());

  if (m_currentPath != normalizedPath) {
    // Remove old path from watcher
    if (!m_currentPath.isEmpty() &&
        m_fsWatcher->directories().contains(m_currentPath)) {
      m_fsWatcher->removePath(m_currentPath);
    }

    m_currentPath = normalizedPath;
    emit currentPathChanged();

    // Add new path to watcher
    if (!m_currentPath.isEmpty() && QDir(m_currentPath).exists()) {
      m_fsWatcher->addPath(m_currentPath);
    }

    // Clear search mode when changing paths
    if (m_isSearchMode) {
      m_isSearchMode = false;
      m_filterString.clear();
      m_filterTag.clear();
      emit isSearchModeChanged();
      emit filterStringChanged();
      emit filterTagChanged();
    }

    refresh();
  }
}

QString NotesModel::rootPath() const { return m_rootPath; }

void NotesModel::setRootPath(const QString &path) {
  QString normalizedPath = path;
  if (normalizedPath.startsWith(QLatin1String("file:///"))) {
    normalizedPath = QUrl(normalizedPath).toLocalFile();
  }
  normalizedPath =
      QDir::cleanPath(QFileInfo(normalizedPath).absoluteFilePath());

  if (m_rootPath != normalizedPath) {
    m_rootPath = normalizedPath;
    emit rootPathChanged();

    // Ensure root directory exists
    QDir dir(m_rootPath);
    if (!dir.exists()) {
      dir.mkpath(m_rootPath);
    }

    // Initialize the NotesIndex
    NotesIndex::instance()->setRootPath(m_rootPath);

    // Reset to root
    m_folderStack.clear();
    emit folderStackChanged();
    setCurrentPath(m_rootPath);
  }
}

bool NotesModel::loading() const { return m_loading; }

QStringList NotesModel::folderStack() const { return m_folderStack; }

QString NotesModel::filterString() const { return m_filterString; }

void NotesModel::setFilterString(const QString &filter) {
  if (m_filterString != filter) {
    m_filterString = filter;
    emit filterStringChanged();

    if (!filter.isEmpty()) {
      m_isSearchMode = true;
      emit isSearchModeChanged();

      // Search by title/tags using the index
      loadFromIndex();
    } else {
      clearFilters();
    }
  }
}

QString NotesModel::filterTag() const { return m_filterTag; }

void NotesModel::setFilterTag(const QString &tag) {
  if (m_filterTag != tag) {
    m_filterTag = tag;
    emit filterTagChanged();

    if (!tag.isEmpty()) {
      m_isSearchMode = true;
      emit isSearchModeChanged();
      loadFromIndex();
    } else {
      clearFilters();
    }
  }
}

bool NotesModel::isSearchMode() const { return m_isSearchMode; }

void NotesModel::refresh() {
  if (m_currentPath.isEmpty())
    return;
  loadFromIndex();
}

void NotesModel::navigateToFolder(const QString &folderPath) {
  m_folderStack.append(m_currentPath);
  emit folderStackChanged();
  setCurrentPath(folderPath);
}

void NotesModel::navigateBack() {
  if (m_folderStack.isEmpty())
    return;

  QString previousPath = m_folderStack.takeLast();
  emit folderStackChanged();
  setCurrentPath(previousPath);
}

void NotesModel::navigateToRoot() {
  m_folderStack.clear();
  emit folderStackChanged();
  setCurrentPath(m_rootPath);
}

void NotesModel::clearFilters() {
  m_filterString.clear();
  m_filterTag.clear();
  m_isSearchMode = false;
  emit filterStringChanged();
  emit filterTagChanged();
  emit isSearchModeChanged();
  loadFromIndex();
}

void NotesModel::togglePin(const QString &path) {
  QString normalizedPath = path;
  if (normalizedPath.startsWith(QLatin1String("file:///"))) {
    normalizedPath = QUrl(normalizedPath).toLocalFile();
  }
  normalizedPath =
      QDir::cleanPath(QFileInfo(normalizedPath).absoluteFilePath());

  // Read the file
  QFile file(normalizedPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return;
  }
  QString content = QTextStream(&file).readAll();
  file.close();

  // Toggle pinned status in frontmatter
  bool currentlyPinned = false;

  if (content.startsWith(QLatin1String("---"))) {
    int endIdx = content.indexOf(QLatin1String("---"), 3);
    if (endIdx > 0) {
      QString frontmatter = content.mid(3, endIdx - 3);
      QRegularExpression pinnedRegex(QStringLiteral("pinned:\\s*(true|false)"),
                                     QRegularExpression::CaseInsensitiveOption);
      QRegularExpressionMatch match = pinnedRegex.match(frontmatter);
      if (match.hasMatch()) {
        currentlyPinned = match.captured(1).toLower() == QLatin1String("true");
        // Replace the value
        QString newFrontmatter = frontmatter;
        newFrontmatter.replace(
            pinnedRegex, currentlyPinned ? QStringLiteral("pinned: false")
                                         : QStringLiteral("pinned: true"));
        content = QStringLiteral("---") + newFrontmatter + content.mid(endIdx);
      } else {
        // Add pinned: true before the closing ---
        QString newFrontmatter = frontmatter + QStringLiteral("pinned: true\n");
        content = QStringLiteral("---") + newFrontmatter + content.mid(endIdx);
      }
    }
  } else {
    // No frontmatter, add it
    content = QStringLiteral("---\npinned: true\n---\n") + content;
  }

  // Write back
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return;
  }
  QTextStream out(&file);
  out << content;
  file.close();

  // Update the index
  NotesIndex::instance()->updateEntry(normalizedPath);
}

void NotesModel::onEntryUpdated(const QString &path) {
  // meaningful optimization: check if path is in current view or relevant
  // For now, simpler to just reload the view to reflect changes (tags, pin
  // status) Logic is fast enough since it reads from memory index
  loadFromIndex();
}

QString NotesModel::findPathByTitle(const QString &title) {
  return NotesIndex::instance()->findPathByTitle(title);
}

QStringList NotesModel::getBacklinks(const QString &title) {
  return NotesIndex::instance()->getBacklinks(title);
}

void NotesModel::searchContent(const QString &query) {
  if (query.isEmpty()) {
    clearFilters();
    return;
  }

  m_loading = true;
  emit loadingChanged();

  QString rootPath = m_rootPath;
  QString lowerQuery = query.toLower();

  m_contentSearchWatcher->setFuture(QtConcurrent::run([rootPath, lowerQuery]() {
    QVector<NoteItem> results;
    QDirIterator it(rootPath, {"*.md"}, QDir::Files,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
      QString filePath = it.next();
      QFile file(filePath);

      if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Stream line by line for memory efficiency
        QTextStream in(&file);
        bool found = false;

        while (!in.atEnd() && !found) {
          QString line = in.readLine();
          if (line.toLower().contains(lowerQuery)) {
            found = true;
          }
        }
        file.close();

        if (found) {
          QFileInfo info(filePath);
          NoteItem item;
          item.type = QStringLiteral("note");
          item.path = filePath;
          item.title = info.baseName();
          item.date =
              info.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
          item.lastModified = info.lastModified();
          item.color = QStringLiteral("#624a73");
          item.isPinned = false;
          results.append(item);
        }
      }
    }

    return results;
  }));
}

QStringList NotesModel::getAllTags() {
  return NotesIndex::instance()->getAllTags();
}

void NotesModel::startScan() {
  // Legacy method - now uses loadFromIndex
  loadFromIndex();
}

void NotesModel::loadFromIndex() {
  m_loading = true;
  emit loadingChanged();

  NotesIndex *index = NotesIndex::instance();

  QVector<NoteMetadata> metaItems;

  if (!m_filterTag.isEmpty()) {
    // Filter by tag
    metaItems = index->getNotesByTag(m_filterTag);
  } else if (!m_filterString.isEmpty()) {
    // Search by title/tags
    metaItems = index->searchByTitle(m_filterString);
  } else {
    // Normal folder view
    metaItems = index->getItemsInFolder(m_currentPath);
  }

  // Convert NoteMetadata to NoteItem
  beginResetModel();
  m_items.clear();

  for (const NoteMetadata &meta : std::as_const(metaItems)) {
    NoteItem item;
    item.type =
        meta.isFolder ? QStringLiteral("folder") : QStringLiteral("note");
    item.title = meta.title;
    item.path = meta.filePath;
    item.date = meta.lastModified.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    item.color = meta.color;
    item.tags = meta.tags;
    item.isPinned = meta.isPinned;
    item.lastModified = meta.lastModified;
    // Preview is lazy loaded in data()
    m_items.append(item);
  }

  applySorting();
  endResetModel();

  m_loading = false;
  emit loadingChanged();
}

void NotesModel::applySorting() {
  std::sort(m_items.begin(), m_items.end(),
            [](const NoteItem &a, const NoteItem &b) {
              // Pinned first
              if (a.isPinned != b.isPinned)
                return a.isPinned > b.isPinned;
              // Folders first
              if (a.type != b.type)
                return a.type == QLatin1String("folder");
              // Then by lastModified descending
              return a.lastModified > b.lastModified;
            });
}

void NotesModel::onScanFinished() {
  beginResetModel();
  m_items = m_watcher->result();
  applySorting();
  endResetModel();

  m_loading = false;
  emit loadingChanged();
}

void NotesModel::onDirectoryChanged(const QString &path) {
  Q_UNUSED(path);
  refresh();
}

void NotesModel::onIndexReady() {
  // Reload from updated index
  if (!m_currentPath.isEmpty()) {
    loadFromIndex();
  }
}

void NotesModel::onContentSearchFinished() {
  beginResetModel();
  m_items = m_contentSearchWatcher->result();
  m_isSearchMode = true;
  applySorting();
  endResetModel();

  m_loading = false;
  emit loadingChanged();
  emit isSearchModeChanged();
}

QString NotesModel::createPreview(const QString &filePath, int maxLength) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString();
  }

  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  QString result = content;

  // Remove frontmatter
  if (result.startsWith(QLatin1String("---"))) {
    int endIndex = result.indexOf(QLatin1String("---"), 3);
    if (endIndex > 0) {
      result = result.mid(endIndex + 3).trimmed();
    }
  }

  // Truncate to maxLength
  if (result.length() > maxLength) {
    result = result.left(maxLength) + QStringLiteral("...");
  }

  return result;
}
