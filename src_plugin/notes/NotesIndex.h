#pragma once

#include "ObsidianParser.h"
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
#include <QtQml/qqmlregistration.h>

enum class LinkResolutionKind {
  Found,
  Ambiguous,
  Missing
};

struct LinkResolution {
  LinkResolutionKind kind = LinkResolutionKind::Missing;
  QString target;
  QString heading;
  QString blockId;
  QString alias;
  QStringList matches;
};

/**
 * @brief Lightweight metadata structure for notes and attachments.
 */
struct NoteMetadata {
  QString filePath;
  QString title;
  QStringList tags;
  QStringList aliases;
  QDateTime lastModified;
  bool isPinned = false;
  bool isFolder = false;
  QString color;
};

/**
 * @brief Centralized index managing metadata, backlinks, and link resolution for Obsidian vaults.
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

  // Lookups & Resolution
  Q_INVOKABLE NoteMetadata getMetadata(const QString &path) const;
  Q_INVOKABLE QVector<NoteMetadata> getItemsInFolder(const QString &folderPath) const;
  Q_INVOKABLE QVector<NoteMetadata> getNotesByTag(const QString &tag) const;
  Q_INVOKABLE QVector<NoteMetadata> searchByTitle(const QString &query) const;
  Q_INVOKABLE QStringList getBacklinks(const QString &title) const;
  Q_INVOKABLE QStringList getAllTags() const;
  Q_INVOKABLE QString findPathByTitle(const QString &title) const;
  Q_INVOKABLE QString findAttachment(const QString &name, const QString &currentNotePath = QString()) const;

  // Structured Link Resolution
  LinkResolution resolveLink(const QString &linkText, const QString &currentNotePath = QString()) const;
  Q_INVOKABLE QVariantMap resolveLinkInfo(const QString &linkText, const QString &currentNotePath = QString()) const;

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

  // Multi-index structures
  QHash<QString, NoteMetadata> m_pathIndex;       // normalized absolute path -> metadata
  QMultiHash<QString, QString> m_titleIndex;      // lowercase title -> file paths
  QMultiHash<QString, QString> m_aliasIndex;      // lowercase alias -> file paths
  QMultiHash<QString, QString> m_tagIndex;        // lowercase tag -> file paths
  QHash<QString, QString> m_attachmentIndex;      // lowercase filename -> absolute path
  QMap<QString, QStringList> m_backlinks;         // lowercase canonical target -> list of source note paths

  // Cache of file mtimes and sizes for incremental rescanning
  QHash<QString, QPair<qint64, QDateTime>> m_fileStats;

  // State
  QString m_rootPath;
  bool m_indexing = false;
  int m_indexProgress = 0;
  int m_totalFiles = 0;

  // Watchers
  QFileSystemWatcher *m_fsWatcher = nullptr;
  struct ScanResult {
    QVector<NoteMetadata> notes;
    QHash<QString, QString> attachments;
    QMap<QString, QStringList> backlinks;
  };
  QFutureWatcher<ScanResult> *m_watcher = nullptr;

  // Internal helpers
  static NoteMetadata parseNoteFile(const QString &path, QStringList *outLinks = nullptr);
  void processScanResults(const ScanResult &result);
  void watchDirectoryRecursively(const QString &path);
};
