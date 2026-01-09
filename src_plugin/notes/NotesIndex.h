#pragma once

#include <QDateTime>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QHash>
#include <QMap>
#include <QMultiHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtQml>

/**
 * @brief Lightweight metadata structure - NO content stored
 */
struct NoteMetadata {
  QString filePath;
  QString title;
  QStringList tags;
  QDateTime lastModified;
  bool isPinned = false;
  bool isFolder = false;
  QString color;
};

/**
 * @brief Singleton index managing metadata for all notes.
 *
 * This class maintains an in-memory index of note metadata using
 * head-only parsing (first 1024 bytes) for memory efficiency.
 * Supports instant lookups by path, tag, and title.
 */
class NotesIndex : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(bool indexing READ isIndexing NOTIFY indexingChanged)
  Q_PROPERTY(int indexProgress READ indexProgress NOTIFY indexProgressChanged)
  Q_PROPERTY(int totalFiles READ totalFiles NOTIFY totalFilesChanged)

public:
  static NotesIndex *instance();
  static NotesIndex *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

  explicit NotesIndex(QObject *parent = nullptr);
  ~NotesIndex() override;

  // Index management
  Q_INVOKABLE void setRootPath(const QString &path);
  Q_INVOKABLE void rebuildIndex();
  Q_INVOKABLE void updateEntry(const QString &path);
  Q_INVOKABLE void removeEntry(const QString &path);
  Q_INVOKABLE void clear();

  // Properties
  bool isIndexing() const;
  int indexProgress() const;
  int totalFiles() const;

  // Lookups
  Q_INVOKABLE NoteMetadata getMetadata(const QString &path) const;
  Q_INVOKABLE QVector<NoteMetadata>
  getItemsInFolder(const QString &folderPath) const;
  Q_INVOKABLE QVector<NoteMetadata> getNotesByTag(const QString &tag) const;
  Q_INVOKABLE QVector<NoteMetadata> searchByTitle(const QString &query) const;
  Q_INVOKABLE QStringList getBacklinks(const QString &title) const;
  Q_INVOKABLE QStringList getAllTags() const;
  Q_INVOKABLE QString findPathByTitle(const QString &title) const;

signals:
  void indexingChanged();
  void indexProgressChanged();
  void totalFilesChanged();
  void indexReady();
  void indexUpdated();
  void entryUpdated(const QString &path);

private slots:
  void onScanFinished();
  void onDirectoryChanged(const QString &path);

private:
  static NotesIndex *s_instance;

  // Core index structures
  QHash<QString, NoteMetadata> m_index;    // path -> metadata
  QMultiHash<QString, QString> m_tagIndex; // tag -> paths
  QMap<QString, QStringList> m_backlinks;  // title -> source paths
  QMap<QString, QString> m_titleToPath;    // title -> path

  // State
  QString m_rootPath;
  bool m_indexing = false;
  int m_indexProgress = 0;
  int m_totalFiles = 0;

  // Watchers
  QFileSystemWatcher *m_fsWatcher = nullptr;
  QFutureWatcher<QVector<NoteMetadata>> *m_watcher = nullptr;

  // Parsing helpers
  static NoteMetadata parseFileHeader(const QString &path);
  static QString parseColor(const QString &frontmatter);
  static QStringList parseTags(const QString &frontmatter);
  static bool parsePinned(const QString &frontmatter);
  static QStringList parseWikiLinks(const QString &content);

  void processIndexResults(const QVector<NoteMetadata> &results);
  void watchDirectory(const QString &path);
};
