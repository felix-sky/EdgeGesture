#include "NotesModel.h"
#include <QDateTime>
#include <QRegularExpression>
#include <QTextStream>
#include <QtConcurrent>

NotesModel::NotesModel(QObject *parent)
    : QAbstractListModel(parent),
      m_watcher(new QFutureWatcher<QVector<NoteItem>>(this)),
      m_fsWatcher(new QFileSystemWatcher(this)) {
  connect(m_watcher, &QFutureWatcher<QVector<NoteItem>>::finished, this,
          &NotesModel::onScanFinished);
  connect(m_fsWatcher, &QFileSystemWatcher::directoryChanged, this,
          &NotesModel::onDirectoryChanged);
}

NotesModel::~NotesModel() {
  if (m_watcher->isRunning()) {
    m_watcher->cancel();
    m_watcher->waitForFinished();
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
    return item.preview;
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> NotesModel::roleNames() const {
  return {{TypeRole, "type"}, {TitleRole, "title"}, {PathRole, "path"},
          {DateRole, "date"}, {ColorRole, "color"}, {PreviewRole, "preview"}};
}

QString NotesModel::currentPath() const { return m_currentPath; }

void NotesModel::setCurrentPath(const QString &path) {
  QString normalizedPath = path;
  if (normalizedPath.startsWith(QLatin1String("file:///"))) {
    normalizedPath = QUrl(normalizedPath).toLocalFile();
  }

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

    refresh();
  }
}

QString NotesModel::rootPath() const { return m_rootPath; }

void NotesModel::setRootPath(const QString &path) {
  QString normalizedPath = path;
  if (normalizedPath.startsWith(QLatin1String("file:///"))) {
    normalizedPath = QUrl(normalizedPath).toLocalFile();
  }

  if (m_rootPath != normalizedPath) {
    m_rootPath = normalizedPath;
    emit rootPathChanged();

    // Ensure root directory exists
    QDir dir(m_rootPath);
    if (!dir.exists()) {
      dir.mkpath(m_rootPath);
    }

    // Reset to root
    m_folderStack.clear();
    emit folderStackChanged();
    setCurrentPath(m_rootPath);
  }
}

bool NotesModel::loading() const { return m_loading; }

QStringList NotesModel::folderStack() const { return m_folderStack; }

void NotesModel::refresh() {
  if (m_currentPath.isEmpty())
    return;
  startScan();
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

void NotesModel::startScan() {
  if (m_watcher->isRunning()) {
    m_watcher->cancel();
    m_watcher->waitForFinished();
  }

  m_loading = true;
  emit loadingChanged();

  QString pathToScan = m_currentPath;
  m_watcher->setFuture(QtConcurrent::run(
      [pathToScan]() { return NotesModel::scanDirectory(pathToScan); }));
}

void NotesModel::onScanFinished() {
  beginResetModel();
  m_items = m_watcher->result();
  endResetModel();

  m_loading = false;
  emit loadingChanged();
}

void NotesModel::onDirectoryChanged(const QString &path) {
  Q_UNUSED(path);
  refresh();
}

QVector<NoteItem> NotesModel::scanDirectory(const QString &path) {
  QVector<NoteItem> items;
  QDir dir(path);

  if (!dir.exists()) {
    return items;
  }

  dir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
  dir.setSorting(QDir::DirsFirst | QDir::Name);

  const QFileInfoList entries = dir.entryInfoList();
  for (const QFileInfo &info : entries) {
    // Only include folders and .md files
    if (!info.isDir() &&
        info.suffix().compare(QLatin1String("md"), Qt::CaseInsensitive) != 0) {
      continue;
    }

    NoteItem item;
    item.path = info.absoluteFilePath();
    item.date =
        info.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm"));

    if (info.isDir()) {
      item.type = QStringLiteral("folder");
      item.title = info.fileName();
      item.color = QStringLiteral("#FFB900"); // Default folder color
      item.preview = QString();
    } else if (info.suffix().compare(QLatin1String("md"),
                                     Qt::CaseInsensitive) == 0) {
      item.type = QStringLiteral("note");
      item.title = info.baseName();

      // Read file content for color and preview
      QFile file(info.absoluteFilePath());
      if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString content = in.readAll();
        file.close();

        item.color = parseColor(content);
        item.preview = createPreview(content);
      } else {
        item.color = QStringLiteral("#624a73"); // Default note color
        item.preview = QString();
      }
    }

    items.append(item);
  }

  return items;
}

QString NotesModel::parseColor(const QString &content) {
  // Parse YAML frontmatter for color
  if (content.startsWith(QLatin1String("---"))) {
    int endIndex = content.indexOf(QLatin1String("---"), 3);
    if (endIndex > 0) {
      QString frontmatter = content.mid(3, endIndex - 3);
      QRegularExpression colorRegex(
          QStringLiteral("color:\\s*(#[a-fA-F0-9]+)"));
      QRegularExpressionMatch match = colorRegex.match(frontmatter);
      if (match.hasMatch()) {
        return match.captured(1);
      }
    }
  }
  return QStringLiteral("#624a73"); // Default color
}

QString NotesModel::createPreview(const QString &content, int maxLength) {
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
